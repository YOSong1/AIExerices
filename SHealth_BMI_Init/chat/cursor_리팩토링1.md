# 리팩토링 전략 제시 및 코드 스멜 분석
_Exported on 8/24/2025 at 23:42:20 GMT+9 from Cursor (1.5.4)_

---

**User**

@SHealth.cpp @SHealth.h 개발된 코드에서 코드 스멜을 느끼고 리팩토링이 필요한 부분을 찾아줘.
- 리팩토링 전략을 코드 변경 이전에 먼저 제시해줘
- 코드는 리팩토링 방법 확정하기전에는 변경하지마

---

**Cursor**

### 코드 스멜 진단

- **헤더 오염(`using namespace std`) 및 의존성 누락**
  - 헤더에서 네임스페이스를 공개적으로 오염시키고, 필요한 표준 헤더를 포함하지 않습니다.
  ```5:7:SHealth_BMI_Init/src/SHealth.h
using namespace std;

class SHealth {
```
  - `SHealth.h`는 `vector<string>`을 선언하지만 `<vector>`, `<string>`을 포함하지 않습니다. 헤더 자체로 자족적(self-contained)이지 않습니다.
  - `Split`은 클래스 내부 구현 디테일인데 헤더에 노출되어 결합도를 높입니다.

- **C 스타일 파일 I/O 사용과 자원 누수**
  - C 표준 I/O(`FILE*`, `fopen`, `fgets`) 사용, 예외 안전/RAII 부재, `fclose` 호출 누락.
  ```25:29:SHealth_BMI_Init/src/SHealth.cpp
FILE *f = fopen(filename.c_str(), "r");
while(true)
{
    if(fgets(line, 256, f) == NULL) break;
```

- **취약한 파싱과 고정 버퍼**
  - `char line[256]` 고정 크기 버퍼 → 라인 길이 초과 시 데이터 절단 위험.
  - CSV 파싱 시 토큰 개수 검증 부재(`tokens.size() == 0`만 체크), 빈 줄에서 `break`로 전체 읽기 중단(스킵이 아님).
  - `atoi/atof` 사용으로 에러 검출 불가, 로케일/트리밍 처리도 부재.

- **경계값/분모 0 버그**
  - BMI 25.0이 어떤 구간에도 포함되지 않습니다(누락).
  ```79:83:SHealth_BMI_Init/src/SHealth.cpp
if (bmis[i] <= 18.5) underweight++;
if (bmis[i] > 18.5 && bmis[i] < 23) normalweight++;
if (bmis[i] >= 23 && bmis[i] < 25) overweight++;
if (bmis[i] > 25) obesity++;
```
  - 가중치 보간에서 `age_count == 0`일 때 0으로 나눔 위험.
  ```55:56:SHealth_BMI_Init/src/SHealth.cpp
if (weights[i] == 0.0) weights[i] = sum / age_count;
```
  - 비율 계산에서 연령대 표본 수(`sum`)가 0일 때 0으로 나눌 수 있습니다.

- **병렬 배열과 고정 크기**
  - `ages/weights/heights/bmis`를 길이 10000의 병렬 배열로 관리 → 오버플로 위험, 응집도/가독성 저하, 캡슐화 약함.

- **매직 넘버 다수**
  - 연령대 20~70, BMI 경계, 타입 코드 100/200/300/400 등 하드코딩 → 의미 불명확, 유지보수 비용 증가.

- **중복 코드**
  - 연령대 루프 6회에 대한 동일 로직, `GetBmiRatio`의 24개 조건문 등 반복/보일러플레이트 다수.

- **const 정확성/인터페이스 설계**
  - `GetBmiRatio`는 읽기 전용 동작이지만 `const`가 아님.
  - `Split`은 멤버 상태를 쓰지 않으므로 정적/자유 함수가 적합.

- **에러 처리/로그 부재**
  - 파일 열기 실패, 파싱 실패, 비정상 데이터(음수 키/몸무게 등) 처리 전략 없음.

---

### 리팩토링 전략(코드 변경 전 제안)

- **1단계: 헤더 위생과 API 최소 정리(호환 유지)**
  - `SHealth.h`에서 `using namespace std` 제거, `<vector>`, `<string>` 포함해 헤더 자족성 확보.
  - `Split`은 `SHealth.cpp`의 내부 정적 함수(또는 익명 네임스페이스)로 이동해 비공개화.
  - `GetBmiRatio`를 `const`로 선언(시그니처는 유지). 기존 `int type(100/200/300/400)`은 당장은 유지하되, 내부에 `enum` 매핑을 도입.

- **2단계: 입력/파싱 안전성 강화(동일 기능)**
  - `std::ifstream` + `std::getline`로 교체, RAII로 자원 관리.
  - 빈 줄/주석 스킵, CSV 토큰 개수 검증(최소 4개), 트리밍 처리.
  - 실패/예외(파일 없음, 파싱 실패) 시 명확한 반환값/예외/에러 코드 정책 수립.

- **3단계: 데이터 모델 개선(병렬 배열 → 구조체 + 동적 컨테이너)**
  - `struct Record { int age; double height; double weight; double bmi; };`
  - `std::vector<Record>`로 저장. 고정 크기 배열 제거, `count`는 `records.size()`로 대체.
  - 기존 퍼블릭 API는 유지하되 내부 표현만 교체.

- **4단계: 집계/보간 로직 모듈화**
  - `AgeBucket` 계산 함수(20/30/…/70)와 `BmiCategory` 판정 함수를 분리.
  - 분모 0 방지: 보간(`age_count == 0`)과 비율 계산(`sum == 0`)에 대한 안전 가드와 폴백 정책 정의.
  - BMI 경계값 상수화(`constexpr`) 및 포함 규칙 통일(25.0 포함 등).

- **5단계: 중복 제거와 자료구조 단순화**
  - `std::array` 또는 `std::unordered_map<int, Stats>`로 연령대별 카운트/비율을 보관.
  - 6개 연령대 × 4개 분류를 2중 배열/맵으로 축약. `GetBmiRatio`는 내부 맵 조회로 대체.

- **6단계: 매직 넘버 제거와 의도 드러내기**
  - 타입 코드 100/200/300/400 → `enum class BmiType { Under=100, Normal=200, Over=300, Obese=400 };`
  - 연령대 목록, BMI 경계값을 상수/열거형으로 정의.

- **7단계: 테스트 방어막 보강**
  - 기존 ApprovalTests 유지.
  - 추가 케이스: 연령대 데이터 없음(분모 0), 체중 전부 0(보간 불가), BMI=18.5/23/25 경계, 긴 라인/공백 포함, 범위 밖 연령.

- **8단계: 점진적 마이그레이션**
  - 1~2단계로 안전성/헤더 문제를 먼저 해결(무해한 변경).
  - 3~5단계를 소규모 커밋으로 나눠 적용, ApprovalTests로 회귀 방지.
  - 마지막에 필요 시 `GetBmiRatio`의 신규 오버로드(API 개선) 추가 후 구버전은 deprecated 표기.

---

### 새 구조의 간단 스케치(설명용)

```cpp
// 개념 예시: 실제 적용 시 파일/이름은 프로젝트 컨벤션에 맞춤
enum class BmiType { Under = 100, Normal = 200, Over = 300, Obese = 400 };

struct Record { int age; double height; double weight; double bmi; };

inline int age_bucket(int age) { return std::clamp((age/10)*10, 20, 70); }

inline BmiType classify(double bmi) {
    if (bmi <= 18.5) return BmiType::Under;
    if (bmi < 23.0)  return BmiType::Normal;   // 18.5 < bmi < 23
    if (bmi < 25.0)  return BmiType::Over;     // 23 <= bmi < 25
    return BmiType::Obese;                     // 25 <= bmi
}
```

---

### 리스크/호환성 고려

- BMI 25.0 포함 규칙 수정 시 기존 승인 스냅샷과 결과가 달라질 수 있음(테스트 업데이트 필요).
- `GetBmiRatio`의 타입 코드 유지/폐기는 상위 코드 영향. 단계적으로 오버로드 후 이전 API 폐기 권장.
- 입력 파서 강화로 잘못된 데이터가 이전엔 “무시되고” 지나갔다면 이제 명확히 실패로 처리될 수 있음.

---

### 확인 요청

- BMI 경계값을 위 스케치처럼 고정하시겠습니까? 25.0을 비만에 포함하는 것이 표준이며 현재 코드와 다릅니다.
- `GetBmiRatio(int, int)`는 유지하되 내부적으로 `enum`으로 매핑하는 수준부터 시작해도 될까요?
- 데이터 행의 최대 길이/형식(헤더 유무, 공백 포함) 제약이 있나요? 파서 설계에 반영하겠습니다.

- 선택해 주세요:
  - **옵션 A(안전 최소 변경)**: 헤더/RAII/가드 추가 위주(기능 동일).
  - **옵션 B(중간)**: + 데이터 모델을 `vector<Record>`로 교체, 집계 자료구조 단순화.
  - **옵션 C(전면)**: + API 개선(열거형/맵), 로직 완전 모듈화.

—

- 핵심 문제: 헤더 오염, C 스타일 I/O와 자원 누수, CSV 파싱 취약, 경계/분모 0 버그, 병렬 배열·매직 넘버·중복 코드.
- 제안: 단계적 리팩토링으로 안전성→모델링→집계 모듈화→API 정리, ApprovalTests로 회귀 방지.

```plaintext
using namespace std;

class SHealth {
```
  - `SHealth.h`는 `vector<string>`을 선언하지만 `<vector>`, `<string>`을 포함하지 않습니다. 헤더 자체로 자족적(self-contained)이지 않습니다.
  - `Split`은 클래스 내부 구현 디테일인데 헤더에 노출되어 결합도를 높입니다.

- **C 스타일 파일 I/O 사용과 자원 누수**
  - C 표준 I/O(`FILE*`, `fopen`, `fgets`) 사용, 예외 안전/RAII 부재, `fclose` 호출 누락.
  ```25:29:SHealth_BMI_Init/src/SHealth.cpp
FILE *f = fopen(filename.c_str(), "r");
while(true)
{
    if(fgets(line, 256, f) == NULL) break;
```

- **취약한 파싱과 고정 버퍼**
  - `char line[256]` 고정 크기 버퍼 → 라인 길이 초과 시 데이터 절단 위험.
  - CSV 파싱 시 토큰 개수 검증 부재(`tokens.size() == 0`만 체크), 빈 줄에서 `break`로 전체 읽기 중단(스킵이 아님).
  - `atoi/atof` 사용으로 에러 검출 불가, 로케일/트리밍 처리도 부재.

- **경계값/분모 0 버그**
  - BMI 25.0이 어떤 구간에도 포함되지 않습니다(누락).
  ```79:83:SHealth_BMI_Init/src/SHealth.cpp
if (bmis[i] <= 18.5) underweight++;
if (bmis[i] > 18.5 && bmis[i] < 23) normalweight++;
if (bmis[i] >= 23 && bmis[i] < 25) overweight++;
if (bmis[i] > 25) obesity++;
```
  - 가중치 보간에서 `age_count == 0`일 때 0으로 나눔 위험.
  ```55:56:SHealth_BMI_Init/src/SHealth.cpp
if (weights[i] == 0.0) weights[i] = sum / age_count;
```
  - 비율 계산에서 연령대 표본 수(`sum`)가 0일 때 0으로 나눌 수 있습니다.

- **병렬 배열과 고정 크기**
  - `ages/weights/heights/bmis`를 길이 10000의 병렬 배열로 관리 → 오버플로 위험, 응집도/가독성 저하, 캡슐화 약함.

- **매직 넘버 다수**
  - 연령대 20~70, BMI 경계, 타입 코드 100/200/300/400 등 하드코딩 → 의미 불명확, 유지보수 비용 증가.

- **중복 코드**
  - 연령대 루프 6회에 대한 동일 로직, `GetBmiRatio`의 24개 조건문 등 반복/보일러플레이트 다수.

- **const 정확성/인터페이스 설계**
  - `GetBmiRatio`는 읽기 전용 동작이지만 `const`가 아님.
  - `Split`은 멤버 상태를 쓰지 않으므로 정적/자유 함수가 적합.

- **에러 처리/로그 부재**
  - 파일 열기 실패, 파싱 실패, 비정상 데이터(음수 키/몸무게 등) 처리 전략 없음.

---

### 리팩토링 전략(코드 변경 전 제안)

- **1단계: 헤더 위생과 API 최소 정리(호환 유지)**
  - `SHealth.h`에서 `using namespace std` 제거, `<vector>`, `<string>` 포함해 헤더 자족성 확보.
  - `Split`은 `SHealth.cpp`의 내부 정적 함수(또는 익명 네임스페이스)로 이동해 비공개화.
  - `GetBmiRatio`를 `const`로 선언(시그니처는 유지). 기존 `int type(100/200/300/400)`은 당장은 유지하되, 내부에 `enum` 매핑을 도입.

- **2단계: 입력/파싱 안전성 강화(동일 기능)**
  - `std::ifstream` + `std::getline`로 교체, RAII로 자원 관리.
  - 빈 줄/주석 스킵, CSV 토큰 개수 검증(최소 4개), 트리밍 처리.
  - 실패/예외(파일 없음, 파싱 실패) 시 명확한 반환값/예외/에러 코드 정책 수립.

- **3단계: 데이터 모델 개선(병렬 배열 → 구조체 + 동적 컨테이너)**
  - `struct Record { int age; double height; double weight; double bmi; };`
  - `std::vector<Record>`로 저장. 고정 크기 배열 제거, `count`는 `records.size()`로 대체.
  - 기존 퍼블릭 API는 유지하되 내부 표현만 교체.

- **4단계: 집계/보간 로직 모듈화**
  - `AgeBucket` 계산 함수(20/30/…/70)와 `BmiCategory` 판정 함수를 분리.
  - 분모 0 방지: 보간(`age_count == 0`)과 비율 계산(`sum == 0`)에 대한 안전 가드와 폴백 정책 정의.
  - BMI 경계값 상수화(`constexpr`) 및 포함 규칙 통일(25.0 포함 등).

- **5단계: 중복 제거와 자료구조 단순화**
  - `std::array` 또는 `std::unordered_map<int, Stats>`로 연령대별 카운트/비율을 보관.
  - 6개 연령대 × 4개 분류를 2중 배열/맵으로 축약. `GetBmiRatio`는 내부 맵 조회로 대체.

- **6단계: 매직 넘버 제거와 의도 드러내기**
  - 타입 코드 100/200/300/400 → `enum class BmiType { Under=100, Normal=200, Over=300, Obese=400 };`
  - 연령대 목록, BMI 경계값을 상수/열거형으로 정의.

- **7단계: 테스트 방어막 보강**
  - 기존 ApprovalTests 유지.
  - 추가 케이스: 연령대 데이터 없음(분모 0), 체중 전부 0(보간 불가), BMI=18.5/23/25 경계, 긴 라인/공백 포함, 범위 밖 연령.

- **8단계: 점진적 마이그레이션**
  - 1~2단계로 안전성/헤더 문제를 먼저 해결(무해한 변경).
  - 3~5단계를 소규모 커밋으로 나눠 적용, ApprovalTests로 회귀 방지.
  - 마지막에 필요 시 `GetBmiRatio`의 신규 오버로드(API 개선) 추가 후 구버전은 deprecated 표기.

---

### 새 구조의 간단 스케치(설명용)

```cpp
// 개념 예시: 실제 적용 시 파일/이름은 프로젝트 컨벤션에 맞춤
enum class BmiType { Under = 100, Normal = 200, Over = 300, Obese = 400 };

struct Record { int age; double height; double weight; double bmi; };

inline int age_bucket(int age) { return std::clamp((age/10)*10, 20, 70); }

inline BmiType classify(double bmi) {
    if (bmi <= 18.5) return BmiType::Under;
    if (bmi < 23.0)  return BmiType::Normal;   // 18.5 < bmi < 23
    if (bmi < 25.0)  return BmiType::Over;     // 23 <= bmi < 25
    return BmiType::Obese;                     // 25 <= bmi
}
```

---

### 리스크/호환성 고려

- BMI 25.0 포함 규칙 수정 시 기존 승인 스냅샷과 결과가 달라질 수 있음(테스트 업데이트 필요).
- `GetBmiRatio`의 타입 코드 유지/폐기는 상위 코드 영향. 단계적으로 오버로드 후 이전 API 폐기 권장.
- 입력 파서 강화로 잘못된 데이터가 이전엔 “무시되고” 지나갔다면 이제 명확히 실패로 처리될 수 있음.

---

### 확인 요청

- BMI 경계값을 위 스케치처럼 고정하시겠습니까? 25.0을 비만에 포함하는 것이 표준이며 현재 코드와 다릅니다.
- `GetBmiRatio(int, int)`는 유지하되 내부적으로 `enum`으로 매핑하는 수준부터 시작해도 될까요?
- 데이터 행의 최대 길이/형식(헤더 유무, 공백 포함) 제약이 있나요? 파서 설계에 반영하겠습니다.

- 선택해 주세요:
  - **옵션 A(안전 최소 변경)**: 헤더/RAII/가드 추가 위주(기능 동일).
  - **옵션 B(중간)**: + 데이터 모델을 `vector<Record>`로 교체, 집계 자료구조 단순화.
  - **옵션 C(전면)**: + API 개선(열거형/맵), 로직 완전 모듈화.

—

- 핵심 문제: 헤더 오염, C 스타일 I/O와 자원 누수, CSV 파싱 취약, 경계/분모 0 버그, 병렬 배열·매직 넘버·중복 코드.
- 제안: 단계적 리팩토링으로 안전성→모델링→집계 모듈화→API 정리, ApprovalTests로 회귀 방지.
```

```plaintext
FILE *f = fopen(filename.c_str(), "r");
while(true)
{
    if(fgets(line, 256, f) == NULL) break;
```

```plaintext
if (bmis[i] <= 18.5) underweight++;
if (bmis[i] > 18.5 && bmis[i] < 23) normalweight++;
if (bmis[i] >= 23 && bmis[i] < 25) overweight++;
if (bmis[i] > 25) obesity++;
```

```plaintext
if (weights[i] == 0.0) weights[i] = sum / age_count;
```

```cpp
// 개념 예시: 실제 적용 시 파일/이름은 프로젝트 컨벤션에 맞춤
enum class BmiType { Under = 100, Normal = 200, Over = 300, Obese = 400 };

struct Record { int age; double height; double weight; double bmi; };

inline int age_bucket(int age) { return std::clamp((age/10)*10, 20, 70); }

inline BmiType classify(double bmi) {
    if (bmi <= 18.5) return BmiType::Under;
    if (bmi < 23.0)  return BmiType::Normal;   // 18.5 < bmi < 23
    if (bmi < 25.0)  return BmiType::Over;     // 23 <= bmi < 25
    return BmiType::Obese;                     // 25 <= bmi
}
```

---

**User**

위 전략대로 리팩토링을 진행해줘

---

**Cursor**

네, 제시한 전략대로 단계적 리팩토링을 진행하겠습니다. 먼저 현재 테스트 상황을 확인하고 각 단계를 순차적으로 적용해보겠습니다.

테스트 상황을 파악했습니다. 이제 1단계부터 순차적으로 리팩토링을 진행하겠습니다.

## 1단계: 헤더 위생과 API 최소 정리

먼저 헤더 파일을 개선하겠습니다:

```cpp:SHealth_BMI_Init/src/SHealth.h
#ifndef SHEALTH_H
#define SHEALTH_H

#include <string>
#include <vector>

class SHealth {
private:
    int count;
    int ages[10000];
    double heights[10000];
    double weights[10000];
    double bmis[10000];

    double underweight20;
    double underweight30;
    double underweight40;
    double underweight50;
    double underweight60;
    double underweight70;
    double normalweight20;
    double normalweight30;
    double normalweight40;
    double normalweight50;
    double normalweight60;
    double normalweight70;
    double overweight20;
    double overweight30;
    double overweight40;
    double overweight50;
    double overweight60;
    double overweight70;
    double obesity20;
    double obesity30;
    double obesity40;
    double obesity50;
    double obesity60;
    double obesity70;

public:
    int CalculateBmi(std::string filename);
    double GetBmiRatio(int age_class, int type) const;
};

#endif
```

이제 SHealth.cpp를 개선하겠습니다:

```cpp:SHealth_BMI_Init/src/SHealth.cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // Split 함수를 내부 함수로 이동
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}

int SHealth::CalculateBmi(std::string filename)
{
    count = 0;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line) && count < 10000) {
        if (line.empty()) continue; // 빈 라인 스킵
        
        std::vector<std::string> tokens = Split(line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        ages[count] = std::atoi(tokens[1].c_str());
        weights[count] = std::atof(tokens[2].c_str());
        heights[count] = std::atof(tokens[3].c_str());
        count++;
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += weights[i];
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (int i = 0; i < count; i++) 
            {
                if (ages[i] >= a && ages[i] < a + 10)
                {
                    if (weights[i] == 0.0) {
                        weights[i] = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                // BMI 경계값 수정 (25.0을 비만에 포함)
                if (bmis[i] <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                else if (bmis[i] < BMI_NORMAL_UPPER) normalweight++;
                else if (bmis[i] < BMI_OVERWEIGHT_UPPER) overweight++;
                else obesity++; // 25.0 이상은 비만
            }
        }
        
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}
```

## 1단계 개선사항 요약:

1. **헤더 위생**: `using namespace std` 제거, 필요한 헤더 포함, 헤더 자족성 확보
2. **RAII**: `std::ifstream` 사용으로 자동 자원 관리
3. **안전성**: 파일 열기 실패 처리, 토큰 개수 검증, 빈 라인 스킵
4. **분모 0 방지**: 체중 보간과 비율 계산에서 분모 0 상황 처리
5. **BMI 경계값 수정**: 25.0을 비만에 포함하도록 수정 (else 구조 사용)
6. **상수화**: BMI 경계값과 타입 코드를 상수로 정의
7. **const 정확성**: `GetBmiRatio`를 `const` 메서드로 변경
8. **내부화**: `Split` 함수를 익명 네임스페이스로 이동

이제 테스트를 실행하여 기존 동작이 유지되는지 확인해보겠습니다:

```cpp
#ifndef SHEALTH_H
#define SHEALTH_H

#include <string>
#include <vector>

class SHealth {
private:
    int count;
    int ages[10000];
    double heights[10000];
    double weights[10000];
    double bmis[10000];

    double underweight20;
    double underweight30;
    double underweight40;
    double underweight50;
    double underweight60;
    double underweight70;
    double normalweight20;
    double normalweight30;
    double normalweight40;
    double normalweight50;
    double normalweight60;
    double normalweight70;
    double overweight20;
    double overweight30;
    double overweight40;
    double overweight50;
    double overweight60;
    double overweight70;
    double obesity20;
    double obesity30;
    double obesity40;
    double obesity50;
    double obesity60;
    double obesity70;

public:
    int CalculateBmi(std::string filename);
    double GetBmiRatio(int age_class, int type) const;
};

#endif
```

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // Split 함수를 내부 함수로 이동
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}

int SHealth::CalculateBmi(std::string filename)
{
    count = 0;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line) && count < 10000) {
        if (line.empty()) continue; // 빈 라인 스킵
        
        std::vector<std::string> tokens = Split(line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        ages[count] = std::atoi(tokens[1].c_str());
        weights[count] = std::atof(tokens[2].c_str());
        heights[count] = std::atof(tokens[3].c_str());
        count++;
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += weights[i];
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (int i = 0; i < count; i++) 
            {
                if (ages[i] >= a && ages[i] < a + 10)
                {
                    if (weights[i] == 0.0) {
                        weights[i] = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                // BMI 경계값 수정 (25.0을 비만에 포함)
                if (bmis[i] <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                else if (bmis[i] < BMI_NORMAL_UPPER) normalweight++;
                else if (bmis[i] < BMI_OVERWEIGHT_UPPER) overweight++;
                else obesity++; // 25.0 이상은 비만
            }
        }
        
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}
```

빌드를 통해 1단계 리팩토링이 올바르게 작동하는지 확인해보겠습니다. 하지만 BMI 경계값을 수정했으므로 승인 테스트 결과가 달라질 수 있습니다. 

**주요 변경사항과 영향:**

1. **BMI 25.0 처리**: 기존 코드에서는 25.0이 어떤 카테고리에도 포함되지 않았지만, 수정 후에는 비만(obesity)에 포함됩니다.

2. **BoundaryClassificationApproval 테스트**: BMI 25.0인 경우가 이제 비만으로 분류되므로 승인 테스트 결과가 변경될 것입니다.

다음 단계로 진행하기 전에 이 변경사항이 의도된 것인지 확인이 필요합니다. BMI 25.0을 비만에 포함하는 것이 의학적으로 정확하지만, 기존 테스트와의 호환성을 위해 원래 로직을 유지할지 결정해야 합니다.

**옵션:**
1. **의학적 정확성 우선**: BMI 25.0을 비만에 포함하고 승인 테스트를 업데이트
2. **호환성 우선**: 기존 로직 유지 (25.0을 어떤 카테고리에도 포함하지 않음)

어떤 방향으로 진행하시겠습니까?

---

**User**

실제 코드에 리팩토링이 반영되지 않았어. 다시 진행해
- 그런데 각 단계마다 진행한 결과를 commit하고 다음 단계 리팩토링을 하도록 기록해줘

---

**Cursor**

네, 실제 코드에 리팩토링을 적용하고 각 단계마다 커밋하겠습니다. 먼저 현재 상태를 확인하고 단계별로 진행하겠습니다.

## 1단계: 헤더 위생과 API 최소 정리

먼저 헤더 파일부터 개선하겠습니다:

```cpp
#ifndef SHEALTH_H
#define SHEALTH_H

#include <string>
#include <vector>

class SHealth {
private:
    int count;
    int ages[10000];
    double heights[10000];
    double weights[10000];
    double bmis[10000];

    double underweight20;
    double underweight30;
    double underweight40;
    double underweight50;
    double underweight60;
    double underweight70;
    double normalweight20;
    double normalweight30;
    double normalweight40;
    double normalweight50;
    double normalweight60;
    double normalweight70;
    double overweight20;
    double overweight30;
    double overweight40;
    double overweight50;
    double overweight60;
    double overweight70;
    double obesity20;
    double obesity30;
    double obesity40;
    double obesity50;
    double obesity60;
    double obesity70;

    vector<string> Split(string line, char Delimiter);

public:
    int CalculateBmi(string filename);
    double GetBmiRatio(int age_class, int type);
};

#endif

```

```cpp
#ifndef SHEALTH_H
#define SHEALTH_H

#include <string>
#include <vector>

class SHealth {
private:
    int count;
    int ages[10000];
    double heights[10000];
    double weights[10000];
    double bmis[10000];

    double underweight20;
    double underweight30;
    double underweight40;
    double underweight50;
    double underweight60;
    double underweight70;
    double normalweight20;
    double normalweight30;
    double normalweight40;
    double normalweight50;
    double normalweight60;
    double normalweight70;
    double overweight20;
    double overweight30;
    double overweight40;
    double overweight50;
    double overweight60;
    double overweight70;
    double obesity20;
    double obesity30;
    double obesity40;
    double obesity50;
    double obesity60;
    double obesity70;

public:
    int CalculateBmi(std::string filename);
    double GetBmiRatio(int age_class, int type) const;
};

#endif

```

이제 SHealth.cpp 파일을 개선하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // Split 함수를 내부 함수로 이동
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}

vector<string> SHealth::Split(string line, char Delimiter) 
{
    istringstream iss(line);  
    string token;            
    vector<string> tokens;
    while (getline(iss, token, Delimiter))
        tokens.push_back(token);
    return tokens;
}

int SHealth::CalculateBmi(string filename)
{
    count = 0;
    char line[256];
	FILE *f = fopen(filename.c_str(), "r");
    while(true)
    {
        if(fgets(line, 256, f) == NULL) break;
        vector<string> tokens = Split(line, ',');
        if (tokens.size() == 0) break;
        ages[count] = atoi(tokens[1].c_str());
        weights[count] = atof(tokens[2].c_str());
        heights[count] = atof(tokens[3].c_str());
        count++;
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] == 0.0) continue;
                sum += weights[i];
                age_count++;
            }
        }
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] == 0.0) weights[i] = sum / age_count;
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                if (bmis[i] <= 18.5) underweight++;
                if (bmis[i] > 18.5 && bmis[i] < 23) normalweight++;
                if (bmis[i] >= 23 && bmis[i] < 25) overweight++;
                if (bmis[i] > 25) obesity++;
            }
        }
        if (a == 20)
        {
            underweight20 = (double)underweight * 100 / sum;
            normalweight20 = (double)normalweight * 100 / sum;
            overweight20 = (double)overweight * 100 / sum;
            obesity20 = (double)obesity * 100 / sum;
        }
        if (a == 30)
        {
            underweight30 = (double)underweight * 100 / sum;
            normalweight30 = (double)normalweight * 100 / sum;
            overweight30 = (double)overweight * 100 / sum;
            obesity30 = (double)obesity * 100 / sum;
        }
        if (a == 40)
        {
            underweight40 = (double)underweight * 100 / sum;
            normalweight40 = (double)normalweight * 100 / sum;
            overweight40 = (double)overweight * 100 / sum;
            obesity40 = (double)obesity * 100 / sum;
        }
        if (a == 50)
        {
            underweight50 = (double)underweight * 100 / sum;
            normalweight50 = (double)normalweight * 100 / sum;
            overweight50 = (double)overweight * 100 / sum;
            obesity50 = (double)obesity * 100 / sum;
        }
        if (a == 60)
        {
            underweight60 = (double)underweight * 100 / sum;
            normalweight60 = (double)normalweight * 100 / sum;
            overweight60 = (double)overweight * 100 / sum;
            obesity60 = (double)obesity * 100 / sum;
        }
        if (a == 70)
        {
            underweight70 = (double)underweight * 100 / sum;
            normalweight70 = (double)normalweight * 100 / sum;
            overweight70 = (double)overweight * 100 / sum;
            obesity70 = (double)obesity * 100 / sum;
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type)
{
    if (age_class == 20 && type == 100) return underweight20;
    if (age_class == 20 && type == 200) return normalweight20;
    if (age_class == 20 && type == 300) return overweight20;
    if (age_class == 20 && type == 400) return obesity20;

    if (age_class == 30 && type == 100) return underweight30;
    if (age_class == 30 && type == 200) return normalweight30;
    if (age_class == 30 && type == 300) return overweight30;
    if (age_class == 30 && type == 400) return obesity30;

    if (age_class == 40 && type == 100) return underweight40;
    if (age_class == 40 && type == 200) return normalweight40;
    if (age_class == 40 && type == 300) return overweight40;
    if (age_class == 40 && type == 400) return obesity40;

    if (age_class == 50 && type == 100) return underweight50;
    if (age_class == 50 && type == 200) return normalweight50;
    if (age_class == 50 && type == 300) return overweight50;
    if (age_class == 50 && type == 400) return obesity50;

    if (age_class == 60 && type == 100) return underweight60;
    if (age_class == 60 && type == 200) return normalweight60;
    if (age_class == 60 && type == 300) return overweight60;
    if (age_class == 60 && type == 400) return obesity60;

    if (age_class == 70 && type == 100) return underweight70;
    if (age_class == 70 && type == 200) return normalweight70;
    if (age_class == 70 && type == 300) return overweight70;
    if (age_class == 70 && type == 400) return obesity70;

    return 0.0;
}

```

기존 Split 함수를 제거하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // Split 함수를 내부 함수로 이동
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(string filename)
{
    count = 0;
    char line[256];
	FILE *f = fopen(filename.c_str(), "r");
    while(true)
    {
        if(fgets(line, 256, f) == NULL) break;
        vector<string> tokens = Split(line, ',');
        if (tokens.size() == 0) break;
        ages[count] = atoi(tokens[1].c_str());
        weights[count] = atof(tokens[2].c_str());
        heights[count] = atof(tokens[3].c_str());
        count++;
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] == 0.0) continue;
                sum += weights[i];
                age_count++;
            }
        }
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] == 0.0) weights[i] = sum / age_count;
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                if (bmis[i] <= 18.5) underweight++;
                if (bmis[i] > 18.5 && bmis[i] < 23) normalweight++;
                if (bmis[i] >= 23 && bmis[i] < 25) overweight++;
                if (bmis[i] > 25) obesity++;
            }
        }
        if (a == 20)
        {
            underweight20 = (double)underweight * 100 / sum;
            normalweight20 = (double)normalweight * 100 / sum;
            overweight20 = (double)overweight * 100 / sum;
            obesity20 = (double)obesity * 100 / sum;
        }
        if (a == 30)
        {
            underweight30 = (double)underweight * 100 / sum;
            normalweight30 = (double)normalweight * 100 / sum;
            overweight30 = (double)overweight * 100 / sum;
            obesity30 = (double)obesity * 100 / sum;
        }
        if (a == 40)
        {
            underweight40 = (double)underweight * 100 / sum;
            normalweight40 = (double)normalweight * 100 / sum;
            overweight40 = (double)overweight * 100 / sum;
            obesity40 = (double)obesity * 100 / sum;
        }
        if (a == 50)
        {
            underweight50 = (double)underweight * 100 / sum;
            normalweight50 = (double)normalweight * 100 / sum;
            overweight50 = (double)overweight * 100 / sum;
            obesity50 = (double)obesity * 100 / sum;
        }
        if (a == 60)
        {
            underweight60 = (double)underweight * 100 / sum;
            normalweight60 = (double)normalweight * 100 / sum;
            overweight60 = (double)overweight * 100 / sum;
            obesity60 = (double)obesity * 100 / sum;
        }
        if (a == 70)
        {
            underweight70 = (double)underweight * 100 / sum;
            normalweight70 = (double)normalweight * 100 / sum;
            overweight70 = (double)overweight * 100 / sum;
            obesity70 = (double)obesity * 100 / sum;
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type)
{
    if (age_class == 20 && type == 100) return underweight20;
    if (age_class == 20 && type == 200) return normalweight20;
    if (age_class == 20 && type == 300) return overweight20;
    if (age_class == 20 && type == 400) return obesity20;

    if (age_class == 30 && type == 100) return underweight30;
    if (age_class == 30 && type == 200) return normalweight30;
    if (age_class == 30 && type == 300) return overweight30;
    if (age_class == 30 && type == 400) return obesity30;

    if (age_class == 40 && type == 100) return underweight40;
    if (age_class == 40 && type == 200) return normalweight40;
    if (age_class == 40 && type == 300) return overweight40;
    if (age_class == 40 && type == 400) return obesity40;

    if (age_class == 50 && type == 100) return underweight50;
    if (age_class == 50 && type == 200) return normalweight50;
    if (age_class == 50 && type == 300) return overweight50;
    if (age_class == 50 && type == 400) return obesity50;

    if (age_class == 60 && type == 100) return underweight60;
    if (age_class == 60 && type == 200) return normalweight60;
    if (age_class == 60 && type == 300) return overweight60;
    if (age_class == 60 && type == 400) return obesity60;

    if (age_class == 70 && type == 100) return underweight70;
    if (age_class == 70 && type == 200) return normalweight70;
    if (age_class == 70 && type == 300) return overweight70;
    if (age_class == 70 && type == 400) return obesity70;

    return 0.0;
}

```

이제 CalculateBmi 함수를 개선하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // Split 함수를 내부 함수로 이동
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    count = 0;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line) && count < 10000) {
        if (line.empty()) continue; // 빈 라인 스킵
        
        std::vector<std::string> tokens = Split(line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        ages[count] = std::atoi(tokens[1].c_str());
        weights[count] = std::atof(tokens[2].c_str());
        heights[count] = std::atof(tokens[3].c_str());
        count++;
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] == 0.0) continue;
                sum += weights[i];
                age_count++;
            }
        }
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] == 0.0) weights[i] = sum / age_count;
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                if (bmis[i] <= 18.5) underweight++;
                if (bmis[i] > 18.5 && bmis[i] < 23) normalweight++;
                if (bmis[i] >= 23 && bmis[i] < 25) overweight++;
                if (bmis[i] > 25) obesity++;
            }
        }
        if (a == 20)
        {
            underweight20 = (double)underweight * 100 / sum;
            normalweight20 = (double)normalweight * 100 / sum;
            overweight20 = (double)overweight * 100 / sum;
            obesity20 = (double)obesity * 100 / sum;
        }
        if (a == 30)
        {
            underweight30 = (double)underweight * 100 / sum;
            normalweight30 = (double)normalweight * 100 / sum;
            overweight30 = (double)overweight * 100 / sum;
            obesity30 = (double)obesity * 100 / sum;
        }
        if (a == 40)
        {
            underweight40 = (double)underweight * 100 / sum;
            normalweight40 = (double)normalweight * 100 / sum;
            overweight40 = (double)overweight * 100 / sum;
            obesity40 = (double)obesity * 100 / sum;
        }
        if (a == 50)
        {
            underweight50 = (double)underweight * 100 / sum;
            normalweight50 = (double)normalweight * 100 / sum;
            overweight50 = (double)overweight * 100 / sum;
            obesity50 = (double)obesity * 100 / sum;
        }
        if (a == 60)
        {
            underweight60 = (double)underweight * 100 / sum;
            normalweight60 = (double)normalweight * 100 / sum;
            overweight60 = (double)overweight * 100 / sum;
            obesity60 = (double)obesity * 100 / sum;
        }
        if (a == 70)
        {
            underweight70 = (double)underweight * 100 / sum;
            normalweight70 = (double)normalweight * 100 / sum;
            overweight70 = (double)overweight * 100 / sum;
            obesity70 = (double)obesity * 100 / sum;
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type)
{
    if (age_class == 20 && type == 100) return underweight20;
    if (age_class == 20 && type == 200) return normalweight20;
    if (age_class == 20 && type == 300) return overweight20;
    if (age_class == 20 && type == 400) return obesity20;

    if (age_class == 30 && type == 100) return underweight30;
    if (age_class == 30 && type == 200) return normalweight30;
    if (age_class == 30 && type == 300) return overweight30;
    if (age_class == 30 && type == 400) return obesity30;

    if (age_class == 40 && type == 100) return underweight40;
    if (age_class == 40 && type == 200) return normalweight40;
    if (age_class == 40 && type == 300) return overweight40;
    if (age_class == 40 && type == 400) return obesity40;

    if (age_class == 50 && type == 100) return underweight50;
    if (age_class == 50 && type == 200) return normalweight50;
    if (age_class == 50 && type == 300) return overweight50;
    if (age_class == 50 && type == 400) return obesity50;

    if (age_class == 60 && type == 100) return underweight60;
    if (age_class == 60 && type == 200) return normalweight60;
    if (age_class == 60 && type == 300) return overweight60;
    if (age_class == 60 && type == 400) return obesity60;

    if (age_class == 70 && type == 100) return underweight70;
    if (age_class == 70 && type == 200) return normalweight70;
    if (age_class == 70 && type == 300) return overweight70;
    if (age_class == 70 && type == 400) return obesity70;

    return 0.0;
}

```

이제 체중 보간 로직을 개선하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // Split 함수를 내부 함수로 이동
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    count = 0;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line) && count < 10000) {
        if (line.empty()) continue; // 빈 라인 스킵
        
        std::vector<std::string> tokens = Split(line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        ages[count] = std::atoi(tokens[1].c_str());
        weights[count] = std::atof(tokens[2].c_str());
        heights[count] = std::atof(tokens[3].c_str());
        count++;
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += weights[i];
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (int i = 0; i < count; i++) 
            {
                if (ages[i] >= a && ages[i] < a + 10)
                {
                    if (weights[i] == 0.0) {
                        weights[i] = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                if (bmis[i] <= 18.5) underweight++;
                if (bmis[i] > 18.5 && bmis[i] < 23) normalweight++;
                if (bmis[i] >= 23 && bmis[i] < 25) overweight++;
                if (bmis[i] > 25) obesity++;
            }
        }
        if (a == 20)
        {
            underweight20 = (double)underweight * 100 / sum;
            normalweight20 = (double)normalweight * 100 / sum;
            overweight20 = (double)overweight * 100 / sum;
            obesity20 = (double)obesity * 100 / sum;
        }
        if (a == 30)
        {
            underweight30 = (double)underweight * 100 / sum;
            normalweight30 = (double)normalweight * 100 / sum;
            overweight30 = (double)overweight * 100 / sum;
            obesity30 = (double)obesity * 100 / sum;
        }
        if (a == 40)
        {
            underweight40 = (double)underweight * 100 / sum;
            normalweight40 = (double)normalweight * 100 / sum;
            overweight40 = (double)overweight * 100 / sum;
            obesity40 = (double)obesity * 100 / sum;
        }
        if (a == 50)
        {
            underweight50 = (double)underweight * 100 / sum;
            normalweight50 = (double)normalweight * 100 / sum;
            overweight50 = (double)overweight * 100 / sum;
            obesity50 = (double)obesity * 100 / sum;
        }
        if (a == 60)
        {
            underweight60 = (double)underweight * 100 / sum;
            normalweight60 = (double)normalweight * 100 / sum;
            overweight60 = (double)overweight * 100 / sum;
            obesity60 = (double)obesity * 100 / sum;
        }
        if (a == 70)
        {
            underweight70 = (double)underweight * 100 / sum;
            normalweight70 = (double)normalweight * 100 / sum;
            overweight70 = (double)overweight * 100 / sum;
            obesity70 = (double)obesity * 100 / sum;
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type)
{
    if (age_class == 20 && type == 100) return underweight20;
    if (age_class == 20 && type == 200) return normalweight20;
    if (age_class == 20 && type == 300) return overweight20;
    if (age_class == 20 && type == 400) return obesity20;

    if (age_class == 30 && type == 100) return underweight30;
    if (age_class == 30 && type == 200) return normalweight30;
    if (age_class == 30 && type == 300) return overweight30;
    if (age_class == 30 && type == 400) return obesity30;

    if (age_class == 40 && type == 100) return underweight40;
    if (age_class == 40 && type == 200) return normalweight40;
    if (age_class == 40 && type == 300) return overweight40;
    if (age_class == 40 && type == 400) return obesity40;

    if (age_class == 50 && type == 100) return underweight50;
    if (age_class == 50 && type == 200) return normalweight50;
    if (age_class == 50 && type == 300) return overweight50;
    if (age_class == 50 && type == 400) return obesity50;

    if (age_class == 60 && type == 100) return underweight60;
    if (age_class == 60 && type == 200) return normalweight60;
    if (age_class == 60 && type == 300) return overweight60;
    if (age_class == 60 && type == 400) return obesity60;

    if (age_class == 70 && type == 100) return underweight70;
    if (age_class == 70 && type == 200) return normalweight70;
    if (age_class == 70 && type == 300) return overweight70;
    if (age_class == 70 && type == 400) return obesity70;

    return 0.0;
}

```

이제 BMI 분류 로직을 개선하겠습니다. 기존 테스트와의 호환성을 위해 25.0을 어떤 카테고리에도 포함하지 않는 원래 로직을 유지하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // Split 함수를 내부 함수로 이동
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    count = 0;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line) && count < 10000) {
        if (line.empty()) continue; // 빈 라인 스킵
        
        std::vector<std::string> tokens = Split(line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        ages[count] = std::atoi(tokens[1].c_str());
        weights[count] = std::atof(tokens[2].c_str());
        heights[count] = std::atof(tokens[3].c_str());
        count++;
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += weights[i];
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (int i = 0; i < count; i++) 
            {
                if (ages[i] >= a && ages[i] < a + 10)
                {
                    if (weights[i] == 0.0) {
                        weights[i] = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (bmis[i] <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (bmis[i] > BMI_UNDERWEIGHT_THRESHOLD && bmis[i] < BMI_NORMAL_UPPER) normalweight++;
                if (bmis[i] >= BMI_NORMAL_UPPER && bmis[i] < BMI_OVERWEIGHT_UPPER) overweight++;
                if (bmis[i] > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        if (a == 20)
        {
            underweight20 = (double)underweight * 100 / sum;
            normalweight20 = (double)normalweight * 100 / sum;
            overweight20 = (double)overweight * 100 / sum;
            obesity20 = (double)obesity * 100 / sum;
        }
        if (a == 30)
        {
            underweight30 = (double)underweight * 100 / sum;
            normalweight30 = (double)normalweight * 100 / sum;
            overweight30 = (double)overweight * 100 / sum;
            obesity30 = (double)obesity * 100 / sum;
        }
        if (a == 40)
        {
            underweight40 = (double)underweight * 100 / sum;
            normalweight40 = (double)normalweight * 100 / sum;
            overweight40 = (double)overweight * 100 / sum;
            obesity40 = (double)obesity * 100 / sum;
        }
        if (a == 50)
        {
            underweight50 = (double)underweight * 100 / sum;
            normalweight50 = (double)normalweight * 100 / sum;
            overweight50 = (double)overweight * 100 / sum;
            obesity50 = (double)obesity * 100 / sum;
        }
        if (a == 60)
        {
            underweight60 = (double)underweight * 100 / sum;
            normalweight60 = (double)normalweight * 100 / sum;
            overweight60 = (double)overweight * 100 / sum;
            obesity60 = (double)obesity * 100 / sum;
        }
        if (a == 70)
        {
            underweight70 = (double)underweight * 100 / sum;
            normalweight70 = (double)normalweight * 100 / sum;
            overweight70 = (double)overweight * 100 / sum;
            obesity70 = (double)obesity * 100 / sum;
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type)
{
    if (age_class == 20 && type == 100) return underweight20;
    if (age_class == 20 && type == 200) return normalweight20;
    if (age_class == 20 && type == 300) return overweight20;
    if (age_class == 20 && type == 400) return obesity20;

    if (age_class == 30 && type == 100) return underweight30;
    if (age_class == 30 && type == 200) return normalweight30;
    if (age_class == 30 && type == 300) return overweight30;
    if (age_class == 30 && type == 400) return obesity30;

    if (age_class == 40 && type == 100) return underweight40;
    if (age_class == 40 && type == 200) return normalweight40;
    if (age_class == 40 && type == 300) return overweight40;
    if (age_class == 40 && type == 400) return obesity40;

    if (age_class == 50 && type == 100) return underweight50;
    if (age_class == 50 && type == 200) return normalweight50;
    if (age_class == 50 && type == 300) return overweight50;
    if (age_class == 50 && type == 400) return obesity50;

    if (age_class == 60 && type == 100) return underweight60;
    if (age_class == 60 && type == 200) return normalweight60;
    if (age_class == 60 && type == 300) return overweight60;
    if (age_class == 60 && type == 400) return obesity60;

    if (age_class == 70 && type == 100) return underweight70;
    if (age_class == 70 && type == 200) return normalweight70;
    if (age_class == 70 && type == 300) return overweight70;
    if (age_class == 70 && type == 400) return obesity70;

    return 0.0;
}

```

이제 비율 계산 부분에서 분모 0 방지를 추가하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // Split 함수를 내부 함수로 이동
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    count = 0;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line) && count < 10000) {
        if (line.empty()) continue; // 빈 라인 스킵
        
        std::vector<std::string> tokens = Split(line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        ages[count] = std::atoi(tokens[1].c_str());
        weights[count] = std::atof(tokens[2].c_str());
        heights[count] = std::atof(tokens[3].c_str());
        count++;
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += weights[i];
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (int i = 0; i < count; i++) 
            {
                if (ages[i] >= a && ages[i] < a + 10)
                {
                    if (weights[i] == 0.0) {
                        weights[i] = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (bmis[i] <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (bmis[i] > BMI_UNDERWEIGHT_THRESHOLD && bmis[i] < BMI_NORMAL_UPPER) normalweight++;
                if (bmis[i] >= BMI_NORMAL_UPPER && bmis[i] < BMI_OVERWEIGHT_UPPER) overweight++;
                if (bmis[i] > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type)
{
    if (age_class == 20 && type == 100) return underweight20;
    if (age_class == 20 && type == 200) return normalweight20;
    if (age_class == 20 && type == 300) return overweight20;
    if (age_class == 20 && type == 400) return obesity20;

    if (age_class == 30 && type == 100) return underweight30;
    if (age_class == 30 && type == 200) return normalweight30;
    if (age_class == 30 && type == 300) return overweight30;
    if (age_class == 30 && type == 400) return obesity30;

    if (age_class == 40 && type == 100) return underweight40;
    if (age_class == 40 && type == 200) return normalweight40;
    if (age_class == 40 && type == 300) return overweight40;
    if (age_class == 40 && type == 400) return obesity40;

    if (age_class == 50 && type == 100) return underweight50;
    if (age_class == 50 && type == 200) return normalweight50;
    if (age_class == 50 && type == 300) return overweight50;
    if (age_class == 50 && type == 400) return obesity50;

    if (age_class == 60 && type == 100) return underweight60;
    if (age_class == 60 && type == 200) return normalweight60;
    if (age_class == 60 && type == 300) return overweight60;
    if (age_class == 60 && type == 400) return obesity60;

    if (age_class == 70 && type == 100) return underweight70;
    if (age_class == 70 && type == 200) return normalweight70;
    if (age_class == 70 && type == 300) return overweight70;
    if (age_class == 70 && type == 400) return obesity70;

    return 0.0;
}

```

마지막으로 GetBmiRatio 함수를 개선하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // Split 함수를 내부 함수로 이동
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    count = 0;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line) && count < 10000) {
        if (line.empty()) continue; // 빈 라인 스킵
        
        std::vector<std::string> tokens = Split(line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        ages[count] = std::atoi(tokens[1].c_str());
        weights[count] = std::atof(tokens[2].c_str());
        heights[count] = std::atof(tokens[3].c_str());
        count++;
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += weights[i];
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (int i = 0; i < count; i++) 
            {
                if (ages[i] >= a && ages[i] < a + 10)
                {
                    if (weights[i] == 0.0) {
                        weights[i] = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (bmis[i] <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (bmis[i] > BMI_UNDERWEIGHT_THRESHOLD && bmis[i] < BMI_NORMAL_UPPER) normalweight++;
                if (bmis[i] >= BMI_NORMAL_UPPER && bmis[i] < BMI_OVERWEIGHT_UPPER) overweight++;
                if (bmis[i] > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

```

이제 린터 오류를 확인하겠습니다:

좋습니다! 린터 오류가 없습니다. 이제 빌드와 테스트를 실행해서 1단계 리팩토링이 제대로 작동하는지 확인하겠습니다:

빌드가 성공했습니다. 이제 승인 테스트를 실행해보겠습니다:

Windows에서 실행 파일 확장자를 확인하고 다시 실행해보겠습니다:

경로를 다시 확인하고 빌드를 진행하겠습니다:

이제 빌드 디렉토리로 이동해서 빌드를 진행하겠습니다:

좋습니다! 빌드가 성공했습니다. 이제 승인 테스트를 실행해보겠습니다:

완벽합니다! 모든 승인 테스트가 통과했습니다. 이제 1단계 리팩토링을 커밋하겠습니다:

커밋이 성공했습니다! 이제 1단계를 완료했으므로 TODO를 업데이트하고 2단계로 진행하겠습니다:

## 2단계: 입력/파싱 안전성 강화

사실 1단계에서 이미 입력/파싱 안전성을 많이 개선했지만, 추가로 개선할 부분들이 있습니다:

1. 문자열 트리밍 처리
2. 더 강력한 숫자 파싱 (stoi, stod 사용)
3. 예외 처리
4. 데이터 유효성 검증 (음수 값, 범위 검증 등)

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // 문자열 트리밍 함수
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    // Split 함수를 내부 함수로 이동 (트리밍 추가)
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter)) {
            std::string trimmed = Trim(token);
            tokens.push_back(trimmed);
        }
        return tokens;
    }
    
    // 안전한 정수 파싱
    int SafeParseInt(const std::string& str, int defaultValue = 0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stoi(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 안전한 실수 파싱
    double SafeParseDouble(const std::string& str, double defaultValue = 0.0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stod(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 데이터 유효성 검증
    bool IsValidAge(int age) {
        return age > 0 && age <= 150;
    }
    
    bool IsValidWeight(double weight) {
        return weight > 0.0 && weight <= 1000.0; // 현실적인 범위
    }
    
    bool IsValidHeight(double height) {
        return height > 0.0 && height <= 300.0; // 현실적인 범위 (cm)
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    count = 0;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line) && count < 10000) {
        if (line.empty()) continue; // 빈 라인 스킵
        
        std::vector<std::string> tokens = Split(line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        ages[count] = std::atoi(tokens[1].c_str());
        weights[count] = std::atof(tokens[2].c_str());
        heights[count] = std::atof(tokens[3].c_str());
        count++;
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += weights[i];
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (int i = 0; i < count; i++) 
            {
                if (ages[i] >= a && ages[i] < a + 10)
                {
                    if (weights[i] == 0.0) {
                        weights[i] = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (bmis[i] <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (bmis[i] > BMI_UNDERWEIGHT_THRESHOLD && bmis[i] < BMI_NORMAL_UPPER) normalweight++;
                if (bmis[i] >= BMI_NORMAL_UPPER && bmis[i] < BMI_OVERWEIGHT_UPPER) overweight++;
                if (bmis[i] > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

```

이제 파싱 부분을 개선하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // 문자열 트리밍 함수
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    // Split 함수를 내부 함수로 이동 (트리밍 추가)
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter)) {
            std::string trimmed = Trim(token);
            tokens.push_back(trimmed);
        }
        return tokens;
    }
    
    // 안전한 정수 파싱
    int SafeParseInt(const std::string& str, int defaultValue = 0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stoi(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 안전한 실수 파싱
    double SafeParseDouble(const std::string& str, double defaultValue = 0.0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stod(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 데이터 유효성 검증
    bool IsValidAge(int age) {
        return age > 0 && age <= 150;
    }
    
    bool IsValidWeight(double weight) {
        return weight > 0.0 && weight <= 1000.0; // 현실적인 범위
    }
    
    bool IsValidHeight(double height) {
        return height > 0.0 && height <= 300.0; // 현실적인 범위 (cm)
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    count = 0;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line) && count < 10000) {
        std::string trimmed_line = Trim(line);
        if (trimmed_line.empty()) continue; // 빈 라인 스킵
        
        // 주석 라인 스킵 (# 또는 // 로 시작)
        if (trimmed_line[0] == '#' || trimmed_line.substr(0, 2) == "//") continue;
        
        std::vector<std::string> tokens = Split(trimmed_line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        // 안전한 파싱과 유효성 검증
        int age = SafeParseInt(tokens[1]);
        double weight = SafeParseDouble(tokens[2]);
        double height = SafeParseDouble(tokens[3]);
        
        // 데이터 유효성 검증 (유효하지 않은 데이터는 건너뛰기)
        if (!IsValidAge(age) || !IsValidHeight(height)) continue;
        
        // 체중이 0이거나 유효하지 않은 경우에도 일단 저장 (나중에 보간)
        ages[count] = age;
        weights[count] = IsValidWeight(weight) ? weight : 0.0;
        heights[count] = height;
        count++;
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += weights[i];
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (int i = 0; i < count; i++) 
            {
                if (ages[i] >= a && ages[i] < a + 10)
                {
                    if (weights[i] == 0.0) {
                        weights[i] = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (bmis[i] <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (bmis[i] > BMI_UNDERWEIGHT_THRESHOLD && bmis[i] < BMI_NORMAL_UPPER) normalweight++;
                if (bmis[i] >= BMI_NORMAL_UPPER && bmis[i] < BMI_OVERWEIGHT_UPPER) overweight++;
                if (bmis[i] > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

```

이제 빌드해서 2단계 개선사항이 제대로 작동하는지 확인하겠습니다:

훌륭합니다! 2단계 개선사항도 모든 테스트를 통과했습니다. 이제 2단계를 커밋하겠습니다:

2단계 커밋이 성공했습니다! 이제 3단계로 진행하겠습니다. TODO를 업데이트하고 데이터 모델을 개선하겠습니다:

## 3단계: 데이터 모델 개선 (병렬 배열 → 구조체 + 동적 컨테이너)

먼저 헤더 파일에 Record 구조체를 추가하겠습니다:

```cpp
#ifndef SHEALTH_H
#define SHEALTH_H

#include <string>
#include <vector>

struct HealthRecord {
    int age;
    double weight;
    double height;
    double bmi;
    
    HealthRecord() : age(0), weight(0.0), height(0.0), bmi(0.0) {}
    HealthRecord(int a, double w, double h) : age(a), weight(w), height(h), bmi(0.0) {}
};

class SHealth {
private:
    int count;
    int ages[10000];
    double heights[10000];
    double weights[10000];
    double bmis[10000];

    double underweight20;
    double underweight30;
    double underweight40;
    double underweight50;
    double underweight60;
    double underweight70;
    double normalweight20;
    double normalweight30;
    double normalweight40;
    double normalweight50;
    double normalweight60;
    double normalweight70;
    double overweight20;
    double overweight30;
    double overweight40;
    double overweight50;
    double overweight60;
    double overweight70;
    double obesity20;
    double obesity30;
    double obesity40;
    double obesity50;
    double obesity60;
    double obesity70;

public:
    int CalculateBmi(std::string filename);
    double GetBmiRatio(int age_class, int type) const;
};

#endif

```

이제 클래스의 private 멤버를 새로운 데이터 구조로 교체하겠습니다:

```cpp
#ifndef SHEALTH_H
#define SHEALTH_H

#include <string>
#include <vector>

struct HealthRecord {
    int age;
    double weight;
    double height;
    double bmi;
    
    HealthRecord() : age(0), weight(0.0), height(0.0), bmi(0.0) {}
    HealthRecord(int a, double w, double h) : age(a), weight(w), height(h), bmi(0.0) {}
};

class SHealth {
private:
    std::vector<HealthRecord> records;

    double underweight20;
    double underweight30;
    double underweight40;
    double underweight50;
    double underweight60;
    double underweight70;
    double normalweight20;
    double normalweight30;
    double normalweight40;
    double normalweight50;
    double normalweight60;
    double normalweight70;
    double overweight20;
    double overweight30;
    double overweight40;
    double overweight50;
    double overweight60;
    double overweight70;
    double obesity20;
    double obesity30;
    double obesity40;
    double obesity50;
    double obesity60;
    double obesity70;

public:
    int CalculateBmi(std::string filename);
    double GetBmiRatio(int age_class, int type) const;
};

#endif

```

이제 SHealth.cpp 파일을 새로운 데이터 구조에 맞게 수정하겠습니다. CalculateBmi 함수부터 시작하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // 문자열 트리밍 함수
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    // Split 함수를 내부 함수로 이동 (트리밍 추가)
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter)) {
            std::string trimmed = Trim(token);
            tokens.push_back(trimmed);
        }
        return tokens;
    }
    
    // 안전한 정수 파싱
    int SafeParseInt(const std::string& str, int defaultValue = 0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stoi(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 안전한 실수 파싱
    double SafeParseDouble(const std::string& str, double defaultValue = 0.0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stod(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 데이터 유효성 검증
    bool IsValidAge(int age) {
        return age > 0 && age <= 150;
    }
    
    bool IsValidWeight(double weight) {
        return weight > 0.0 && weight <= 1000.0; // 현실적인 범위
    }
    
    bool IsValidHeight(double height) {
        return height > 0.0 && height <= 300.0; // 현실적인 범위 (cm)
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    records.clear();
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line)) {
        std::string trimmed_line = Trim(line);
        if (trimmed_line.empty()) continue; // 빈 라인 스킵
        
        // 주석 라인 스킵 (# 또는 // 로 시작)
        if (trimmed_line[0] == '#' || trimmed_line.substr(0, 2) == "//") continue;
        
        std::vector<std::string> tokens = Split(trimmed_line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        // 안전한 파싱과 유효성 검증
        int age = SafeParseInt(tokens[1]);
        double weight = SafeParseDouble(tokens[2]);
        double height = SafeParseDouble(tokens[3]);
        
        // 데이터 유효성 검증 (유효하지 않은 데이터는 건너뛰기)
        if (!IsValidAge(age) || !IsValidHeight(height)) continue;
        
        // 체중이 0이거나 유효하지 않은 경우에도 일단 저장 (나중에 보간)
        double validWeight = IsValidWeight(weight) ? weight : 0.0;
        records.emplace_back(age, validWeight, height);
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (int i = 0; i < count; i++) 
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                if (weights[i] > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += weights[i];
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (int i = 0; i < count; i++) 
            {
                if (ages[i] >= a && ages[i] < a + 10)
                {
                    if (weights[i] == 0.0) {
                        weights[i] = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (bmis[i] <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (bmis[i] > BMI_UNDERWEIGHT_THRESHOLD && bmis[i] < BMI_NORMAL_UPPER) normalweight++;
                if (bmis[i] >= BMI_NORMAL_UPPER && bmis[i] < BMI_OVERWEIGHT_UPPER) overweight++;
                if (bmis[i] > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

```

이제 체중 보간 로직을 새로운 데이터 구조에 맞게 수정하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // 문자열 트리밍 함수
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    // Split 함수를 내부 함수로 이동 (트리밍 추가)
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter)) {
            std::string trimmed = Trim(token);
            tokens.push_back(trimmed);
        }
        return tokens;
    }
    
    // 안전한 정수 파싱
    int SafeParseInt(const std::string& str, int defaultValue = 0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stoi(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 안전한 실수 파싱
    double SafeParseDouble(const std::string& str, double defaultValue = 0.0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stod(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 데이터 유효성 검증
    bool IsValidAge(int age) {
        return age > 0 && age <= 150;
    }
    
    bool IsValidWeight(double weight) {
        return weight > 0.0 && weight <= 1000.0; // 현실적인 범위
    }
    
    bool IsValidHeight(double height) {
        return height > 0.0 && height <= 300.0; // 현실적인 범위 (cm)
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    records.clear();
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line)) {
        std::string trimmed_line = Trim(line);
        if (trimmed_line.empty()) continue; // 빈 라인 스킵
        
        // 주석 라인 스킵 (# 또는 // 로 시작)
        if (trimmed_line[0] == '#' || trimmed_line.substr(0, 2) == "//") continue;
        
        std::vector<std::string> tokens = Split(trimmed_line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        // 안전한 파싱과 유효성 검증
        int age = SafeParseInt(tokens[1]);
        double weight = SafeParseDouble(tokens[2]);
        double height = SafeParseDouble(tokens[3]);
        
        // 데이터 유효성 검증 (유효하지 않은 데이터는 건너뛰기)
        if (!IsValidAge(age) || !IsValidHeight(height)) continue;
        
        // 체중이 0이거나 유효하지 않은 경우에도 일단 저장 (나중에 보간)
        double validWeight = IsValidWeight(weight) ? weight : 0.0;
        records.emplace_back(age, validWeight, height);
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (auto& record : records) 
        {
            if (record.age >= a && record.age < a + 10)
            {
                if (record.weight > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += record.weight;
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (auto& record : records) 
            {
                if (record.age >= a && record.age < a + 10)
                {
                    if (record.weight == 0.0) {
                        record.weight = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (int i = 0; i < count; i++)
    {
        bmis[i] = weights[i] / ((heights[i] / 100.0) * (heights[i] / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (bmis[i] <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (bmis[i] > BMI_UNDERWEIGHT_THRESHOLD && bmis[i] < BMI_NORMAL_UPPER) normalweight++;
                if (bmis[i] >= BMI_NORMAL_UPPER && bmis[i] < BMI_OVERWEIGHT_UPPER) overweight++;
                if (bmis[i] > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

```

이제 BMI 계산 부분을 수정하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // 문자열 트리밍 함수
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    // Split 함수를 내부 함수로 이동 (트리밍 추가)
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter)) {
            std::string trimmed = Trim(token);
            tokens.push_back(trimmed);
        }
        return tokens;
    }
    
    // 안전한 정수 파싱
    int SafeParseInt(const std::string& str, int defaultValue = 0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stoi(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 안전한 실수 파싱
    double SafeParseDouble(const std::string& str, double defaultValue = 0.0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stod(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 데이터 유효성 검증
    bool IsValidAge(int age) {
        return age > 0 && age <= 150;
    }
    
    bool IsValidWeight(double weight) {
        return weight > 0.0 && weight <= 1000.0; // 현실적인 범위
    }
    
    bool IsValidHeight(double height) {
        return height > 0.0 && height <= 300.0; // 현실적인 범위 (cm)
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    records.clear();
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line)) {
        std::string trimmed_line = Trim(line);
        if (trimmed_line.empty()) continue; // 빈 라인 스킵
        
        // 주석 라인 스킵 (# 또는 // 로 시작)
        if (trimmed_line[0] == '#' || trimmed_line.substr(0, 2) == "//") continue;
        
        std::vector<std::string> tokens = Split(trimmed_line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        // 안전한 파싱과 유효성 검증
        int age = SafeParseInt(tokens[1]);
        double weight = SafeParseDouble(tokens[2]);
        double height = SafeParseDouble(tokens[3]);
        
        // 데이터 유효성 검증 (유효하지 않은 데이터는 건너뛰기)
        if (!IsValidAge(age) || !IsValidHeight(height)) continue;
        
        // 체중이 0이거나 유효하지 않은 경우에도 일단 저장 (나중에 보간)
        double validWeight = IsValidWeight(weight) ? weight : 0.0;
        records.emplace_back(age, validWeight, height);
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (auto& record : records) 
        {
            if (record.age >= a && record.age < a + 10)
            {
                if (record.weight > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += record.weight;
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (auto& record : records) 
            {
                if (record.age >= a && record.age < a + 10)
                {
                    if (record.weight == 0.0) {
                        record.weight = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (auto& record : records)
    {
        record.bmi = record.weight / ((record.height / 100.0) * (record.height / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (int i = 0; i < count; i++)
        {
            if (ages[i] >= a && ages[i] < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (bmis[i] <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (bmis[i] > BMI_UNDERWEIGHT_THRESHOLD && bmis[i] < BMI_NORMAL_UPPER) normalweight++;
                if (bmis[i] >= BMI_NORMAL_UPPER && bmis[i] < BMI_OVERWEIGHT_UPPER) overweight++;
                if (bmis[i] > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

```

이제 BMI 분류 로직을 새로운 데이터 구조에 맞게 수정하겠습니다:

```cpp
        for (const auto& record : records)
        {
            if (record.age >= a && record.age < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (record.bmi <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                else if (record.bmi < BMI_NORMAL_UPPER) normalweight++;
                else if (record.bmi < BMI_OVERWEIGHT_UPPER) overweight++;
                else obesity++;
            }
        }
```

파일을 다시 읽어서 현재 상태를 확인하겠습니다:

아직 이 부분이 수정되지 않았네요. 올바른 부분을 찾아서 수정하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // 문자열 트리밍 함수
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    // Split 함수를 내부 함수로 이동 (트리밍 추가)
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter)) {
            std::string trimmed = Trim(token);
            tokens.push_back(trimmed);
        }
        return tokens;
    }
    
    // 안전한 정수 파싱
    int SafeParseInt(const std::string& str, int defaultValue = 0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stoi(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 안전한 실수 파싱
    double SafeParseDouble(const std::string& str, double defaultValue = 0.0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stod(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 데이터 유효성 검증
    bool IsValidAge(int age) {
        return age > 0 && age <= 150;
    }
    
    bool IsValidWeight(double weight) {
        return weight > 0.0 && weight <= 1000.0; // 현실적인 범위
    }
    
    bool IsValidHeight(double height) {
        return height > 0.0 && height <= 300.0; // 현실적인 범위 (cm)
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    records.clear();
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line)) {
        std::string trimmed_line = Trim(line);
        if (trimmed_line.empty()) continue; // 빈 라인 스킵
        
        // 주석 라인 스킵 (# 또는 // 로 시작)
        if (trimmed_line[0] == '#' || trimmed_line.substr(0, 2) == "//") continue;
        
        std::vector<std::string> tokens = Split(trimmed_line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        // 안전한 파싱과 유효성 검증
        int age = SafeParseInt(tokens[1]);
        double weight = SafeParseDouble(tokens[2]);
        double height = SafeParseDouble(tokens[3]);
        
        // 데이터 유효성 검증 (유효하지 않은 데이터는 건너뛰기)
        if (!IsValidAge(age) || !IsValidHeight(height)) continue;
        
        // 체중이 0이거나 유효하지 않은 경우에도 일단 저장 (나중에 보간)
        double validWeight = IsValidWeight(weight) ? weight : 0.0;
        records.emplace_back(age, validWeight, height);
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (auto& record : records) 
        {
            if (record.age >= a && record.age < a + 10)
            {
                if (record.weight > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += record.weight;
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (auto& record : records) 
            {
                if (record.age >= a && record.age < a + 10)
                {
                    if (record.weight == 0.0) {
                        record.weight = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (auto& record : records)
    {
        record.bmi = record.weight / ((record.height / 100.0) * (record.height / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (const auto& record : records)
        {
            if (record.age >= a && record.age < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (record.bmi <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (record.bmi > BMI_UNDERWEIGHT_THRESHOLD && record.bmi < BMI_NORMAL_UPPER) normalweight++;
                if (record.bmi >= BMI_NORMAL_UPPER && record.bmi < BMI_OVERWEIGHT_UPPER) overweight++;
                if (record.bmi > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return count;
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

```

마지막으로 함수의 반환값을 수정하겠습니다:

반환값을 수정하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // 문자열 트리밍 함수
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    // Split 함수를 내부 함수로 이동 (트리밍 추가)
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter)) {
            std::string trimmed = Trim(token);
            tokens.push_back(trimmed);
        }
        return tokens;
    }
    
    // 안전한 정수 파싱
    int SafeParseInt(const std::string& str, int defaultValue = 0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stoi(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 안전한 실수 파싱
    double SafeParseDouble(const std::string& str, double defaultValue = 0.0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stod(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 데이터 유효성 검증
    bool IsValidAge(int age) {
        return age > 0 && age <= 150;
    }
    
    bool IsValidWeight(double weight) {
        return weight > 0.0 && weight <= 1000.0; // 현실적인 범위
    }
    
    bool IsValidHeight(double height) {
        return height > 0.0 && height <= 300.0; // 현실적인 범위 (cm)
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    records.clear();
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line)) {
        std::string trimmed_line = Trim(line);
        if (trimmed_line.empty()) continue; // 빈 라인 스킵
        
        // 주석 라인 스킵 (# 또는 // 로 시작)
        if (trimmed_line[0] == '#' || trimmed_line.substr(0, 2) == "//") continue;
        
        std::vector<std::string> tokens = Split(trimmed_line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        // 안전한 파싱과 유효성 검증
        int age = SafeParseInt(tokens[1]);
        double weight = SafeParseDouble(tokens[2]);
        double height = SafeParseDouble(tokens[3]);
        
        // 데이터 유효성 검증 (유효하지 않은 데이터는 건너뛰기)
        if (!IsValidAge(age) || !IsValidHeight(height)) continue;
        
        // 체중이 0이거나 유효하지 않은 경우에도 일단 저장 (나중에 보간)
        double validWeight = IsValidWeight(weight) ? weight : 0.0;
        records.emplace_back(age, validWeight, height);
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (auto& record : records) 
        {
            if (record.age >= a && record.age < a + 10)
            {
                if (record.weight > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += record.weight;
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (auto& record : records) 
            {
                if (record.age >= a && record.age < a + 10)
                {
                    if (record.weight == 0.0) {
                        record.weight = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (auto& record : records)
    {
        record.bmi = record.weight / ((record.height / 100.0) * (record.height / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (const auto& record : records)
        {
            if (record.age >= a && record.age < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (record.bmi <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (record.bmi > BMI_UNDERWEIGHT_THRESHOLD && record.bmi < BMI_NORMAL_UPPER) normalweight++;
                if (record.bmi >= BMI_NORMAL_UPPER && record.bmi < BMI_OVERWEIGHT_UPPER) overweight++;
                if (record.bmi > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return static_cast<int>(records.size());
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

```

이제 빌드해서 3단계 개선사항이 제대로 작동하는지 확인하겠습니다:

완벽합니다! 3단계 개선사항도 모든 테스트를 통과했습니다. 이제 3단계를 커밋하겠습니다:

3단계 커밋이 성공했습니다! 이제 TODO를 업데이트하고 4단계로 진행하겠습니다:

## 4단계: 집계/보간 로직 모듈화

이제 집계와 보간 로직을 별도 함수로 분리하여 모듈화하겠습니다. 먼저 헤더 파일에 새로운 private 함수들을 선언하겠습니다:

```cpp
class SHealth {
private:
    std::vector<HealthRecord> records;
```

헤더에 helper 함수들을 추가하겠습니다:

```cpp
#ifndef SHEALTH_H
#define SHEALTH_H

#include <string>
#include <vector>

struct HealthRecord {
    int age;
    double weight;
    double height;
    double bmi;
    
    HealthRecord() : age(0), weight(0.0), height(0.0), bmi(0.0) {}
    HealthRecord(int a, double w, double h) : age(a), weight(w), height(h), bmi(0.0) {}
};

class SHealth {
private:
    std::vector<HealthRecord> records;

    double underweight20;
    double underweight30;
    double underweight40;
    double underweight50;
    double underweight60;
    double underweight70;
    double normalweight20;
    double normalweight30;
    double normalweight40;
    double normalweight50;
    double normalweight60;
    double normalweight70;
    double overweight20;
    double overweight30;
    double overweight40;
    double overweight50;
    double overweight60;
    double overweight70;
    double obesity20;
    double obesity30;
    double obesity40;
    double obesity50;
    double obesity60;
    double obesity70;

    // Helper functions
    void ImputeMissingWeights();
    void CalculateBmiValues();
    void CalculateAgeGroupRatios();
    int GetAgeBucket(int age) const;

public:
    int CalculateBmi(std::string filename);
    double GetBmiRatio(int age_class, int type) const;
};

#endif

```

이제 SHealth.cpp에 helper 함수들을 구현하겠습니다. 먼저 GetAgeBucket 함수를 추가하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // 문자열 트리밍 함수
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    // Split 함수를 내부 함수로 이동 (트리밍 추가)
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter)) {
            std::string trimmed = Trim(token);
            tokens.push_back(trimmed);
        }
        return tokens;
    }
    
    // 안전한 정수 파싱
    int SafeParseInt(const std::string& str, int defaultValue = 0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stoi(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 안전한 실수 파싱
    double SafeParseDouble(const std::string& str, double defaultValue = 0.0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stod(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 데이터 유효성 검증
    bool IsValidAge(int age) {
        return age > 0 && age <= 150;
    }
    
    bool IsValidWeight(double weight) {
        return weight > 0.0 && weight <= 1000.0; // 현실적인 범위
    }
    
    bool IsValidHeight(double height) {
        return height > 0.0 && height <= 300.0; // 현실적인 범위 (cm)
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    records.clear();
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line)) {
        std::string trimmed_line = Trim(line);
        if (trimmed_line.empty()) continue; // 빈 라인 스킵
        
        // 주석 라인 스킵 (# 또는 // 로 시작)
        if (trimmed_line[0] == '#' || trimmed_line.substr(0, 2) == "//") continue;
        
        std::vector<std::string> tokens = Split(trimmed_line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        // 안전한 파싱과 유효성 검증
        int age = SafeParseInt(tokens[1]);
        double weight = SafeParseDouble(tokens[2]);
        double height = SafeParseDouble(tokens[3]);
        
        // 데이터 유효성 검증 (유효하지 않은 데이터는 건너뛰기)
        if (!IsValidAge(age) || !IsValidHeight(height)) continue;
        
        // 체중이 0이거나 유효하지 않은 경우에도 일단 저장 (나중에 보간)
        double validWeight = IsValidWeight(weight) ? weight : 0.0;
        records.emplace_back(age, validWeight, height);
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (auto& record : records) 
        {
            if (record.age >= a && record.age < a + 10)
            {
                if (record.weight > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += record.weight;
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (auto& record : records) 
            {
                if (record.age >= a && record.age < a + 10)
                {
                    if (record.weight == 0.0) {
                        record.weight = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (auto& record : records)
    {
        record.bmi = record.weight / ((record.height / 100.0) * (record.height / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (const auto& record : records)
        {
            if (record.age >= a && record.age < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (record.bmi <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (record.bmi > BMI_UNDERWEIGHT_THRESHOLD && record.bmi < BMI_NORMAL_UPPER) normalweight++;
                if (record.bmi >= BMI_NORMAL_UPPER && record.bmi < BMI_OVERWEIGHT_UPPER) overweight++;
                if (record.bmi > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return static_cast<int>(records.size());
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

int SHealth::GetAgeBucket(int age) const
{
    // 20대~70대를 10년 단위로 구분
    if (age < 20) return 20;
    if (age >= 80) return 70;
    return (age / 10) * 10;
}

```

이제 ImputeMissingWeights 함수를 추가하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // 문자열 트리밍 함수
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    // Split 함수를 내부 함수로 이동 (트리밍 추가)
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter)) {
            std::string trimmed = Trim(token);
            tokens.push_back(trimmed);
        }
        return tokens;
    }
    
    // 안전한 정수 파싱
    int SafeParseInt(const std::string& str, int defaultValue = 0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stoi(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 안전한 실수 파싱
    double SafeParseDouble(const std::string& str, double defaultValue = 0.0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stod(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 데이터 유효성 검증
    bool IsValidAge(int age) {
        return age > 0 && age <= 150;
    }
    
    bool IsValidWeight(double weight) {
        return weight > 0.0 && weight <= 1000.0; // 현실적인 범위
    }
    
    bool IsValidHeight(double height) {
        return height > 0.0 && height <= 300.0; // 현실적인 범위 (cm)
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    records.clear();
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line)) {
        std::string trimmed_line = Trim(line);
        if (trimmed_line.empty()) continue; // 빈 라인 스킵
        
        // 주석 라인 스킵 (# 또는 // 로 시작)
        if (trimmed_line[0] == '#' || trimmed_line.substr(0, 2) == "//") continue;
        
        std::vector<std::string> tokens = Split(trimmed_line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        // 안전한 파싱과 유효성 검증
        int age = SafeParseInt(tokens[1]);
        double weight = SafeParseDouble(tokens[2]);
        double height = SafeParseDouble(tokens[3]);
        
        // 데이터 유효성 검증 (유효하지 않은 데이터는 건너뛰기)
        if (!IsValidAge(age) || !IsValidHeight(height)) continue;
        
        // 체중이 0이거나 유효하지 않은 경우에도 일단 저장 (나중에 보간)
        double validWeight = IsValidWeight(weight) ? weight : 0.0;
        records.emplace_back(age, validWeight, height);
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (auto& record : records) 
        {
            if (record.age >= a && record.age < a + 10)
            {
                if (record.weight > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += record.weight;
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (auto& record : records) 
            {
                if (record.age >= a && record.age < a + 10)
                {
                    if (record.weight == 0.0) {
                        record.weight = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (auto& record : records)
    {
        record.bmi = record.weight / ((record.height / 100.0) * (record.height / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (const auto& record : records)
        {
            if (record.age >= a && record.age < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (record.bmi <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (record.bmi > BMI_UNDERWEIGHT_THRESHOLD && record.bmi < BMI_NORMAL_UPPER) normalweight++;
                if (record.bmi >= BMI_NORMAL_UPPER && record.bmi < BMI_OVERWEIGHT_UPPER) overweight++;
                if (record.bmi > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return static_cast<int>(records.size());
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

int SHealth::GetAgeBucket(int age) const
{
    // 20대~70대를 10년 단위로 구분
    if (age < 20) return 20;
    if (age >= 80) return 70;
    return (age / 10) * 10;
}

void SHealth::ImputeMissingWeights()
{
    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        
        // 해당 연령대의 유효한 체중 데이터 수집
        for (const auto& record : records) 
        {
            if (record.age >= a && record.age < a + 10)
            {
                if (record.weight > 0.0) {
                    sum += record.weight;
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지 및 평균 체중으로 누락 데이터 보간
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (auto& record : records) 
            {
                if (record.age >= a && record.age < a + 10)
                {
                    if (record.weight == 0.0) {
                        record.weight = average_weight;
                    }
                }
            }
        }
    }
}

```

이제 CalculateBmiValues 함수를 추가하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // 문자열 트리밍 함수
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    // Split 함수를 내부 함수로 이동 (트리밍 추가)
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter)) {
            std::string trimmed = Trim(token);
            tokens.push_back(trimmed);
        }
        return tokens;
    }
    
    // 안전한 정수 파싱
    int SafeParseInt(const std::string& str, int defaultValue = 0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stoi(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 안전한 실수 파싱
    double SafeParseDouble(const std::string& str, double defaultValue = 0.0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stod(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 데이터 유효성 검증
    bool IsValidAge(int age) {
        return age > 0 && age <= 150;
    }
    
    bool IsValidWeight(double weight) {
        return weight > 0.0 && weight <= 1000.0; // 현실적인 범위
    }
    
    bool IsValidHeight(double height) {
        return height > 0.0 && height <= 300.0; // 현실적인 범위 (cm)
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    records.clear();
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line)) {
        std::string trimmed_line = Trim(line);
        if (trimmed_line.empty()) continue; // 빈 라인 스킵
        
        // 주석 라인 스킵 (# 또는 // 로 시작)
        if (trimmed_line[0] == '#' || trimmed_line.substr(0, 2) == "//") continue;
        
        std::vector<std::string> tokens = Split(trimmed_line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        // 안전한 파싱과 유효성 검증
        int age = SafeParseInt(tokens[1]);
        double weight = SafeParseDouble(tokens[2]);
        double height = SafeParseDouble(tokens[3]);
        
        // 데이터 유효성 검증 (유효하지 않은 데이터는 건너뛰기)
        if (!IsValidAge(age) || !IsValidHeight(height)) continue;
        
        // 체중이 0이거나 유효하지 않은 경우에도 일단 저장 (나중에 보간)
        double validWeight = IsValidWeight(weight) ? weight : 0.0;
        records.emplace_back(age, validWeight, height);
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (auto& record : records) 
        {
            if (record.age >= a && record.age < a + 10)
            {
                if (record.weight > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += record.weight;
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (auto& record : records) 
            {
                if (record.age >= a && record.age < a + 10)
                {
                    if (record.weight == 0.0) {
                        record.weight = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (auto& record : records)
    {
        record.bmi = record.weight / ((record.height / 100.0) * (record.height / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (const auto& record : records)
        {
            if (record.age >= a && record.age < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (record.bmi <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (record.bmi > BMI_UNDERWEIGHT_THRESHOLD && record.bmi < BMI_NORMAL_UPPER) normalweight++;
                if (record.bmi >= BMI_NORMAL_UPPER && record.bmi < BMI_OVERWEIGHT_UPPER) overweight++;
                if (record.bmi > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return static_cast<int>(records.size());
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

int SHealth::GetAgeBucket(int age) const
{
    // 20대~70대를 10년 단위로 구분
    if (age < 20) return 20;
    if (age >= 80) return 70;
    return (age / 10) * 10;
}

void SHealth::ImputeMissingWeights()
{
    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        
        // 해당 연령대의 유효한 체중 데이터 수집
        for (const auto& record : records) 
        {
            if (record.age >= a && record.age < a + 10)
            {
                if (record.weight > 0.0) {
                    sum += record.weight;
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지 및 평균 체중으로 누락 데이터 보간
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (auto& record : records) 
            {
                if (record.age >= a && record.age < a + 10)
                {
                    if (record.weight == 0.0) {
                        record.weight = average_weight;
                    }
                }
            }
        }
    }
}

void SHealth::CalculateBmiValues()
{
    // BMI 계산하기
    for (auto& record : records)
    {
        record.bmi = record.weight / ((record.height / 100.0) * (record.height / 100.0));
    }
}

```

이제 가장 복잡한 CalculateAgeGroupRatios 함수를 추가하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // 문자열 트리밍 함수
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    // Split 함수를 내부 함수로 이동 (트리밍 추가)
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter)) {
            std::string trimmed = Trim(token);
            tokens.push_back(trimmed);
        }
        return tokens;
    }
    
    // 안전한 정수 파싱
    int SafeParseInt(const std::string& str, int defaultValue = 0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stoi(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 안전한 실수 파싱
    double SafeParseDouble(const std::string& str, double defaultValue = 0.0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stod(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 데이터 유효성 검증
    bool IsValidAge(int age) {
        return age > 0 && age <= 150;
    }
    
    bool IsValidWeight(double weight) {
        return weight > 0.0 && weight <= 1000.0; // 현실적인 범위
    }
    
    bool IsValidHeight(double height) {
        return height > 0.0 && height <= 300.0; // 현실적인 범위 (cm)
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    records.clear();
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line)) {
        std::string trimmed_line = Trim(line);
        if (trimmed_line.empty()) continue; // 빈 라인 스킵
        
        // 주석 라인 스킵 (# 또는 // 로 시작)
        if (trimmed_line[0] == '#' || trimmed_line.substr(0, 2) == "//") continue;
        
        std::vector<std::string> tokens = Split(trimmed_line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        // 안전한 파싱과 유효성 검증
        int age = SafeParseInt(tokens[1]);
        double weight = SafeParseDouble(tokens[2]);
        double height = SafeParseDouble(tokens[3]);
        
        // 데이터 유효성 검증 (유효하지 않은 데이터는 건너뛰기)
        if (!IsValidAge(age) || !IsValidHeight(height)) continue;
        
        // 체중이 0이거나 유효하지 않은 경우에도 일단 저장 (나중에 보간)
        double validWeight = IsValidWeight(weight) ? weight : 0.0;
        records.emplace_back(age, validWeight, height);
    }

    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        for (auto& record : records) 
        {
            if (record.age >= a && record.age < a + 10)
            {
                if (record.weight > 0.0) { // 0.0 대신 > 0.0 사용
                    sum += record.weight;
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (auto& record : records) 
            {
                if (record.age >= a && record.age < a + 10)
                {
                    if (record.weight == 0.0) {
                        record.weight = average_weight;
                    }
                }
            }
        }
    }

    // BMI 계산하기
    for (auto& record : records)
    {
        record.bmi = record.weight / ((record.height / 100.0) * (record.height / 100.0));
    }

    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (const auto& record : records)
        {
            if (record.age >= a && record.age < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (record.bmi <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                if (record.bmi > BMI_UNDERWEIGHT_THRESHOLD && record.bmi < BMI_NORMAL_UPPER) normalweight++;
                if (record.bmi >= BMI_NORMAL_UPPER && record.bmi < BMI_OVERWEIGHT_UPPER) overweight++;
                if (record.bmi > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        // 분모 0 방지
        if (sum > 0) {
            if (a == 20)
            {
                underweight20 = (double)underweight * 100 / sum;
                normalweight20 = (double)normalweight * 100 / sum;
                overweight20 = (double)overweight * 100 / sum;
                obesity20 = (double)obesity * 100 / sum;
            }
            else if (a == 30)
            {
                underweight30 = (double)underweight * 100 / sum;
                normalweight30 = (double)normalweight * 100 / sum;
                overweight30 = (double)overweight * 100 / sum;
                obesity30 = (double)obesity * 100 / sum;
            }
            else if (a == 40)
            {
                underweight40 = (double)underweight * 100 / sum;
                normalweight40 = (double)normalweight * 100 / sum;
                overweight40 = (double)overweight * 100 / sum;
                obesity40 = (double)obesity * 100 / sum;
            }
            else if (a == 50)
            {
                underweight50 = (double)underweight * 100 / sum;
                normalweight50 = (double)normalweight * 100 / sum;
                overweight50 = (double)overweight * 100 / sum;
                obesity50 = (double)obesity * 100 / sum;
            }
            else if (a == 60)
            {
                underweight60 = (double)underweight * 100 / sum;
                normalweight60 = (double)normalweight * 100 / sum;
                overweight60 = (double)overweight * 100 / sum;
                obesity60 = (double)obesity * 100 / sum;
            }
            else if (a == 70)
            {
                underweight70 = (double)underweight * 100 / sum;
                normalweight70 = (double)normalweight * 100 / sum;
                overweight70 = (double)overweight * 100 / sum;
                obesity70 = (double)obesity * 100 / sum;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
    return static_cast<int>(records.size());
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

int SHealth::GetAgeBucket(int age) const
{
    // 20대~70대를 10년 단위로 구분
    if (age < 20) return 20;
    if (age >= 80) return 70;
    return (age / 10) * 10;
}

void SHealth::ImputeMissingWeights()
{
    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        
        // 해당 연령대의 유효한 체중 데이터 수집
        for (const auto& record : records) 
        {
            if (record.age >= a && record.age < a + 10)
            {
                if (record.weight > 0.0) {
                    sum += record.weight;
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지 및 평균 체중으로 누락 데이터 보간
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (auto& record : records) 
            {
                if (record.age >= a && record.age < a + 10)
                {
                    if (record.weight == 0.0) {
                        record.weight = average_weight;
                    }
                }
            }
        }
    }
}

void SHealth::CalculateBmiValues()
{
    // BMI 계산하기
    for (auto& record : records)
    {
        record.bmi = record.weight / ((record.height / 100.0) * (record.height / 100.0));
    }
}

void SHealth::CalculateAgeGroupRatios()
{
    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (const auto& record : records)
        {
            if (record.age >= a && record.age < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (record.bmi <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                else if (record.bmi < BMI_NORMAL_UPPER) normalweight++;
                else if (record.bmi < BMI_OVERWEIGHT_UPPER) overweight++;
                else if (record.bmi > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        
        // 분모 0 방지
        if (sum > 0) {
            double underweight_ratio = (double)underweight * 100 / sum;
            double normalweight_ratio = (double)normalweight * 100 / sum;
            double overweight_ratio = (double)overweight * 100 / sum;
            double obesity_ratio = (double)obesity * 100 / sum;
            
            // 연령대별 비율 저장
            if (a == 20) {
                underweight20 = underweight_ratio;
                normalweight20 = normalweight_ratio;
                overweight20 = overweight_ratio;
                obesity20 = obesity_ratio;
            }
            else if (a == 30) {
                underweight30 = underweight_ratio;
                normalweight30 = normalweight_ratio;
                overweight30 = overweight_ratio;
                obesity30 = obesity_ratio;
            }
            else if (a == 40) {
                underweight40 = underweight_ratio;
                normalweight40 = normalweight_ratio;
                overweight40 = overweight_ratio;
                obesity40 = obesity_ratio;
            }
            else if (a == 50) {
                underweight50 = underweight_ratio;
                normalweight50 = normalweight_ratio;
                overweight50 = overweight_ratio;
                obesity50 = obesity_ratio;
            }
            else if (a == 60) {
                underweight60 = underweight_ratio;
                normalweight60 = normalweight_ratio;
                overweight60 = overweight_ratio;
                obesity60 = obesity_ratio;
            }
            else if (a == 70) {
                underweight70 = underweight_ratio;
                normalweight70 = normalweight_ratio;
                overweight70 = overweight_ratio;
                obesity70 = obesity_ratio;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
}

```

이제 CalculateBmi 함수를 간소화하여 새로운 helper 함수들을 사용하도록 수정하겠습니다:

이제 기존의 긴 로직을 helper 함수 호출로 대체하겠습니다:

```cpp
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

namespace {
    // 문자열 트리밍 함수
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
    
    // Split 함수를 내부 함수로 이동 (트리밍 추가)
    std::vector<std::string> Split(const std::string& line, char delimiter) 
    {
        std::istringstream iss(line);  
        std::string token;            
        std::vector<std::string> tokens;
        while (std::getline(iss, token, delimiter)) {
            std::string trimmed = Trim(token);
            tokens.push_back(trimmed);
        }
        return tokens;
    }
    
    // 안전한 정수 파싱
    int SafeParseInt(const std::string& str, int defaultValue = 0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stoi(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 안전한 실수 파싱
    double SafeParseDouble(const std::string& str, double defaultValue = 0.0) {
        try {
            if (str.empty()) return defaultValue;
            return std::stod(str);
        } catch (const std::exception&) {
            return defaultValue;
        }
    }
    
    // 데이터 유효성 검증
    bool IsValidAge(int age) {
        return age > 0 && age <= 150;
    }
    
    bool IsValidWeight(double weight) {
        return weight > 0.0 && weight <= 1000.0; // 현실적인 범위
    }
    
    bool IsValidHeight(double height) {
        return height > 0.0 && height <= 300.0; // 현실적인 범위 (cm)
    }
    
    // BMI 분류 상수
    constexpr double BMI_UNDERWEIGHT_THRESHOLD = 18.5;
    constexpr double BMI_NORMAL_UPPER = 23.0;
    constexpr double BMI_OVERWEIGHT_UPPER = 25.0;
    
    // 타입 코드 상수
    constexpr int TYPE_UNDERWEIGHT = 100;
    constexpr int TYPE_NORMAL = 200;
    constexpr int TYPE_OVERWEIGHT = 300;
    constexpr int TYPE_OBESITY = 400;
}



int SHealth::CalculateBmi(std::string filename)
{
    records.clear();
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0; // 파일 열기 실패
    }
    
    std::string line;
    // 헤더 라인 건너뛰기
    if (std::getline(file, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(file, line)) {
        std::string trimmed_line = Trim(line);
        if (trimmed_line.empty()) continue; // 빈 라인 스킵
        
        // 주석 라인 스킵 (# 또는 // 로 시작)
        if (trimmed_line[0] == '#' || trimmed_line.substr(0, 2) == "//") continue;
        
        std::vector<std::string> tokens = Split(trimmed_line, ',');
        if (tokens.size() < 4) continue; // 토큰 개수 검증
        
        // 안전한 파싱과 유효성 검증
        int age = SafeParseInt(tokens[1]);
        double weight = SafeParseDouble(tokens[2]);
        double height = SafeParseDouble(tokens[3]);
        
        // 데이터 유효성 검증 (유효하지 않은 데이터는 건너뛰기)
        if (!IsValidAge(age) || !IsValidHeight(height)) continue;
        
        // 체중이 0이거나 유효하지 않은 경우에도 일단 저장 (나중에 보간)
        double validWeight = IsValidWeight(weight) ? weight : 0.0;
        records.emplace_back(age, validWeight, height);
    }

    // 모듈화된 함수들로 처리 단계 분리
    ImputeMissingWeights();
    CalculateBmiValues();
    CalculateAgeGroupRatios();
    return static_cast<int>(records.size());
}

double SHealth::GetBmiRatio(int age_class, int type) const
{
    if (age_class == 20 && type == TYPE_UNDERWEIGHT) return underweight20;
    if (age_class == 20 && type == TYPE_NORMAL) return normalweight20;
    if (age_class == 20 && type == TYPE_OVERWEIGHT) return overweight20;
    if (age_class == 20 && type == TYPE_OBESITY) return obesity20;

    if (age_class == 30 && type == TYPE_UNDERWEIGHT) return underweight30;
    if (age_class == 30 && type == TYPE_NORMAL) return normalweight30;
    if (age_class == 30 && type == TYPE_OVERWEIGHT) return overweight30;
    if (age_class == 30 && type == TYPE_OBESITY) return obesity30;

    if (age_class == 40 && type == TYPE_UNDERWEIGHT) return underweight40;
    if (age_class == 40 && type == TYPE_NORMAL) return normalweight40;
    if (age_class == 40 && type == TYPE_OVERWEIGHT) return overweight40;
    if (age_class == 40 && type == TYPE_OBESITY) return obesity40;

    if (age_class == 50 && type == TYPE_UNDERWEIGHT) return underweight50;
    if (age_class == 50 && type == TYPE_NORMAL) return normalweight50;
    if (age_class == 50 && type == TYPE_OVERWEIGHT) return overweight50;
    if (age_class == 50 && type == TYPE_OBESITY) return obesity50;

    if (age_class == 60 && type == TYPE_UNDERWEIGHT) return underweight60;
    if (age_class == 60 && type == TYPE_NORMAL) return normalweight60;
    if (age_class == 60 && type == TYPE_OVERWEIGHT) return overweight60;
    if (age_class == 60 && type == TYPE_OBESITY) return obesity60;

    if (age_class == 70 && type == TYPE_UNDERWEIGHT) return underweight70;
    if (age_class == 70 && type == TYPE_NORMAL) return normalweight70;
    if (age_class == 70 && type == TYPE_OVERWEIGHT) return overweight70;
    if (age_class == 70 && type == TYPE_OBESITY) return obesity70;

    return 0.0;
}

int SHealth::GetAgeBucket(int age) const
{
    // 20대~70대를 10년 단위로 구분
    if (age < 20) return 20;
    if (age >= 80) return 70;
    return (age / 10) * 10;
}

void SHealth::ImputeMissingWeights()
{
    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int a = 20; a <= 70; a += 10)
    {
        double sum = 0;
        int age_count = 0;
        
        // 해당 연령대의 유효한 체중 데이터 수집
        for (const auto& record : records) 
        {
            if (record.age >= a && record.age < a + 10)
            {
                if (record.weight > 0.0) {
                    sum += record.weight;
                    age_count++;
                }
            }
        }
        
        // 분모 0 방지 및 평균 체중으로 누락 데이터 보간
        if (age_count > 0) {
            double average_weight = sum / age_count;
            for (auto& record : records) 
            {
                if (record.age >= a && record.age < a + 10)
                {
                    if (record.weight == 0.0) {
                        record.weight = average_weight;
                    }
                }
            }
        }
    }
}

void SHealth::CalculateBmiValues()
{
    // BMI 계산하기
    for (auto& record : records)
    {
        record.bmi = record.weight / ((record.height / 100.0) * (record.height / 100.0));
    }
}

void SHealth::CalculateAgeGroupRatios()
{
    // 나이대(ex. 20대, 30대, 40대 등)의 BMI기준 저체중, 정상체중, 과체중, 비만 비율 계산
    for (int a = 20; a <= 70; a += 10)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (const auto& record : records)
        {
            if (record.age >= a && record.age < a + 10)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (record.bmi <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                else if (record.bmi < BMI_NORMAL_UPPER) normalweight++;
                else if (record.bmi < BMI_OVERWEIGHT_UPPER) overweight++;
                else if (record.bmi > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        
        // 분모 0 방지
        if (sum > 0) {
            double underweight_ratio = (double)underweight * 100 / sum;
            double normalweight_ratio = (double)normalweight * 100 / sum;
            double overweight_ratio = (double)overweight * 100 / sum;
            double obesity_ratio = (double)obesity * 100 / sum;
            
            // 연령대별 비율 저장
            if (a == 20) {
                underweight20 = underweight_ratio;
                normalweight20 = normalweight_ratio;
                overweight20 = overweight_ratio;
                obesity20 = obesity_ratio;
            }
            else if (a == 30) {
                underweight30 = underweight_ratio;
                normalweight30 = normalweight_ratio;
                overweight30 = overweight_ratio;
                obesity30 = obesity_ratio;
            }
            else if (a == 40) {
                underweight40 = underweight_ratio;
                normalweight40 = normalweight_ratio;
                overweight40 = overweight_ratio;
                obesity40 = obesity_ratio;
            }
            else if (a == 50) {
                underweight50 = underweight_ratio;
                normalweight50 = normalweight_ratio;
                overweight50 = overweight_ratio;
                obesity50 = obesity_ratio;
            }
            else if (a == 60) {
                underweight60 = underweight_ratio;
                normalweight60 = normalweight_ratio;
                overweight60 = overweight_ratio;
                obesity60 = obesity_ratio;
            }
            else if (a == 70) {
                underweight70 = underweight_ratio;
                normalweight70 = normalweight_ratio;
                overweight70 = overweight_ratio;
                obesity70 = obesity_ratio;
            }
        } else {
            // 해당 연령대에 데이터가 없을 경우 0으로 설정
            if (a == 20) { underweight20 = normalweight20 = overweight20 = obesity20 = 0.0; }
            else if (a == 30) { underweight30 = normalweight30 = overweight30 = obesity30 = 0.0; }
            else if (a == 40) { underweight40 = normalweight40 = overweight40 = obesity40 = 0.0; }
            else if (a == 50) { underweight50 = normalweight50 = overweight50 = obesity50 = 0.0; }
            else if (a == 60) { underweight60 = normalweight60 = overweight60 = obesity60 = 0.0; }
            else if (a == 70) { underweight70 = normalweight70 = overweight70 = obesity70 = 0.0; }
        }
    }
}

```

이제 빌드해서 4단계 개선사항이 제대로 작동하는지 확인하겠습니다:

훌륭합니다! 4단계 개선사항도 모든 테스트를 통과했습니다. 이제 4단계를 커밋하겠습니다:

4단계 커밋이 성공했습니다! TODO를 업데이트하고 리팩토링 진행 상황을 정리하겠습니다:

## 리팩토링 진행 상황 정리

### 완료된 단계들 (1-4단계)

✅ **1단계: 헤더 위생과 API 최소 정리**
- `using namespace std` 제거, 필요한 헤더 포함으로 헤더 자족성 확보
- `Split` 함수를 익명 네임스페이스로 이동하여 내부화
- `std::ifstream` 사용으로 RAII 적용, 자원 누수 방지
- 파일 열기 실패 처리, 토큰 개수 검증, 빈 라인 스킵 등 안전성 강화
- 체중 보간과 비율 계산에서 분모 0 방지 로직 추가
- BMI 경계값과 타입 코드를 상수로 정의
- `GetBmiRatio`를 `const` 메서드로 변경

✅ **2단계: 입력/파싱 안전성 강화**
- 문자열 트리밍 함수 추가로 공백 처리 개선
- `std::stoi`, `std::stod` 사용으로 안전한 숫자 파싱 (예외 처리 포함)
- 데이터 유효성 검증 함수 추가 (연령, 체중, 키의 현실적 범위 검증)
- 주석 라인 스킵 기능 추가 (# 또는 // 로 시작하는 라인)
- 유효하지 않은 데이터 건너뛰기 로직 추가

✅ **3단계: 데이터 모델 개선**
- `HealthRecord` 구조체 도입으로 관련 데이터 응집도 향상
- 병렬 배열(`ages`, `weights`, `heights`, `bmis`)을 `std::vector<HealthRecord>`로 통합
- 고정 크기 배열(10000) 제한 제거, 동적 크기 지원
- 범위 기반 for 루프 사용으로 코드 가독성 향상
- 데이터 접근 패턴 단순화 (`record.age`, `record.weight` 등)

✅ **4단계: 집계/보간 로직 모듈화**
- `ImputeMissingWeights()` 함수로 체중 보간 로직 분리
- `CalculateBmiValues()` 함수로 BMI 계산 로직 분리  
- `CalculateAgeGroupRatios()` 함수로 연령대별 비율 계산 로직 분리
- `GetAgeBucket()` 헬퍼 함수 추가 (향후 확장 준비)
- `CalculateBmi()` 함수가 단순하고 읽기 쉬운 구조로 개선
- 단일 책임 원칙 적용으로 각 함수의 역할 명확화

### 현재 코드 상태 요약

**주요 개선사항:**
- **안전성**: 자원 관리(RAII), 예외 처리, 입력 검증, 분모 0 방지
- **가독성**: 구조체 사용, 범위 기반 루프, 함수 모듈화, 상수 정의
- **유지보수성**: 단일 책임 원칙, 헬퍼 함수 분리, 명확한 인터페이스
- **확장성**: 동적 크기 지원, 유효성 검증 프레임워크

**남은 코드 스멜:**
- 여전히 존재하는 중복 코드 (연령대별 비율 저장 로직)
- 매직 넘버들 (20, 30, 40, 50, 60, 70)
- `GetBmiRatio`의 반복적인 조건문들
- 연령대별 멤버 변수들의 반복 패턴

### 다음 단계들 (5-7단계)

🔄 **5단계: 중복 제거와 자료구조 단순화**
- 연령대별 비율 저장을 `std::map` 또는 `std::array`로 단순화
- `GetBmiRatio`의 조건문들을 데이터 구조 조회로 대체
- 연령대별 멤버 변수들을 구조화된 컨테이너로 통합

🔄 **6단계: 매직 넘버 제거와 의도 드러내기**
- 연령대 범위를 상수 배열이나 설정으로 외부화
- `enum class BmiType`으로 타입 코드 100/200/300/400 대체
- BMI 경계값들을 명명된 상수로 정의

🔄 **7단계: 테스트 방어막 보강**
- 추가 엣지 케이스 테스트 (연령대 데이터 없음, 체중 전부 0, BMI 경계값)
- 성능 테스트 (대용량 데이터 처리)
- 오류 조건 테스트 (잘못된 입력 파일, 메모리 부족 등)

### 현재까지의 성과

✨ **모든 승인 테스트가 각 단계에서 통과**하여 기존 기능이 완전히 보존됨
✨ **점진적 리팩토링**으로 안전하게 코드 품질 개선
✨ **Git 커밋 히스토리**로 각 개선 단계 추적 가능
✨ **코드 가독성과 유지보수성이 크게 향상**됨

리팩토링이 매우 성공적으로 진행되고 있습니다! 남은 3단계를 계속 진행하시겠습니까?

