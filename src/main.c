#include "esp_log.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "sys/unistd.h" //для usleep
#include "soc/rtc.h"



#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#define TAG "MAIN"



void print_chip_info() {
    ESP_LOGI(TAG, "Free heap %d kb, minimum %d kb", esp_get_free_heap_size()>>10, esp_get_minimum_free_heap_size()>>10);
    
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "CPU core %d", chip_info.cores);
       
    //установить частоту работы CPU
    //void rtc_clk_cpu_freq_set_config(const rtc_cpu_freq_config_t* config);
    rtc_cpu_freq_config_t cpu_freq;
    rtc_clk_cpu_freq_get_config(&cpu_freq);
    ESP_LOGI(TAG, "CPU freq=%d MHz, source freq=%d MHz, divider=%d", cpu_freq.freq_mhz, cpu_freq.source_freq_mhz, cpu_freq.div);
    ESP_LOGI(TAG, "APB freq=%0.2f MHz", rtc_clk_apb_freq_get()/(1000.0*1000));
    
    ESP_LOGI(TAG, "ESP-IDF version [%s]", esp_get_idf_version());
}


void app_main() {

    while (true)
    {
        print_chip_info();
        usleep(5L*1000*1000);
    }
        


}