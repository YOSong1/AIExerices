cmake_minimum_required(VERSION 3.16)

project(unit_converter_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optional: Code coverage support (GCC/Clang)
option(ENABLE_CODE_COVERAGE "Enable code coverage flags and targets" OFF)
if(ENABLE_CODE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Code coverage enabled")
        add_compile_options(-O0 -g --coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "Code coverage is only supported with GCC/Clang. Current: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

add_subdirectory(src)

option(UNIT_CONVERTER_BUILD_TESTS "Build unit tests" ON)
if(UNIT_CONVERTER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Convenience target to run the program after build
add_custom_target(run
    COMMAND $<TARGET_FILE:unit_converter>
    DEPENDS unit_converter
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    USES_TERMINAL
)


# Coverage aggregate target: runs tests and emits gcovr reports when coverage enabled
if(ENABLE_CODE_COVERAGE)
    # Create coverage directory if it doesn't exist
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/coverage)
    
    add_custom_target(coverage
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND python -m gcovr -r .. --filter ../src --xml-pretty -o ../coverage/coverage.xml
        COMMAND python -m gcovr -r .. --filter ../src --html-details -o ../coverage/coverage.html
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS converter_tests
        USES_TERMINAL
    )
endif()


