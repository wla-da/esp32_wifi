# Эксперименты с сетью wi-fi и lan

### Постановка задачи ###
1. реализовать подключение к существующей wifi сети 2,4 ГГц, где находится целевой веб сервер на определенном IP и порту 80
2. поднять проксирующий веб-сервер, доступный по локальной сети ethernet (LAN)
3. обеспечить проксирование запросов, приходяющие на локальный (LAN) веб-сервер к целевому в wifi сети 
с модификацией определенных данных лету

### Окружение 
* Visual Studio Code v1.82.2
* PlatformioIO Core v6.1.11
* ESP-IDF v5.1.1 см инструкцию по установке https://registry.platformio.org/tools/platformio/framework-espidf


### Особенности
* в ESP-IDF v5 стала более жесткая проверка типов данных, 
см https://github.com/espressif/esp-idf/issues/9511
format '%d' expects argument of type 'int', but argument 6 has type 'uint32_t' {aka 'long unsigned int'} [-Werror=format=]
https://github.com/espressif/esp-protocols/commit/71401a0f2ffb5a0b374a692e50e4fa7d0e12397b