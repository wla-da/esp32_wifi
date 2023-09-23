#include "esp_log.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "sys/unistd.h" //для usleep
#include "soc/rtc.h"
#include "nvs_flash.h"
#include "esp_app_format.h"
#include "esp_chip_info.h"
#include "esp_app_desc.h" //в esp-idf v5

#include "wifi.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#define TAG "MAIN"


//вывод общей информации  о системе для отладки
void print_chip_info() {
    ESP_LOGI(TAG, "ESP-IDF version [%s]", esp_get_idf_version());
    ESP_LOGI(TAG, "Free heap %" PRIu32 " kb, minimum %" PRIu32 " kb", esp_get_free_heap_size()>>10, esp_get_minimum_free_heap_size()>>10);
      
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "CPU core %" PRIu8, chip_info.cores);
    ESP_LOGI(TAG, "CPU revision %" PRIu16, chip_info.revision);

    //информация о версии приложения esp_app_get_description() или esp_app_get_elf_sha256()/esp_ota_get_partition_description()
    //в esp-idf v5
    
    const esp_app_desc_t *app_desc = esp_app_get_description();
    
    ESP_LOGI(TAG, "Project: %s", app_desc->project_name);
    ESP_LOGI(TAG, "App version: %s", app_desc->version);
    ESP_LOGI(TAG, "Compile date: %s %s", app_desc->date, app_desc->time);
    

    //установить частоту работы CPU
    //void rtc_clk_cpu_freq_set_config(const rtc_cpu_freq_config_t* config);
    rtc_cpu_freq_config_t cpu_freq;
    rtc_clk_cpu_freq_get_config(&cpu_freq);
    ESP_LOGI(TAG, "CPU freq=%" PRIu32 " MHz, source freq=%" PRIu32 " MHz, divider=%" PRIu32, cpu_freq.freq_mhz, cpu_freq.source_freq_mhz, cpu_freq.div);
    ESP_LOGI(TAG, "APB freq=%0.2f MHz", rtc_clk_apb_freq_get()/(1000.0*1000));
}


void app_main() {

    usleep(3L*1000*1000); //костыль, чтобы успел подключиться монитор USB порта
    print_chip_info();

    wifi_scan(10);

    //wifi_init_sta();
}