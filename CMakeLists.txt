cmake_minimum_required(VERSION 3.7)
project(OS_ex4)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES ex41.c)

add_executable(ex41.out ${SOURCE_FILES})
add_executable(ex42.out ex42.c)

TARGET_LINK_LIBRARIES( ex41.out
        pthread
        )

TARGET_LINK_LIBRARIES( ex42.out
        pthread
        )