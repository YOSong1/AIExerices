@echo off
echo Starting Code Coverage Analysis for SHealth...

REM Clean previous build
if exist build (
    rmdir /s /q build
)

REM Create build directory
mkdir build
cd build

REM Configure with coverage enabled
echo Configuring CMake with coverage enabled...
cmake -DENABLE_COVERAGE=ON ..

REM Build the project
echo Building the project...
cmake --build .

REM Run tests to generate coverage data
echo Running tests...
ctest --output-on-failure

REM Go back to project root for gcovr
cd ..

REM Generate coverage reports using gcovr
echo Generating coverage reports with gcovr...

REM Create HTML coverage report with detailed line-by-line view
bash -c "gcovr --root . --html --html-details -o coverage/coverage.html --exclude 'build/_deps/.*' --exclude 'test/.*' --exclude 'ApprovalTest/.*' --exclude 'coverage/.*'"

REM Create XML coverage report for CI/CD integration
bash -c "gcovr --root . --xml -o coverage/coverage.xml --exclude 'build/_deps/.*' --exclude 'test/.*' --exclude 'ApprovalTest/.*' --exclude 'coverage/.*'"

REM Create text coverage summary
bash -c "gcovr --root . -o coverage/coverage.txt --exclude 'build/_deps/.*' --exclude 'test/.*' --exclude 'ApprovalTest/.*' --exclude 'coverage/.*'"

REM Also create a console summary
echo.
echo ========================================
echo Coverage Summary:
echo ========================================
bash -c "gcovr --root . --exclude 'build/_deps/.*' --exclude 'test/.*' --exclude 'ApprovalTest/.*' --exclude 'coverage/.*'"

echo.
echo ========================================
echo Coverage Analysis Complete!
echo ========================================
echo Files generated in coverage/ folder:
echo - coverage.html (Main HTML report)
echo - coverage.*.html (Detailed per-file reports)
echo - coverage.xml (XML report for CI/CD)
echo - coverage.txt (Text summary)
echo.
echo Open coverage/coverage.html in your browser to view detailed results.
echo.

pause
