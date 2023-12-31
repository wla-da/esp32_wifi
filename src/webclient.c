/*
простой веб http клиент без шифрования, авторизации, редиректа, явной поддержкой кук
подключается к указанному хосту, выполняет запрос, позволяет вычитать ответ сервера в буфер порциями

в ESP-IDF есть компонент ESP HTTP Client
https://docs.espressif.com/projects/esp-idf/en/v5.1.1/esp32/api-reference/protocols/esp_http_client.html
https://github.com/espressif/esp-idf/blob/release/v5.1/components/esp_http_client/esp_http_client.c 

ESP-IFD есть компонент HTTP Parser
https://github.com/espressif/esp-idf/blob/release/v5.1/components/http_parser/http_parser.c


в устройстве мало оперативной памяти, вычитать всё содержимое ответа за один запрос сервера нельзя - не хватит памяти
необходимо вычитывать небольшими порциями, передавая обработчику


Типовое использование простого веб клиента
1. отправить запрос на сервер web_client_GET(...), проверить возвращенный код
2. проверить ответ сервера web_client_is_http_ok()
3. получить тип содержимого сервера is_text_content()/сырые заголовки get_response_header_raw() 
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

#include "miniz.h" //встроенная библиотека сжатия/распаковки zlib https://github.com/richgel999/miniz/

#include "webclient.h"


#define HTTP_HEADER_CONTENT_TYPE                "content-type"
#define HTTP_HEADER_CONTENT_TYPE_TEXT_STARTS     "text/"

//типы поддерживаемого сжатия контента, см https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Accept-Encoding
#define HTTP_HEADER_ACCEPT_ENCODING             "Accept-Encoding"
#define HTTP_HEADER_ACCEPT_ENCODING_VALUES      "gzip"//"identity"

//таймаут HTTP запроса
#define CONFIG_HTTP_TIMEOUT_MS  15*1000


#define TAG "WEB_CLIENT"

//контекст текущего веб-клиент и подключений
static volatile esp_http_client_handle_t client = NULL;
//курсор чтения ответа сервера
static volatile int64_t current_read_pos = 0;
//является ли контент, переданный сервером, текстовым
static volatile bool is_text_content_resp = false;



/*
обработчик событий html парсера
*/
static esp_err_t _on_http_event(esp_http_client_event_t *event) {
    //см так же https://esp32.com/viewtopic.php?f=13&t=21598
    if (HTTP_EVENT_ON_HEADER != event->event_id) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "HTTP [%s]:[%s]", event->header_key, event->header_value);

    if (strcasecmp(HTTP_HEADER_CONTENT_TYPE, event->header_key) != 0) {
        return ESP_OK;
    }

    if (strncasecmp(HTTP_HEADER_CONTENT_TYPE_TEXT_STARTS, 
                    event->header_value, 
                    strlen(HTTP_HEADER_CONTENT_TYPE_TEXT_STARTS)) == 0) {
        ESP_LOGI(TAG, "HTTP response is text: [%s]:[%s]", event->header_key, event->header_value);
        is_text_content_resp = true;        
    } 
    return ESP_OK;
}


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
        .path = "/",// "/production.png",
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        //ставим обработчик событий HTTP парсера для обработки HTTP заголовков
        .event_handler = _on_http_event,
    };

    client = esp_http_client_init(&config);
    assert(NULL != client);

    //TODO поддержка установки HTTP заголовков
    //пока не поддерживаем сжатие 
    esp_err_t res;
    res = esp_http_client_set_header(client, HTTP_HEADER_ACCEPT_ENCODING, HTTP_HEADER_ACCEPT_ENCODING_VALUES);
    if (ESP_OK == res) {
        ESP_LOGI(TAG, "Success set header [%s:%s]", 
                HTTP_HEADER_ACCEPT_ENCODING, HTTP_HEADER_ACCEPT_ENCODING_VALUES);
    }
    else {
        ESP_LOGW(TAG, "Error on set header [%s:%s]! (error: %s)", 
                HTTP_HEADER_ACCEPT_ENCODING, HTTP_HEADER_ACCEPT_ENCODING_VALUES, esp_err_to_name(res)); 
    }
   
    res = esp_http_client_set_timeout_ms(client, CONFIG_HTTP_TIMEOUT_MS);
    if (ESP_OK == res) {
        ESP_LOGI(TAG, "Success set timeout connection %i", CONFIG_HTTP_TIMEOUT_MS);
    }
    else {
        ESP_LOGW(TAG, "Error on set timeout connection %i! (error: %s)", 
                CONFIG_HTTP_TIMEOUT_MS, esp_err_to_name(res)); 
    }

    esp_err_t err;
    if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        web_client_close_conn();
        return err;
    }

    int64_t content_length = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "HTTP status=%i, content length is %" PRId64, 
                    esp_http_client_get_status_code(client), content_length); 

    if (content_length > 0) {
        err = ESP_OK;
        *resp_len = content_length;
    }
    else if(0 == content_length) {
        if(esp_http_client_is_chunked_response(client)) {
            //ВАЖНО! esp-idf возводит флаг is_chunked_response в случае длины ответа сервера <= 0 в esp_http_client_fetch_headers() 
            //описание стандарта в части использования заголовка Content-Length https://datatracker.ietf.org/doc/html/rfc7230#section-3.3.2
            int chunk_len = 0;
            res = esp_http_client_get_chunk_length(client, &chunk_len);
            //TODO 
            ESP_LOGW(TAG, "Unsupported chunked response! chunk_len=%i (error: %s)", chunk_len, esp_err_to_name(res)); 
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
если определить тип контента не получилось так же возвращает false
возвращает true, если Content-type начинается с "text/" (text/css,text/csv,text/html,text/javascript,text/plain,text/xml etc)
*/
bool is_text_content() {
    assert(client != NULL);
    //см https://esp32.com/viewtopic.php?f=13&t=21598
    return is_text_content_resp;
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
TODO вынести в библиотечный метод распаковки gzip
пробует распаковать переданные gzip данные в переданный буфер (данные будут перезатерты)
использует zlib, сделана по мотивам https://yuanze.wang/posts/esp32-unzip-gzip-http-response/
встроенный в esp-idf miniz сильно обрезан, нет функций для работы с архивами https://www.esp32.com/viewtopic.php?t=1076
возвращает true в случае успеха распаковки данных, false в иных случаях
*/
bool static _ungzip(char * buffer, uint32_t *byte_count, const uint32_t buf_len) {
/*
    mz_ulong dest_len, source_len;
    dest_len = 10*1024;// TINFL_LZ_DICT_SIZE;
    source_len = *byte_count;

    mz_uint8 *buf_uncompress = malloc(dest_len);

    ESP_LOGI(TAG, "Uncompress start dest_len=%ld, source_len=%ld", dest_len, source_len);
    int res = mz_uncompress(buf_uncompress, &dest_len, (unsigned char *) buffer, source_len);
    
    ESP_LOGI(TAG, "Uncompress res=%i, [%s]", res, mz_error(res));
*/


    int header_szie = 24;
    int tail_size = 8;
    *byte_count = *byte_count - header_szie - tail_size;
    
    size_t outbytes = 2*TINFL_LZ_DICT_SIZE;
    
    //mz_version();
    mz_uint8 *buf_uncompress = malloc(outbytes);

    tinfl_decompressor *decomp = calloc(1, sizeof(tinfl_decompressor));
    assert(decomp != NULL);
    tinfl_init(decomp);
    
    
    //используем библиотеку miniz для распаковки данных
    //size_t res = tinfl_decompress_mem_to_mem(
    //    buf_uncompress,  /*выходной буфер */
    //    TINFL_LZ_DICT_SIZE,  /*размер выходного буфера, должен быть степенью двойки и не меньше TINFL_LZ_DICT_SIZE 32кБ*/
    //    buffer + header_szie, *byte_count, /*входной буфер с сжатыми данными*/
    //    TINFL_FLAG_PARSE_ZLIB_HEADER /*никаких флагов не передаем*/);
    //ESP_LOGI(TAG, "Uncompress status=%i, outbytes=%" PRIu32, res, *byte_count);
    
    
    
    size_t inbytes = *byte_count;
    ESP_LOGI(TAG, "Uncompress inbytes=%d, outbytes=%d", inbytes, outbytes);
    tinfl_status decomp_status = tinfl_decompress(decomp, 
                                    (mz_uint8 *) (buffer + header_szie), &inbytes, 
                                    buf_uncompress, buf_uncompress, &outbytes, 
                                    0);//TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    ESP_LOGI(TAG, "Uncompress status=%d, inbytes=%d, outbytes=%d", decomp_status, inbytes, outbytes);

    //if (TINFL_DECOMPRESS_MEM_TO_MEM_FAILED == res) {
    if (TINFL_STATUS_DONE != decomp_status) {
        free(buf_uncompress);
        *byte_count = 0;
        return false;
    }
    //копируем распакованные данные в переданный буфер
    if (buf_len < outbytes) {
        *byte_count = buf_len;
    }
    else {
        *byte_count = outbytes;
    }
    memcpy(buffer, buf_uncompress, *byte_count);
    free(buf_uncompress);
    
    return true;
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
        //если данные запакованы, распакуем их
        _ungzip(buffer, write_len, buf_len);
        ESP_LOGI(TAG, "write_len=%" PRId32, *write_len);
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
    is_text_content_resp = false;
    ESP_LOGI(TAG, "Connection closed");
}

