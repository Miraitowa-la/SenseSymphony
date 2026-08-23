# SenseSymphony｜感知交响

> 用视觉感知连接动作与音乐的交互式音乐系统。

SenseSymphony 将人脸、表情与手势识别接入触摸屏音乐主机，并通过移动端 App 提供蓝牙连接、曲谱管理和成绩查看能力。用户可以用身体动作触发音符、完成节奏演奏，或参与多人互动音乐体验。

## 项目亮点

- **AI 视觉交互**：摄像头节点支持人脸、表情、手部与手势识别，并通过 UART 将识别结果发送至主机。
- **三种音乐模式**：自由演奏与录制、下落式节奏演奏、基于人脸的单人/双人/多人互动演奏。
- **触摸屏音乐主机**：基于 LVGL 的图形界面，支持音频播放、成绩记录、用户曲谱存储和触摸操作。
- **移动端协同**：Android App（当前版本 **1.0.1**）通过经典蓝牙 SPP 连接设备，提供首页、录制历史、挑战成绩、曲目管理与个人中心入口。
- **完整硬件资料**：提供可编辑的 SolidWorks 外壳模型及 STL 打印模型。

## 系统架构

```text
┌─────────────────────┐      UART      ┌─────────────────────┐    Bluetooth SPP    ┌─────────────────────┐
│ Vision-Node         │ ────────────► │ Music-Station       │ ◄─────────────────► │ Mobile-APP          │
│ 摄像头与 AI 视觉节点 │                │ 触摸屏音乐交互主机   │                    │ Android 移动端应用   │
└─────────────────────┘                └─────────────────────┘                    └─────────────────────┘
           │                                         │
           │ 人脸 / 表情 / 手势                      │ 音频、节奏、存储、显示
           ▼                                         ▼
      感知用户动作                               生成互动音乐体验
```

## 硬件平台与官方资料

本项目使用 **Waveshare ESP32-P4-Module-DEV-KIT-C** 作为硬件开发平台，基于 ESP32-P4 与 ESP32-C6，配合 MIPI-CSI 摄像头、MIPI-DSI 触摸屏及音频外设实现视觉感知和音乐交互。

- [开发板产品页：ESP32-P4-Module-DEV-KIT-C](https://www.waveshare.net/shop/ESP32-P4-Module-DEV-KIT-C.htm)
- [官方开发文档：ESP32-P4-Module-DEV-KIT](https://docs.waveshare.net/ESP32-P4-Module-DEV-KIT/)
- ESP-IDF：**v5.5.4**

## 核心功能

### Mode 1：自由演奏

基于手势识别触发音符，可录制演奏过程、回放记录并查看历史数据。

### Mode 2：节奏演奏

提供下落式音符玩法与判定、连击、得分统计。内置曲目可切换，也支持在移动端编辑并上传自定义曲谱。

### Mode 3：感知互动

通过人脸检测驱动互动音符，支持单人、双人和多人模式，使多人能够共同参与音乐演奏。

## 仓库结构

```text
SenseSymphony/
├── Vision-Node/       # 摄像头 AI 视觉节点（ESP-IDF）
├── Music-Station/     # 触摸屏音乐主机（ESP-IDF + LVGL）
├── Mobile-APP/        # Android 移动端（Vue 3 + uni-app）
└── 3D-Model/          # 外壳 3D 设计与打印模型
```

| 目录 | 说明 | 关键入口 |
| --- | --- | --- |
| [`Vision-Node`](Vision-Node) | 采集摄像头画面并执行人脸/手势检测 | `main/main_slave.c` |
| [`Music-Station`](Music-Station) | 运行 LVGL 交互界面、音乐模式、存储与通信服务 | `main/main.c`、`main/ui/home_screen.c` |
| [`Mobile-APP`](Mobile-APP) | Vue 3 + uni-app Android App；通过蓝牙 SPP 管理录制、成绩、曲目和自定义曲谱 | `App.vue`、`services/spp.js` |
| [`3D-Model`](3D-Model) | 外壳的 SolidWorks、STL 等模型文件 | `*.SLDPRT`、`*.STL` |

## 开发环境

| 子项目 | 推荐环境 |
| --- | --- |
| `Vision-Node` | ESP-IDF **v5.5.4**、CMake、ESP32-P4-Module-DEV-KIT-C 对应工具链 |
| `Music-Station` | ESP-IDF **v5.5.4**、CMake、LVGL、ESP32-P4-Module-DEV-KIT-C 显示与触摸屏 BSP |
| `Mobile-APP` | HBuilderX / uni-app、Vue 3、Android 真机或模拟环境 |
| `3D-Model` | SolidWorks（编辑）、切片软件或 3D 查看器（STL） |

## 构建与运行

### 1. 构建视觉节点

在已初始化 **ESP-IDF v5.5.4** 环境的终端中进入 `Vision-Node`，按 ESP32-P4-Module-DEV-KIT-C 的实际串口执行构建与烧录：

```bash
cd Vision-Node
idf.py set-target <target>
idf.py build
idf.py -p <serial-port> flash monitor
```

### 2. 构建音乐主机

在已初始化 **ESP-IDF v5.5.4** 环境的终端中进入 `Music-Station`：

```bash
cd Music-Station
idf.py set-target <target>
idf.py build
idf.py -p <serial-port> flash monitor
```

> 首次构建前，请参考上述 Waveshare 官方文档完成 ESP32-P4-Module-DEV-KIT-C 的驱动、串口和 ESP-IDF v5.5.4 环境配置。

### 3. 运行移动端 App

1. 使用 HBuilderX 打开 `Mobile-APP` 目录。
2. 选择 Android App 平台运行或云打包。
3. 在 Android 设备中开启蓝牙，并与主机端使用的串口蓝牙模块完成系统配对；当前 App 会优先显示 **QianSai-MusicLab**。
4. 在首页扫描已配对设备并建立 SPP 连接；底部导航可进入录制历史、挑战成绩和曲目管理。
5. 若重新打包，请保留 `Mobile-APP/unpackage/res/icons/`：该目录包含 `manifest.json` 引用的 Android/iOS 应用图标。

### 移动端 1.0.1 更新

- 将“歌曲与成绩”中转页改为五栏底部导航：首页、录制历史、成绩、我的曲目和我的。
- 更新首页、录制历史、录制详情、成绩和曲目管理的移动端界面；录制轨迹画布会根据手机屏幕宽度自适应。
- 新增个人中心入口，并补充 Android/iOS 多尺寸应用图标。
- 默认推荐蓝牙设备名称调整为 `QianSai-MusicLab`；SPP UUID 和主机通信协议保持不变。

## 贡献指南

欢迎提交 Issue 和 Pull Request。提交前请注意：

- 不要提交 `build/`、`unpackage/` 中的缓存、编译产物、安装包、签名证书或本地 IDE 缓存；仅 `Mobile-APP/unpackage/res/icons/` 是打包所需的图标源资源，应保留并提交。
- ESP-IDF 项目应提交源代码、`CMakeLists.txt`、分区表和依赖锁定文件；请勿提交本机生成的 `sdkconfig`。
- 变更蓝牙或 UART 协议时，请同步检查主机端与移动端的协议兼容性。
- 新增资源时，请确认其拥有可公开发布的版权与授权。

## 许可证

本项目采用 [MIT License](LICENSE) 开源。使用、修改或分发本项目时，请保留原始版权与许可证声明。

---

**SenseSymphony｜让感知成为旋律，让互动化作交响。**
