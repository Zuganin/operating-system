# Минимальная версия CMake
cmake_minimum_required(VERSION 3.10)

# Название проекта
project(IHW_1 C)

# Настройки компиляции
set(CMAKE_C_STANDARD 17)  # Указываем стандарт языка
set(CMAKE_C_FLAGS "-g -Wall")  # Флаги компиляции (отладка и предупреждения)

# Добавляем директорию с заголовочными файлами
include_directories(include)

# Указываем все файлы исходников
file(GLOB SOURCES "src/*.c")

# Создаем исполняемый файл
add_executable(main2 main2.c)
