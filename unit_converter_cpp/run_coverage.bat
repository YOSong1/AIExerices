@echo off
echo Building with code coverage...
cmake -S . -B build -DENABLE_CODE_COVERAGE=ON
cmake --build build
echo.
echo Running tests and generating coverage report...
cmake --build build --target coverage
echo.
echo Coverage report generated in coverage/ folder
echo Open coverage/coverage.html in your browser to view the report
pause
