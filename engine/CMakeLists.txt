cmake_minimum_required(VERSION 3.20)
project(poecraft LANGUAGES C CXX)

add_library(poecraft_headers INTERFACE)
target_include_directories(poecraft_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(poecraft_headers INTERFACE cxx_std_20)

include(CTest)
if(BUILD_TESTING)
    add_executable(poecraft_header_smoke tests/header_smoke.cpp)
    target_link_libraries(poecraft_header_smoke PRIVATE poecraft_headers)
    add_test(NAME poecraft_header_smoke COMMAND poecraft_header_smoke)
endif()
