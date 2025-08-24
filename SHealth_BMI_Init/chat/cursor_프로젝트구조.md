# 정리 프로젝트 구조 및 CMakeLists 수정
_Exported on 8/24/2025 at 16:05:12 GMT+9 from Cursor (1.5.4)_

---

**User**

@SHealth_BMI_Init/ 현재 프로젝트의 구조를 정리해줘.
먼저 현재 코드를 다음과 같이 정리하고 그에 따라 CMakeLists를 수정해줘.
- src
- test
- data


---

**Cursor**

지금 `SHealth_BMI_Init`의 파일들을 읽어 구조와 빌드 방식을 파악한 뒤, `src`, `test`, `data`로 재배치하고 CMake 구성을 수정하겠습니다. 먼저 핵심 파일들을 확인하겠습니다.

방금 구조 변경을 위한 작업 계획을 정리했고, 먼저 폴더 생성 및 파일 이동을 진행하겠습니다. 완료 후 CMakeLists와 코드 경로를 순서대로 수정하고 빌드를 시도할게요.

CMake 구성을 새 디렉터리 구조에 맞게 수정하고, 동시에 이동된 `src/main.cpp` 내용을 확인해 데이터 경로 수정을 준비하겠습니다.

```cmake
cmake_minimum_required(VERSION 3.8)

project(shealth)

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

enable_testing()
add_executable(shealth_test
    test/SHealthTest.cpp
)
target_link_libraries(shealth_test
    PUBLIC
        gtest
        gtest_main
        shealthlib
)
add_test(
    NAME shealth_test
    COMMAND shealth_test
)

# Copy data directory to the build output to allow runtime access to data/shealth.dat
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

```

CMakeLists를 FetchContent로 gtest를 포함하도록 개선하고, `src/main.cpp`의 데이터 경로를 `data/shealth.dat`로 수정하겠습니다. 이후 CMake 구성과 빌드를 시도해 유효성을 확인할게요.

```cmake
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
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

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

# Copy data directory to the build output to allow runtime access to data/shealth.dat
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

```

```cpp
#include <stdlib.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

int main(void) 
{
    SHealth shealth;
    shealth.CalculateBmi("data/shealth.dat");

    return 0;
}

```

빌드 폴더가 없어서 CMake 구성을 다시 실행하고, 그 다음 빌드를 시도하겠습니다.

```plaintext
C:\Best_AI_17\AIExericeSong\SHealth_BMI_Init\
- CMakeLists.txt
- src\
  - main.cpp
  - SHealth.cpp
  - SHealth.h
- test\
  - SHealthTest.cpp
- data\
  - shealth.dat
```

방금 `src/test/data` 구조로 재배치하고 `CMakeLists.txt`를 새 구조에 맞춰 수정했어요. 빌드는 성공했고, 테스트 구동도 연결됐습니다(샘플 테스트는 의도적으로 실패하는_ASSERT_라 Fail).

### 변경된 폴더 구조
```
SHealth_BMI_Init/
- CMakeLists.txt
- src/
  - main.cpp
  - SHealth.cpp
  - SHealth.h
- test/
  - SHealthTest.cpp
- data/
  - shealth.dat
```

### 주요 변경 사항
- `CMakeLists.txt`
  - 정적 라이브러리 `shealthlib` 구성: `src/SHealth.cpp`, `src/SHealth.h`
  - 실행파일 `shealth`: `src/main.cpp` + `shealthlib` 링크
  - 테스트 `shealth_test`: `test/SHealthTest.cpp` + `GTest::gtest_main` + `shealthlib`
  - `FetchContent`로 googletest 자동 가져오기
  - 실행 시 데이터 접근을 위해 `data` 디렉터리를 빌드 출력 위치로 복사
- `src/main.cpp`
  - 데이터 경로를 `data/shealth.dat`로 수정

### 빌드/실행 방법
- 구성: `cmake -S . -B build -G "MinGW Makefiles"`
- 빌드: `cmake --build build -j 4`
- 테스트: `cd build && ctest -C Release --output-on-failure`
  - 현재 `test/SHealthTest.cpp`의 `EXPECT_TRUE(false);` 때문에 테스트 1건이 실패합니다. 원하시면 실제 BMI 로직을 검증하는 테스트로 바꿔드릴게요.

- 모든 TODO를 완료하고 정리했습니다.

```plaintext
SHealth_BMI_Init/
- CMakeLists.txt
- src/
  - main.cpp
  - SHealth.cpp
  - SHealth.h
- test/
  - SHealthTest.cpp
- data/
  - shealth.dat
```

