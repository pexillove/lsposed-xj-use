cmake_minimum_required(VERSION 3.22.1)
project(xj_use LANGUAGES C CXX)

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

add_library(xjhook SHARED
        native_hook.cpp
        vendor/hide_utils.cpp
        vendor/elf_util.cpp
        vendor/memory_utils.cpp)

target_include_directories(xjhook PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
target_compile_features(xjhook PRIVATE cxx_std_20)
# vendor/ 代码已在实机长期验证，告警不作为阻断条件
set_source_files_properties(
        vendor/hide_utils.cpp vendor/elf_util.cpp vendor/memory_utils.cpp
        PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wno-error")
target_compile_options(xjhook PRIVATE -Wall -Wextra -Werror)
target_link_libraries(xjhook PRIVATE dobby log dl)
