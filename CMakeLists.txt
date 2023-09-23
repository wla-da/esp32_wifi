cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_wifi)

#исправляем баг компиляции https://github.com/espressif/esp-idf/issues/9511#issuecomment-1207817643
#idf_component_get_property(lib espressif__button COMPONENT_LIB)
#target_compile_options(${lib} PRIVATE -Wno-error=format)
