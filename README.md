# Эксперименты с сетью wi-fi и lan

### Постановка задачи ###
1. Реализовать подключение к существующей Wi-Fi сети 2,4 ГГц, где находится целевой веб сервер на определенном IP и порту 80
2. Поднять проксирующий веб-сервер, доступный по локальной сети ethernet (LAN)
3. Обеспечить проксирование запросов, приходяющие на локальный (LAN) веб-сервер к целевому в wifi сети с модификацией определенных данных на лету
4. Желательно, использовать питание PoE

### Окружение 
* Операционная система Ubuntu 22.04.3 LTS
* Visual Studio Code v1.82.2
* PlatformioIO Core v6.1.11
* ESP-IDF v5.1.1 см инструкцию по установке https://registry.platformio.org/tools/platformio/framework-espidf
* Плата esp32doit-devkit-v1, пока без LAN ![esp32doit-devkit-v1](/img/esp32_devkit_v1.png) стоит припаять кондесатор 4,7-10 мкф параллельно кнопке EN, обеспечивает более стабильную перезагрузку платы при прошивке, не требуется нажимать кнопки, смотри https://esp8266.ru/forum/threads/v-klon-esp32-devkit-v1-avtomaticheski-ne-zalivaetsja-proshivka.4072/
* Существует плата esp32 с PoE на борту "T-Internet-POE" от lilygo ~2800 руб на осень 2023 https://www.lilygo.cc/products/t-internet-poe пример использования https://habr.com/ru/articles/547044/ 

### Особенности
1. В ESP-IDF v5 стала более жесткая проверка типов данных, 
см https://github.com/espressif/esp-idf/issues/9511 
Предложен обходной путь, использовать константы PRIu32 из файла inttypes.h для исправления бага: 
`format '%d' expects argument of type 'int', but argument 6 has type 'uint32_t' {aka 'long unsigned int'} [-Werror=format=]` см https://github.com/espressif/esp-protocols/commit/71401a0f2ffb5a0b374a692e50e4fa7d0e12397b

2. Platformio пока не поддерживает плату T-Internet-POE от lilygo, есть запрос от 2021 года, пока не решен https://github.com/platformio/platform-espressif32/issues/475
