cmake_minimum_required(VERSION 3.8)

project(shealth)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(shealthlib STATIC
    src/SHealth.cpp
    src/SHealth.h
)
target_include_directories(shealthlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(shealth
    src/main.cpp
)
target_link_libraries(shealth
    PUBLIC
        shealthlib
)

# Fetch GoogleTest if not available
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Fetch ApprovalTests
FetchContent_Declare(
    ApprovalTests
    GIT_REPOSITORY https://github.com/approvals/ApprovalTests.cpp.git
    GIT_TAG v.10.12.0
)
FetchContent_MakeAvailable(ApprovalTests)

enable_testing()
add_executable(shealth_test
    test/SHealthTest.cpp
)
target_link_libraries(shealth_test
    PUBLIC
        GTest::gtest_main
        shealthlib
)
add_test(
    NAME shealth_test
    COMMAND shealth_test
)

# ApprovalTests + GoogleTest executable
add_executable(shealth_approval_tests
    ApprovalTest/SHealthApprovalTests.cpp
)
target_link_libraries(shealth_approval_tests
    PUBLIC
        shealthlib
        GTest::gtest_main
        ApprovalTests::ApprovalTests
)
add_test(
    NAME shealth_approval_tests
    COMMAND shealth_approval_tests
)

# Copy data directory to the build output to allow runtime access to data/shealth.dat
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Copy approvals directory so that approved files are found at test runtime
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/ApprovalTest/approvals DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
