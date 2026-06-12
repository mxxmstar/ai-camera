cmake_minimum_required(VERSION 3.16)
project(ai-camera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
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
# Main executable
# ============================================================
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.hpp
    src/*.h
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${ASIO_INCLUDE_DIR}
)

# Link pthreads (required by ASIO on Linux)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

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
        add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

        target_include_directories(${PROJECT_NAME}_tests PRIVATE
            ${ASIO_INCLUDE_DIR}
        )

        target_link_libraries(${PROJECT_NAME}_tests PRIVATE Threads::Threads)
    endif()
endif()