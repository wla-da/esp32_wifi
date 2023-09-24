#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"

/*
прочитать очередную часть ответа сервера
*/
esp_err_t web_client_read_next(char * buffer, const uint32_t buf_len, uint32_t * write_len);

/*
закрыть последнее подключение, если было открыто
*/
void web_client_close_conn();

/*
получить длину ответа сервера
0 - в случае порций (чанков)
*/
int64_t web_client_get_data_len();

/*
возвращает признак успешного ответа сервера
сейчас успешный - если HTTP код 200
*/
bool web_client_is_http_ok();

/*
отправить GET запрос на хост с указанными заголовками
@param *resp_len uint32_t возвращает длину ответа либо -1, если не возможно определить
*/
esp_err_t web_client_GET(char * host, int port, char * header, const uint8_t header_len, int64_t *resp_len);