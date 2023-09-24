/*
простой веб http клиент без шифрования, авторизации, редиректа, явной поддержкой кук
подключается к указанному хосту, выполняет запрос, позволяет вычитать ответ сервера в буфер порциями

в ESP-IDF есть компонент ESP HTTP Client
https://docs.espressif.com/projects/esp-idf/en/v5.1.1/esp32/api-reference/protocols/esp_http_client.html


в устройстве мало оперативной памяти, вычитать всё содержимое ответа за один запрос сервера нельзя - не хватит памяти
необходимо вычитывать небольшими порциями, передавая обработчику


Типовое использование простого веб клиента
1. отправить запрос на сервер GET(...), проверить возвращенный код
2. проверить ответ сервера web_client_is_http_ok()
3. TODO получить тип файла is_text_content()/сырые заголовки get_response_header_raw() 
4. вычитывать ответ сервера в буфер web_client_read_next(...), пока не будет вычитан полностью либо не произойдет ошибка
5. закрыть подключение web_client_close_conn() 



TODO ограничить число одновременных обрабатываемых подключений, пока возможно только одно одновременное подключение
TODO почитать о мостах в esp-idf в файле esp_netif.h
https://github.com/espressif/esp-idf/blob/master/components/esp_netif/lwip/esp_netif_lwip.c

#if CONFIG_ESP_NETIF_BRIDGE_EN
**
 * @brief Add a port to the bridge
 *
 * @param esp_netif_br Handle to bridge esp-netif instance
 * @param esp_netif_port Handle to port esp-netif instance
 * @return ESP_OK on success
 *
//esp_err_t esp_netif_bridge_add_port(esp_netif_t *esp_netif_br, esp_netif_t *esp_netif_port);
*/


#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "webclient.h"

//таймаут HTTP запроса
#define CONFIG_EXAMPLE_HTTP_TIMEOUT_MS  5*1000


#define TAG "WEB_CLIENT"

//контекст текущего веб-клиент и подключений
static volatile esp_http_client_handle_t client = NULL;
//курсор чтения ответа сервера
static volatile int64_t current_read_pos = 0;


/*
отправить GET запрос на хост с указанными заголовками
@param *resp_len uint32_t возвращает длину ответа либо -1, если не возможно определить
*/
esp_err_t web_client_GET(char * host, int port, char * header, const uint8_t header_len, int64_t *resp_len) {
    //на основе примера http_perform_as_stream_reader(void) 
    //см https://github.com/espressif/esp-idf/blob/v5.1.1/examples/protocols/esp_http_client/main/esp_http_client_example.c#L561

    //TODO пока только одно действующее подключение
    assert(0 == current_read_pos);
    assert(NULL == client);

    esp_http_client_config_t config = {
        //.url = "http://"CONFIG_EXAMPLE_HTTP_ENDPOINT"/get",
        .host = host,
        .port = port,
        .path = "/",
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
    };

    client = esp_http_client_init(&config);
    esp_err_t err;
    if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        web_client_close_conn();
        return err;
    }

    esp_http_client_set_timeout_ms(client, CONFIG_EXAMPLE_HTTP_TIMEOUT_MS);

    //TODO поддержка установки HTTP заголовков
    //esp_http_client_set_header(client, );

    int64_t content_length = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "Content length is %" PRId64, content_length); 

    if (content_length > 0) {
        err = ESP_OK;
        *resp_len = content_length;
    }
    else if(0 == content_length) {
        if(esp_http_client_is_chunked_response(client)) {
            //TODO esp_http_client_get_chunk_length()
            ESP_LOGW(TAG, "Unsupported chunked response!"); 
            err = ESP_ERR_NOT_SUPPORTED;           
        }
        else {
            ESP_LOGW(TAG, "Data empty on GET %s!", host);
            err = ESP_FAIL;
        }
    }
    else if (ESP_FAIL == content_length) {
        ESP_LOGW(TAG, "Fail on GET %s!", host);
        err = ESP_FAIL;
    }
    else if (ESP_ERR_HTTP_EAGAIN == content_length) {
        ESP_LOGW(TAG, "Timeout on GET %s!", host);
        err = ESP_ERR_TIMEOUT;
    }
    else {
        ESP_LOGE(TAG, "Unknown error on GET %s!", host);
        err = ESP_FAIL;
    }
    return err;
}

/*
отправить POST запрос на хост с указанными заголовками и телом
@param *resp_len uint32_t возвращает длину ответа либо -1, если не возможно определить
*/
esp_err_t POST(char * host, char * header, uint8_t header_len, char * post, const uint8_t post_len, uint32_t *resp_len) {
    return ESP_ERR_NOT_SUPPORTED;
}

/*
получить в сыром виде заголовок ответа сервера последнего запроса
*/
esp_err_t get_response_header_raw() {
    return ESP_ERR_NOT_SUPPORTED;
}

/*
возвращает признак успешного ответа сервера
сейчас успешный - если HTTP код 200
*/
bool web_client_is_http_ok() {
    assert(client != NULL);
    //TODO не совсем точно, т.к. бывают и HTTP 201/203 и тп
    return HttpStatus_Ok == esp_http_client_get_status_code(client);
}

/*
пытается определить на основе http заголовков сервера текстовых контент или нет
*/
bool is_text_content() {
    //TODO
    return false;
}

/*
получить длину ответа сервера
0 - в случае порций (чанков)
*/
int64_t web_client_get_data_len() {
    assert(client != NULL);
    return esp_http_client_get_content_length(client);
}


/*
прочитать очередную часть ответа сервера
*/
esp_err_t web_client_read_next(char * buffer, const uint32_t buf_len, uint32_t * write_len) {
    assert(client != NULL);
    assert(buf_len > 0);
    
    const int64_t total_len = esp_http_client_get_content_length(client);
    //определим сколько байт можем еще прочитать с учетом текущего курсора
    int64_t bytes_to_read = buf_len;
    if ((current_read_pos + buf_len) > total_len) {
        bytes_to_read = total_len - current_read_pos; 
    }

    if (0 == bytes_to_read) {
        ESP_LOGI(TAG, "Bytes to read is 0!");
        *write_len = bytes_to_read;
        return ESP_OK;
    }

    esp_err_t err;
    const int read_len = esp_http_client_read(client, buffer, bytes_to_read);
    ESP_LOGI(TAG, "Bytes read is %d", read_len);
    if (read_len > 0) {
        current_read_pos += read_len;
        *write_len = read_len;
        return ESP_OK;
    }
    else if (ESP_FAIL == read_len) {
        ESP_LOGW(TAG, "Fail on read!");
        err = ESP_FAIL;
    }
    else if (ESP_ERR_HTTP_EAGAIN == read_len) {
        ESP_LOGW(TAG, "Timeout on read!");
        err = ESP_ERR_TIMEOUT;
    }
    else {
        ESP_LOGW(TAG, "Unknown erro on read %d!", read_len);
        err = ESP_FAIL;
    }
    return err;
}

/*
закрыть последнее подключение, если было открыто
*/
void web_client_close_conn() {
    if (NULL != client) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    client = NULL;
    current_read_pos = 0;
    ESP_LOGI(TAG, "Connection closed");
}

