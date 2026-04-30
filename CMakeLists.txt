cmake_minimum_required(VERSION 3.24)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

# Build definition for the gameplay DLL plus the smoke host executable.
project(gs1_game
    VERSION 0.1.0
    LANGUAGES C CXX)

include(GNUInstallDirs)

# Toggle test executable builds without affecting the gameplay DLL target.
option(GS1_BUILD_TESTS "Build GS1 test executables" ON)
set(
    GS1_SYSTEM_TEST_SYSTEMS
    ""
    CACHE STRING
    "Semicolon-separated system filters passed to gs1_system_test_host. Empty runs all discovered system tests.")
set(GS1_TOMLPLUSPLUS_INCLUDE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tomlplusplus)

file(GLOB_RECURSE GS1_SYSTEM_TEST_SOURCE_FILES CONFIGURE_DEPENDS
    tests/system/source/*.cpp)

# Use a modern, compiler-independent C++ baseline for both targets.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(
    GS1_CONTENT_TABLE_DIR_DEFINITION
    GS1_CONTENT_TABLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/content/tables")

function(gs1_enable_strict_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(gs1_enable_internal_pch target_name)
    target_precompile_headers(${target_name}
        PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/gs1_pch.h>")
endfunction()

if(MSVC)
    # Visual Studio builds share a compile PDB per target by default.
    # /FS prevents parallel cl.exe instances from racing on vc143.pdb.
    add_compile_options(/FS)
    # Use embedded debug information for compile steps so parallel compiles
    # do not all contend on a single vc143.pdb file.
    set(
        CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
endif()

add_library(flecs STATIC
    third_party/flecs/flecs.c)

target_include_directories(flecs
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/flecs)

set_target_properties(flecs PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

set(GS1_CONTENT_RUNTIME_SOURCES
    src/content/content_loader.cpp
    src/content/content_lookup.cpp
    src/content/content_validator.cpp
    src/content/prototype_content.cpp)

set(GS1_GAMEPLAY_CORE_SOURCES
    src/app/campaign_factory.cpp
    src/app/site_run_factory.cpp
    src/campaign/systems/campaign_flow_system.cpp
    src/campaign/systems/campaign_time_system.cpp
    src/campaign/systems/faction_reputation_system.cpp
    src/campaign/systems/loadout_planner_system.cpp
    src/campaign/systems/technology_system.cpp
    src/messages/message_dispatcher.cpp
    ${GS1_CONTENT_RUNTIME_SOURCES}
    src/site/systems/action_execution_system.cpp
    src/site/systems/camp_durability_system.cpp
    src/site/systems/device_maintenance_system.cpp
    src/site/systems/device_support_system.cpp
    src/site/systems/device_weather_contribution_system.cpp
    src/site/systems/ecology_system.cpp
    src/site/systems/economy_phone_system.cpp
    src/site/systems/failure_recovery_system.cpp
    src/site/systems/inventory_system.cpp
    src/site/systems/craft_system.cpp
    src/site/systems/local_weather_resolve_system.cpp
    src/site/systems/modifier_system.cpp
    src/site/systems/phone_panel_system.cpp
    src/site/systems/plant_weather_contribution_system.cpp
    src/site/systems/placement_validation_system.cpp
    src/site/systems/site_completion_system.cpp
    src/site/systems/site_flow_system.cpp
    src/site/systems/site_time_system.cpp
    src/site/systems/task_board_system.cpp
    src/site/systems/weather_event_system.cpp
    src/site/systems/worker_condition_system.cpp
    src/site/craft_logic.cpp
    src/site/inventory_storage.cpp
    src/site/site_world.cpp
    src/runtime/game_runtime.cpp)

add_library(gs1_gameplay_core STATIC
    ${GS1_GAMEPLAY_CORE_SOURCES})

target_include_directories(gs1_gameplay_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE
        ${GS1_TOMLPLUSPLUS_INCLUDE_DIR})

target_compile_definitions(gs1_gameplay_core
    PRIVATE
        ${GS1_CONTENT_TABLE_DIR_DEFINITION})

target_link_libraries(gs1_gameplay_core
    PUBLIC
        flecs)

set_target_properties(gs1_gameplay_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

gs1_enable_strict_warnings(gs1_gameplay_core)
gs1_enable_internal_pch(gs1_gameplay_core)

# Build the pure gameplay runtime as a shared library (the DLL).
add_library(gs1_game SHARED
    src/app/game_api.cpp
    src/testing/system_test_asset.cpp
    src/testing/system_test_registry.cpp)

# Export public DLL symbols when compiling the gameplay library itself.
target_compile_definitions(gs1_game
    PRIVATE GS1_BUILD_DLL=1)

# Public headers form the engine-facing SDK. Private headers stay internal.
target_include_directories(gs1_game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

set_target_properties(gs1_game PROPERTIES
    OUTPUT_NAME "gs1_game"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF)

target_link_libraries(gs1_game
    PRIVATE gs1_gameplay_core)

# Install the DLL/import library and the public headers as the gameplay SDK output.
install(TARGETS gs1_game
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

gs1_enable_strict_warnings(gs1_game)
gs1_enable_internal_pch(gs1_game)

if(GS1_BUILD_TESTS)
    enable_testing()

    set(GS1_SMOKE_HOST_COMMON_SOURCES
        tests/smoke/smoke_engine_host.cpp
        tests/smoke/smoke_log.cpp
        tests/smoke/runtime_dll_loader.cpp
        tests/smoke/smoke_live_state_json.cpp
        tests/smoke/visual_smoke_silent_onboarding.cpp
        ${GS1_CONTENT_RUNTIME_SOURCES})

    add_library(gs1_smoke_support STATIC
        ${GS1_SMOKE_HOST_COMMON_SOURCES})

    target_include_directories(gs1_smoke_support
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke
        PRIVATE
            ${GS1_TOMLPLUSPLUS_INCLUDE_DIR})

    target_compile_definitions(gs1_smoke_support
        PRIVATE
            ${GS1_CONTENT_TABLE_DIR_DEFINITION})

    gs1_enable_strict_warnings(gs1_smoke_support)

    add_library(gs1_system_test_suite OBJECT
        src/testing/system_test_asset.cpp
        src/testing/system_test_registry.cpp
        ${GS1_SYSTEM_TEST_SOURCE_FILES})

    target_include_directories(gs1_system_test_suite
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/system/source
            ${GS1_TOMLPLUSPLUS_INCLUDE_DIR})

    target_compile_definitions(gs1_system_test_suite
        PRIVATE
            ${GS1_CONTENT_TABLE_DIR_DEFINITION})

    target_link_libraries(gs1_system_test_suite
        PRIVATE
            flecs)

    gs1_enable_strict_warnings(gs1_system_test_suite)
    gs1_enable_internal_pch(gs1_system_test_suite)

    # Build the text smoke host executable separately from the DLL.
    # This executable loads the gameplay DLL at runtime instead of linking to it.
    add_executable(gs1_smoke_host
        tests/smoke/smoke_host_main.cpp
        tests/smoke/smoke_script_runner.cpp)

    # Build the live visual smoke host in parallel to the text smoke host.
    # This host is a manual visual adapter and is not part of the script-driven smoke test path.
    add_executable(gs1_visual_smoke_host
        tests/smoke/visual_smoke_host_main.cpp
        tests/smoke/smoke_live_http_server.cpp)

    add_executable(gs1_system_test_host
        tests/system/system_test_host_main.cpp
        $<TARGET_OBJECTS:gs1_system_test_suite>)

    add_executable(gs1_runtime_projection_test
        tests/runtime/game_runtime_projection_test.cpp)

    add_executable(gs1_runtime_message_chain_test
        tests/runtime/game_runtime_message_chain_test.cpp)

    add_executable(gs1_timed_modifier_projection_test
        tests/runtime/timed_modifier_projection_test.cpp)

    add_executable(gs1_site_system_message_flow_test
        tests/runtime/site_system_message_flow_test.cpp)

    add_executable(gs1_local_weather_perf_probe
        tests/runtime/local_weather_perf_probe.cpp)

    add_executable(gs1_runtime_performance_test
        tests/runtime/game_runtime_performance_test.cpp)

    add_executable(gs1_smoke_engine_host_threading_test
        tests/runtime/smoke_engine_host_threading_test.cpp)

    target_link_libraries(gs1_smoke_host PRIVATE gs1_smoke_support)
    target_link_libraries(gs1_visual_smoke_host PRIVATE gs1_smoke_support ws2_32 winmm)
    target_link_libraries(gs1_runtime_projection_test PRIVATE gs1_gameplay_core)
    target_link_libraries(gs1_runtime_message_chain_test PRIVATE gs1_gameplay_core)
    target_link_libraries(gs1_timed_modifier_projection_test PRIVATE gs1_gameplay_core)
    target_link_libraries(gs1_site_system_message_flow_test PRIVATE gs1_gameplay_core)
    target_link_libraries(gs1_local_weather_perf_probe PRIVATE gs1_gameplay_core)
    target_link_libraries(gs1_runtime_performance_test PRIVATE gs1_gameplay_core)
    target_link_libraries(gs1_smoke_engine_host_threading_test PRIVATE gs1_smoke_support)
    target_link_libraries(gs1_system_test_host PRIVATE gs1_gameplay_core)

    # Ensure the DLL is built before the smoke host is run.
    # This is a build-order dependency, not a link-time dependency.
    add_dependencies(gs1_smoke_host gs1_game)
    add_dependencies(gs1_visual_smoke_host gs1_game)

    gs1_enable_strict_warnings(gs1_system_test_host)
    gs1_enable_strict_warnings(gs1_runtime_projection_test)
    gs1_enable_strict_warnings(gs1_runtime_message_chain_test)
    gs1_enable_strict_warnings(gs1_timed_modifier_projection_test)
    gs1_enable_strict_warnings(gs1_site_system_message_flow_test)
    gs1_enable_strict_warnings(gs1_local_weather_perf_probe)
    gs1_enable_strict_warnings(gs1_runtime_performance_test)
    gs1_enable_strict_warnings(gs1_smoke_engine_host_threading_test)

    # Run the smoke host with the built DLL path and the default smoke script.
    add_test(
        NAME gs1_smoke_host
        COMMAND gs1_smoke_host
            $<TARGET_FILE:gs1_game>
            tests/smoke/scripts/main_menu_to_site_active.smoke)

    add_test(
        NAME gs1_runtime_projection_test
        COMMAND gs1_runtime_projection_test)

    add_test(
        NAME gs1_runtime_message_chain_test
        COMMAND gs1_runtime_message_chain_test)

    add_test(
        NAME gs1_timed_modifier_projection_test
        COMMAND gs1_timed_modifier_projection_test)

    add_test(
        NAME gs1_site_system_message_flow_test
        COMMAND gs1_site_system_message_flow_test)

    add_test(
        NAME gs1_smoke_engine_host_threading_test
        COMMAND gs1_smoke_engine_host_threading_test)

    add_test(
        NAME gs1_visual_smoke_silent_onboarding
        COMMAND gs1_visual_smoke_host
            $<TARGET_FILE:gs1_game>
            --silent-onboarding
            --max-frames 18000)

    set(GS1_SYSTEM_TEST_HOST_COMMAND
        gs1_system_test_host)
    if(GS1_SYSTEM_TEST_SYSTEMS)
        foreach(gs1_system_name IN LISTS GS1_SYSTEM_TEST_SYSTEMS)
            list(APPEND GS1_SYSTEM_TEST_HOST_COMMAND --system ${gs1_system_name})
        endforeach()
    else()
        list(APPEND GS1_SYSTEM_TEST_HOST_COMMAND --all)
    endif()

    add_test(
        NAME gs1_system_test_host
        COMMAND ${GS1_SYSTEM_TEST_HOST_COMMAND})

    # Resolve the default script path relative to the repository root.
    set_tests_properties(gs1_smoke_host PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(gs1_runtime_projection_test PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(gs1_runtime_message_chain_test PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(gs1_site_system_message_flow_test PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(gs1_smoke_engine_host_threading_test PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(gs1_visual_smoke_silent_onboarding PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        TIMEOUT 120)
    set_tests_properties(gs1_system_test_host PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        LABELS "system;dll")
endif()
