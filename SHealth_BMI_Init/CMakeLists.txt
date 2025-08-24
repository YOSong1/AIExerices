cmake_minimum_required(VERSION 3.8)

project(shealth)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Coverage configuration
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
if(ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# Main library
add_library(shealthlib STATIC
    src/SHealth.cpp
    src/SHealth.h
)
target_include_directories(shealthlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 테스트 빌드 시에만 필요한 매크로 정의 (현재는 불필요하지만 향후 확장성을 위해 유지)
if(BUILD_TESTING)
    target_compile_definitions(shealthlib PRIVATE ENABLE_TESTING)
endif()

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
