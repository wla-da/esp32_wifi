## Проблематика

1. Веб-сервера часто используют сжатие контента для минимизации трафика, причем
некоторые из них не реагируют на заголовок клиента, показывающий, что клиент не поддерживает никакого сжатия:\
```Accept-Encoding: identity```\
Детальнее о 
HTTP заголовке [Accept-Encoding](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Accept-Encoding)\
2. В esp-idf v5.1 нет явной поддержки сжатия/распаковки compress (LZ-алгоритм)/gzip/deflate/brotli, которую используют веб-сервера для сжатия контента.\
3. В esf-idf есть сильно обрезанная библиотека [miniz](https://github.com/richgel999/miniz) версии 9.1.15 для esp-idf 5.1.\
Находится в ROM памяти, заголовочный файл [miniz.h](https://github.com/espressif/esp-idf/blob/master/components/esp_rom/include/miniz.h). Данная библиотека предлагает несколько функций для работы с низкоуровневым сжатием/распаковки, таких как: 
``` 
....
tinfl_decompressor *decomp = calloc(1, sizeof(tinfl_decompressor));
assert(decomp != NULL);
tinfl_init(decomp);
tinfl_status decomp_status = tinfl_decompress(decomp, 
                                    in_buffer, &inbytes, 
                                    out_buffer, out_buffer[TINFL_LZ_DICT_SIZE], &outbytes, 
                                    TINFL_FLAG_PARSE_ZLIB_HEADER);
...
``` 
4. В утилитах прошивальщика от Espressif есть полная версия доработанная под esp32 версия файла [miniz.h](https://github.com/espressif/esptool/blob/master/flasher_stub/miniz.c). Файл miniz.h производитель уже переносил из другого раздела, актуальность ссылки обеспечена на осень 2023.




## Как решают проблему в интернете
1. Подключение алгоритма Brotli в виде библиотеки для фреймворка Arduino, [подробнее](https://techtutorialsx.com/2018/12/08/esp32-http-2-decompressing-brotli-encoded-content/) 
2. Подключение библиотеки [zlib](https://www.zlib.net/) с примерами кода распаковки gzip контента сервера, [подробнее](https://yuanze.wang/posts/esp32-unzip-gzip-http-response/) (китайский, код читаем и без переводчика)
3. Попытки подключить полную версию библиотеки [miniz](https://github.com/richgel999/miniz) обсуждаются [тут](https://www.esp32.com/viewtopic.php?t=1076), [тут](https://esp32.com/viewtopic.php?t=9166). Предлагается добавить полный исходный miniz.c файл в свою библиотеку или создать свой [компонент](https://esp32.com/download/file.php?id=3145 "скачать архив с исходным кодом компонента с форума esp32.com"). 