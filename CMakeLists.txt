cmake_minimum_required(VERSION 3.16)

project(logforge
    VERSION 0.3.0
    DESCRIPTION "A small dependency-free logging bridge with a spdlog backend"
    LANGUAGES CXX)

include(GNUInstallDirs)

# ---------------------------------------------------------------------------
# Layout of this file
#
#   1. options
#   2. logforge::core   -- the interface every producer talks to (header only)
#   3. logforge::spdlog -- the backend (added in a later milestone)
#   4. tests / examples this repository builds for itself
#   5. install / export rules
#
# The split mirrors the two decisions the library is built around:
#   * a log record crosses the interface as plain text, so the interface is a
#     handful of lines and never has to change;
#   * rendering that text is a front-end concern, so the renderer lives next to
#     the macros and the backend receives something it can already write.
#
# Nothing a consumer needs is allowed to leak out of `logforge::core`: no
# C++ standard above C++11, no warnings policy, no third-party package.
# ---------------------------------------------------------------------------

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(LOGFORGE_IS_TOP_LEVEL ON)
else()
    set(LOGFORGE_IS_TOP_LEVEL OFF)
endif()

option(LOGFORGE_BUILD_TESTS   "Build the unit tests"                     ${LOGFORGE_IS_TOP_LEVEL})
option(LOGFORGE_INSTALL       "Generate install and package rules"        ${LOGFORGE_IS_TOP_LEVEL})
option(LOGFORGE_REQUIRE_TESTS "Fail configure when GoogleTest is missing" OFF)
option(LOGFORGE_FETCH_GTEST   "Download googletest when it is not installed" OFF)
option(LOGFORGE_WERROR        "Treat warnings in this repository's own targets as errors" OFF)

# ---------------------------------------------------------------------------
# Version header
#
# Generated rather than hand written so that CMake and the headers can never
# disagree about what version is being built.
# ---------------------------------------------------------------------------
configure_file(cmake/version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/logforge/version.hpp
    @ONLY)

# ---------------------------------------------------------------------------
# logforge::core
#
# Header only.  It owns the level enum, the logger interface, the per-thread
# binding, the macros and the non-throwing renderer.  It has no dependencies at
# all -- in particular it does not know that spdlog exists, which is what keeps
# the backend replaceable.
# ---------------------------------------------------------------------------
add_library(logforge_core INTERFACE)
add_library(logforge::core ALIAS logforge_core)

# The export name is what a consumer sees after find_package(): without it the
# installed target would be logforge::logforge_core.
set_target_properties(logforge_core PROPERTIES EXPORT_NAME core)

target_include_directories(logforge_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# C++11 is a promise, not an accident: pipeforge is a C++11 library and it links
# this target, so nothing here may require std::string_view or std::optional.
target_compile_features(logforge_core INTERFACE cxx_std_11)

# ---------------------------------------------------------------------------
# Flags for the targets this repository builds for itself.
#
# Deliberately a target and not add_compile_options(): a consumer must not
# inherit our warnings policy, and a parent project must not receive our /utf-8
# next to its own charset flags (MSVC rejects the combination with D8016 when a
# fetched dependency sets the other spelling).
# ---------------------------------------------------------------------------
add_library(logforge_build_flags INTERFACE)

if(MSVC)
    target_compile_options(logforge_build_flags INTERFACE /utf-8 /W4)
    if(LOGFORGE_WERROR)
        target_compile_options(logforge_build_flags INTERFACE /WX)
    endif()
    target_compile_definitions(logforge_build_flags INTERFACE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX)
else()
    target_compile_options(logforge_build_flags INTERFACE -Wall -Wextra)
    if(LOGFORGE_WERROR)
        target_compile_options(logforge_build_flags INTERFACE -Werror)
    endif()
endif()

# ---------------------------------------------------------------------------
# Unit tests
#
# GoogleTest is optional so the library alone builds anywhere.  CI turns
# LOGFORGE_REQUIRE_TESTS on, which makes a missing GoogleTest a configure error
# instead of a silent "skipping tests", and LOGFORGE_FETCH_GTEST on, which
# downloads it when the system has none (the Windows runners do not ship it).
# ---------------------------------------------------------------------------
if(LOGFORGE_BUILD_TESTS)
    enable_testing()

    find_package(GTest QUIET)
    if(NOT GTest_FOUND AND LOGFORGE_FETCH_GTEST)
        include(FetchContent)
        # 1.14 is the last googletest that still supports C++14; newer releases
        # require C++17 and the backend eventually has to be buildable next to
        # C++11 consumers.
        FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
            GIT_SHALLOW TRUE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
        set(GTest_FOUND TRUE)
    endif()

    if(GTest_FOUND)
        # An installed GoogleTest comes through CMake's FindGTest and offers
        # GTest::GTest / GTest::Main; the fetched one only has the upstream
        # aliases.
        if(TARGET GTest::GTest)
            set(LOGFORGE_GTEST_LIBS GTest::GTest GTest::Main)
        else()
            set(LOGFORGE_GTEST_LIBS GTest::gtest GTest::gtest_main)
        endif()

        # Test-only dependency: the library itself never creates a thread, but
        # ambient_test has to prove that a thread binding is invisible to other
        # threads, and std::thread needs the right flags on some toolchains.
        find_package(Threads REQUIRED)

        function(logforge_add_test name)
            add_executable(${name} tests/${name}.cpp)
            target_link_libraries(${name} PRIVATE
                logforge::core ${LOGFORGE_GTEST_LIBS} logforge_build_flags Threads::Threads)
            add_test(NAME ${name} COMMAND ${name})
        endfunction()

        logforge_add_test(level_test)
        logforge_add_test(logger_test)
        logforge_add_test(ambient_test)
        logforge_add_test(stderr_logger_test)
        logforge_add_test(format_test)
        logforge_add_test(macro_test)

        # Same headers, but with the source location compiled out; the macro has
        # to pass a literal null in that configuration.
        add_executable(macro_no_source_test tests/macro_no_source_test.cpp)
        target_link_libraries(macro_no_source_test PRIVATE
            logforge::core ${LOGFORGE_GTEST_LIBS} logforge_build_flags Threads::Threads)
        target_compile_definitions(macro_no_source_test PRIVATE LOGFORGE_SOURCE_LOCATION=0)
        add_test(NAME macro_no_source_test COMMAND macro_no_source_test)
    elseif(LOGFORGE_REQUIRE_TESTS)
        message(FATAL_ERROR
            "GoogleTest not found and LOGFORGE_REQUIRE_TESTS is ON. "
            "Install it (libgtest-dev / vcpkg) or configure with -DLOGFORGE_FETCH_GTEST=ON.")
    else()
        message(STATUS "GoogleTest not found, skipping unit tests")
    endif()
endif()

# ---------------------------------------------------------------------------
# Install and package
#
# logforge::core is header only, so exporting it is a matter of shipping the
# header tree plus a config file.  The backend target joins the same export in a
# later milestone; consumers still only ever name logforge::core or
# logforge::spdlog.
# ---------------------------------------------------------------------------
if(LOGFORGE_INSTALL)
    include(CMakePackageConfigHelpers)

    install(TARGETS logforge_core EXPORT logforgeTargets)
    install(DIRECTORY include/logforge DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/logforge/version.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/logforge)

    set(LOGFORGE_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/logforge)
    install(EXPORT logforgeTargets
        NAMESPACE logforge::
        FILE logforgeTargets.cmake
        DESTINATION ${LOGFORGE_CMAKE_DIR})

    configure_package_config_file(cmake/logforgeConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/logforgeConfig.cmake
        INSTALL_DESTINATION ${LOGFORGE_CMAKE_DIR})
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/logforgeConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/logforgeConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/logforgeConfigVersion.cmake
        DESTINATION ${LOGFORGE_CMAKE_DIR})
endif()

if(LOGFORGE_IS_TOP_LEVEL)
    message(STATUS "logforge ${PROJECT_VERSION} on ${CMAKE_SYSTEM_NAME}")
    message(STATUS "  C++ compiler  : ${CMAKE_CXX_COMPILER}")
    message(STATUS "  library target: logforge::core")
    message(STATUS "  tests         : ${LOGFORGE_BUILD_TESTS}")
    message(STATUS "  install rules : ${LOGFORGE_INSTALL}")
endif()
