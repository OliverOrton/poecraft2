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
# shared C ABI library for Python/WASM-facing bindings. The text inventory is
# also consumed by the PowerShell native fallback and the WASM build.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/engine-sources.txt"
    POECRAFT_ENGINE_SOURCE_ENTRIES)
set(POECRAFT_ENGINE_SOURCES "")
foreach(SOURCE_ENTRY IN LISTS POECRAFT_ENGINE_SOURCE_ENTRIES)
    string(STRIP "${SOURCE_ENTRY}" SOURCE_ENTRY)
    if(SOURCE_ENTRY STREQUAL "" OR SOURCE_ENTRY MATCHES "^#")
        continue()
    endif()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_ENTRY}")
        message(FATAL_ERROR
            "Engine source inventory entry does not exist: ${SOURCE_ENTRY}")
    endif()
    list(APPEND POECRAFT_ENGINE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_ENTRY}")
endforeach()
list(LENGTH POECRAFT_ENGINE_SOURCES POECRAFT_ENGINE_SOURCE_COUNT)
list(REMOVE_DUPLICATES POECRAFT_ENGINE_SOURCES)
list(LENGTH POECRAFT_ENGINE_SOURCES POECRAFT_UNIQUE_ENGINE_SOURCE_COUNT)
if(NOT POECRAFT_ENGINE_SOURCE_COUNT EQUAL POECRAFT_UNIQUE_ENGINE_SOURCE_COUNT)
    message(FATAL_ERROR "engine-sources.txt contains duplicate entries")
endif()

file(GLOB_RECURSE POECRAFT_DISCOVERED_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(POECRAFT_SORTED_ENGINE_SOURCES ${POECRAFT_ENGINE_SOURCES})
list(SORT POECRAFT_SORTED_ENGINE_SOURCES)
list(SORT POECRAFT_DISCOVERED_ENGINE_SOURCES)
if(NOT POECRAFT_SORTED_ENGINE_SOURCES STREQUAL POECRAFT_DISCOVERED_ENGINE_SOURCES)
    message(FATAL_ERROR
        "engine-sources.txt must contain every engine/src translation unit exactly once")
endif()

add_library(poecraft_engine_objects OBJECT ${POECRAFT_ENGINE_SOURCES})
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
if(EMSCRIPTEN)
    target_compile_options(poecraft_engine_objects PRIVATE -fexceptions)
endif()

add_library(poecraft_engine STATIC
    $<TARGET_OBJECTS:poecraft_engine_objects>)
target_include_directories(poecraft_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(poecraft_engine PUBLIC cxx_std_20)

if(EMSCRIPTEN)
    set(POECRAFT_WASM_EXPORTS_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/../bindings/wasm/wasm-exports.txt")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${POECRAFT_WASM_EXPORTS_FILE}")
    file(STRINGS "${POECRAFT_WASM_EXPORTS_FILE}" POECRAFT_WASM_EXPORTS)
    list(FILTER POECRAFT_WASM_EXPORTS EXCLUDE REGEX "^[ \t]*(#|$)")
    list(LENGTH POECRAFT_WASM_EXPORTS POECRAFT_WASM_EXPORT_COUNT)
    list(REMOVE_DUPLICATES POECRAFT_WASM_EXPORTS)
    list(LENGTH POECRAFT_WASM_EXPORTS POECRAFT_WASM_UNIQUE_EXPORT_COUNT)
    if(POECRAFT_WASM_EXPORT_COUNT EQUAL 0 OR
            NOT POECRAFT_WASM_EXPORT_COUNT EQUAL
                POECRAFT_WASM_UNIQUE_EXPORT_COUNT)
        message(FATAL_ERROR "wasm-exports.txt is empty or contains duplicates")
    endif()
    foreach(WASM_EXPORT IN LISTS POECRAFT_WASM_EXPORTS)
        if(NOT WASM_EXPORT MATCHES "^_[A-Za-z0-9_]+$")
            message(FATAL_ERROR "Invalid WASM export: ${WASM_EXPORT}")
        endif()
    endforeach()
    list(JOIN POECRAFT_WASM_EXPORTS "," POECRAFT_WASM_EXPORTED_FUNCTIONS)

    add_executable(poecraft_engine_wasm
        "${CMAKE_CURRENT_SOURCE_DIR}/../bindings/wasm/wasm_api.cpp")
    target_link_libraries(poecraft_engine_wasm PRIVATE poecraft_engine)
    target_include_directories(poecraft_engine_wasm PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${HARVEST_GENERATED_DIR}")
    target_compile_options(poecraft_engine_wasm PRIVATE
        -O3 -ffp-contract=off -fexceptions)
    target_link_options(poecraft_engine_wasm PRIVATE
        -O3 -ffp-contract=off -fexceptions
        "-sMODULARIZE=1"
        "-sEXPORT_ES6=1"
        "-sEXPORT_NAME=createPoecraftEngine"
        "-sENVIRONMENT=web,worker,node"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=134217728"
        "-sMAXIMUM_MEMORY=4GB"
        "-sSTACK_SIZE=67108864"
        "-sEXIT_RUNTIME=0"
        "-sEXPORTED_FUNCTIONS=${POECRAFT_WASM_EXPORTED_FUNCTIONS}"
        "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,HEAPU8")
    set_target_properties(poecraft_engine_wasm PROPERTIES
        OUTPUT_NAME poecraft_engine
        SUFFIX ".mjs"
        RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_CURRENT_SOURCE_DIR}/../bindings/wasm/dist")
endif()

add_library(poecraft_engine_shared SHARED
    $<TARGET_OBJECTS:poecraft_engine_objects>)
set_target_properties(poecraft_engine_shared PROPERTIES
    OUTPUT_NAME poecraft_engine
    WINDOWS_EXPORT_ALL_SYMBOLS ON)
if(WIN32)
    set_target_properties(poecraft_engine_shared PROPERTIES PREFIX "")
endif()
target_include_directories(poecraft_engine_shared PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(poecraft_engine_shared PUBLIC cxx_std_20)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(poecraft_engine_shared PRIVATE
        -static-libstdc++ -static-libgcc)
    if(MINGW)
        get_filename_component(POECRAFT_MINGW_BIN
            "${CMAKE_CXX_COMPILER}" DIRECTORY)
        find_file(POECRAFT_WINPTHREAD_RUNTIME
            NAMES libwinpthread-1.dll
            PATHS "${POECRAFT_MINGW_BIN}"
            NO_DEFAULT_PATH)
        if(NOT POECRAFT_WINPTHREAD_RUNTIME)
            message(FATAL_ERROR "MinGW winpthread runtime was not found")
        endif()
        add_custom_command(TARGET poecraft_engine_shared POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${POECRAFT_WINPTHREAD_RUNTIME}"
                "$<TARGET_FILE_DIR:poecraft_engine_shared>")
    endif()
endif()

# S7 optimal-solver baseline harness. It consumes versioned corpus specs and
# only calls the public engine API; json.hpp is reused solely for tooling-side
# spec/report parsing.
add_executable(poecraft_solver_benchmark
    benchmarks/solver_executable_fragment.cpp
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
        tests/test_solver_refinement.cpp
        benchmarks/solver_executable_fragment.cpp
        tests/test_solver_fragment.cpp
        tests/test_solver_quotient_proof.cpp
        tests/test_solver_quotient_partition.cpp
        tests/test_solver_quotient_bellman.cpp
    )
    target_link_libraries(poecraft_engine_tests PRIVATE poecraft_engine)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Avoids an ld.bfd crash (exit 116) in MSYS2 ucrt64 binutils when
        # linking the shared C++ runtime against exception-bearing COMDAT
        # sections. Harmless elsewhere.
        target_link_options(poecraft_engine_tests PRIVATE
            -static-libstdc++ -static-libgcc)
    endif()
    set(POECRAFT_COMPILED_ARTIFACT
        "${CMAKE_CURRENT_SOURCE_DIR}/../data/compiled/current")
    set(POECRAFT_SPEC_FIXTURES
        "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/spec")

    # These entries partition the monolithic test executable without overlap,
    # so CTest can obtain real process-level parallelism. Focused subset modes
    # such as policy-refinement remain available through dev-engine.ps1 but are
    # intentionally not duplicated in the all-native CTest set.
    add_test(NAME poecraft_core
        COMMAND poecraft_engine_tests --core-only
            "${POECRAFT_COMPILED_ARTIFACT}" "${POECRAFT_SPEC_FIXTURES}")
    add_test(NAME poecraft_solver_abstract
        COMMAND poecraft_engine_tests --solver-abstract-only
            "${POECRAFT_COMPILED_ARTIFACT}")
    add_test(NAME poecraft_solver_calc
        COMMAND poecraft_engine_tests --solver-calc-only
            "${POECRAFT_COMPILED_ARTIFACT}")
    add_test(NAME poecraft_solver_family_contract
        COMMAND poecraft_engine_tests --solver-family-contract-only
            "${POECRAFT_COMPILED_ARTIFACT}")
    add_test(NAME poecraft_solver_solve
        COMMAND poecraft_engine_tests --solver-solve-only
            "${POECRAFT_COMPILED_ARTIFACT}")
    add_test(NAME poecraft_solver_compile
        COMMAND poecraft_engine_tests --solver-compile-only
            "${POECRAFT_COMPILED_ARTIFACT}")
    add_test(NAME poecraft_solver_eval
        COMMAND poecraft_engine_tests --solver-eval-only
            "${POECRAFT_COMPILED_ARTIFACT}")
    add_test(NAME poecraft_solver_api
        COMMAND poecraft_engine_tests --solver-api-only
            "${POECRAFT_COMPILED_ARTIFACT}")
    add_test(NAME poecraft_solver_s8_3
        COMMAND poecraft_engine_tests --solver-s8-3-only)
    add_test(NAME poecraft_solver_refinement
        COMMAND poecraft_engine_tests --solver-refinement-only)
    add_test(NAME poecraft_solver_fragment
        COMMAND poecraft_engine_tests --solver-fragment-only)
    add_test(NAME poecraft_solver_quotient_proof
        COMMAND poecraft_engine_tests --solver-quotient-proof-only)

    set_tests_properties(
        poecraft_header_smoke
        poecraft_core
        poecraft_solver_abstract
        poecraft_solver_calc
        poecraft_solver_family_contract
        poecraft_solver_solve
        poecraft_solver_compile
        poecraft_solver_eval
        poecraft_solver_api
        poecraft_solver_s8_3
        poecraft_solver_refinement
        poecraft_solver_fragment
        poecraft_solver_quotient_proof
        PROPERTIES LABELS "native")
    set_tests_properties(
        poecraft_solver_abstract
        poecraft_solver_calc
        poecraft_solver_family_contract
        poecraft_solver_solve
        poecraft_solver_compile
        poecraft_solver_eval
        poecraft_solver_api
        poecraft_solver_s8_3
        poecraft_solver_refinement
        poecraft_solver_fragment
        poecraft_solver_quotient_proof
        PROPERTIES LABELS "native;solver")
    set_tests_properties(poecraft_solver_refinement PROPERTIES
        LABELS "native;solver;solver-refinement")
    set_tests_properties(poecraft_solver_fragment PROPERTIES
        LABELS "native;solver;solver-fragment")
    set_tests_properties(poecraft_solver_quotient_proof PROPERTIES
        LABELS "native;solver;solver-quotient-proof")
    set_tests_properties(poecraft_solver_solve PROPERTIES
        LABELS "native;solver;solver-policy")
    add_test(
        NAME poecraft_solver_benchmark_validate
        COMMAND poecraft_solver_benchmark
            --artifact "${CMAKE_CURRENT_SOURCE_DIR}/../data/compiled/current"
            --corpus "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/solver-benchmarks/v1/manifest.json"
            --validate-only
    )
    add_test(
        NAME poecraft_solver_goal_realignment_validate
        COMMAND poecraft_solver_benchmark
            --artifact "${CMAKE_CURRENT_SOURCE_DIR}/../data/compiled/current"
            --corpus "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/solver-goal-realignment/v1/manifest.json"
            --validate-only
    )
    add_test(
        NAME poecraft_solver_goal_realignment_five_mod_validate
        COMMAND poecraft_solver_benchmark
            --artifact "${CMAKE_CURRENT_SOURCE_DIR}/../data/compiled/current"
            --corpus "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/solver-goal-realignment/v1/five-mod-manifest.json"
            --validate-only
    )
    add_test(
        NAME poecraft_solver_goal_realignment_gate6_validate
        COMMAND poecraft_solver_benchmark
            --artifact "${CMAKE_CURRENT_SOURCE_DIR}/../data/compiled/current"
            --corpus "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/solver-goal-realignment/v1/gate6-manifest.json"
            --validate-only
    )
    set_tests_properties(
        poecraft_solver_benchmark_validate
        poecraft_solver_goal_realignment_validate
        poecraft_solver_goal_realignment_five_mod_validate
        poecraft_solver_goal_realignment_gate6_validate
        PROPERTIES
            LABELS "native;solver;solver-corpus-validation"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")

endif()
