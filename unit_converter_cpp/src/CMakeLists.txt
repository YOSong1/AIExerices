# Library for conversion core
add_library(converter
    converter.cpp
    config_loader.cpp
)
# Expose public headers from src
# Consumers (tests, app) can include headers via this directory
# e.g., #include "converter.h"
target_include_directories(converter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Executable using the existing main
add_executable(unit_converter
    unit_converter.cpp
)
target_link_libraries(unit_converter PRIVATE converter)
