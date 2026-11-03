#include "online_setup.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static char nonce[33];
static bool saved;
static esp_timer_handle_t reboot_timer;
bool online_load_config(online_config_t *c) {
    nvs_handle_t n; size_t size=sizeof(*c); memset(c,0,size);
    if(nvs_open("dsh_voice",NVS_READWRITE,&n)!=ESP_OK) return false;
    uint8_t setup=0; nvs_get_u8(n,"setup",&setup);
    if(setup) { nvs_erase_key(n,"setup"); nvs_commit(n); }
    esp_err_t e=nvs_get_blob(n,"config",c,&size); nvs_close(n);
    return !setup && e==ESP_OK && size==sizeof(*c) && online_config_valid(c);
}
static void reboot(void *arg) { (void)arg;esp_restart(); }
#define NETWORK_LIMIT 32
static wifi_ap_record_t nearby[NETWORK_LIMIT];
static uint16_t nearby_count;
static bool scan_ok;
static esp_err_t networks(httpd_req_t *r) {
    cJSON *j=cJSON_CreateObject(),*list=cJSON_AddArrayToObject(j,"networks");
    cJSON_AddBoolToObject(j,"scanned",scan_ok);
    for(unsigned i=0;i<nearby_count;i++) {
        const char *ssid=(const char *)nearby[i].ssid;if(!*ssid)continue;
        bool duplicate=false;
        for(unsigned k=0;k<i;k++)if(!strcmp((const char *)nearby[k].ssid,ssid))duplicate=true;
        if(duplicate)continue;
        cJSON *item=cJSON_CreateObject();cJSON_AddStringToObject(item,"ssid",ssid);
        cJSON_AddNumberToObject(item,"rssi",nearby[i].rssi);
        cJSON_AddBoolToObject(item,"secure",nearby[i].authmode!=WIFI_AUTH_OPEN);cJSON_AddItemToArray(list,item);
    }
    char *raw=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    if(!raw)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"读取列表失败");
    httpd_resp_set_type(r,"application/json; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t e=httpd_resp_sendstr(r,raw);free(raw);return e;
}
extern const unsigned char setup_page_start[] asm("_binary_setup_page_html_start");
extern const unsigned char setup_page_end[] asm("_binary_setup_page_html_end");
static online_config_t previous;
static bool has_previous;
static esp_err_t home(httpd_req_t *r) {
    httpd_resp_set_type(r,"text/html; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    httpd_resp_set_hdr(r,"X-Frame-Options","DENY");
    return httpd_resp_send(r,(const char *)setup_page_start,setup_page_end-setup_page_start-1);
}
static esp_err_t current(httpd_req_t *r) {
    cJSON *j=cJSON_CreateObject();
    cJSON_AddStringToObject(j,"nonce",nonce);
    cJSON_AddBoolToObject(j,"configured",has_previous);
    if(has_previous) {
        cJSON_AddStringToObject(j,"ssid",previous.ssid);
        const char *host=strstr(previous.url,"://");host=host?host+3:previous.url;
        char ip[64]={0};size_t len=strcspn(host,":/");
        if(len<sizeof(ip))memcpy(ip,host,len);
        cJSON_AddStringToObject(j,"ip",ip);
        const char *colon=strchr(host,':');
        cJSON_AddNumberToObject(j,"port",colon?atoi(colon+1):3101);
    }
    char *raw=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    if(!raw)return ESP_ERR_NO_MEM;
    httpd_resp_set_type(r,"application/json; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t result=httpd_resp_sendstr(r,raw);free(raw);return result;
}
static bool field(cJSON *j,const char *key,char *out,size_t cap) {
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);
    if(!cJSON_IsString(v) || strlen(v->valuestring)>=cap) return false;
    strcpy(out,v->valuestring); return true;
}
static esp_err_t save(httpd_req_t *r) {
    char provided[40];
    if(httpd_req_get_hdr_value_str(r,"X-Setup-Nonce",provided,sizeof(provided))!=ESP_OK || strcmp(provided,nonce))
        return httpd_resp_send_err(r,HTTPD_403_FORBIDDEN,"请重新打开配置页");
    if(saved || r->content_len<=0 || r->content_len>1200) return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"配置无效");
    char body[1201];size_t used=0;
    while(used<(size_t)r->content_len) {
        int n=httpd_req_recv(r,body+used,r->content_len-used);
        if(n<=0) return ESP_FAIL;
        used+=n;
    }
    body[used]=0;cJSON *j=cJSON_Parse(body);online_config_t c={.version=1};
    char ip[16]={0};
    cJSON *port=cJSON_GetObjectItemCaseSensitive(j,"port");
    bool keep=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"keepWifi"));
    bool wifi_valid;
    if(keep && has_previous) {
        wifi_valid=online_reuse_wifi(&c,&previous);
    } else wifi_valid=!keep && j && field(j,"ssid",c.ssid,sizeof(c.ssid)) && field(j,"password",c.password,sizeof(c.password));
    bool valid=j && wifi_valid &&
        cJSON_IsNumber(port) && port->valuedouble==port->valueint && port->valueint>0 &&
        field(j,"ip",ip,sizeof(ip)) && online_endpoint_from_ip_port(ip,(unsigned)port->valueint,c.url,sizeof(c.url)) &&
        field(j,"token",c.token,sizeof(c.token)) && online_config_valid(&c);
    cJSON_Delete(j);memset(body,0,sizeof(body));
    if(!valid) {memset(&c,0,sizeof(c));return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"检查 Wi-Fi、电脑 IP 和 4 位配对码");}
    nvs_handle_t n;esp_err_t e=nvs_open("dsh_voice",NVS_READWRITE,&n);
    if(e==ESP_OK) {e=nvs_set_blob(n,"config",&c,sizeof(c));if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);}
    memset(&c,0,sizeof(c));
    if(e!=ESP_OK) return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"保存失败，请重试");
    saved=true;httpd_resp_set_type(r,"text/plain; charset=utf-8");
    httpd_resp_sendstr(r,"已保存，设备即将重启。请让手机重新连接原来的 Wi-Fi。");
    esp_timer_start_once(reboot_timer,1500000);return ESP_OK;
}
esp_err_t online_start_setup(char *description,size_t size) {
    nvs_handle_t stored;size_t stored_size=sizeof(previous);
    if(nvs_open("dsh_voice",NVS_READONLY,&stored)==ESP_OK) {
        has_previous=nvs_get_blob(stored,"config",&previous,&stored_size)==ESP_OK && stored_size==sizeof(previous) && online_config_valid(&previous);
        nvs_close(stored);
    }
    wifi_config_t ap={0};
    snprintf(nonce,sizeof(nonce),"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());
    snprintf((char *)ap.ap.ssid,sizeof(ap.ap.ssid),"DSH-Voice-%04lx",(unsigned long)(esp_random()&65535));
    ap.ap.ssid_len=strlen((char *)ap.ap.ssid);
    ap.ap.authmode=WIFI_AUTH_OPEN;ap.ap.max_connection=1;ap.ap.channel=1;
    if(!esp_netif_create_default_wifi_sta() || !esp_netif_create_default_wifi_ap()) return ESP_FAIL;
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t e=esp_wifi_init(&cfg); if(e!=ESP_OK)return e;
    if((e=esp_wifi_set_storage(WIFI_STORAGE_RAM))!=ESP_OK || (e=esp_wifi_set_mode(WIFI_MODE_STA))!=ESP_OK ||
       (e=esp_wifi_start())!=ESP_OK)return e;
    /* Blocking scan belongs to this boot worker and happens before AP startup. */
    scan_ok=esp_wifi_scan_start(NULL,true)==ESP_OK;
    if(scan_ok) {
        nearby_count=NETWORK_LIMIT;
        if(esp_wifi_scan_get_ap_records(&nearby_count,nearby)!=ESP_OK){nearby_count=0;scan_ok=false;}
    } else esp_wifi_clear_ap_list();
    ESP_LOGI("bean_setup","pre-hotspot scan: %u networks, success=%d",nearby_count,scan_ok);
    if((e=esp_wifi_stop())!=ESP_OK || (e=esp_wifi_set_mode(WIFI_MODE_AP))!=ESP_OK ||
       (e=esp_wifi_set_config(WIFI_IF_AP,&ap))!=ESP_OK || (e=esp_wifi_start())!=ESP_OK)return e;
    esp_timer_create_args_t ta={.callback=reboot,.name="setup_reboot"};
    if((e=esp_timer_create(&ta,&reboot_timer))!=ESP_OK)return e;
    httpd_config_t hc=HTTPD_DEFAULT_CONFIG();hc.stack_size=6144;hc.max_open_sockets=2;hc.lru_purge_enable=true;
    httpd_handle_t server=NULL;if((e=httpd_start(&server,&hc))!=ESP_OK)return e;
    httpd_uri_t get={.uri="/",.method=HTTP_GET,.handler=home};
    httpd_uri_t post={.uri="/save",.method=HTTP_POST,.handler=save};
    httpd_uri_t list={.uri="/networks",.method=HTTP_GET,.handler=networks};
    httpd_uri_t saved_config={.uri="/current",.method=HTTP_GET,.handler=current};
    if((e=httpd_register_uri_handler(server,&get))!=ESP_OK || (e=httpd_register_uri_handler(server,&post))!=ESP_OK || (e=httpd_register_uri_handler(server,&list))!=ESP_OK || (e=httpd_register_uri_handler(server,&saved_config))!=ESP_OK)return e;
    snprintf(description,size,"%s",ap.ap.ssid);
    return ESP_OK;
}
