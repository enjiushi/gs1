cmake_minimum_required(VERSION 3.24)

# Build definition for the gameplay DLL plus the smoke host executable.
project(gs1_game
    VERSION 0.1.0
    LANGUAGES CXX)

include(GNUInstallDirs)

# Toggle smoke-test host build without affecting the gameplay DLL target.
option(GS1_BUILD_TESTS "Build GS1 smoke tests" ON)

# Use a modern, compiler-independent C++ baseline for both targets.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build the pure gameplay runtime as a shared library (the DLL).
add_library(gs1_game SHARED
    src/app/game_api.cpp
    src/app/campaign_factory.cpp
    src/app/site_run_factory.cpp
    src/commands/command_dispatcher.cpp
    src/content/prototype_content.cpp
    src/runtime/game_runtime.cpp
)

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

# Install the DLL/import library and the public headers as the gameplay SDK output.
install(TARGETS gs1_game
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Keep warning levels strict for the gameplay library on all toolchains.
if(MSVC)
    target_compile_options(gs1_game PRIVATE /W4 /permissive-)
else()
    target_compile_options(gs1_game PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(GS1_BUILD_TESTS)
    enable_testing()

    set(GS1_SMOKE_HOST_COMMON_SOURCES
        tests/smoke/smoke_engine_host.cpp
        tests/smoke/smoke_log.cpp
        tests/smoke/runtime_dll_loader.cpp
        tests/smoke/smoke_live_state_json.cpp)

    # Build the text smoke host executable separately from the DLL.
    # This executable loads the gameplay DLL at runtime instead of linking to it.
    add_executable(gs1_smoke_host
        tests/smoke/smoke_host_main.cpp
        tests/smoke/smoke_script_runner.cpp
        ${GS1_SMOKE_HOST_COMMON_SOURCES})

    # Build the live visual smoke host in parallel to the text smoke host.
    # This host is a manual visual adapter and is not part of the script-driven smoke test path.
    add_executable(gs1_visual_smoke_host
        tests/smoke/visual_smoke_host_main.cpp
        tests/smoke/smoke_live_http_server.cpp
        ${GS1_SMOKE_HOST_COMMON_SOURCES})

    # The smoke host includes only the public gameplay SDK headers.
    target_include_directories(gs1_smoke_host PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_include_directories(gs1_visual_smoke_host PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(gs1_visual_smoke_host PRIVATE ws2_32)

    # Ensure the DLL is built before the smoke host is run.
    # This is a build-order dependency, not a link-time dependency.
    add_dependencies(gs1_smoke_host gs1_game)
    add_dependencies(gs1_visual_smoke_host gs1_game)

    # Run the smoke host with the built DLL path and the default smoke script.
    add_test(
        NAME gs1_smoke_host
        COMMAND gs1_smoke_host
            $<TARGET_FILE:gs1_game>
            tests/smoke/scripts/main_menu_to_first_site.smoke)

    # Resolve the default script path relative to the repository root.
    set_tests_properties(gs1_smoke_host PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
