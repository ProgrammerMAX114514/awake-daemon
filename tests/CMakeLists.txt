# =============================================================================
# tests/CMakeLists.txt - unit test target (R7).
#
# The tests link awake_common, the same static library both executables use,
# so they exercise the real implementation instead of a copy of it.
#
# Catch2 v2 is vendored as a single header in third_party/catch2 (see the
# README there): no network access is needed to configure or build, which
# keeps the CI job and an offline developer machine on the same footing.
# =============================================================================

add_executable(awake_tests
    main.cpp
    test_config.cpp
    test_protocol.cpp
)

target_include_directories(awake_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/catch2
)
target_link_libraries(awake_tests PRIVATE awake_common)

# The vendored header is third-party code and does not have to be warning
# clean; the project's own test sources still build with the global /W4.
if(MSVC)
    target_compile_options(awake_tests PRIVATE /wd4996)
endif()

# Registered so that "ctest --test-dir build" works locally as well.
add_test(NAME awake_tests COMMAND awake_tests)
