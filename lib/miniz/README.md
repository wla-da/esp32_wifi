## Проблематика

1. Веб-сервера часто используют сжатие контента для минимизации трафика, причем
некоторые из них не реагируют на заголовок клиента, показывающий, что клиент не поддерживает никакого сжатия:\
```Accept-Encoding: identity``` \
Детальнее о 
HTTP заголовке [Accept-Encoding](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Accept-Encoding)
2. Отличия zlib|gzip|deflate, общий вывод: gzip и zlib - это обертки над deflate. Deflate - это комбинация алгоритма LZ77 и алгоритма Хаффмана (см [статью](https://habr.com/ru/articles/221849/)) с дополнительными метаданными. У gzip метаданных больше - заголовок gzip из 10 и более байт и CRC с размером несжатых данных в конце (см [спецификацию gizp v4.3](http://www.zlib.org/rfc-gzip.html) или описание gzip [в коротком виде](https://web.archive.org/web/20200220015003/http://www.onicos.com/staff/iz/formats/gzip.html)). У zlib есть только небольшой заголовок, см [спецификацию zlib](https://datatracker.ietf.org/doc/html/rfc1950). Посмотреть обсуждения можно в [ответе](https://ru.stackoverflow.com/questions/364068/Чем-deflate-отличается-от-gzip) и в [этом обсуждении](https://stackoverflow.com/questions/9170338/why-are-major-web-sites-using-GZIP/9186091#9186091)
3. В esp-idf v5.1 нет явной поддержки сжатия/распаковки compress (LZ-алгоритм)/gzip/deflate/brotli, которую используют веб-сервера для сжатия контента.
4. В esf-idf есть сильно обрезанная библиотека [miniz](https://github.com/richgel999/miniz). Версия miniz 9.1.15 для esp-idf 5.1. Скомпилированная библиотека miniz находится в ROM памяти. Смотреть заголовочный файл [miniz.h](https://github.com/espressif/esp-idf/blob/master/components/esp_rom/include/miniz.h). Данная библиотека предлагает несколько функций для работы с низкоуровневым сжатием/распаковки, таких как: 
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
5. В обрезанной версии miniz удалена функция вида mz_uncompress и ряд других необходимых для распаковки gzip функций вида mz_XXX путем взведения флага  ```MINIZ_NO_ZLIB_APIS``` в miniz.h. При попытке использовать функцию mz_uncompress компоновщик не может собрать итоговый бинарный файл из-за отсутствий функции в предоставляемом производителем объектом файле. Смотреть список экспортируемых функций в [esp32.rom.ld](https://github.com/espressif/esp-idf/blob/master/components/esp_rom/esp32/ld/esp32.rom.ld), есть только примитивы вида tinfl_ХХХ
6. В утилитах прошивальщика от Espressif есть полная версия доработанная под esp32 версия файла [miniz.c](https://github.com/espressif/esptool/blob/master/flasher_stub/miniz.c). Файл miniz.c производитель уже переносил из другого раздела, актуальность ссылки обеспечена на осень 2023.
7. Библиотека miniz НЕ поддерживает заголовки gzip, только zlib, смотри [ответ](https://stackoverflow.com/questions/32225133/how-to-use-miniz-to-create-a-compressed-file-that-can-be-decompressd-by-gzip "How to use miniz to create a compressed file that can be decompressd by gzip?") Mark Adler

## Как решают проблему в интернете
1. Подключение библиотеки [zlib](https://www.zlib.net/) с примерами исходного кода распаковки gzip контента сервера, [подробнее](https://yuanze.wang/posts/esp32-unzip-gzip-http-response/) (китайский, код читаем без переводчика)
2. Попытки подключить полную версию библиотеки [miniz](https://github.com/richgel999/miniz) обсуждаются [тут](https://www.esp32.com/viewtopic.php?t=1076), [тут](https://esp32.com/viewtopic.php?t=9166). Предлагается добавить полный исходный miniz.c файл в свою библиотеку или создать свой [компонент](https://esp32.com/download/file.php?id=3145 "скачать архив с исходным кодом компонента с форума esp32.com"). 
3. Подключение алгоритма Brotli в виде библиотеки для фреймворка Arduino, [подробнее](https://techtutorialsx.com/2018/12/08/esp32-http-2-decompressing-brotli-encoded-content/). Так же также есть [форк алгоритма Brotli](https://github.com/martinberlin/brotli) для esp32 и PlatformIO