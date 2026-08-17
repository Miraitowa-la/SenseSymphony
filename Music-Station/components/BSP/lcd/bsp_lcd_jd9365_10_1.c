#include "bsp_lcd_jd9365_10_1.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_jd9365_10_1.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "i2c_bus.h"

/*
 * GT9271 触摸寄存器定义。
 *
 * Product ID 用于探测设备是否存在；Status 寄存器 bit7 表示有新触摸数据，
 * bit[3:0] 表示触摸点数量；触摸点数据从 0x814F 开始，每个触摸点占 8 字节。
 */
#define GT9271_REG_PRODUCT_ID      0x8140
#define GT9271_REG_STATUS          0x814E
#define GT9271_REG_POINT_BASE      0x814F
#define GT9271_STATUS_DATA_READY   (1 << 7)
#define GT9271_STATUS_POINT_MASK   0x0F
#define GT9271_POINT_INFO_BYTES    8

static const char *TAG = "bsp_lcd";

/*
 * LCD 运行期资源。
 *
 * 这些句柄在 bsp_lcd_init() 中创建，在 bsp_lcd_deinit() 中释放。应用层不直接
 * 操作这些静态变量，而是通过本文件提供的 BSP 接口访问。
 */
static esp_lcd_panel_handle_t s_panel_handle;
static esp_lcd_dsi_bus_handle_t s_mipi_dsi_bus;
static esp_lcd_panel_io_handle_t s_mipi_dbi_io;
static esp_ldo_channel_handle_t s_ldo_mipi_phy;
static SemaphoreHandle_t s_refresh_finish;
static bool s_lcd_initialized;

/*
 * 触摸运行期资源。
 *
 * GT9271 通过 I2C 访问。这里保存 i2c_bus 句柄和触摸设备句柄，避免每次读取触摸
 * 都重复创建设备。
 */
static i2c_bus_handle_t s_touch_i2c_bus;
static i2c_bus_device_handle_t s_touch_dev;
static bool s_touch_initialized;

/**
 * @brief DPI 颜色数据传输完成回调。
 *
 * 该回调在 ISR 上下文执行，因此只释放一个信号量，不做日志、不分配内存、
 * 不执行复杂逻辑。应用层通过 bsp_lcd_wait_flush_done() 等待该信号量。
 */
IRAM_ATTR static bool notify_refresh_ready(esp_lcd_panel_handle_t panel,
                                           esp_lcd_dpi_panel_event_data_t *edata,
                                           void *user_ctx)
{
    SemaphoreHandle_t sem = (SemaphoreHandle_t)user_ctx;
    BaseType_t need_yield = pdFALSE;

    xSemaphoreGiveFromISR(sem, &need_yield);

    return need_yield == pdTRUE;
}

esp_err_t bsp_lcd_init(void)
{
    if (s_lcd_initialized) {
        return ESP_OK;
    }

    /*
     * 1. 配置并打开背光。
     *
     * 背光不是 panel init 的一部分，通常由独立 GPIO 控制。先打开背光便于观察
     * 后续初始化是否成功。
     */
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BSP_LCD_BK_LIGHT_GPIO,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bk_gpio_config), TAG, "configure backlight GPIO failed");
    ESP_RETURN_ON_ERROR(bsp_lcd_set_backlight(true), TAG, "turn on backlight failed");

    /*
     * 2. 打开 MIPI DSI PHY 供电。
     *
     * ESP32-P4 的 MIPI DSI PHY 需要通过 LDO 供电后才能工作。
     */
    ESP_LOGI(TAG, "Power on MIPI DSI PHY");
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = BSP_LCD_MIPI_PHY_LDO_CHAN,
        .voltage_mv = BSP_LCD_MIPI_PHY_LDO_VOLTAGE_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &s_ldo_mipi_phy), TAG,
                        "enable MIPI PHY LDO failed");

    /*
     * 3. 创建 MIPI DSI bus。
     *
     * JD9365_PANEL_BUS_DSI_2CH_CONFIG() 默认配置 2 lane、1500 Mbps lane bit rate。
     */
    ESP_LOGI(TAG, "Initialize MIPI DSI bus");
    esp_lcd_dsi_bus_config_t bus_config = JD9365_PANEL_BUS_DSI_2CH_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_config, &s_mipi_dsi_bus), TAG, "create DSI bus failed");

    /*
     * 4. 创建 DBI command IO。
     *
     * MIPI-DSI 屏既需要 DPI 视频流，也需要 DBI command 通道发送初始化命令。
     */
    ESP_LOGI(TAG, "Install MIPI DBI panel IO");
    esp_lcd_dbi_io_config_t dbi_config = JD9365_PANEL_IO_DBI_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(s_mipi_dsi_bus, &dbi_config, &s_mipi_dbi_io), TAG,
                        "create DBI IO failed");

    /*
     * 5. 创建 JD9365 panel。
     *
     * dpi_config 描述视频时序和像素格式；vendor_config 将 DSI bus、DPI 配置、
     * lane 数传入 JD9365 组件。
     */
    ESP_LOGI(TAG, "Install JD9365 panel driver");
    esp_lcd_dpi_panel_config_t dpi_config =
        JD9365_800_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    dpi_config.num_fbs = 2;
    jd9365_vendor_config_t vendor_config = {
        .flags = {
            .use_mipi_interface = 1,
        },
        .mipi_config = {
            .dsi_bus = s_mipi_dsi_bus,
            .dpi_config = &dpi_config,
            .lane_num = BSP_LCD_MIPI_DSI_LANE_NUM,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_jd9365(s_mipi_dbi_io, &panel_config, &s_panel_handle), TAG,
                        "create JD9365 panel failed");

    /*
     * 6. 按标准顺序启动 panel。
     *
     * esp_lcd_panel_init() 内部会发送 JD9365 默认初始化序列，并启动底层 DPI panel。
     */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel_handle), TAG, "reset panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel_handle), TAG, "init panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel_handle, true), TAG, "turn on display failed");

    /*
     * 7. 注册 DPI 刷新完成回调。
     *
     * draw_bitmap() 只是提交传输，数据真正传完需要等 on_color_trans_done。这里用
     * 二值信号量把 ISR 回调转成任务可等待的同步点。
     */
    s_refresh_finish = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_refresh_finish, ESP_ERR_NO_MEM, TAG, "create refresh semaphore failed");

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = notify_refresh_ready,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_register_event_callbacks(s_panel_handle, &cbs, s_refresh_finish), TAG,
                        "register refresh callback failed");

    s_lcd_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_lcd_deinit(void)
{
    esp_err_t ret = ESP_OK;

    /*
     * 按创建顺序的反向释放资源。这里保留最后一次释放动作的返回值，便于发现
     * 资源释放过程中的错误。
     */
    if (s_panel_handle) {
        ret = esp_lcd_panel_del(s_panel_handle);
        s_panel_handle = NULL;
    }
    if (s_mipi_dbi_io) {
        ret = esp_lcd_panel_io_del(s_mipi_dbi_io);
        s_mipi_dbi_io = NULL;
    }
    if (s_mipi_dsi_bus) {
        ret = esp_lcd_del_dsi_bus(s_mipi_dsi_bus);
        s_mipi_dsi_bus = NULL;
    }
    if (s_ldo_mipi_phy) {
        ret = esp_ldo_release_channel(s_ldo_mipi_phy);
        s_ldo_mipi_phy = NULL;
    }

    /*
     * 触摸设备依赖同一组 I2C 引脚，但生命周期独立，因此统一在 LCD deinit 中
     * 做一次兜底释放。
     */
    bsp_touch_deinit();

    if (s_refresh_finish) {
        vSemaphoreDelete(s_refresh_finish);
        s_refresh_finish = NULL;
    }

    gpio_reset_pin(BSP_LCD_BK_LIGHT_GPIO);
    s_lcd_initialized = false;
    return ret;
}

esp_lcd_panel_handle_t bsp_lcd_get_panel(void)
{
    return s_panel_handle;
}

esp_err_t bsp_lcd_get_frame_buffers(void **fb0, void **fb1)
{
    ESP_RETURN_ON_FALSE(s_panel_handle && fb0 && fb1, ESP_ERR_INVALID_STATE, TAG,
                        "LCD frame buffers are not available");
    return esp_lcd_dpi_panel_get_frame_buffer(s_panel_handle, 2, fb0, fb1);
}

i2c_master_bus_handle_t bsp_touch_get_i2c_bus_handle(void)
{
    return s_touch_i2c_bus ? i2c_bus_get_internal_bus_handle(s_touch_i2c_bus) : NULL;
}

esp_err_t bsp_lcd_set_backlight(bool on)
{
    return gpio_set_level(BSP_LCD_BK_LIGHT_GPIO, on ? BSP_LCD_BK_LIGHT_ON_LEVEL : BSP_LCD_BK_LIGHT_OFF_LEVEL);
}

esp_err_t bsp_lcd_display_on(bool on)
{
    ESP_RETURN_ON_FALSE(s_panel_handle, ESP_ERR_INVALID_STATE, TAG, "LCD is not initialized");
    return esp_lcd_panel_disp_on_off(s_panel_handle, on);
}

esp_err_t bsp_lcd_invert_color(bool invert)
{
    ESP_RETURN_ON_FALSE(s_panel_handle, ESP_ERR_INVALID_STATE, TAG, "LCD is not initialized");
    return esp_lcd_panel_invert_color(s_panel_handle, invert);
}

esp_err_t bsp_lcd_mirror(bool mirror_x, bool mirror_y)
{
    ESP_RETURN_ON_FALSE(s_panel_handle, ESP_ERR_INVALID_STATE, TAG, "LCD is not initialized");
    return esp_lcd_panel_mirror(s_panel_handle, mirror_x, mirror_y);
}

esp_err_t bsp_lcd_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    ESP_RETURN_ON_FALSE(s_panel_handle, ESP_ERR_INVALID_STATE, TAG, "LCD is not initialized");
    ESP_RETURN_ON_FALSE(color_data, ESP_ERR_INVALID_ARG, TAG, "color data is NULL");
    return esp_lcd_panel_draw_bitmap(s_panel_handle, x_start, y_start, x_end, y_end, color_data);
}

esp_err_t bsp_lcd_wait_flush_done(TickType_t timeout_ticks)
{
    ESP_RETURN_ON_FALSE(s_refresh_finish, ESP_ERR_INVALID_STATE, TAG, "refresh semaphore is not created");
    return xSemaphoreTake(s_refresh_finish, timeout_ticks) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

/**
 * @brief 尝试按指定 I2C 地址创建 GT9271 设备。
 *
 * GT9271 常见地址有 0x14 和 0x5D。函数创建设备后读取 Product ID，读取成功
 * 才认为触摸芯片存在。
 */
static esp_err_t touch_try_create_device(uint8_t address)
{
    i2c_bus_device_handle_t dev = i2c_bus_device_create(s_touch_i2c_bus, address, BSP_TOUCH_I2C_CLK_HZ);
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_NOT_FOUND, TAG, "create GT9271 device 0x%02X failed", address);

    uint8_t product_id[4] = {0};
    esp_err_t ret = i2c_bus_read_reg16(dev, GT9271_REG_PRODUCT_ID, sizeof(product_id), product_id);
    if (ret != ESP_OK) {
        i2c_bus_device_delete(&dev);
        return ret;
    }

    s_touch_dev = dev;
    ESP_LOGI(TAG, "GT9271 touch found at 0x%02X, product id: %c%c%c%c",
             address, product_id[0], product_id[1], product_id[2], product_id[3]);
    return ESP_OK;
}

esp_err_t bsp_touch_init(void)
{
    if (s_touch_initialized) {
        return ESP_OK;
    }

    /*
     * 创建触摸 I2C bus。
     *
     * i2c_bus 组件采用单例模型，同一个硬件 I2C 端口多次 create 会复用或更新内部
     * bus 状态。这里选择与屏幕板载器件相同的 I2C1/GPIO7/GPIO8。
     */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BSP_TOUCH_I2C_SDA_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = BSP_TOUCH_I2C_SCL_GPIO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BSP_TOUCH_I2C_CLK_HZ,
    };

    s_touch_i2c_bus = i2c_bus_create((i2c_port_t)BSP_TOUCH_I2C_PORT, &conf);
    ESP_RETURN_ON_FALSE(s_touch_i2c_bus, ESP_FAIL, TAG, "create touch I2C bus failed");

    /*
     * GT9271 地址可能由硬件 reset/INT 状态决定。这里优先尝试 Waveshare 文档
     * 常见地址 0x14，再尝试备用地址 0x5D。
     */
    esp_err_t ret = touch_try_create_device(BSP_TOUCH_GT9271_ADDR_PRIMARY);
    if (ret != ESP_OK) {
        ret = touch_try_create_device(BSP_TOUCH_GT9271_ADDR_SECONDARY);
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "GT9271 touch not found");

    /*
     * 清除一次状态寄存器，避免上电残留数据影响第一次读取。
     */
    uint8_t clear_status = 0;
    ESP_RETURN_ON_ERROR(i2c_bus_write_reg16(s_touch_dev, GT9271_REG_STATUS, 1, &clear_status), TAG,
                        "clear touch status failed");

    s_touch_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_touch_deinit(void)
{
    if (s_touch_dev) {
        i2c_bus_device_delete(&s_touch_dev);
        s_touch_dev = NULL;
    }

    /*
     * 不主动删除 I2C bus。
     *
     * JD9365 LCD 组件内部也会使用 I2C1/GPIO7/GPIO8 控制屏幕板载器件。直接删除
     * I2C bus 可能影响 LCD 驱动内部已创建的资源，因此这里只清空本 BSP 保存的
     * bus 引用。
     */
    s_touch_i2c_bus = NULL;
    s_touch_initialized = false;
    return ESP_OK;
}

esp_err_t bsp_touch_read_points(bsp_touch_data_t *data)
{
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "touch data is NULL");
    ESP_RETURN_ON_FALSE(s_touch_dev, ESP_ERR_INVALID_STATE, TAG, "touch is not initialized");

    data->count = 0;

    /*
     * 先读取状态寄存器。bit7 未置位表示没有新的触摸数据，此时直接返回 count=0。
     */
    uint8_t status = 0;
    ESP_RETURN_ON_ERROR(i2c_bus_read_reg16(s_touch_dev, GT9271_REG_STATUS, 1, &status), TAG,
                        "read touch status failed");

    if ((status & GT9271_STATUS_DATA_READY) == 0) {
        return ESP_OK;
    }

    /*
     * 状态寄存器低 4 bit 是触摸点数量。为防止异常数据越界，限制在
     * BSP_TOUCH_MAX_POINTS 以内。
     */
    uint8_t point_count = status & GT9271_STATUS_POINT_MASK;
    if (point_count > BSP_TOUCH_MAX_POINTS) {
        point_count = BSP_TOUCH_MAX_POINTS;
    }

    /*
     * 每个触摸点 8 字节：
     * byte0: track id
     * byte1-2: X 坐标，低字节在前
     * byte3-4: Y 坐标，低字节在前
     * byte5-6: size
     * byte7: 保留
     */
    if (point_count > 0) {
        uint8_t point_buf[BSP_TOUCH_MAX_POINTS * GT9271_POINT_INFO_BYTES] = {0};
        ESP_RETURN_ON_ERROR(i2c_bus_read_reg16(s_touch_dev, GT9271_REG_POINT_BASE,
                                               point_count * GT9271_POINT_INFO_BYTES, point_buf),
                            TAG, "read touch points failed");

        data->count = point_count;
        for (uint8_t i = 0; i < point_count; i++) {
            const uint8_t *p = &point_buf[i * GT9271_POINT_INFO_BYTES];
            data->points[i].id = p[0];
            data->points[i].x = p[1] | ((uint16_t)p[2] << 8);
            data->points[i].y = p[3] | ((uint16_t)p[4] << 8);
            data->points[i].size = p[5] | ((uint16_t)p[6] << 8);
        }
    }

    /*
     * GT9271 要求主机读取完触摸数据后写 0 清除状态寄存器，否则下一次可能读到
     * 同一批触摸数据。
     */
    uint8_t clear_status = 0;
    ESP_RETURN_ON_ERROR(i2c_bus_write_reg16(s_touch_dev, GT9271_REG_STATUS, 1, &clear_status), TAG,
                        "clear touch status failed");

    return ESP_OK;
}
