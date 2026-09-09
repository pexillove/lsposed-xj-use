cmake_minimum_required(VERSION 3.22.1)
project(lsposed_xj_use LANGUAGES C CXX)

if(NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "The bundled DobbyXJ library supports arm64-v8a only")
endif()

add_library(dobby STATIC IMPORTED)
set_target_properties(
    dobby
    PROPERTIES
    IMPORTED_LOCATION
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/dobby/lib/${ANDROID_ABI}/libdobby.a"
    INTERFACE_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/dobby/include"
)

add_library(xjhook SHARED native_hook.cpp)

target_compile_features(xjhook PRIVATE cxx_std_20)
target_compile_options(xjhook PRIVATE -Wall -Wextra -Werror)
target_link_libraries(xjhook PRIVATE dobby log dl)
