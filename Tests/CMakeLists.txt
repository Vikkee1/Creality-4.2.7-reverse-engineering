cmake_minimum_required(VERSION 3.16)
project(soft_i2c_tests C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(SOFT_I2C_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../App/Soft_I2C)

add_executable(soft_i2c_tests
    test_soft_i2c.cpp
    mock_bus.c
    ${SOFT_I2C_DIR}/soft_i2c.c
)
target_include_directories(soft_i2c_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SOFT_I2C_DIR}
)
target_link_libraries(soft_i2c_tests PRIVATE gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(soft_i2c_tests)
