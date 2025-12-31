cmake_minimum_required(VERSION 3.28)
project(TVLCOM C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Workaround: some MinGW toolchains ship an assembler that doesn't understand DWARF-5.
# Force DWARF-4 in Debug builds when using GCC/Clang.
# NOTE: Some bundled toolchains also reject -gdwarf-4, so we keep this disabled by default.
# If you hit 'as: unknown option -- gdwarf-5', prefer configuring with:
#   -DCMAKE_C_FLAGS_DEBUG="-g -gdwarf-4"  (only if your assembler supports it)
# or use a newer MinGW toolchain.
# if(NOT MSVC)
#     add_compile_options($<$<CONFIG:Debug>:-gdwarf-4>)
# endif()

option(TVLCOM_ENABLE_TESTS "Build unit tests" ON)
set(TVLCOM_PLATFORM "WINDOWS" CACHE STRING "Target platform: WINDOWS or STM32")
set_property(CACHE TVLCOM_PLATFORM PROPERTY STRINGS WINDOWS STM32)

# ------------------------ sources (explicit, no glob) ------------------------
set(TVLCOM_CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/GLOBAL_CONFIG.h
    ${CMAKE_SOURCE_DIR}/src/main.c

    ${CMAKE_SOURCE_DIR}/src/SoftwareAnalysis/S_TLV_PROTOCOL.c
    ${CMAKE_SOURCE_DIR}/src/SoftwareAnalysis/S_RECEIVE_PROTOCOL.c
    ${CMAKE_SOURCE_DIR}/src/SoftwareAnalysis/S_TRANSPORT_PROTOCOL.c

    ${CMAKE_SOURCE_DIR}/src/HAL/hal.c
)

set(TVLCOM_PLATFORM_SOURCES
    # filled below
)

if(TVLCOM_PLATFORM STREQUAL "WINDOWS")
    add_compile_definitions(TVLCOM_PLATFORM_WINDOWS=1 TVLCOM_PLATFORM_STM32=0)
    list(APPEND TVLCOM_PLATFORM_SOURCES
        ${CMAKE_SOURCE_DIR}/src/HAL/windows/hal_windows.c
        ${CMAKE_SOURCE_DIR}/src/Serial/SERIAL.c
    )
elseif(TVLCOM_PLATFORM STREQUAL "STM32")
    add_compile_definitions(TVLCOM_PLATFORM_WINDOWS=0 TVLCOM_PLATFORM_STM32=1)
    list(APPEND TVLCOM_PLATFORM_SOURCES
        ${CMAKE_SOURCE_DIR}/src/HAL/stm32/hal_stm32.c
    )
else()
    message(FATAL_ERROR "Unknown TVLCOM_PLATFORM='${TVLCOM_PLATFORM}'. Use WINDOWS or STM32")
endif()

add_executable(${PROJECT_NAME}
    ${TVLCOM_CORE_SOURCES}
    ${TVLCOM_PLATFORM_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/Serial
    ${CMAKE_SOURCE_DIR}/src/SoftwareAnalysis
)

# ------------------------ warnings / optimizations ------------------------
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    # Sensible defaults; users can override via CMAKE_BUILD_TYPE or toolchain.
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:-O3>)
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:RelWithDebInfo>:-O2>)
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:-Og>)
endif()

# ------------------------ unit tests ------------------------
if(TVLCOM_ENABLE_TESTS)
    enable_testing()

    add_executable(tvlcom_tests
        ${CMAKE_SOURCE_DIR}/tests/test_protocol.c
        ${CMAKE_SOURCE_DIR}/src/SoftwareAnalysis/S_TLV_PROTOCOL.c
        ${CMAKE_SOURCE_DIR}/src/SoftwareAnalysis/S_RECEIVE_PROTOCOL.c
        ${CMAKE_SOURCE_DIR}/src/SoftwareAnalysis/S_TRANSPORT_PROTOCOL.c
        ${CMAKE_SOURCE_DIR}/src/HAL/hal.c
        ${CMAKE_SOURCE_DIR}/src/HAL/windows/hal_windows.c
    )

    target_include_directories(tvlcom_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/SoftwareAnalysis
        ${CMAKE_SOURCE_DIR}/tests
    )

    if(MSVC)
        target_compile_options(tvlcom_tests PRIVATE /W4)
    else()
        target_compile_options(tvlcom_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME tvlcom_tests COMMAND tvlcom_tests)
endif()