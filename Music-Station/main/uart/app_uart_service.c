#include "app_uart_service.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage/mode1_history_service.h"
#include "storage/mode2_history_service.h"
#include "storage/mode2_user_song_service.h"
#include "ui/home_screen.h"
#define APP_UART_PORT UART_NUM_1
#define APP_UART_TX_GPIO GPIO_NUM_4
#define APP_UART_RX_GPIO GPIO_NUM_5
#define APP_UART_BAUD_RATE 9600
static TaskHandle_t s_task;
static mode2_user_song_t s_upload_song;
static uint8_t s_upload_received;
static bool s_uploading;
static void send_line(const char *format, ...) { char line[96]; va_list args; va_start(args,format); int n=vsnprintf(line,sizeof(line),format,args); va_end(args); if(n>0) uart_write_bytes(APP_UART_PORT,line,n<(int)sizeof(line)?n:sizeof(line)-1); }
static void handle_line(const char *line) {
    if(!strcmp(line,"M1,LIST")) { send_line("M1,LIST,%u\n",mode1_history_count()); for(uint8_t slot=0;slot<MODE1_HISTORY_MAX_RECORDS;slot++){uint8_t count;uint32_t sequence;if(mode1_history_get(slot,NULL,&count,&sequence))send_line("M1,ITEM,%u,%lu,%u\n",slot,(unsigned long)sequence,count);} send_line("M1,END\n"); return; }
    unsigned slot; if(sscanf(line,"M1,GET,%u",&slot)==1 && slot<MODE1_HISTORY_MAX_RECORDS) { mode1_history_event_t events[MODE1_HISTORY_MAX_EVENTS];uint8_t count;uint32_t sequence;if(!mode1_history_get(slot,events,&count,&sequence)){send_line("M1,ERR,NOT_FOUND\n");return;}send_line("M1,REC,%u,%lu,%u\n",slot,(unsigned long)sequence,count);for(uint8_t i=0;i<count;i++)send_line("M1,EV,%lu,%u,%u,%u,%u\n",(unsigned long)events[i].time_ms,events[i].band,events[i].note,events[i].x10,events[i].y10);send_line("M1,END\n");return; }
    if(sscanf(line,"M1,DELETE,%u",&slot)==1 && slot<MODE1_HISTORY_MAX_RECORDS) { send_line(mode1_history_delete(slot) ? "M1,OK,DELETE,%u\n" : "M1,ERR,NOT_FOUND\n",slot); return; }
    if(!strcmp(line,"M2,LIST")) { send_line("M2,LIST,%u\n",mode2_history_count()); for(uint8_t item_slot=0;item_slot<MODE2_HISTORY_MAX_RECORDS;item_slot++){mode2_history_result_t result;uint32_t sequence;if(mode2_history_get(item_slot,&result,&sequence))send_line("M2,ITEM,%u,%lu,%u,%lu,%u,%u,%u,%u,%u,%u\n",item_slot,(unsigned long)sequence,result.song_id,(unsigned long)result.score,result.max_combo,result.accuracy,result.perfect,result.great,result.good,result.miss);} send_line("M2,END\n"); return; }
    if(sscanf(line,"M2,DELETE,%u",&slot)==1 && slot<MODE2_HISTORY_MAX_RECORDS) { send_line(mode2_history_delete(slot) ? "M2,OK,DELETE,%u\n" : "M2,ERR,NOT_FOUND\n",slot); return; }
    if(!strcmp(line,"M2,SONGS")) { send_line("M2,SONGS,%u,%u\n",home_screen_mode2_song_count(),home_screen_mode2_selected_song()); for(uint8_t id=0;id<home_screen_mode2_song_id_limit();id++){const char *name;if(home_screen_mode2_song_get(id,&name))send_line("M2,SONG,%u,%s\n",id,name);} send_line("M2,END\n"); return; }
    unsigned song_id; if(sscanf(line,"M2,SELECT,%u",&song_id)==1 && song_id<256) { const char *name;if(!home_screen_mode2_song_get((uint8_t)song_id,&name)){send_line("M2,ERR,NOT_FOUND\n");return;}send_line(home_screen_mode2_select_song((uint8_t)song_id) ? "M2,OK,SELECT,%u\n" : "M2,ERR,BUSY\n",song_id); return; }
    char name[MODE2_USER_SONG_NAME_LEN]; unsigned beat_ms, event_count;
    if(sscanf(line,"M2,UPLOAD,BEGIN,%20[^,],%u,%u",name,&beat_ms,&event_count)==3) { if(beat_ms<200 || beat_ms>2000 || !event_count || event_count>MODE2_USER_SONG_MAX_EVENTS){send_line("M2,ERR,INVALID\n");return;}memset(&s_upload_song,0,sizeof(s_upload_song));strncpy(s_upload_song.name,name,sizeof(s_upload_song.name)-1);s_upload_song.beat_ms=beat_ms;s_upload_song.event_count=event_count;s_upload_received=0;s_uploading=true;send_line("M2,OK,UPLOAD_BEGIN\n");return; }
    unsigned time_ms, note; if(s_uploading && sscanf(line,"M2,UPLOAD,NOTE,%u,%u",&time_ms,&note)==2) { if(s_upload_received>=s_upload_song.event_count || note>=7 || time_ms>120000 || (s_upload_received && time_ms<=s_upload_song.events[s_upload_received-1].time_ms)){s_uploading=false;send_line("M2,ERR,INVALID\n");return;}s_upload_song.events[s_upload_received++]=(mode2_user_song_event_t){.time_ms=time_ms,.note=note};send_line("M2,OK,NOTE,%u\n",s_upload_received);return; }
    if(!strcmp(line,"M2,UPLOAD,COMMIT")) { uint8_t id;if(!s_uploading || s_upload_received!=s_upload_song.event_count){s_uploading=false;send_line("M2,ERR,INCOMPLETE\n");return;}s_uploading=false;if(mode2_user_song_save(&s_upload_song,&id))send_line("M2,OK,UPLOAD,%u\n",id);else send_line("M2,ERR,FULL\n");return; }
    if(!strcmp(line,"M2,UPLOAD,CANCEL")) { s_uploading=false;send_line("M2,OK,CANCEL\n");return; }
    if(sscanf(line,"M2,SONG,DELETE,%u",&song_id)==1 && song_id<256) { if(song_id<MODE2_USER_SONG_ID_BASE){send_line("M2,ERR,READONLY\n");return;}send_line(home_screen_mode2_delete_song((uint8_t)song_id)?"M2,OK,SONG_DELETE,%u\n":"M2,ERR,NOT_FOUND\n",song_id);return; }
    send_line("M1,ERR,COMMAND\n");
}
static void app_uart_task(void *arg) { (void)arg;char line[96];size_t length=0;for(;;){char byte;if(uart_read_bytes(APP_UART_PORT,&byte,1,pdMS_TO_TICKS(50))!=1)continue;if(byte=='\r')continue;if(byte=='\n'){line[length]='\0';if(length)handle_line(line);length=0;}else if(length<sizeof(line)-1)line[length++]=byte;else length=0;} }
esp_err_t app_uart_service_start(void) { if(s_task)return ESP_OK;uart_config_t config={.baud_rate=APP_UART_BAUD_RATE,.data_bits=UART_DATA_8_BITS,.parity=UART_PARITY_DISABLE,.stop_bits=UART_STOP_BITS_1,.flow_ctrl=UART_HW_FLOWCTRL_DISABLE,.source_clk=UART_SCLK_DEFAULT};ESP_RETURN_ON_ERROR(uart_driver_install(APP_UART_PORT,512,512,0,NULL,0),"app_uart","install");ESP_RETURN_ON_ERROR(uart_param_config(APP_UART_PORT,&config),"app_uart","config");ESP_RETURN_ON_ERROR(uart_set_pin(APP_UART_PORT,APP_UART_TX_GPIO,APP_UART_RX_GPIO,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE),"app_uart","pins");return xTaskCreate(app_uart_task,"app_uart",4096,NULL,4,&s_task)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;}
