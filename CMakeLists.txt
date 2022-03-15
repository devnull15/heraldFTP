cmake_minimum_required(VERSION 3.13)

set(PROJECT capstone)
set(BIN_DIR ../bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

project(${PROJECT} LANGUAGES C)

file(MAKE_DIRECTORY ${BIN_DIR})

option(NDEBUG "" ON)
configure_file(${PROJECT_SOURCE_DIR}/include/server.h.in ${PROJECT_SOURCE_DIR}/include/server.h)

list(APPEND INCLUDES src/threadpool/include)
list(APPEND INCLUDES src/ll/include)  
list(APPEND INCLUDES ${PROJECT_SOURCE_DIR}/include)
list(APPEND LIBS threadpool)
list(APPEND SOURCES src/server.c)

add_subdirectory(src/ll)
add_subdirectory(src/threadpool)
target_link_libraries(threadpool ll)
#add_dependencies(threadpool ll)

add_compile_options(-Werror -Wextra -Wall -pedantic -g -fsanitize=address)
link_libraries(-fsanitize=address)

include_directories(${INCLUDES})
add_executable(${PROJECT} ${SOURCES})
target_link_libraries(${PROJECT} ${LIBS})
