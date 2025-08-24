cmake_minimum_required(VERSION 3.16)

# Assume GoogleTest is installed on the system (per environment.txt)
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(converter_tests
    test_conversion.cpp
    test_quality.cpp
)
target_link_libraries(converter_tests PRIVATE converter GTest::gtest GTest::gtest_main Threads::Threads)

# Cygwin: expose POSIX functions (fileno, fdopen) for GTest port
if(CYGWIN)
    target_compile_definitions(converter_tests PRIVATE _POSIX_C_SOURCE=200809L _GNU_SOURCE)
endif()

include(GoogleTest)
gtest_discover_tests(converter_tests)


