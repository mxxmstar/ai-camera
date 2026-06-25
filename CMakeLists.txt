cmake_minimum_required(VERSION 3.16)
project(ai-camera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# Build options
# ============================================================
option(BUILD_TESTS "Build test programs" OFF)

# ============================================================
# ASIO (header-only)
# ============================================================
set(ASIO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/asio-1.36.0)

# Define ASIO standalone (without Boost)
add_definitions(-DASIO_STANDALONE)

# ============================================================
# spdlog (header-only)
# ============================================================
set(SPDLOG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/spdlog-1.17.0/include)

# ============================================================
# GB28181 Module Configuration
# ============================================================
# GB28181 模块使用自主实现的轻量级 SIP 协议栈（基于 asio）
# 不需要外部 SIP 库依赖（如 PJSIP、eXosip2 等）

# GB28181 模块默认可用
set(GB28181_AVAILABLE TRUE)
message(STATUS "GB28181 module enabled (using lightweight SIP stack based on asio)")

# ============================================================
# Main executable
# ============================================================
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.hpp
    src/*.h
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ai-camera
    ${ASIO_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/tinyxml2
)

# Add GB28181 definitions
if(GB28181_AVAILABLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE GB28181_AVAILABLE=1)
    message(STATUS "GB28181 definitions added")
endif()

# Add /utf-8 option to support Chinese characters in source code (MSVC)
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    message(STATUS "Added /utf-8 flag for MSVC")
endif()

# Add tinyxml2 source file (not in src/, so not picked up by GLOB_RECURSE)
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/tinyxml2/tinyxml2.cpp
)

# Link pthreads (required by ASIO on Linux)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 mswsock)
    
    # Windows CryptAPI for MD5 calculation (used in SIP authentication)
    target_link_libraries(${PROJECT_NAME} PRIVATE advapi32)
endif()

# ============================================================
# Test programs (optional, controlled by BUILD_TESTS)
# ============================================================
if(BUILD_TESTS)
    file(GLOB_RECURSE TEST_SOURCES
        tests/*.cpp
        tests/*.hpp
        tests/*.h
    )

    if(TEST_SOURCES)
        # 测试程序复用主程序的所有源文件（包括 ws/http/rtsp 等模块实现），
        # 但排除主程序入口 src/main.cpp 以避免 main 符号冲突。
        set(TEST_SRC_SOURCES ${SOURCES})
        list(FILTER TEST_SRC_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

        add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES} ${TEST_SRC_SOURCES})

        target_include_directories(${PROJECT_NAME}_tests PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/ai-camera
            ${ASIO_INCLUDE_DIR}
            ${SPDLOG_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/third_party/tinyxml2
        )

        # Add GB28181 definitions
        if(GB28181_AVAILABLE)
            target_compile_definitions(${PROJECT_NAME}_tests PRIVATE GB28181_AVAILABLE=1)
        endif()

        target_sources(${PROJECT_NAME}_tests PRIVATE
            ${CMAKE_SOURCE_DIR}/third_party/tinyxml2/tinyxml2.cpp
        )

        target_link_libraries(${PROJECT_NAME}_tests PRIVATE Threads::Threads)
        if(WIN32)
            target_link_libraries(${PROJECT_NAME}_tests PRIVATE ws2_32 mswsock advapi32)
        endif()
    endif()
endif()

# ============================================================
# MQTT test program (standalone)
# ============================================================
add_executable(test_mqtt tests/test_mqtt.cpp
    src/mqtt/mqtt_client.cpp
    src/mqtt/mqtt_manager.cpp
    src/mqtt/mqtt_types.cpp
)

target_include_directories(test_mqtt PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ai-camera
    ${ASIO_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
)

target_compile_definitions(test_mqtt PRIVATE ASIO_STANDALONE)
target_compile_options(test_mqtt PRIVATE /utf-8)

target_link_libraries(test_mqtt PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(test_mqtt PRIVATE ws2_32 mswsock advapi32)
endif()

# ============================================================
# Logger test program
# ============================================================
add_executable(test_logger tests/test_logger.cpp)

target_include_directories(test_logger PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ai-camera
    ${ASIO_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
)

target_compile_definitions(test_logger PRIVATE ASIO_STANDALONE)
target_compile_options(test_logger PRIVATE /utf-8)

target_link_libraries(test_logger PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(test_logger PRIVATE ws2_32 mswsock advapi32)
endif()

# ============================================================
# mDNS test program (standalone)
# ============================================================
add_executable(test_mdns tests/test_mdns.cpp
    src/mdns/mdns_service.cpp
    src/mdns/mdns_impl.cpp
)

target_include_directories(test_mdns PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/ai-camera
    ${ASIO_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
)

target_compile_definitions(test_mdns PRIVATE ASIO_STANDALONE)
target_compile_options(test_mdns PRIVATE /utf-8)

target_link_libraries(test_mdns PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(test_mdns PRIVATE ws2_32 mswsock advapi32)
endif()

# ============================================================
# Print configuration summary
# ============================================================
message(STATUS "")
message(STATUS "============================================")
message(STATUS "Configuration Summary:")
message(STATUS "  Project:          ${PROJECT_NAME}")
message(STATUS "  CXX Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "  ASIO:             ${ASIO_INCLUDE_DIR}")
message(STATUS "  GB28181 Module:   ${GB28181_AVAILABLE}")
if(GB28181_AVAILABLE)
    message(STATUS "    - SIP Stack:   Lightweight (asio-based)")
    message(STATUS "    - Features:    REGISTER, MESSAGE, INVITE, BYE, SUBSCRIBE")
    message(STATUS "    - Auth:        SIP Digest (MD5)")
endif()
message(STATUS "  Build Tests:      ${BUILD_TESTS}")
message(STATUS "============================================")
message(STATUS "")
