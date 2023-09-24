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
#include "esp_netif.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "math.h"


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define TAG "WIFI_STA"

//параметры поключения к Wi-Fi сети---------------------------------------------------
#define EXAMPLE_ESP_WIFI_SSID               "MyWiFi" //CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS               "mypass" //CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY           3 //CONFIG_ESP_MAXIMUM_RETRY
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SAE_MODE                   WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER              ""
#define EXAMPLE_ESP_MAX_WAIT_CONNECT_MS     30*1000  
//конец параметры поключения к Wi-Fi сети----------------------------------------------


//флаг инициализации подсистемы wifi
static bool is_wifi_init = false;

//счетчик количества попыток подключения к wifi сети
static volatile int s_retry_num = 0;

static esp_netif_t *sta_netif;



static void _print_auth_mode(int authmode)
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

static void _print_cipher_type(int pairwise_cipher, int group_cipher)
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
    
    case WIFI_CIPHER_TYPE_GCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP");
        break;
    case WIFI_CIPHER_TYPE_GCMP256:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP256");
        break;
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
    case WIFI_CIPHER_TYPE_GCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP");
        break;
    case WIFI_CIPHER_TYPE_GCMP256:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP256");
        break;
    default:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}


/*
обработчик событий WiFi
*/
static void _wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    assert(WIFI_EVENT == event_base);
    ESP_LOGI(TAG, "WIFI_EVENT id=%" PRId32, event_id); //события WiFi определены в перечислении wifi_event_t

    if (WIFI_EVENT_STA_START == event_id) {
        esp_wifi_connect();
    }
    else if (WIFI_EVENT_STA_CONNECTED == event_id) {
        wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data; 
        ESP_LOGI(TAG, "wi-fi success connected with auth id=%d", event->aid);
    }
    else if (WIFI_EVENT_STA_DISCONNECTED == event_id) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } 
        else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }

        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data; 
        //детальные причины дисконекта приведены в перечислении wifi_err_reason_t в файле esp_wifi_types.h
        ESP_LOGW(TAG,"connect to the AP fail, try=%d, reason=%d", s_retry_num, event->reason);
    } 
}

/*
обработчик событий IP
*/
static void _ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    assert(IP_EVENT == event_base);
    ESP_LOGI(TAG, "IP_EVENT id=%" PRId32, event_id);

    //события, связанные с IP определены перечислением ip_event_t из файла esp_netif_types.h   
    //    typedef enum {
    //        IP_EVENT_STA_GOT_IP,               /*!< station got IP from connected AP */
    //        IP_EVENT_STA_LOST_IP,              /*!< station lost IP and the IP is reset to 0 */
    //        IP_EVENT_AP_STAIPASSIGNED,         /*!< soft-AP assign an IP to a connected station */
    //        IP_EVENT_GOT_IP6,                  /*!< station or ap or ethernet interface v6IP addr is preferred */
    //        IP_EVENT_ETH_GOT_IP,               /*!< ethernet got IP from connected AP */
    //        IP_EVENT_ETH_LOST_IP,              /*!< ethernet lost IP and the IP is reset to 0 */
    //        IP_EVENT_PPP_GOT_IP,               /*!< PPP interface got IP */
    //        IP_EVENT_PPP_LOST_IP,              /*!< PPP interface lost IP */
    //    } ip_event_t;

    if (IP_EVENT_STA_GOT_IP == event_id) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        //сбрасываем счетчик попыток установки подключений
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (IP_EVENT_STA_LOST_IP == event_id) {
        //TODO получить новый IP адрес, лучше переустановить wi-fi подключение в отдельном потоке
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
базовая инициация NVS и дефолтной конфигурации WiFi
делается один раз, регулируется флагом is_wifi_init
*/
static void _init_wifi_default() {
    if(is_wifi_init) {
        return;
    }

    sta_netif = NULL;

    _init_nvs();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //успешно завершили инициализацию подсистемы WiFi
    is_wifi_init = true;
    ESP_LOGI(TAG, "Wi-Fi init complete!");
}


/*
распечатать IP адреса шлюза, клиента, маску сети, mac
*/
static void _print_tcpip_info() {
    assert(sta_netif != NULL);

    //читаем mac адреса разными способами
    uint8_t mac = 0;
    esp_efuse_mac_get_default(&mac);
    ESP_LOGI(TAG, "Efuse MAC:\t"MACSTR, MAC2STR(&mac));

    mac = 0;
    esp_base_mac_addr_get(&mac);
    ESP_LOGI(TAG, "Base MAC:\t"MACSTR, MAC2STR(&mac));

    mac = 0;
    esp_read_mac(&mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "STA MAC:\t"MACSTR, MAC2STR(&mac));

    mac = 0;
    esp_read_mac(&mac, ESP_MAC_ETH);
    ESP_LOGI(TAG, "Ether MAC:\t"MACSTR, MAC2STR(&mac));

    mac = 0;
    esp_netif_get_mac(sta_netif, &mac);
    ESP_LOGI(TAG, "WiFi MAC:\t"MACSTR, MAC2STR(&mac));
      
    //получаем IP адреса шлюза, станции, маску подсети
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(sta_netif, &ip_info);

    ESP_LOGI(TAG, "My IP:\t" IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(TAG, "Gateway IP:\t" IPSTR, IP2STR(&ip_info.gw));
    ESP_LOGI(TAG, "Netmask:\t" IPSTR, IP2STR(&ip_info.netmask));
}


/*
сканирование и вывод в консоль информации об обнаруженных Wi-Fi точках доступа
@param max_ap u_int8_t максимальное число точек доступа для вывода подробной информации
*/
void wifi_scan(uint16_t max_ap) {
    _init_wifi_default();

    uint16_t number = max_ap;
    wifi_ap_record_t ap_info[max_ap];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    //для автоматической сортировки сетей можно задать параметр конфигурации sort_method  
    //в структуре wifi_sta_config_t 
    //см так же wifi_sort_method_t
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
        mac2str(bssid, mac); //см также макрос MAC2STR в esp_mac.h
        ESP_LOGI(TAG, "BSSID (MAC): \t%s", mac);
        ESP_LOGI(TAG, "RSSI: \t\t%d dB", ap_info[i].rssi);

        _print_auth_mode(ap_info[i].authmode);
        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
            _print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
        }
        ESP_LOGI(TAG, "Channel \t%d", ap_info[i].primary);
    }

    esp_wifi_clear_ap_list();//очистим память после сканирования
    ESP_LOGI(TAG, "Scan Wi-Fi net's is complete!\n");
}


/*
инициализация wifi
*/
void wifi_init_sta(void) {   
    _init_wifi_default();

    s_wifi_event_group = xEventGroupCreate();


    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &_wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        ESP_EVENT_ANY_ID, //IP_EVENT_STA_GOT_IP,
                                                        &_ip_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            //если длина пароля больше 8 символов, то режим авторизации переключается к стандарту WPA2
            //если необходимо подключение к устаревшим WEP/WPA сетям, необходимо установить длину пароля и 
            //явно указать режим авторизации WIFI_AUTH_WEP или WIFI_AUTH_WPA_PSK 
            //см также описание структуры wifi_auth_mode_t
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi connecting waiting...");

    //ожидаем либо установки битов (WIFI_CONNECTED_BIT | WIFI_FAIL_BIT) с учетом попыток переподкючения, либо 
    //завершения времени ожидания EXAMPLE_ESP_MAX_WAIT_CONNECT_MS
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(EXAMPLE_ESP_MAX_WAIT_CONNECT_MS));
            //portMAX_DELAY);

    //xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually happened
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
        //получим назначенный нам IP адрес и IP адрес шлюза 
        _print_tcpip_info();
    } 
    else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } 
    else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT: bits=%" PRIu32, bits);
    }
}

/*
Останавливаем подсистему WiFi, очищаем ресурсы
источник https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/esp_ble_mesh/coex_test/components/case/wifi_connect.c
*/
/*void stop(void) {
    is_wifi_init = false;
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip));
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif));
    esp_netif_destroy(sta_netif);
    sta_netif = NULL;
    //esp_netif_deinit();
}*/
