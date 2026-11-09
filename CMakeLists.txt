cmake_minimum_required(VERSION 3.22)
project(rosbagcheck VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(GTest REQUIRED)

set(WARNINGS -Wall -Wextra -Wpedantic -Werror)

# All logic lives in one library, so the program and the unit tests use the same code.
add_library(rosbagcheck_core STATIC src/report.cpp src/checksum.cpp)
target_include_directories(rosbagcheck_core PUBLIC src)
target_compile_definitions(rosbagcheck_core PUBLIC ROSBAGCHECK_VERSION="${PROJECT_VERSION}")
target_link_libraries(rosbagcheck_core PUBLIC OpenSSL::Crypto nlohmann_json::nlohmann_json)
target_compile_options(rosbagcheck_core PRIVATE ${WARNINGS})

add_executable(rosbagcheck src/main.cpp)
target_link_libraries(rosbagcheck PRIVATE rosbagcheck_core CLI11::CLI11)
target_compile_options(rosbagcheck PRIVATE ${WARNINGS})

enable_testing()
add_executable(unit_tests tests/unit/test_report.cpp tests/unit/test_checksum.cpp)
target_link_libraries(unit_tests PRIVATE rosbagcheck_core GTest::gtest_main)
target_compile_options(unit_tests PRIVATE ${WARNINGS})
include(GoogleTest)
gtest_discover_tests(unit_tests)
