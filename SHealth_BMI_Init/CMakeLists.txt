cmake_minimum_required(VERSION 3.8)

set(PROJECT shealthlib)
set(SOURCES
    SHealth.cpp
    SHealth.h
)
add_library(${PROJECT} STATIC ${SOURCES})

project(${PROJECT})

set(PROJECT shealth)
set(SOURCES
    main.cpp
)
add_executable(${PROJECT} ${SOURCES})
target_link_libraries(${PROJECT}
    PUBLIC
    shealthlib
)

set(PROJECT shealth_test)
set(SOURCES
    SHealthTest.cpp
)
add_executable(${PROJECT} ${SOURCES})
target_link_libraries(${PROJECT}
    PUBLIC
        gtest
        gtest_main
        shealthlib
)
add_test(
    NAME ${PROJECT}
    COMMAND ${PROJECT}
)
