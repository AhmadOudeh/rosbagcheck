cmake_minimum_required(VERSION 3.22)
project(rosbagcheck VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# mcap (the official MCAP reader) is not packaged in Ubuntu: download a pinned release and
# verify its SHA-256, the same idea rosbagcheck applies to recordings.
include(FetchContent)
FetchContent_Declare(mcap
  URL https://github.com/foxglove/mcap/archive/refs/tags/releases/cpp/v2.2.0.tar.gz
  URL_HASH SHA256=68d9b81b1c0454b139d1e7adaa572d415525edc6ee9c57c7bae2dd4eaa4a42b0
  SOURCE_SUBDIR cpp/mcap  # no CMakeLists.txt there: we only want the headers
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(mcap)

find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)  # compression used inside MCAP files
pkg_check_modules(LZ4 REQUIRED IMPORTED_TARGET liblz4)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(GTest REQUIRED)

set(WARNINGS -Wall -Wextra -Wpedantic -Werror)

# All logic lives in one library, so the program and the unit tests use the same code.
add_library(rosbagcheck_core STATIC src/report.cpp src/checksum.cpp src/closed.cpp src/mcap_impl.cpp
  src/rules.cpp src/sensors.cpp)
target_include_directories(rosbagcheck_core PUBLIC src)
target_include_directories(rosbagcheck_core SYSTEM PUBLIC ${mcap_SOURCE_DIR}/cpp/mcap/include)
target_compile_definitions(rosbagcheck_core PUBLIC ROSBAGCHECK_VERSION="${PROJECT_VERSION}")
target_link_libraries(rosbagcheck_core PUBLIC
  OpenSSL::Crypto PkgConfig::ZSTD PkgConfig::LZ4 nlohmann_json::nlohmann_json yaml-cpp::yaml-cpp)
target_compile_options(rosbagcheck_core PRIVATE ${WARNINGS})

add_executable(rosbagcheck src/main.cpp)
target_link_libraries(rosbagcheck PRIVATE rosbagcheck_core CLI11::CLI11)
target_compile_options(rosbagcheck PRIVATE ${WARNINGS})

enable_testing()
add_executable(unit_tests tests/unit/test_report.cpp tests/unit/test_checksum.cpp
  tests/unit/test_rules.cpp tests/unit/test_sensors.cpp)
target_link_libraries(unit_tests PRIVATE rosbagcheck_core GTest::gtest_main)
target_compile_options(unit_tests PRIVATE ${WARNINGS})
include(GoogleTest)
gtest_discover_tests(unit_tests)
