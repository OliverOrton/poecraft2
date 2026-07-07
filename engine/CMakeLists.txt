cmake_minimum_required(VERSION 3.20)
project(poecraft LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only interface target for callers that only need the public API types.
add_library(poecraft_headers INTERFACE)
target_include_directories(poecraft_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(poecraft_headers INTERFACE cxx_std_20)

# Build the core once, then expose both a static library for native tests and a
# shared C ABI library for Python/WASM-facing bindings.
add_library(poecraft_engine_objects OBJECT
    src/api.cpp
    src/bitset.cpp
    src/data_loader.cpp
    src/item_state.cpp
    src/rng.cpp
    src/session_builder.cpp
    src/actions_basic.cpp
    src/simulator.cpp
    src/solver_registry.cpp
    src/solver_abstract.cpp
    src/solver_calc.cpp
    src/solver_reforge.cpp
    src/solver_solve.cpp
    src/solver_compile.cpp
    src/solver_api.cpp
)
set_target_properties(poecraft_engine_objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
target_include_directories(poecraft_engine_objects
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(poecraft_engine_objects PUBLIC cxx_std_20)

add_library(poecraft_engine STATIC
    $<TARGET_OBJECTS:poecraft_engine_objects>)
target_include_directories(poecraft_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(poecraft_engine PUBLIC cxx_std_20)

add_library(poecraft_engine_shared SHARED
    $<TARGET_OBJECTS:poecraft_engine_objects>)
set_target_properties(poecraft_engine_shared PROPERTIES
    OUTPUT_NAME poecraft_engine
    WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_include_directories(poecraft_engine_shared PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(poecraft_engine_shared PUBLIC cxx_std_20)

include(CTest)
if(BUILD_TESTING)
    add_executable(poecraft_header_smoke tests/header_smoke.cpp)
    target_link_libraries(poecraft_header_smoke PRIVATE poecraft_headers)
    add_test(NAME poecraft_header_smoke COMMAND poecraft_header_smoke)

    add_executable(poecraft_engine_tests
        tests/test_main.cpp
        tests/test_bitset.cpp
        tests/test_rng.cpp
        tests/test_item_state.cpp
        tests/test_blocking.cpp
        tests/test_data_loader.cpp
        tests/test_session_builder.cpp
        tests/test_actions.cpp
        tests/test_simulator.cpp
        tests/test_solver_abstract.cpp
        tests/test_solver_calc.cpp
        tests/test_solver_solve.cpp
        tests/test_solver_compile.cpp
        tests/test_solver_api.cpp
    )
    target_link_libraries(poecraft_engine_tests PRIVATE poecraft_engine)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Avoids an ld.bfd crash (exit 116) in MSYS2 ucrt64 binutils when
        # linking the shared C++ runtime against exception-bearing COMDAT
        # sections. Harmless elsewhere.
        target_link_options(poecraft_engine_tests PRIVATE
            -static-libstdc++ -static-libgcc)
    endif()
    # Pass the compiled artifact dir and spec fixtures dir so the data-loader
    # and session-builder suites run.
    add_test(
        NAME poecraft_engine_tests
        COMMAND poecraft_engine_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/../data/compiled/current"
            "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/spec"
    )
endif()
