cmake_minimum_required(VERSION 3.20)
project(poecraft LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Harvest craft availability and economy recipe keys have one owner-approved
# source. Generate the native allowlists from that same checked-in manifest so
# the engine cannot drift back to exposing arbitrary tag-derived actions.
set(HARVEST_RECIPES_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/economy/harvest-recipes-v1.json")
file(READ "${HARVEST_RECIPES_PATH}" HARVEST_RECIPES_JSON)

function(harvest_recipe_keys MEMBER OUT_ENTRIES OUT_COUNT)
    string(JSON ENTRY_COUNT LENGTH "${HARVEST_RECIPES_JSON}" "${MEMBER}")
    set(TAGS "")
    if(ENTRY_COUNT GREATER 0)
        math(EXPR LAST_ENTRY "${ENTRY_COUNT} - 1")
        foreach(INDEX RANGE 0 ${LAST_ENTRY})
            string(JSON TAG MEMBER "${HARVEST_RECIPES_JSON}" "${MEMBER}" ${INDEX})
            list(APPEND TAGS "${TAG}")
        endforeach()
    endif()
    list(SORT TAGS)
    set(ENTRIES "")
    foreach(TAG IN LISTS TAGS)
            string(APPEND ENTRIES "    std::string_view{\"${TAG}\"},\n")
    endforeach()
    set(${OUT_ENTRIES} "${ENTRIES}" PARENT_SCOPE)
    list(LENGTH TAGS SORTED_ENTRY_COUNT)
    set(${OUT_COUNT} "${SORTED_ENTRY_COUNT}" PARENT_SCOPE)
endfunction()

harvest_recipe_keys(reforge HARVEST_REFORGE_ENTRIES HARVEST_REFORGE_COUNT)
harvest_recipe_keys(augment HARVEST_AUGMENT_ENTRIES HARVEST_AUGMENT_COUNT)
set(HARVEST_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${HARVEST_GENERATED_DIR}")
configure_file(
    src/harvest_crafts.generated.hpp.in
    "${HARVEST_GENERATED_DIR}/harvest_crafts.generated.hpp"
    @ONLY)

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
    src/actions_bestiary.cpp
    src/bestiary_api.cpp
    src/simulator.cpp
    src/solver_registry.cpp
    src/solver_abstract.cpp
    src/solver_calc.cpp
    src/solver_reforge.cpp
    src/solver_options.cpp
    src/solver_solve.cpp
    src/solver_solve_expand.cpp
    src/solver_solve_bellman.cpp
    src/solver_solve_focused.cpp
    src/solver_solve_constructive.cpp
    src/solver_solve_heuristics.cpp
    src/solver_solve_quotient.cpp
    src/solver_solve_finish.cpp
    src/solver_solve_telemetry.cpp
    src/solver_compile.cpp
    src/solver_eval.cpp
    src/solver_api.cpp
)
set_target_properties(poecraft_engine_objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
target_include_directories(poecraft_engine_objects
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${HARVEST_GENERATED_DIR}")
target_compile_features(poecraft_engine_objects PUBLIC cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(poecraft_engine_objects PRIVATE -ffp-contract=off)
endif()

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

# S7 optimal-solver baseline harness. It consumes versioned corpus specs and
# only calls the public engine API; json.hpp is reused solely for tooling-side
# spec/report parsing.
add_executable(poecraft_solver_benchmark
    benchmarks/solver_benchmark.cpp)
target_include_directories(poecraft_solver_benchmark
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(poecraft_solver_benchmark PRIVATE poecraft_engine)
target_compile_features(poecraft_solver_benchmark PRIVATE cxx_std_20)
if(WIN32)
    target_link_libraries(poecraft_solver_benchmark PRIVATE Psapi)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(poecraft_solver_benchmark PRIVATE
        -static-libstdc++ -static-libgcc)
endif()

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
        tests/test_bestiary.cpp
        tests/test_simulator.cpp
        tests/test_solver_abstract.cpp
        tests/test_solver_calc.cpp
        tests/test_solver_solve.cpp
        tests/test_solver_compile.cpp
        tests/test_solver_s8_3.cpp
        tests/test_solver_eval.cpp
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
    add_test(
        NAME poecraft_solver_benchmark_validate
        COMMAND poecraft_solver_benchmark
            --artifact "${CMAKE_CURRENT_SOURCE_DIR}/../data/compiled/current"
            --corpus "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/solver-benchmarks/v1/manifest.json"
            --validate-only
    )
endif()
