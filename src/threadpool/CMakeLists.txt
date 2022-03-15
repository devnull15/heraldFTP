cmake_minimum_required(VERSION 3.13)

set(PROJECT threadpool)
set(DEPENDS ll)

project(${PROJECT} LANGUAGES "C")

add_compile_options(-Werror -Wextra -Wall -pedantic -g -fsanitize=address)
link_libraries(-fsanitize=address)

include_directories(include)
include_directories(../${DEPENDS}/include/)

set(SOURCES src/${PROJECT})

add_library(${PROJECT} SHARED ${SOURCES})
