# Эксперименты с сетью wi-fi и lan

## Постановка задачи 
1. Реализовать подключение к существующей Wi-Fi сети 2,4 ГГц, где находится целевой веб сервер на определенном IP и порту 80
2. Поднять проксирующий веб-сервер, доступный по локальной сети ethernet (LAN)
3. Обеспечить проксирование запросов, приходящие на локальный (LAN) веб-сервер к целевому в wifi сети с модификацией определенных данных на лету
4. Желательно, использовать питание PoE

## Окружение 
* Операционная система Ubuntu 22.04.3 LTS
* Visual Studio Code v1.82.2
* PlatformioIO Core v6.1.11
* ESP-IDF v5.1.1 см инструкцию по установке https://registry.platformio.org/tools/platformio/framework-espidf
* Плата esp32doit-devkit-v1, пока без LAN ![esp32doit-devkit-v1](/img/esp32_devkit_v1.png) стоит припаять кондесатор 4,7-10 мкф параллельно кнопке EN, обеспечивает более стабильную перезагрузку платы при прошивке, не требуется нажимать кнопки, [подробнее]( https://esp8266.ru/forum/threads/v-klon-esp32-devkit-v1-avtomaticheski-ne-zalivaetsja-proshivka.4072/)
* Существует плата esp32 с PoE на борту "T-Internet-POE" от Lilygo ~2800 руб на осень 2023 https://www.lilygo.cc/products/t-internet-poe пример использования https://habr.com/ru/articles/547044/ 


## Способы подключения проводной сети Ethernet к микроконтроллеру
### Варианты подключений микроконтроллера к Ethernet *
![Способы подключения мк к Ethernet](/img/ethernet_mc.png)
### Контроллер LAN8720A с реализацией физического уровня (PHY) и шиной RMII **
![LAN8720A](/img/LAN8720A.png)
### Контроллер Ethernet ENC28J60 с реализацией физического (PHY), канального (MAC) уровней и шиной SPI *
![ENC28J60](/img/enc28j60.png)
### Контроллер Ethernet W5500 с реализацией физического (PHY), канального (MAC), транспортного (TCP/UDP) уровней и шиной SPI *
![W5500](/img/w5500.png)

*Скриншоты взяты из видео [Ethernet для МК. W5500 и ENC28J60. MQTT](https://www.youtube.com/watch?v=LwDDEIx63cA) за авторством [Руслана](https://www.youtube.com/@rnadyrshin/about)  \
**Скриншот блок схемы LAN8720A взят из официальной документации на микросхему (datasheet)

### Выбор дополнительного "железа" для esp32
1. Esp32 "из коробки" поддерживает канальный уровень (MAC) и транспортный уровень (TCP/UDP).\
Для реализации физического уровня (PHY) требуется внешняя микросхема. Подключение к ней производится по шине RMII (упрощенный MII), которую поддерживает esp32.
2. Проводное подключение через разъем RJ45 к локальной сети Ethernet может быть осуществлено быть несколькими способами:\
        2.1. через шину RMII для подключения к внешней реализации физического уровня (PHY). Канальный (MAC) и транспортный (TCP/UDP) уровни в этом используются из самого esp32\
        2.2. через шину SPI. Потребуется внешний модуль с реализацией, как минимум, физического (PHY) и канального (MAC) уровней. Транспортный уровень (TCP/UDP) можно будет использовать либо во внешнем контроллере, либо из самого esp32
3. Питание платы с esp32 производим через Ethernet (PoE), чтобы уменьшить количество адаптеров и проводов

[Подробнее о поддержке Ethernet в esp32](https://docs.espressif.com/projects/esp-idf/en/release-v5.1/esp32/api-reference/network/esp_eth.html)

Наиболее популярная недорогая плата esp32 с поддержкой Ethernet (без PoE) является WT32-ETH01.\
Физический уровень реализован на микросхеме LAN8720A, связь с esp32 по шине RMII.\
В проекте умного дома [ESP-home](https://esphome.io) одновременная работа данной платы и в режиме Wi-Fi и через сетевой кабель не поддерживается.
Стоимость платы WT32-ETH01 на осень 2023 порядка 900-1100 руб.

Предварительно выбрана плата T-Internet-POE от Lilygo.\
Так же интересной выглядит плата PoESP32 от M5Stack, имеет пластиковый корпус, небольшие размеры, прошивка "из коробки" поддерживает AT команды.\  
См примеры других плат esp32 с PoE далее.

### Примеры плат esp32 с PoE

| Модуль | Фото | Цена на осень 2023, тыс руб  | Ссылка  |
|----|---|---|---|
| ESP32-Ethernet-Kit от Espressif</br></br>[ESP32-Ethernet-Kit V1.2](https://docs.espressif.com/projects/esp-idf/en/release-v5.1/esp32/hw-reference/esp32/get-started-ethernet-kit.html)</br>[ESP32-Ethernet-Kit V1.1](https://docs.espressif.com/projects/esp-idf/en/release-v4.1/hw-reference/get-started-ethernet-kit.html#get-started-esp32-ethernet-kit-v1-1) | ![ESP32-Ethernet-Kit V1.2](/img/esp32-ethernet-kit-v1.2-overview.png)  | 8-10 | [Ali ESP32-Ethernet-Kit v1.2](https://aliexpress.ru/item/1005004066004397.html?sku_id=12000027924152484)  |
|[T-Internet-POE](https://www.lilygo.cc/products/t-internet-poe) от lilygo |![T-Internet-POE](/img/T-Internet-POE-Lilygo_11.png)|2,8-3,2 включая программатор| [Ali T-Internet-POE](https://aliexpress.ru/item/4001122992446.html) |
|[wESP32](https://www.crowdsupply.com/silicognition/wesp32)|![wesp32](/img/wesp32-top.jpg)|~7 с программатором + доставка отдельно|[Tindie wESP32](https://www.tindie.com/products/silicognition/wesp32/)|
|[PoESP32](https://docs.m5stack.com/en/unit/poesp32) от M5Stack|![PoESP32](/img/poesp32_01.png)|~3|[M5Stack PoESP32](https://shop.m5stack.com/products/esp32-ethernet-unit-with-poe)  [Ali PoESP32](https://aliexpress.ru/item/1005004106438265.html)|
|Esp32-Stick-PoE-A от [Aleksei Karavaev](https://www.tindie.com/products/allexok/esp32-stick-poe-a16mb-flash/)|![Esp32-Stick-PoE-A](/img/Esp32-Stick-PoE-A.jpg)|~2,7 с программатором + доставка отдельно|[Tindie Esp32-Stick-PoE-A](https://www.tindie.com/products/allexok/esp32-stick-poe-a16mb-flash/)|
|ESP32-POE open source hardware|![ESP32-POE](/img/ESP32-POE.jpg)|~2 с программатором + доставка отдельно|[ESP32-POE](https://www.olimex.com/Products/IoT/ESP32/ESP32-POE/open-source-hardware)|



## Особенности
1. В ESP-IDF v5 стала более жесткая проверка типов данных, 
см https://github.com/espressif/esp-idf/issues/9511   
Предложен обходной путь, использовать константы вида PRIu32 из файла inttypes.h для исправления бага:  
`format '%d' expects argument of type 'int', but argument 6 has type 'uint32_t' {aka 'long unsigned int'} [-Werror=format=]`  
[Детальнее](https://github.com/espressif/esp-protocols/commit/71401a0f2ffb5a0b374a692e50e4fa7d0e12397b)

2. PlatformIO пока не поддерживает плату T-Internet-POE от Lilygo, есть запрос от 2021 года на поддержку платы, на осень 2023 не решен https://github.com/platformio/platform-espressif32/issues/475
