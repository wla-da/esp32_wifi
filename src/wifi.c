/*
инициализация, сканирование, подключение к сети Wi-Fi, обнаружение целевого веб-сервера, ретраи

используем только STA режим (режим станции)

примеры и документация
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html
https://esp32tutorials.com/esp32-esp-idf-connect-wifi-station-mode-example/
пример подключения к существующей wifi сети
https://github.com/espressif/esp-idf/blob/master/examples/wifi/getting_started/station/main/station_example_main.c
пример wifi сканера
https://github.com/espressif/esp-idf/blob/master/examples/wifi/scan/main/scan.c  


не забываем инициировать NVS, там хранится конфигурация WiFI
при последующих стартах можно читать конфигурацию функциями вида esp_wifi_get_xxx
см детали https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#wi-fi-nvs-flash
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "math.h"


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define TAG "WIFI STANTION"

#define EXAMPLE_ESP_WIFI_SSID       "ssid" //CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      "pass" //CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  3 //CONFIG_ESP_MAXIMUM_RETRY


static int s_retry_num = 0;



static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    /*
    case WIFI_AUTH_OWE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OWE");
        break;
    */
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    /*
    case WIFI_AUTH_WPA3_ENT_192:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENT_192");
        break;
    */
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    case WIFI_CIPHER_TYPE_AES_CMAC128:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_AES_CMAC128");
        break;
    case WIFI_CIPHER_TYPE_SMS4:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_SMS4");
        break;
    /*
    case WIFI_CIPHER_TYPE_GCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP");
        break;
    case WIFI_CIPHER_TYPE_GCMP256:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP256");
        break;
    */
    default:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    case WIFI_CIPHER_TYPE_SMS4:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_SMS4");
        break;
    /*
    case WIFI_CIPHER_TYPE_GCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP");
        break;
    case WIFI_CIPHER_TYPE_GCMP256:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP256");
        break;
    */
    default:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}


/*
обработчик событий FiFi
*/
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } 
        else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail, try=%d", s_retry_num);
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
формирует строку для чтения MAC (BSSID) адреса из байтового массива
@param bssid[6] uint8_t байтовый массив с mac адресом
@param mac char* должен быть проинициализирован вызывающей стороной
*/
static void mac2str(uint8_t bssid[], char mac[]) {
    sprintf(mac, "%X:%X:%X:%X:%X:%X", 
                bssid[0],bssid[1],bssid[2],
                bssid[3],bssid[4],bssid[5]);
}


static void _init_nvs() {
    //не забываем инициализировать NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

/*
сканирование и вывод в консоль информации об обнаруженных Wi-Fi точках доступа
@param max_ap u_int8_t максимальное число точек доступа для вывода подробной информации
*/
void wifi_scan(uint16_t max_ap) {
    _init_nvs();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint16_t number = max_ap;
    wifi_ap_record_t ap_info[max_ap];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total Wi-Fi APs found: %u, first %d detail:\n", ap_count, number);
    char mac[20];
    ap_count = (ap_count < max_ap) ? ap_count : max_ap;
    for (int i = 0; i < ap_count; i++) {
        ESP_LOGI(TAG, "------------Wi-Fi AP: %d----------------------------", i+1);
        ESP_LOGI(TAG, "SSID: \t\t%s", ap_info[i].ssid);
        uint8_t *bssid = ap_info[i].bssid;
        mac2str(bssid, mac); //см макрос MAC2STR в esp_mac.h
        ESP_LOGI(TAG, "BSSID (MAC): \t%s", mac);
        ESP_LOGI(TAG, "RSSI: \t\t%d dB", ap_info[i].rssi);

        print_auth_mode(ap_info[i].authmode);
        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
            print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
        }
        ESP_LOGI(TAG, "Channel \t%d", ap_info[i].primary);
    }
    ESP_LOGI(TAG, "Scan Wi-Fi is complete!\n");
}


/*
инициализация wifi
*/
void wifi_init_sta(void) {   
    _init_nvs();



/*    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            //Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
            //If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
            //to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
            //WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    //Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
    //number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    //xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually happened
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    */
}
