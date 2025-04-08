cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(cactuslux_firmware)

#add libraries
#include_directories(include)
#add_library(Adafruit_VEML7700 STATIC lib/Adafruit_VEML7700.cpp)
#add_library(simple_math STATIC lib/simple_math.c)

target_sources(app PRIVATE src/main.c)
#target_link_libraries(app PRIVATE simple_math)
