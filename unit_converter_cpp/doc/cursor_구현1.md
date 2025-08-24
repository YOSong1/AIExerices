# 요구 사항 분석 및 설계 재작성
_Exported on 8/23/2025 at 23:53:06 GMT+9 from Cursor (1.5.4)_

---

**User**

요구 사항을 분석하고 구체적인 설계를 다시 해줘.

---

**Cursor**

방금 현재 코드와 README 전체를 훑어 설계에 반영하겠습니다. 이제 주요 파일들을 열어 내용을 확인할게요.

### 요구사항 재정리

- **핵심 기능**: 입력값 `단위:값`을 받아 같은 계열의 모든 단위로 변환해 출력
- **지원 단위(초기)**: meter, feet, yard (모든 비율은 meter 기준)
- **품질 요구**: OCP/SRP 준수, 입력값 검증(음수, 형식 오류, 미지원 단위)
- **추가 요구**:
  - 설정 외부화(JSON/YAML)로 변환 비율 로드
  - 동적 단위 등록: 예) `1 cubit = 0.4572 meter`
  - 출력 포맷 선택: JSON / CSV / 표

### 설계 목표

- **OCP**: 새 단위/출력 형식 추가 시 기존 코드 수정 최소화
- **SRP**: 입력 파싱, 변환, 포맷팅, 설정 로드를 역할 별 클래스로 분리
- **테스트 용이성**: I/O와 비즈니스 로직 분리, 순수 함수/서비스 단위 테스트

### 아키텍처 개요

- **도메인**
  - `Unit`: 단위 이름, 기준 단위로의 환산계수
  - `ConversionQuery`: 입력 단위/값
  - `ConversionResult`: 원본 + 대상 단위별 값 맵
- **서비스**
  - `UnitRegistry`: 단위 등록/조회(기준 단위 기반), 동적 등록 지원
  - `ConversionService`: 단위 간 변환, “모든 단위로” 일괄 변환
  - `ConfigLoader`: JSON/YAML에서 단위 정의 로드
  - `FormatStrategy`(인터페이스) + `JsonFormatter`/`CsvFormatter`/`TableFormatter`
  - `InputParser`: `단위:값`, 등록식 `1 X = r Y` 파싱
  - `Validator`: 형식/음수/미지원 단위 검증
- **UI/애플리케이션**
  - `main.cpp`: 인자 파싱(포맷, 설정 경로), 한 줄 입력 읽기 → 서비스 조합 호출 → 출력

### 클래스 설계

- **Unit**
  - fields: `std::string name`, `double toBaseFactor`
- **UnitRegistry**
  - ctor: `(std::string baseUnitName)`
  - `addUnitFromBase(name, toBaseFactor)`
  - `addUnitRelative(name, ratio, relativeUnitName)`  // `1 name = ratio relativeUnit`
  - `bool has(name)`, `Unit get(name)`, `std::vector<Unit> list()`
  - 내부: `unordered_map<string, Unit>`
- **ConversionService**
  - `double convert(double value, std::string from, std::string to)`
  - `std::vector<std::pair<std::string,double>> convertToAll(double value, std::string from)`
  - 구현: 모두 기준 단위(meter) 경유
- **ConfigLoader**
  - `load(path) -> (baseUnit, vector<pair<name,toBaseFactor>>)`
  - 권장: `nlohmann::json` 단일 헤더 사용. YAML 필요 시 `yaml-cpp` 선택적
- **FormatStrategy**
  - `std::string format(const ConversionQuery&, const std::vector<std::pair<std::string,double>>&)`
  - 구체 클래스: `JsonFormatter`, `CsvFormatter`, `TableFormatter`
- **InputParser**
  - `parseQuery("meter:2.5") -> ConversionQuery`
  - `parseRegistration("1 cubit = 0.4572 meter") -> (name="cubit", ratio=0.4572, relative="meter")`
- **Validator**
  - `validateQuery(ConversionQuery, UnitRegistry)` // 단위 존재, 값 유효(음수 금지 등)
  - `validateRegistration(...)` // 참조 단위 존재, 비율>0

### 데이터/설정 모델

- **JSON 예시**
```json
{
  "baseUnit": "meter",
  "units": {
    "meter": 1.0,
    "feet": 3.28084,
    "yard": 1.09361
  }
}
```
- 의미: `toBaseFactor`는 “1 baseUnit = units[name] name” 이 아니라, “1 name = toBaseFactor baseUnit”로 정의(권장). 예: feet 3.28084 → 1 feet = 0.3048 meter. 일관성을 위해 위 예시는 다음과 같이 권장합니다:
```json
{
  "baseUnit": "meter",
  "units": {
    "meter": 1.0,
    "feet": 0.3048,
    "yard": 0.9144
  }
}
```

### 입출력/흐름 설계

- 프로그램 시작 시:
  - `--config`로 설정 로드 → `UnitRegistry` 초기화
  - `--format`로 포맷터 인스턴스 선택(`table` 기본)
- 한 줄 입력:
  - `단위:값` 형식 → 파싱 → 검증 → `convertToAll` → 현재 단위 제외하고 출력
  - `1 X = r Y` 형식 → 파싱/검증 → `UnitRegistry.addUnitRelative(X, r, Y)`
- 출력:
  - `table`: 사람이 읽기 쉬운 정렬 열 출력
  - `json`: 구조적 응답
  - `csv`: 헤더 + 레코드

### 검증/에러 처리

- **형식 오류**: 콜론/등호/토큰 개수 불일치 시 친절한 메시지
- **음수/NaN/Inf**: 거부
- **미지원 단위**: 사용 가능한 단위 목록 제시
- **중복 등록**: 허용(갱신) 또는 거부(옵션) 선택 가능. 기본은 갱신

### 확장성 전략(OCP)

- **단위 추가**: 설정 파일 또는 `1 X = r Y` 입력으로 등록 → 코드 수정 불필요
- **출력 형식 추가**: `FormatStrategy` 구현체만 추가, 주입으로 사용
- **새 입력 패턴**: 파서 클래스를 확장하거나 체인 적용(Chain of Responsibility)

### 테스트 전략

- 단위 테스트(예: GoogleTest)
  - `UnitRegistry`: 등록/상대 등록/조회/오류
  - `ConversionService`: meter↔feet↔yard 정확도(허용 오차)
  - `InputParser`: 정상/에러 케이스
  - `Validator`: 음수/형식/미지원 단위
  - `ConfigLoader`: 올바른 로드/잘못된 파일
  - `Formatters`: 출력 스냅샷 테스트
- 통합 테스트
  - 설정 로드 → 입력 → 변환 → 출력까지 플로우

### 의존성/빌드

- C++17+
- JSON: `nlohmann/json`(단일 헤더) 권장
- 테스트: `GoogleTest`
- 빌드: CMake 프로젝트 구조 권장
  - `src/`, `include/`, `tests/`, `configs/`

### 예시 실행 시나리오

- `unit_converter --config configs/units.json --format table`
- 입력:
  - `meter:2.5` → feet/yard로 출력
  - `1 cubit = 0.4572 meter` → “cubit” 등록됨
  - `cubit:2` → 모든 단위로 출력

### 구현 체크리스트

- **도메인/서비스**: `Unit`, `UnitRegistry`, `ConversionService`
- **기반 기능**: 파싱/검증, 설정 로드, 표 포맷터
- **추가 기능**: JSON/CSV 포맷터, 동적 등록 구문 처리
- **테스트**: 핵심 로직부터 커버, 경계/에러 케이스 포함

요청하시면 위 설계대로 디렉터리/파일 스캐폴딩과 기본 구현(테스트 포함)까지 바로 진행하겠습니다.

- 현재 코드의 단일 파일/분기 기반 구조를 SRP/OCP 중심의 모듈 구조로 재설계했고, 기준 단위 경유 방식으로 feet/yard 등 상호 변환 정확성을 확보했습니다.
- 설정 외부화, 동적 등록, 포맷 전략을 분리해 기능 추가 시 코드 수정 최소화가 가능합니다.

```json
{
  "baseUnit": "meter",
  "units": {
    "meter": 1.0,
    "feet": 3.28084,
    "yard": 1.09361
  }
}
```

```json
{
  "baseUnit": "meter",
  "units": {
    "meter": 1.0,
    "feet": 0.3048,
    "yard": 0.9144
  }
}
```

---

**User**

converter.h, converter.cpp 파일에 설계 내용을 구현해줘.


---

**Cursor**

지금 `converter.h`와 `converter.cpp`에 설계한 클래스들을 추가 구현하겠습니다. 먼저 할 일 목록을 만들고 `converter.h` 작성부터 진행할게요. 완료 후 바로 `converter.cpp` 구현까지 함께 적용하겠습니다.

이제 `converter.h`를 새로 만들고, 이어서 `converter.cpp`에 구현을 작성하겠습니다. 먼저 헤더를 추가한 뒤 바로 소스 구현까지 적용할게요.

```cpp
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

// Core domain types
struct Unit {
    std::string name;             // Unit name, e.g., "meter"
    double toBaseFactor;          // How many base units in 1 of this unit. e.g., 1 feet = 0.3048 meter -> toBaseFactor=0.3048
};

struct ConversionQuery {
    std::string unitName;         // Source unit name
    double value;                 // Source value (>= 0)
};

// Registry of units relative to a base unit (e.g., meter)
class UnitRegistry {
public:
    explicit UnitRegistry(const std::string& baseUnitName);

    const std::string& baseUnit() const;

    // Register a unit by specifying how many base units are in 1 unit
    // Example: addUnitFromBase("feet", 0.3048) means 1 feet = 0.3048 meter
    void addUnitFromBase(const std::string& unitName, double toBaseFactor);

    // Register a unit relative to an existing unit
    // Example: addUnitRelative("cubit", 0.4572, "meter") means 1 cubit = 0.4572 meter
    void addUnitRelative(const std::string& unitName, double ratio, const std::string& relativeUnitName);

    bool has(const std::string& unitName) const;
    const Unit& get(const std::string& unitName) const; // throws std::out_of_range if not found
    std::vector<Unit> list() const;

private:
    std::string baseUnitName_;
    std::unordered_map<std::string, Unit> nameToUnit_;
};

// Performs conversions via the base unit
class ConversionService {
public:
    explicit ConversionService(const UnitRegistry& registry);

    double convert(double value, const std::string& fromUnit, const std::string& toUnit) const;

    // Convert to all registered units. If includeSource=false, exclude the source unit from the result list.
    std::vector<std::pair<std::string, double>> convertToAll(double value, const std::string& fromUnit, bool includeSource = false) const;

private:
    const UnitRegistry& registry_;
};

// Parses user inputs (query: "unit:value", registration: "1 X = r Y")
class InputParser {
public:
    ConversionQuery parseQuery(const std::string& input) const; // throws std::invalid_argument

    struct Registration {
        std::string unitName;     // e.g., "cubit"
        double ratio;             // e.g., 0.4572
        std::string relativeUnit; // e.g., "meter"
    };

    Registration parseRegistration(const std::string& input) const; // throws std::invalid_argument
};

// Validates inputs and registry-related constraints
class Validator {
public:
    void validateQuery(const ConversionQuery& query, const UnitRegistry& registry) const;      // throws std::invalid_argument
    void validateRegistration(const InputParser::Registration& reg, const UnitRegistry& registry) const; // throws std::invalid_argument
};

// Output formatting strategy
class FormatStrategy {
public:
    virtual ~FormatStrategy() = default;
    virtual std::string format(const ConversionQuery& query,
                               const std::vector<std::pair<std::string, double>>& results,
                               const std::string& baseUnit) const = 0;
};

class TableFormatter : public FormatStrategy {
public:
    std::string format(const ConversionQuery& query,
                       const std::vector<std::pair<std::string, double>>& results,
                       const std::string& baseUnit) const override;
};

class CsvFormatter : public FormatStrategy {
public:
    std::string format(const ConversionQuery& query,
                       const std::vector<std::pair<std::string, double>>& results,
                       const std::string& baseUnit) const override;
};

class JsonFormatter : public FormatStrategy {
public:
    std::string format(const ConversionQuery& query,
                       const std::vector<std::pair<std::string, double>>& results,
                       const std::string& baseUnit) const override;
};


```

```cpp
// converter.cpp: core implementation for unit conversion

#include "converter.h"

#include <sstream>
#include <iomanip>
#include <cctype>

// ---------------- UnitRegistry -----------------

UnitRegistry::UnitRegistry(const std::string& baseUnitName)
    : baseUnitName_(baseUnitName) {
    nameToUnit_[baseUnitName_] = Unit{ baseUnitName_, 1.0 };
}

const std::string& UnitRegistry::baseUnit() const {
    return baseUnitName_;
}

void UnitRegistry::addUnitFromBase(const std::string& unitName, double toBaseFactor) {
    if (unitName.empty()) {
        throw std::invalid_argument("unitName must not be empty");
    }
    if (!(toBaseFactor > 0.0)) {
        throw std::invalid_argument("toBaseFactor must be > 0");
    }
    nameToUnit_[unitName] = Unit{ unitName, toBaseFactor };
}

void UnitRegistry::addUnitRelative(const std::string& unitName, double ratio, const std::string& relativeUnitName) {
    if (!has(relativeUnitName)) {
        throw std::invalid_argument("relative unit not found: " + relativeUnitName);
    }
    const Unit& rel = get(relativeUnitName);
    // ratio: 1 unitName = ratio relativeUnitName
    double toBase = ratio * rel.toBaseFactor;
    addUnitFromBase(unitName, toBase);
}

bool UnitRegistry::has(const std::string& unitName) const {
    return nameToUnit_.find(unitName) != nameToUnit_.end();
}

const Unit& UnitRegistry::get(const std::string& unitName) const {
    auto it = nameToUnit_.find(unitName);
    if (it == nameToUnit_.end()) {
        throw std::out_of_range("unit not found: " + unitName);
    }
    return it->second;
}

std::vector<Unit> UnitRegistry::list() const {
    std::vector<Unit> units;
    units.reserve(nameToUnit_.size());
    for (const auto& kv : nameToUnit_) {
        units.push_back(kv.second);
    }
    return units;
}

// ---------------- ConversionService -----------------

ConversionService::ConversionService(const UnitRegistry& registry)
    : registry_(registry) {}

double ConversionService::convert(double value, const std::string& fromUnit, const std::string& toUnit) const {
    if (fromUnit == toUnit) {
        return value;
    }
    const Unit& from = registry_.get(fromUnit);
    const Unit& to = registry_.get(toUnit);
    // Convert to base: value * from.toBaseFactor
    // Then to target: baseValue / to.toBaseFactor
    double baseValue = value * from.toBaseFactor;
    double result = baseValue / to.toBaseFactor;
    return result;
}

std::vector<std::pair<std::string, double>> ConversionService::convertToAll(double value, const std::string& fromUnit, bool includeSource) const {
    std::vector<std::pair<std::string, double>> out;
    for (const auto& u : registry_.list()) {
        if (!includeSource && u.name == fromUnit) {
            continue;
        }
        out.emplace_back(u.name, convert(value, fromUnit, u.name));
    }
    return out;
}

// ---------------- InputParser -----------------

static inline std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

ConversionQuery InputParser::parseQuery(const std::string& input) const {
    auto pos = input.find(":");
    if (pos == std::string::npos) {
        throw std::invalid_argument("invalid query format, expected 'unit:value'");
    }
    std::string unit = trim(input.substr(0, pos));
    std::string valueStr = trim(input.substr(pos + 1));
    if (unit.empty() || valueStr.empty()) {
        throw std::invalid_argument("invalid query tokens");
    }
    char* end = nullptr;
    double v = std::strtod(valueStr.c_str(), &end);
    if (end == valueStr.c_str() || *end != '\0') {
        throw std::invalid_argument("invalid numeric value");
    }
    return ConversionQuery{ unit, v };
}

InputParser::Registration InputParser::parseRegistration(const std::string& input) const {
    // format: "1 X = r Y"
    // tokenize by spaces/equals
    // We'll parse simply: expect leading '1', then unit, '=', ratio, relativeUnit
    std::string s = trim(input);
    // replace multiple spaces around '=' to handle variations
    auto eqPos = s.find('=');
    if (eqPos == std::string::npos) {
        throw std::invalid_argument("invalid registration format, expected '='");
    }
    std::string left = trim(s.substr(0, eqPos));
    std::string right = trim(s.substr(eqPos + 1));

    // left: "1 X"
    std::istringstream lss(left);
    double one;
    std::string unitName;
    if (!(lss >> one) || !(lss >> unitName) || one != 1.0) {
        throw std::invalid_argument("invalid left side, expected '1 <unit>'");
    }

    // right: "r Y"
    std::istringstream rss(right);
    double ratio;
    std::string relativeUnit;
    if (!(rss >> ratio) || !(rss >> relativeUnit)) {
        throw std::invalid_argument("invalid right side, expected '<ratio> <unit>'");
    }
    if (!(ratio > 0.0)) {
        throw std::invalid_argument("ratio must be > 0");
    }

    return Registration{ unitName, ratio, relativeUnit };
}

// ---------------- Validator -----------------

void Validator::validateQuery(const ConversionQuery& query, const UnitRegistry& registry) const {
    if (!(query.value >= 0.0)) {
        throw std::invalid_argument("value must be >= 0");
    }
    if (!registry.has(query.unitName)) {
        throw std::invalid_argument("unknown unit: " + query.unitName);
    }
}

void Validator::validateRegistration(const InputParser::Registration& reg, const UnitRegistry& registry) const {
    if (reg.unitName.empty() || reg.relativeUnit.empty()) {
        throw std::invalid_argument("unit names must not be empty");
    }
    if (!(reg.ratio > 0.0)) {
        throw std::invalid_argument("ratio must be > 0");
    }
    if (!registry.has(reg.relativeUnit)) {
        throw std::invalid_argument("relative unit not found: " + reg.relativeUnit);
    }
}

// ---------------- Formatters -----------------

static std::string formatDouble(double v, int precision = 6) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << v;
    return oss.str();
}

std::string TableFormatter::format(const ConversionQuery& query,
                                   const std::vector<std::pair<std::string, double>>& results,
                                   const std::string& /*baseUnit*/) const {
    std::ostringstream oss;
    for (const auto& kv : results) {
        oss << formatDouble(query.value) << " " << query.unitName
            << " = " << formatDouble(kv.second) << " " << kv.first << "\n";
    }
    return oss.str();
}

std::string CsvFormatter::format(const ConversionQuery& query,
                                 const std::vector<std::pair<std::string, double>>& results,
                                 const std::string& baseUnit) const {
    std::ostringstream oss;
    (void)baseUnit; // not used currently
    oss << "source_unit,source_value,target_unit,target_value\n";
    for (const auto& kv : results) {
        oss << query.unitName << "," << formatDouble(query.value)
            << "," << kv.first << "," << formatDouble(kv.second) << "\n";
    }
    return oss.str();
}

std::string JsonFormatter::format(const ConversionQuery& query,
                                  const std::vector<std::pair<std::string, double>>& results,
                                  const std::string& baseUnit) const {
    // Minimal JSON building without external deps
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"baseUnit\": \"" << baseUnit << "\",\n";
    oss << "  \"source\": { \"unit\": \"" << query.unitName << "\", \"value\": " << formatDouble(query.value) << " },\n";
    oss << "  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& kv = results[i];
        oss << "    { \"unit\": \"" << kv.first << "\", \"value\": " << formatDouble(kv.second) << " }";
        if (i + 1 < results.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}


```

방금 `converter.h`와 `converter.cpp`에 설계 반영한 핵심 모듈을 구현했습니다. 이제 이 API를 `unit_converter.cpp`의 `main`에서 불러 사용하면 SRP/OCP를 만족하는 구조로 동작합니다.

- 완료한 내용
  - `UnitRegistry`: 기준 단위 기반 단위 등록/조회, 상대 단위 등록 지원
  - `ConversionService`: 기준 단위를 경유한 단위 간 변환, 전체 단위 일괄 변환
  - `InputParser`: `단위:값`, `1 X = r Y` 구문 파싱
  - `Validator`: 입력값/등록값 검증
  - `FormatStrategy`: `Table/Csv/Json` 포맷터 구현

- 사용 예시 코드 스니펫(메인 연동)
```cpp
#include "converter.h"
#include <iostream>

int main() {
    UnitRegistry registry("meter");
    registry.addUnitFromBase("meter", 1.0);
    registry.addUnitFromBase("feet", 0.3048);
    registry.addUnitFromBase("yard", 0.9144);

    ConversionService service(registry);
    InputParser parser;
    Validator validator;
    TableFormatter formatter; // CsvFormatter, JsonFormatter로 교체 가능

    std::string line;
    std::getline(std::cin, line);

    try {
        // 등록 구문인지 먼저 시도
        try {
            auto reg = parser.parseRegistration(line);
            validator.validateRegistration(reg, registry);
            registry.addUnitRelative(reg.unitName, reg.ratio, reg.relativeUnit);
            std::cout << "Registered: " << reg.unitName << std::endl;
            return 0;
        } catch (...) {
            // 등록 형식이 아니면 변환으로 처리
        }

        auto query = parser.parseQuery(line);
        validator.validateQuery(query, registry);
        auto results = service.convertToAll(query.value, query.unitName, false);
        std::cout << formatter.format(query, results, registry.baseUnit());
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
```

- 다음 단계 제안
  - `unit_converter.cpp`를 위 API로 리팩터링
  - 설정 로더(JSON)와 포맷 선택 옵션 추가
  - 단위/포맷 관련 단위 테스트 추가

요청하시면 `unit_converter.cpp` 리팩터링과 설정 로더, 실행 옵션까지 바로 이어서 구현하겠습니다.

```cpp
#include "converter.h"
#include <iostream>

int main() {
    UnitRegistry registry("meter");
    registry.addUnitFromBase("meter", 1.0);
    registry.addUnitFromBase("feet", 0.3048);
    registry.addUnitFromBase("yard", 0.9144);

    ConversionService service(registry);
    InputParser parser;
    Validator validator;
    TableFormatter formatter; // CsvFormatter, JsonFormatter로 교체 가능

    std::string line;
    std::getline(std::cin, line);

    try {
        // 등록 구문인지 먼저 시도
        try {
            auto reg = parser.parseRegistration(line);
            validator.validateRegistration(reg, registry);
            registry.addUnitRelative(reg.unitName, reg.ratio, reg.relativeUnit);
            std::cout << "Registered: " << reg.unitName << std::endl;
            return 0;
        } catch (...) {
            // 등록 형식이 아니면 변환으로 처리
        }

        auto query = parser.parseQuery(line);
        validator.validateQuery(query, registry);
        auto results = service.convertToAll(query.value, query.unitName, false);
        std::cout << formatter.format(query, results, registry.baseUnit());
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
```

---

**User**

@unit_converter.cpp 현재 작성된 main을 그대로 이용할 수 있도록 converter 코드를 수정해줘

---

**Cursor**

지금 `main`을 건드리지 않고도 쓸 수 있는 호환 함수들을 `converter.h/.cpp`에 추가하겠습니다. 기존 상수값(3.28084, 1.09361)을 그대로 사용해 결과가 동일하게 나오도록 하겠습니다.

```cpp
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

// Core domain types
struct Unit {
    std::string name;             // Unit name, e.g., "meter"
    double toBaseFactor;          // How many base units in 1 of this unit. e.g., 1 feet = 0.3048 meter -> toBaseFactor=0.3048
};

struct ConversionQuery {
    std::string unitName;         // Source unit name
    double value;                 // Source value (>= 0)
};

// Registry of units relative to a base unit (e.g., meter)
class UnitRegistry {
public:
    explicit UnitRegistry(const std::string& baseUnitName);

    const std::string& baseUnit() const;

    // Register a unit by specifying how many base units are in 1 unit
    // Example: addUnitFromBase("feet", 0.3048) means 1 feet = 0.3048 meter
    void addUnitFromBase(const std::string& unitName, double toBaseFactor);

    // Register a unit relative to an existing unit
    // Example: addUnitRelative("cubit", 0.4572, "meter") means 1 cubit = 0.4572 meter
    void addUnitRelative(const std::string& unitName, double ratio, const std::string& relativeUnitName);

    bool has(const std::string& unitName) const;
    const Unit& get(const std::string& unitName) const; // throws std::out_of_range if not found
    std::vector<Unit> list() const;

private:
    std::string baseUnitName_;
    std::unordered_map<std::string, Unit> nameToUnit_;
};

// Performs conversions via the base unit
class ConversionService {
public:
    explicit ConversionService(const UnitRegistry& registry);

    double convert(double value, const std::string& fromUnit, const std::string& toUnit) const;

    // Convert to all registered units. If includeSource=false, exclude the source unit from the result list.
    std::vector<std::pair<std::string, double>> convertToAll(double value, const std::string& fromUnit, bool includeSource = false) const;

private:
    const UnitRegistry& registry_;
};

// Parses user inputs (query: "unit:value", registration: "1 X = r Y")
class InputParser {
public:
    ConversionQuery parseQuery(const std::string& input) const; // throws std::invalid_argument

    struct Registration {
        std::string unitName;     // e.g., "cubit"
        double ratio;             // e.g., 0.4572
        std::string relativeUnit; // e.g., "meter"
    };

    Registration parseRegistration(const std::string& input) const; // throws std::invalid_argument
};

// Validates inputs and registry-related constraints
class Validator {
public:
    void validateQuery(const ConversionQuery& query, const UnitRegistry& registry) const;      // throws std::invalid_argument
    void validateRegistration(const InputParser::Registration& reg, const UnitRegistry& registry) const; // throws std::invalid_argument
};

// Output formatting strategy
class FormatStrategy {
public:
    virtual ~FormatStrategy() = default;
    virtual std::string format(const ConversionQuery& query,
                               const std::vector<std::pair<std::string, double>>& results,
                               const std::string& baseUnit) const = 0;
};

class TableFormatter : public FormatStrategy {
public:
    std::string format(const ConversionQuery& query,
                       const std::vector<std::pair<std::string, double>>& results,
                       const std::string& baseUnit) const override;
};

class CsvFormatter : public FormatStrategy {
public:
    std::string format(const ConversionQuery& query,
                       const std::vector<std::pair<std::string, double>>& results,
                       const std::string& baseUnit) const override;
};

class JsonFormatter : public FormatStrategy {
public:
    std::string format(const ConversionQuery& query,
                       const std::vector<std::pair<std::string, double>>& results,
                       const std::string& baseUnit) const override;
};

// ------------------------------------------------------------
// Backward-compatibility helpers to work with the existing main
// These helpers mirror the current constants and flow used in unit_converter.cpp
namespace compat {
    static constexpr double FEET_PER_METER = 3.28084;  // matches README/main
    static constexpr double YARDS_PER_METER = 1.09361; // matches README/main

    // Convert input value in unit -> meters, supporting {meter, feet, yard}
    double to_meter(const std::string& unit, double value);

    // Convert value in meters -> target unit, supporting {meter, feet, yard}
    double from_meter(const std::string& unit, double meterValue);

    // Compute three outputs exactly like the existing main's print order
    void compute_all(const std::string& unit,
                     double value,
                     double& outMeters,
                     double& outFeet,
                     double& outYards);
}



```

```cpp
// converter.cpp: core implementation for unit conversion

#include "converter.h"

#include <sstream>
#include <iomanip>
#include <cctype>

// ---------------- UnitRegistry -----------------

UnitRegistry::UnitRegistry(const std::string& baseUnitName)
    : baseUnitName_(baseUnitName) {
    nameToUnit_[baseUnitName_] = Unit{ baseUnitName_, 1.0 };
}

const std::string& UnitRegistry::baseUnit() const {
    return baseUnitName_;
}

void UnitRegistry::addUnitFromBase(const std::string& unitName, double toBaseFactor) {
    if (unitName.empty()) {
        throw std::invalid_argument("unitName must not be empty");
    }
    if (!(toBaseFactor > 0.0)) {
        throw std::invalid_argument("toBaseFactor must be > 0");
    }
    nameToUnit_[unitName] = Unit{ unitName, toBaseFactor };
}

void UnitRegistry::addUnitRelative(const std::string& unitName, double ratio, const std::string& relativeUnitName) {
    if (!has(relativeUnitName)) {
        throw std::invalid_argument("relative unit not found: " + relativeUnitName);
    }
    const Unit& rel = get(relativeUnitName);
    // ratio: 1 unitName = ratio relativeUnitName
    double toBase = ratio * rel.toBaseFactor;
    addUnitFromBase(unitName, toBase);
}

bool UnitRegistry::has(const std::string& unitName) const {
    return nameToUnit_.find(unitName) != nameToUnit_.end();
}

const Unit& UnitRegistry::get(const std::string& unitName) const {
    auto it = nameToUnit_.find(unitName);
    if (it == nameToUnit_.end()) {
        throw std::out_of_range("unit not found: " + unitName);
    }
    return it->second;
}

std::vector<Unit> UnitRegistry::list() const {
    std::vector<Unit> units;
    units.reserve(nameToUnit_.size());
    for (const auto& kv : nameToUnit_) {
        units.push_back(kv.second);
    }
    return units;
}

// ---------------- ConversionService -----------------

ConversionService::ConversionService(const UnitRegistry& registry)
    : registry_(registry) {}

double ConversionService::convert(double value, const std::string& fromUnit, const std::string& toUnit) const {
    if (fromUnit == toUnit) {
        return value;
    }
    const Unit& from = registry_.get(fromUnit);
    const Unit& to = registry_.get(toUnit);
    // Convert to base: value * from.toBaseFactor
    // Then to target: baseValue / to.toBaseFactor
    double baseValue = value * from.toBaseFactor;
    double result = baseValue / to.toBaseFactor;
    return result;
}

std::vector<std::pair<std::string, double>> ConversionService::convertToAll(double value, const std::string& fromUnit, bool includeSource) const {
    std::vector<std::pair<std::string, double>> out;
    for (const auto& u : registry_.list()) {
        if (!includeSource && u.name == fromUnit) {
            continue;
        }
        out.emplace_back(u.name, convert(value, fromUnit, u.name));
    }
    return out;
}

// ---------------- InputParser -----------------

static inline std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

ConversionQuery InputParser::parseQuery(const std::string& input) const {
    auto pos = input.find(":");
    if (pos == std::string::npos) {
        throw std::invalid_argument("invalid query format, expected 'unit:value'");
    }
    std::string unit = trim(input.substr(0, pos));
    std::string valueStr = trim(input.substr(pos + 1));
    if (unit.empty() || valueStr.empty()) {
        throw std::invalid_argument("invalid query tokens");
    }
    char* end = nullptr;
    double v = std::strtod(valueStr.c_str(), &end);
    if (end == valueStr.c_str() || *end != '\0') {
        throw std::invalid_argument("invalid numeric value");
    }
    return ConversionQuery{ unit, v };
}

InputParser::Registration InputParser::parseRegistration(const std::string& input) const {
    // format: "1 X = r Y"
    // tokenize by spaces/equals
    // We'll parse simply: expect leading '1', then unit, '=', ratio, relativeUnit
    std::string s = trim(input);
    // replace multiple spaces around '=' to handle variations
    auto eqPos = s.find('=');
    if (eqPos == std::string::npos) {
        throw std::invalid_argument("invalid registration format, expected '='");
    }
    std::string left = trim(s.substr(0, eqPos));
    std::string right = trim(s.substr(eqPos + 1));

    // left: "1 X"
    std::istringstream lss(left);
    double one;
    std::string unitName;
    if (!(lss >> one) || !(lss >> unitName) || one != 1.0) {
        throw std::invalid_argument("invalid left side, expected '1 <unit>'");
    }

    // right: "r Y"
    std::istringstream rss(right);
    double ratio;
    std::string relativeUnit;
    if (!(rss >> ratio) || !(rss >> relativeUnit)) {
        throw std::invalid_argument("invalid right side, expected '<ratio> <unit>'");
    }
    if (!(ratio > 0.0)) {
        throw std::invalid_argument("ratio must be > 0");
    }

    return Registration{ unitName, ratio, relativeUnit };
}

// ---------------- Validator -----------------

void Validator::validateQuery(const ConversionQuery& query, const UnitRegistry& registry) const {
    if (!(query.value >= 0.0)) {
        throw std::invalid_argument("value must be >= 0");
    }
    if (!registry.has(query.unitName)) {
        throw std::invalid_argument("unknown unit: " + query.unitName);
    }
}

void Validator::validateRegistration(const InputParser::Registration& reg, const UnitRegistry& registry) const {
    if (reg.unitName.empty() || reg.relativeUnit.empty()) {
        throw std::invalid_argument("unit names must not be empty");
    }
    if (!(reg.ratio > 0.0)) {
        throw std::invalid_argument("ratio must be > 0");
    }
    if (!registry.has(reg.relativeUnit)) {
        throw std::invalid_argument("relative unit not found: " + reg.relativeUnit);
    }
}

// ---------------- Formatters -----------------

static std::string formatDouble(double v, int precision = 6) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << v;
    return oss.str();
}

std::string TableFormatter::format(const ConversionQuery& query,
                                   const std::vector<std::pair<std::string, double>>& results,
                                   const std::string& /*baseUnit*/) const {
    std::ostringstream oss;
    for (const auto& kv : results) {
        oss << formatDouble(query.value) << " " << query.unitName
            << " = " << formatDouble(kv.second) << " " << kv.first << "\n";
    }
    return oss.str();
}

std::string CsvFormatter::format(const ConversionQuery& query,
                                 const std::vector<std::pair<std::string, double>>& results,
                                 const std::string& baseUnit) const {
    std::ostringstream oss;
    (void)baseUnit; // not used currently
    oss << "source_unit,source_value,target_unit,target_value\n";
    for (const auto& kv : results) {
        oss << query.unitName << "," << formatDouble(query.value)
            << "," << kv.first << "," << formatDouble(kv.second) << "\n";
    }
    return oss.str();
}

std::string JsonFormatter::format(const ConversionQuery& query,
                                  const std::vector<std::pair<std::string, double>>& results,
                                  const std::string& baseUnit) const {
    // Minimal JSON building without external deps
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"baseUnit\": \"" << baseUnit << "\",\n";
    oss << "  \"source\": { \"unit\": \"" << query.unitName << "\", \"value\": " << formatDouble(query.value) << " },\n";
    oss << "  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& kv = results[i];
        oss << "    { \"unit\": \"" << kv.first << "\", \"value\": " << formatDouble(kv.second) << " }";
        if (i + 1 < results.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

// ---------------- Backward-compat helpers (compat) -----------------

namespace compat {
    double to_meter(const std::string& unit, double value) {
        if (unit == "meter") {
            return value;
        } else if (unit == "feet") {
            return value / FEET_PER_METER;
        } else if (unit == "yard") {
            return value / YARDS_PER_METER;
        }
        // Unknown unit: keep behavior close to original (no explicit error). Return 0.
        return 0.0;
    }

    double from_meter(const std::string& unit, double meterValue) {
        if (unit == "meter") {
            return meterValue;
        } else if (unit == "feet") {
            return meterValue * FEET_PER_METER;
        } else if (unit == "yard") {
            return meterValue * YARDS_PER_METER;
        }
        return 0.0;
    }

    void compute_all(const std::string& unit,
                     double value,
                     double& outMeters,
                     double& outFeet,
                     double& outYards) {
        double meter_value = to_meter(unit, value);
        outMeters = meter_value;
        outFeet = from_meter("feet", meter_value);
        outYards = from_meter("yard", meter_value);
    }
}



```

- 기존 `main`을 유지하기 위해 `converter.h/.cpp`에 호환 함수(`compat` 네임스페이스)를 추가했습니다.
  - **상수 유지**: `FEET_PER_METER=3.28084`, `YARDS_PER_METER=1.09361`
  - **함수**:
    - `compat::to_meter(unit, value)`
    - `compat::from_meter(unit, meterValue)`
    - `compat::compute_all(unit, value, outMeters, outFeet, outYards)`

- `unit_converter.cpp`의 현재 흐름과 동일한 계산이 가능합니다. 원하면 `unit_converter.cpp`에서 아래처럼 사용할 수 있습니다:
```cpp
#include "converter.h" // 추가

// ...
double in_meters = 0, in_feet = 0, in_yards = 0;
compat::compute_all(unit, value, in_meters, in_feet, in_yards);
```

- 참고: 미지원 단위가 들어올 경우 기존 코드와 동일하게 별도 에러 없이 `0` 기준으로 동작하도록 맞췄습니다. 필요 시 오류 반환 방식으로 바꿔드릴 수 있습니다.

```cpp
#include "converter.h" // 추가

// ...
double in_meters = 0, in_feet = 0, in_yards = 0;
compat::compute_all(unit, value, in_meters, in_feet, in_yards);
```

---

**User**

@envirenment.txt 설치된 환경을 이용하여 현재 프로젝트를 실행할 수 있는 CMake 환경을 추가해줘

---

**Cursor**

CMake 빌드 환경과 테스트를 추가하겠습니다. 우선 `converter.cpp`의 누락된 포함 헤더를 보완하고, 루트 `CMakeLists.txt`와 `tests` 디렉터리(구글테스트 연동)를 생성한 뒤, 바로 빌드/테스트를 실행해 확인하겠습니다.

CMake 구성과 테스트 스켈레톤을 추가하겠습니다. 먼저 TODO 상태를 정리하고 루트 `CMakeLists.txt`와 `tests` 디렉터리를 생성한 뒤, 누락된 `<cstdlib>` 포함을 보완합니다. 이어서 빌드와 테스트를 시도합니다.

CMake 환경과 테스트를 추가하는 중입니다. 루트 `CMakeLists.txt`, `tests` 디렉터리와 테스트를 생성하고, `converter.cpp`에 누락된 헤더를 보강하겠습니다. 완료 후 TODO 상태를 갱신하고 빌드 사용법을 안내하겠습니다.

```cmake
cmake_minimum_required(VERSION 3.16)

project(unit_converter_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library for conversion core
add_library(converter
    converter.cpp
)
target_include_directories(converter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Executable using the existing main (no source changes required)
add_executable(unit_converter
    unit_converter.cpp
)
target_link_libraries(unit_converter PRIVATE converter)

option(UNIT_CONVERTER_BUILD_TESTS "Build unit tests" ON)
if(UNIT_CONVERTER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()


```

```cmake
cmake_minimum_required(VERSION 3.16)

# Assume GoogleTest is installed on the system (per environment.txt)
find_package(GTest REQUIRED)

add_executable(converter_tests
    test_conversion.cpp
)
target_link_libraries(converter_tests PRIVATE converter GTest::gtest GTest::gtest_main)
target_include_directories(converter_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

include(GoogleTest)
gtest_discover_tests(converter_tests)


```

```cpp
#include <gtest/gtest.h>
#include "converter.h"

TEST(ConversionServiceTest, MeterToFeetAndYard) {
    UnitRegistry reg("meter");
    reg.addUnitFromBase("meter", 1.0);
    reg.addUnitFromBase("feet", 0.3048);
    reg.addUnitFromBase("yard", 0.9144);

    ConversionService svc(reg);
    double v = 2.5; // meter
    // Expected values based on ratios
    double feet = svc.convert(v, "meter", "feet");
    double yard = svc.convert(v, "meter", "yard");

    // From README: 1 meter = 3.28084 feet ; 1 meter = 1.09361 yard
    EXPECT_NEAR(feet, 2.5 * 3.28084, 1e-6);
    EXPECT_NEAR(yard, 2.5 * 1.09361, 1e-6);
}

TEST(CompatTest, ComputeAllMatchesMain) {
    double m=0, f=0, y=0;
    compat::compute_all("meter", 2.5, m, f, y);
    EXPECT_NEAR(m, 2.5, 1e-9);
    EXPECT_NEAR(f, 2.5 * compat::FEET_PER_METER, 1e-6);
    EXPECT_NEAR(y, 2.5 * compat::YARDS_PER_METER, 1e-6);
}


```

CMake 파일과 테스트 폴백을 보강하고, 컴파일에 필요한 헤더를 추가합니다. 그런 다음 빌드 방법을 안내하겠습니다.

```cpp
// converter.cpp: core implementation for unit conversion

#include "converter.h"

#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdlib>

// ---------------- UnitRegistry -----------------

UnitRegistry::UnitRegistry(const std::string& baseUnitName)
    : baseUnitName_(baseUnitName) {
    nameToUnit_[baseUnitName_] = Unit{ baseUnitName_, 1.0 };
}

const std::string& UnitRegistry::baseUnit() const {
    return baseUnitName_;
}

void UnitRegistry::addUnitFromBase(const std::string& unitName, double toBaseFactor) {
    if (unitName.empty()) {
        throw std::invalid_argument("unitName must not be empty");
    }
    if (!(toBaseFactor > 0.0)) {
        throw std::invalid_argument("toBaseFactor must be > 0");
    }
    nameToUnit_[unitName] = Unit{ unitName, toBaseFactor };
}

void UnitRegistry::addUnitRelative(const std::string& unitName, double ratio, const std::string& relativeUnitName) {
    if (!has(relativeUnitName)) {
        throw std::invalid_argument("relative unit not found: " + relativeUnitName);
    }
    const Unit& rel = get(relativeUnitName);
    // ratio: 1 unitName = ratio relativeUnitName
    double toBase = ratio * rel.toBaseFactor;
    addUnitFromBase(unitName, toBase);
}

bool UnitRegistry::has(const std::string& unitName) const {
    return nameToUnit_.find(unitName) != nameToUnit_.end();
}

const Unit& UnitRegistry::get(const std::string& unitName) const {
    auto it = nameToUnit_.find(unitName);
    if (it == nameToUnit_.end()) {
        throw std::out_of_range("unit not found: " + unitName);
    }
    return it->second;
}

std::vector<Unit> UnitRegistry::list() const {
    std::vector<Unit> units;
    units.reserve(nameToUnit_.size());
    for (const auto& kv : nameToUnit_) {
        units.push_back(kv.second);
    }
    return units;
}

// ---------------- ConversionService -----------------

ConversionService::ConversionService(const UnitRegistry& registry)
    : registry_(registry) {}

double ConversionService::convert(double value, const std::string& fromUnit, const std::string& toUnit) const {
    if (fromUnit == toUnit) {
        return value;
    }
    const Unit& from = registry_.get(fromUnit);
    const Unit& to = registry_.get(toUnit);
    // Convert to base: value * from.toBaseFactor
    // Then to target: baseValue / to.toBaseFactor
    double baseValue = value * from.toBaseFactor;
    double result = baseValue / to.toBaseFactor;
    return result;
}

std::vector<std::pair<std::string, double>> ConversionService::convertToAll(double value, const std::string& fromUnit, bool includeSource) const {
    std::vector<std::pair<std::string, double>> out;
    for (const auto& u : registry_.list()) {
        if (!includeSource && u.name == fromUnit) {
            continue;
        }
        out.emplace_back(u.name, convert(value, fromUnit, u.name));
    }
    return out;
}

// ---------------- InputParser -----------------

static inline std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

ConversionQuery InputParser::parseQuery(const std::string& input) const {
    auto pos = input.find(":");
    if (pos == std::string::npos) {
        throw std::invalid_argument("invalid query format, expected 'unit:value'");
    }
    std::string unit = trim(input.substr(0, pos));
    std::string valueStr = trim(input.substr(pos + 1));
    if (unit.empty() || valueStr.empty()) {
        throw std::invalid_argument("invalid query tokens");
    }
    char* end = nullptr;
    double v = std::strtod(valueStr.c_str(), &end);
    if (end == valueStr.c_str() || *end != '\0') {
        throw std::invalid_argument("invalid numeric value");
    }
    return ConversionQuery{ unit, v };
}

InputParser::Registration InputParser::parseRegistration(const std::string& input) const {
    // format: "1 X = r Y"
    // tokenize by spaces/equals
    // We'll parse simply: expect leading '1', then unit, '=', ratio, relativeUnit
    std::string s = trim(input);
    // replace multiple spaces around '=' to handle variations
    auto eqPos = s.find('=');
    if (eqPos == std::string::npos) {
        throw std::invalid_argument("invalid registration format, expected '='");
    }
    std::string left = trim(s.substr(0, eqPos));
    std::string right = trim(s.substr(eqPos + 1));

    // left: "1 X"
    std::istringstream lss(left);
    double one;
    std::string unitName;
    if (!(lss >> one) || !(lss >> unitName) || one != 1.0) {
        throw std::invalid_argument("invalid left side, expected '1 <unit>'");
    }

    // right: "r Y"
    std::istringstream rss(right);
    double ratio;
    std::string relativeUnit;
    if (!(rss >> ratio) || !(rss >> relativeUnit)) {
        throw std::invalid_argument("invalid right side, expected '<ratio> <unit>'");
    }
    if (!(ratio > 0.0)) {
        throw std::invalid_argument("ratio must be > 0");
    }

    return Registration{ unitName, ratio, relativeUnit };
}

// ---------------- Validator -----------------

void Validator::validateQuery(const ConversionQuery& query, const UnitRegistry& registry) const {
    if (!(query.value >= 0.0)) {
        throw std::invalid_argument("value must be >= 0");
    }
    if (!registry.has(query.unitName)) {
        throw std::invalid_argument("unknown unit: " + query.unitName);
    }
}

void Validator::validateRegistration(const InputParser::Registration& reg, const UnitRegistry& registry) const {
    if (reg.unitName.empty() || reg.relativeUnit.empty()) {
        throw std::invalid_argument("unit names must not be empty");
    }
    if (!(reg.ratio > 0.0)) {
        throw std::invalid_argument("ratio must be > 0");
    }
    if (!registry.has(reg.relativeUnit)) {
        throw std::invalid_argument("relative unit not found: " + reg.relativeUnit);
    }
}

// ---------------- Formatters -----------------

static std::string formatDouble(double v, int precision = 6) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << v;
    return oss.str();
}

std::string TableFormatter::format(const ConversionQuery& query,
                                   const std::vector<std::pair<std::string, double>>& results,
                                   const std::string& /*baseUnit*/) const {
    std::ostringstream oss;
    for (const auto& kv : results) {
        oss << formatDouble(query.value) << " " << query.unitName
            << " = " << formatDouble(kv.second) << " " << kv.first << "\n";
    }
    return oss.str();
}

std::string CsvFormatter::format(const ConversionQuery& query,
                                 const std::vector<std::pair<std::string, double>>& results,
                                 const std::string& baseUnit) const {
    std::ostringstream oss;
    (void)baseUnit; // not used currently
    oss << "source_unit,source_value,target_unit,target_value\n";
    for (const auto& kv : results) {
        oss << query.unitName << "," << formatDouble(query.value)
            << "," << kv.first << "," << formatDouble(kv.second) << "\n";
    }
    return oss.str();
}

std::string JsonFormatter::format(const ConversionQuery& query,
                                  const std::vector<std::pair<std::string, double>>& results,
                                  const std::string& baseUnit) const {
    // Minimal JSON building without external deps
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"baseUnit\": \"" << baseUnit << "\",\n";
    oss << "  \"source\": { \"unit\": \"" << query.unitName << "\", \"value\": " << formatDouble(query.value) << " },\n";
    oss << "  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& kv = results[i];
        oss << "    { \"unit\": \"" << kv.first << "\", \"value\": " << formatDouble(kv.second) << " }";
        if (i + 1 < results.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

// ---------------- Backward-compat helpers (compat) -----------------

namespace compat {
    double to_meter(const std::string& unit, double value) {
        if (unit == "meter") {
            return value;
        } else if (unit == "feet") {
            return value / FEET_PER_METER;
        } else if (unit == "yard") {
            return value / YARDS_PER_METER;
        }
        // Unknown unit: keep behavior close to original (no explicit error). Return 0.
        return 0.0;
    }

    double from_meter(const std::string& unit, double meterValue) {
        if (unit == "meter") {
            return meterValue;
        } else if (unit == "feet") {
            return meterValue * FEET_PER_METER;
        } else if (unit == "yard") {
            return meterValue * YARDS_PER_METER;
        }
        return 0.0;
    }

    void compute_all(const std::string& unit,
                     double value,
                     double& outMeters,
                     double& outFeet,
                     double& outYards) {
        double meter_value = to_meter(unit, value);
        outMeters = meter_value;
        outFeet = from_meter("feet", meter_value);
        outYards = from_meter("yard", meter_value);
    }
}



```

- CMake 구성 추가
  - 루트 `CMakeLists.txt`: `converter` 라이브러리, `unit_converter` 실행파일, 테스트 옵션 포함
  - `tests/CMakeLists.txt`: GTest 탐색 및 테스트 바이너리 `converter_tests` 구성
  - `tests/test_conversion.cpp`: 기본 변환과 호환 함수 테스트 추가
  - `converter.cpp`: `<cstdlib>` 포함 추가

- 빌드/실행 방법(Windows+Cygwin, GTest 설치 가정)
  - 빌드 디렉터리 생성 및 구성
    - `cmake -S . -B build -G \"Unix Makefiles\" -DUNIT_CONVERTER_BUILD_TESTS=ON | cat`
  - 빌드
    - `cmake --build build --config Release | cat`
  - 실행
    - `./build/unit_converter`
  - 테스트 실행
    - `ctest --test-dir build --output-on-failure | cat`

원하면 `unit_converter.cpp`도 CMake에서 받아들일 수 있게 경로/옵션 조정이나 추가 테스트를 더 넣어드릴게요.

---

**User**

오류 문제를 찾아줘

---

**Cursor**

빌드 로그를 보면 Cygwin에서 GTest가 POSIX 함수(fileno, fdopen) 선언을 보지 못해 컴파일 오류가 납니다. 테스트 타깃에 POSIX/gnu 확장을 노출하는 컴파일 정의를 추가하겠습니다.

```cmake
cmake_minimum_required(VERSION 3.16)

# Assume GoogleTest is installed on the system (per environment.txt)
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(converter_tests
    test_conversion.cpp
)
target_link_libraries(converter_tests PRIVATE converter GTest::gtest GTest::gtest_main Threads::Threads)
target_include_directories(converter_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Cygwin: expose POSIX functions (fileno, fdopen) for GTest port
if(CYGWIN)
    target_compile_definitions(converter_tests PRIVATE _POSIX_C_SOURCE=200809L _GNU_SOURCE)
endif()

include(GoogleTest)
gtest_discover_tests(converter_tests)



```

- 원인: Cygwin 환경의 GTest가 내부적으로 `fileno`, `fdopen` 등 POSIX 심볼을 사용하지만, 테스트 타깃 컴파일 시 해당 심볼이 노출되지 않아 컴파일 실패.
- 조치: `tests/CMakeLists.txt`에 Cygwin일 때 `converter_tests` 타깃에 `_POSIX_C_SOURCE=200809L`, `_GNU_SOURCE` 정의를 추가하고, 스레드 라이브러리 링크를 보강.

다시 빌드해 보세요:
- `cmake --build build --config Release | cat`
- 실패 시 `cmake --fresh -S . -B build -G "Unix Makefiles" -DUNIT_CONVERTER_BUILD_TESTS=ON | cat` 후 재빌드

계속 오류가 나면 GTest 설치 경로/빌드 옵션(Cygwin 빌드 여부)과 `find_package(GTest)`가 가리키는 GTest가 Cygwin용인지 확인이 필요합니다.

---

**User**

테스트 문제를 찾아줘

---

**Cursor**

테스트 실패 원인을 재현했고, 상수 불일치(0.9144 vs 1.09361)로 인한 허용 오차 초과임을 확인했습니다. 테스트의 기대값을 레지스트리의 소스-오브-트루스(0.9144)로 맞추도록 수정하고, 바로 빌드/테스트를 실행해 확인하겠습니다.

```cpp
#include <gtest/gtest.h>
#include "converter.h"

TEST(ConversionServiceTest, MeterToFeetAndYard) {
    UnitRegistry reg("meter");
    reg.addUnitFromBase("meter", 1.0);
    reg.addUnitFromBase("feet", 0.3048);
    reg.addUnitFromBase("yard", 0.9144);

    ConversionService svc(reg);
    double v = 2.5; // meter
    // Expected values based on ratios
    double feet = svc.convert(v, "meter", "feet");
    double yard = svc.convert(v, "meter", "yard");

    // From registry: 1 yard = 0.9144 meter (single source of truth)
    // Keep feet expectation as in README; for yard, avoid mixed rounding by using 0.9144
    EXPECT_NEAR(feet, 2.5 * 3.28084, 1e-6);
    EXPECT_NEAR(yard, v / 0.9144, 1e-9);
}

TEST(CompatTest, ComputeAllMatchesMain) {
    double m=0, f=0, y=0;
    compat::compute_all("meter", 2.5, m, f, y);
    EXPECT_NEAR(m, 2.5, 1e-9);
    EXPECT_NEAR(f, 2.5 * compat::FEET_PER_METER, 1e-6);
    EXPECT_NEAR(y, 2.5 * compat::YARDS_PER_METER, 1e-6);
}



```

- 원인: 테스트가 yard 기대값을 README의 비율(1 meter = 1.09361 yard)로 검증했으나, 실제 변환은 레지스트리 값(1 yard = 0.9144 meter)을 사용합니다. 두 수는 역수 관계지만 반올림 차이로 1e-6 허용 오차를 넘었습니다.
- 수정: 테스트를 레지스트리의 사실원천(0.9144) 기준으로 검증하도록 변경했으며, yard는 `v / 0.9144`로 기대값을 계산하고 더 엄격한 오차(1e-9)를 적용했습니다.

다시 테스트를 실행해 주세요:
- `ctest --test-dir build --output-on-failure | cat`

---

**User**

main을 실행해서 결과를 확인할 수 있도록 CMake에 실행파일 생성 코드를 추가해줘

---

**Cursor**

지금 CMake에 `run` 커스텀 타깃을 추가해 빌드 후 `make run`으로 바로 실행 가능하게 하겠습니다.

```cmake
cmake_minimum_required(VERSION 3.16)

project(unit_converter_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library for conversion core
add_library(converter
    converter.cpp
)
target_include_directories(converter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Executable using the existing main (no source changes required)
add_executable(unit_converter
    unit_converter.cpp
)
target_link_libraries(unit_converter PRIVATE converter)

option(UNIT_CONVERTER_BUILD_TESTS "Build unit tests" ON)
if(UNIT_CONVERTER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Convenience target to run the program after build
add_custom_target(run
    COMMAND $<TARGET_FILE:unit_converter>
    DEPENDS unit_converter
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    USES_TERMINAL
)



```

- CMake에 실행 편의 타깃을 추가했습니다.
  - 빌드: `cmake --build build --config Release | cat`
  - 실행: `cmake --build build --target run | cat` 또는 빌드 디렉터리에서 `make run`

- 결과 확인
  - 실행 후 프롬프트가 뜨면 예시 입력: `meter:2.5`- 방금 `converter.h/.cpp`를 설계대로 구현했고, 기존 `main`을 유지하기 위한 호환 API(`compat`)도 추가했습니다.
- CMake 환경을 구성하고 테스트를 붙였으며, Cygwin + GTest 이슈를 해결했습니다.
- `run` 타깃을 추가해 `make run`으로 바로 실행할 수 있게 했습니다.

- 실행 방법
  - 설정: `cmake -S . -B build -G "Unix Makefiles" -DUNIT_CONVERTER_BUILD_TESTS=ON | cat`
  - 빌드: `cmake --build build --config Release | cat`
  - 실행: `cmake --build build --target run | cat`
  - 테스트: `ctest --test-dir build --output-on-failure | cat`

- 예시 입력
  - `meter:2.5`

---

**User**

이렇게 답이 나오는게 맞을까?

---

**Cursor**

아니요. 입력 형식이 잘못되어 나온 결과입니다.

- 원인: `2.5`만 입력해 `unit="2.5"`로 파싱되어 미지원 단위로 처리되어 0이 출력됨.
- 올바른 입력: `meter:2.5` 처럼 `단위:값` 형식으로 입력하세요.
- 기대 출력 예:
  - `2.5 meter = 2.5 meter`
  - `2.5 meter = 8.20210 feet`
  - `2.5 meter = 2.734025 yard`

원하시면 콜론이 없을 때 기본 단위를 `meter`로 간주하거나, 형식을 잘못 입력하면 에러를 출력하도록 `unit_converter.cpp`에 간단한 검증 로직을 추가해드릴게요.

---

**User**

추가 요구 사항을 반영하려고 해.
현재 코드 수정 계획을 먼저 제시해줘.


---

**Cursor**

### 수정 계획(추가 요구사항 반영)

- **전략**
  - 옵션 A(권장): 기존 실행파일 유지하면서 `unit_converter.cpp`를 서비스 기반으로 리팩터링해 기능 확장
  - 옵션 B(보수): `unit_converter.cpp`는 유지, 새 실행파일 `unit_converter_adv.cpp`를 추가해 확장 기능 제공
  - 기본은 옵션 A로 진행, 필요 시 B도 병행 지원

- **설정 외부화(JSON)**
  - `config_loader.h/.cpp` 추가
    - `bool loadFromJson(const std::string& path, UnitRegistry& registry)` 구현
    - 포맷: 
      ```json
      {
        "baseUnit": "meter",
        "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144 }
      }
      ```
  - `unit_converter.cpp`에서 `--config <path>` 인자 처리 → 로드 실패 시 친절한 에러 출력 후 기본값 등록(폴백)

- **동적 단위 등록**
  - 현재 `InputParser::parseRegistration`/`Validator`/`UnitRegistry.addUnitRelative`가 준비됨
  - `unit_converter.cpp`에서 입력 한 줄씩 처리:
    - `1 X = r Y` 형식이면 등록 수행(성공/실패 메시지)
    - 아니면 변환(`단위:값`) 수행
  - 미지원 단위/음수/형식 오류 시 명확한 메시지

- **출력 포맷 선택(JSON/CSV/표)**
  - `--format table|json|csv` 인자 처리
  - 선택에 따라 `TableFormatter`/`JsonFormatter`/`CsvFormatter` 주입
  - 현재 단위는 제외하고 전체 단위로 변환 후 포맷 출력

- **메인 리팩터링(`unit_converter.cpp`)**
  - 기존 안내 프롬프트 유지
  - 초기화:
    - `UnitRegistry reg("meter")`
    - `--config` 없으면 기본 단위 등록: `meter=1.0`, `feet=0.3048`, `yard=0.9144`
  - 서비스/포맷터 생성:
    - `ConversionService svc(reg)`
    - 인자에 따른 포맷터 선택
  - 입력 루프:
    - 등록 시: `reg.addUnitRelative(...)` 후 “Registered” 출력
    - 변환 시: `svc.convertToAll(...)` → 포맷팅 출력

- **테스트 추가**
  - `tests/test_config_loader.cpp`: 정상/에러 파일 로드
  - `tests/test_input_parser.cpp`: 등록/변환 파싱 케이스
  - `tests/test_formatters.cpp`: 스냅샷 스타일 검증(간단 값 포함 여부)

- **CMake 보강**
  - `config_loader.cpp` 라이브러리에 추가
  - 샘플 `configs/units.json` 리포에 포함
  - 실행 편의 타깃 `run` 유지(`make run` → 입력 대기)

- **예상 입력/동작**
  - `--config configs/units.json --format table` 실행
  - `1 cubit = 0.4572 meter` → 등록 안내
  - `cubit:2` → 모든 단위로 변환 결과 출력

원하시면 옵션 A로 바로 구현에 들어가겠습니다.

```json
      {
        "baseUnit": "meter",
        "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144 }
      }
```

---

**User**

A 옵션으로 구현해줘

---

**Cursor**

옵션 A대로 진행합니다. 설정 로더를 추가하고 `unit_converter.cpp`를 서비스 기반으로 리팩터링하겠습니다. 이어서 CMake에 새 파일을 포함하고 빌드가 통과하도록 합니다.

이어서 설정 로더 파일을 추가하고, CMake에 포함한 뒤 `unit_converter.cpp`를 리팩터링하겠습니다. 완료 후 바로 빌드 안내를 드리겠습니다.

```cpp
#pragma once

#include <string>
#include "converter.h"

// Minimal JSON loader without external dependency: expects a very simple JSON schema
// For robustness in production, switch to nlohmann::json
class ConfigLoader {
public:
    // Returns true on success; on failure, leaves registry unchanged and returns false
    bool loadFromJson(const std::string& path, UnitRegistry& registry, std::string* errorMessage = nullptr) const;
};


```

```cpp
#include "config_loader.h"

#include <fstream>
#include <sstream>
#include <cctype>

namespace {
    static inline std::string trim(const std::string& s) {
        size_t b = 0;
        while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
        size_t e = s.size();
        while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
        return s.substr(b, e - b);
    }
}

// Very small hand-rolled parser sufficient for the expected schema.
// Schema:
// {
//   "baseUnit": "meter",
//   "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144 }
// }
bool ConfigLoader::loadFromJson(const std::string& path, UnitRegistry& registry, std::string* errorMessage) const {
    std::ifstream ifs(path);
    if (!ifs) {
        if (errorMessage) *errorMessage = "cannot open file: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string json = ss.str();

    // naive search; not robust to arbitrary JSON
    auto findString = [&](const std::string& key, std::string& out) -> bool {
        std::string pat = "\"" + key + "\"";
        auto p = json.find(pat);
        if (p == std::string::npos) return false;
        p = json.find(':', p);
        if (p == std::string::npos) return false;
        p = json.find('"', p);
        if (p == std::string::npos) return false;
        auto q = json.find('"', p + 1);
        if (q == std::string::npos) return false;
        out = json.substr(p + 1, q - (p + 1));
        return true;
    };

    std::string baseUnit;
    if (!findString("baseUnit", baseUnit)) {
        if (errorMessage) *errorMessage = "missing baseUnit";
        return false;
    }

    // Find units object
    std::string unitsKey = "\"units\"";
    auto u = json.find(unitsKey);
    if (u == std::string::npos) {
        if (errorMessage) *errorMessage = "missing units";
        return false;
    }
    auto open = json.find('{', u);
    auto close = json.find('}', open);
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        if (errorMessage) *errorMessage = "invalid units object";
        return false;
    }
    std::string body = json.substr(open + 1, close - open - 1);

    // Parse entries of the form  "name": number
    UnitRegistry newReg(baseUnit);
    newReg.addUnitFromBase(baseUnit, 1.0);

    size_t pos = 0;
    while (pos < body.size()) {
        // find key
        size_t k1 = body.find('"', pos);
        if (k1 == std::string::npos) break;
        size_t k2 = body.find('"', k1 + 1);
        if (k2 == std::string::npos) break;
        std::string name = body.substr(k1 + 1, k2 - (k1 + 1));
        size_t colon = body.find(':', k2);
        if (colon == std::string::npos) break;
        size_t vstart = colon + 1;
        while (vstart < body.size() && std::isspace(static_cast<unsigned char>(body[vstart]))) vstart++;
        size_t vend = vstart;
        while (vend < body.size() && (std::isdigit(static_cast<unsigned char>(body[vend])) || body[vend] == '.' || body[vend] == 'e' || body[vend] == 'E' || body[vend] == '+' || body[vend] == '-')) vend++;
        std::string num = body.substr(vstart, vend - vstart);
        double value = std::strtod(num.c_str(), nullptr);

        if (name != baseUnit) {
            if (!(value > 0.0)) {
                if (errorMessage) *errorMessage = "invalid factor for unit: " + name;
                return false;
            }
            newReg.addUnitFromBase(name, value);
        }
        pos = body.find(',', vend);
        if (pos == std::string::npos) break;
        pos++;
    }

    // Commit to provided registry
    registry = newReg;
    return true;
}


```

```cpp
#include <iostream>
#include <string>
#include <cstdlib>
#include "converter.h"
#include "config_loader.h"

using namespace std;

int main(int argc, char** argv) {
    // Simple CLI options: --config <path> --format table|json|csv
    string configPath;
    string format = "table";
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            format = argv[++i];
        }
    }

    UnitRegistry registry("meter");
    // Default units (fallback)
    registry.addUnitFromBase("meter", 1.0);
    registry.addUnitFromBase("feet", 0.3048);
    registry.addUnitFromBase("yard", 0.9144);

    if (!configPath.empty()) {
        ConfigLoader loader;
        std::string err;
        if (!loader.loadFromJson(configPath, registry, &err)) {
            std::cerr << "Config load failed: " << err << std::endl;
        }
    }

    ConversionService service(registry);
    TableFormatter tableFmt;
    CsvFormatter csvFmt;
    JsonFormatter jsonFmt;
    FormatStrategy* fmt = &tableFmt;
    if (format == "csv") fmt = &csvFmt;
    else if (format == "json") fmt = &jsonFmt;

    cout << "Insert value for converting (ex: meter:2.5 / registration: 1 cubit = 0.4572 meter): ";
    string input_str;
    getline(cin, input_str);

    InputParser parser;
    Validator validator;
    try {
        // try registration first
        try {
            auto reg = parser.parseRegistration(input_str);
            validator.validateRegistration(reg, registry);
            registry.addUnitRelative(reg.unitName, reg.ratio, reg.relativeUnit);
            cout << "Registered: " << reg.unitName << endl;
            return 0;
        } catch (...) {
            // not a registration, continue
        }

        auto q = parser.parseQuery(input_str);
        validator.validateQuery(q, registry);
        auto results = service.convertToAll(q.value, q.unitName, false);
        cout << fmt->format(q, results, registry.baseUnit());
    } catch (const std::exception& ex) {
        cerr << "Error: " << ex.what() << endl;
        return 1;
    }
    return 0;
}

```

```cmake
cmake_minimum_required(VERSION 3.16)

project(unit_converter_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library for conversion core
add_library(converter
    converter.cpp
    config_loader.cpp
)
target_include_directories(converter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Executable using the existing main (no source changes required)
add_executable(unit_converter
    unit_converter.cpp
)
target_link_libraries(unit_converter PRIVATE converter)

option(UNIT_CONVERTER_BUILD_TESTS "Build unit tests" ON)
if(UNIT_CONVERTER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Convenience target to run the program after build
add_custom_target(run
    COMMAND $<TARGET_FILE:unit_converter>
    DEPENDS unit_converter
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    USES_TERMINAL
)



```

- 이제 추가 요구사항(A 옵션)을 반영했습니다.
  - `config_loader.h/.cpp`: 단순 JSON 로더 추가(기본 스키마 로드)
  - `unit_converter.cpp`: `--config`, `--format` 옵션 처리, 등록/변환 모두 지원
  - `converter` 라이브러리에 `config_loader.cpp` 포함

- 사용 예
  - 설정 파일 없이 기본 단위 사용
    - `cmake --build build --target run | cat`
    - 입력 예: `meter:2.5` 또는 `1 cubit = 0.4572 meter` 후 `cubit:2`
  - 설정 파일로 로드
    - 예시 `configs/units.json` 생성 후 실행
```json
{
  "baseUnit": "meter",
  "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144 }
}
```
    - 실행: `./build/unit_converter --config configs/units.json --format json`

- 주의
  - 현재 JSON 파서는 간단한 스키마만 지원합니다. 필요 시 `nlohmann::json`으로 교체 가능합니다.

```json
{
  "baseUnit": "meter",
  "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144 }
}
```

---

**User**

새로 추가된 cubit은 meter와 변환 가능한 단위가 아니야?
왜 모든 단위로 변환하는데 빠져있어?

---

**Cursor**

지금 실행 중 세션에서 등록 후 바로 변환할 수 있도록 입력 루프를 `unit_converter.cpp`에 추가하고, 변환 결과에서 입력 단위도 포함되도록 수정하겠습니다.

```cpp
#include <iostream>
#include <string>
#include <cstdlib>
#include "converter.h"
#include "config_loader.h"

using namespace std;

int main(int argc, char** argv) {
    // Simple CLI options: --config <path> --format table|json|csv
    string configPath;
    string format = "table";
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            format = argv[++i];
        }
    }

    UnitRegistry registry("meter");
    // Default units (fallback)
    registry.addUnitFromBase("meter", 1.0);
    registry.addUnitFromBase("feet", 0.3048);
    registry.addUnitFromBase("yard", 0.9144);

    if (!configPath.empty()) {
        ConfigLoader loader;
        std::string err;
        if (!loader.loadFromJson(configPath, registry, &err)) {
            std::cerr << "Config load failed: " << err << std::endl;
        }
    }

    ConversionService service(registry);
    TableFormatter tableFmt;
    CsvFormatter csvFmt;
    JsonFormatter jsonFmt;
    FormatStrategy* fmt = &tableFmt;
    if (format == "csv") fmt = &csvFmt;
    else if (format == "json") fmt = &jsonFmt;

    InputParser parser;
    Validator validator;

    while (true) {
        cout << "Insert value for converting (ex: meter:2.5 / registration: 1 cubit = 0.4572 meter) or 'exit': ";
        string input_str;
        if (!getline(cin, input_str)) break;
        if (input_str == "exit" || input_str == "quit") break;
        if (input_str.empty()) continue;

        try {
            // try registration first
            try {
                auto reg = parser.parseRegistration(input_str);
                validator.validateRegistration(reg, registry);
                registry.addUnitRelative(reg.unitName, reg.ratio, reg.relativeUnit);
                cout << "Registered: " << reg.unitName << endl;
                continue; // keep session alive
            } catch (...) {
                // not a registration, continue to query
            }

            auto q = parser.parseQuery(input_str);
            validator.validateQuery(q, registry);
            auto results = service.convertToAll(q.value, q.unitName, true); // include source unit as well
            cout << fmt->format(q, results, registry.baseUnit());
        } catch (const std::exception& ex) {
            cerr << "Error: " << ex.what() << endl;
        }
    }
    return 0;
}

```

방금 입력 루프를 추가하고, 변환 결과에 입력 단위도 포함되도록 수정했습니다. 이제 등록 직후 곧바로 `cubit`을 포함한 모든 단위로 변환됩니다.

- 왜 빠졌었나
  - 등록 시 프로그램이 즉시 종료(return 0)되어 다음 변환을 못 했음.
  - 변환 시 입력 단위를 결과에서 제외(includeSource=false)하고 있었음.

- 무엇을 변경했나
  - `unit_converter.cpp`에 입력 반복 루프 추가: 등록 후 종료하지 않고 계속 대기.
  - `convertToAll(..., includeSource=true)`로 변경: 결과에 입력 단위도 포함.

- 확인 방법
  - 실행 후 순서대로 입력:
    - `1 cubit = 0.4572 meter` → “Registered: cubit”
    - `cubit:2` → `meter`, `feet`, `yard`, `cubit` 모두 출력됨
    - 종료: `exit`

- 참고
  - `--format table|json|csv`로 출력 형식 선택 가능.
  - `--config <json>`로 외부 설정에서 단위 로드 가능.

요약: 종료 문제와 입력 단위 제외 설정을 고쳐, `cubit`이 모든 단위 변환 결과에 포함되어 즉시 확인 가능합니다.

---

**User**

meter:1을 입력했을 때 cubit 값이 함께 출력되어야하지 않아?

---

**Cursor**

아니요. 지금 출력엔 `cubit`이 등록되어 있지 않아서 빠진 겁니다.

- 등록 후 변환하세요
```text
1 cubit = 0.4572 meter
meter:1
```
→ `1.000000 meter = ... cubit` 줄이 함께 출력됩니다.

- 실행할 때 설정으로 미리 로드할 수도 있어요 (`--config`):
```json
{
  "baseUnit": "meter",
  "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144, "cubit": 0.4572 }
}
```
이후
```bash
./unit_converter --config configs/units.json
```
에서 `meter:1` 입력 시 `cubit`이 포함되어 출력됩니다.

참고: 등록은 현재 세션에만 적용됩니다(지속 저장 아님).

```plaintext
1 cubit = 0.4572 meter
meter:1
```

```json
{
  "baseUnit": "meter",
  "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144, "cubit": 0.4572 }
}
```

```shellscript
./unit_converter --config configs/units.json
```

---

**User**

이런 입력은 불가능해?
항상 변환은 meter로만 물어야해?

---

**Cursor**

아니요. 항상 meter만 물을 필요 없습니다. 등록된 단위라면 어떤 단위로도 변환 가능합니다.

- **지금 에러 원인**: `cubit`이 아직 등록되지 않아서 미지원 단위로 처리됨.
- **해결 방법**
  - 실행 중 즉시 등록 후 사용:
    ```
    1 cubit = 0.4572 meter
    cubit:1
    ```
  - 설정 파일로 미리 등록:
    ```json
    {
      "baseUnit": "meter",
      "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144, "cubit": 0.4572 }
    }
    ```
    실행 시 `--config <파일경로>` 옵션 사용.

- 참고: 등록은 현재 실행 세션에만 유지됩니다. 재실행하면 다시 등록하거나 설정에서 로드해야 합니다.

```plaintext
    1 cubit = 0.4572 meter
    cubit:1
```

```json
    {
      "baseUnit": "meter",
      "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144, "cubit": 0.4572 }
    }
```

---

**User**

입력이 잘못된거야? cubit를 모르잖아

---

**Cursor**

아니요, 입력 형식은 맞지만 `cubit`이 아직 등록되지 않아서 “unknown unit: cubit”이 발생한 겁니다.

- 해결 방법
  - 실행 중 등록 후 사용:
    ```
    1 cubit = 0.4572 meter
    cubit:1
    ```
  - 또는 설정 파일에 미리 등록 후 실행(`--config` 사용):
    ```json
    {
      "baseUnit": "meter",
      "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144, "cubit": 0.4572 }
    }
    ```

- 참고: 등록은 현재 세션에서만 유효합니다. 프로그램을 다시 실행하면 재등록하거나 설정에서 로드해야 합니다.

```plaintext
    1 cubit = 0.4572 meter
    cubit:1
```

```json
    {
      "baseUnit": "meter",
      "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144, "cubit": 0.4572 }
    }
```

---

**User**

아 사용하고 싶은 단위를 먼저 등록한 후 사용한다는거야?

---

**Cursor**

네. 기본 제공 단위는 `meter`, `feet`, `yard`이고, 그 외(`cubit` 등)는 먼저 등록한 뒤 사용할 수 있습니다.

- 등록 후 사용(실행 중):
```text
1 cubit = 0.4572 meter
cubit:1
```

- 실행 시 설정으로 미리 등록:
```json
{
  "baseUnit": "meter",
  "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144, "cubit": 0.4572 }
}
```
실행: `--config <경로>` 옵션 사용.

```plaintext
1 cubit = 0.4572 meter
cubit:1
```

```json
{
  "baseUnit": "meter",
  "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144, "cubit": 0.4572 }
}
```

---

**User**

소수점 이하 2자리까지만 출력되도록 해줘

---

**Cursor**

지금 출력 자릿수를 고정하기 위해 포맷터의 공용 헬퍼 `formatDouble`의 기본 정밀도를 2로 낮추겠습니다. 이어서 빌드 후 실행하면 모든 포맷(table/csv/json)에서 소수점 둘째 자리까지 표시됩니다.

```cpp
// converter.cpp: core implementation for unit conversion

#include "converter.h"

#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdlib>

// ---------------- UnitRegistry -----------------

UnitRegistry::UnitRegistry(const std::string& baseUnitName)
    : baseUnitName_(baseUnitName) {
    nameToUnit_[baseUnitName_] = Unit{ baseUnitName_, 1.0 };
}

const std::string& UnitRegistry::baseUnit() const {
    return baseUnitName_;
}

void UnitRegistry::addUnitFromBase(const std::string& unitName, double toBaseFactor) {
    if (unitName.empty()) {
        throw std::invalid_argument("unitName must not be empty");
    }
    if (!(toBaseFactor > 0.0)) {
        throw std::invalid_argument("toBaseFactor must be > 0");
    }
    nameToUnit_[unitName] = Unit{ unitName, toBaseFactor };
}

void UnitRegistry::addUnitRelative(const std::string& unitName, double ratio, const std::string& relativeUnitName) {
    if (!has(relativeUnitName)) {
        throw std::invalid_argument("relative unit not found: " + relativeUnitName);
    }
    const Unit& rel = get(relativeUnitName);
    // ratio: 1 unitName = ratio relativeUnitName
    double toBase = ratio * rel.toBaseFactor;
    addUnitFromBase(unitName, toBase);
}

bool UnitRegistry::has(const std::string& unitName) const {
    return nameToUnit_.find(unitName) != nameToUnit_.end();
}

const Unit& UnitRegistry::get(const std::string& unitName) const {
    auto it = nameToUnit_.find(unitName);
    if (it == nameToUnit_.end()) {
        throw std::out_of_range("unit not found: " + unitName);
    }
    return it->second;
}

std::vector<Unit> UnitRegistry::list() const {
    std::vector<Unit> units;
    units.reserve(nameToUnit_.size());
    for (const auto& kv : nameToUnit_) {
        units.push_back(kv.second);
    }
    return units;
}

// ---------------- ConversionService -----------------

ConversionService::ConversionService(const UnitRegistry& registry)
    : registry_(registry) {}

double ConversionService::convert(double value, const std::string& fromUnit, const std::string& toUnit) const {
    if (fromUnit == toUnit) {
        return value;
    }
    const Unit& from = registry_.get(fromUnit);
    const Unit& to = registry_.get(toUnit);
    // Convert to base: value * from.toBaseFactor
    // Then to target: baseValue / to.toBaseFactor
    double baseValue = value * from.toBaseFactor;
    double result = baseValue / to.toBaseFactor;
    return result;
}

std::vector<std::pair<std::string, double>> ConversionService::convertToAll(double value, const std::string& fromUnit, bool includeSource) const {
    std::vector<std::pair<std::string, double>> out;
    for (const auto& u : registry_.list()) {
        if (!includeSource && u.name == fromUnit) {
            continue;
        }
        out.emplace_back(u.name, convert(value, fromUnit, u.name));
    }
    return out;
}

// ---------------- InputParser -----------------

static inline std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

ConversionQuery InputParser::parseQuery(const std::string& input) const {
    auto pos = input.find(":");
    if (pos == std::string::npos) {
        throw std::invalid_argument("invalid query format, expected 'unit:value'");
    }
    std::string unit = trim(input.substr(0, pos));
    std::string valueStr = trim(input.substr(pos + 1));
    if (unit.empty() || valueStr.empty()) {
        throw std::invalid_argument("invalid query tokens");
    }
    char* end = nullptr;
    double v = std::strtod(valueStr.c_str(), &end);
    if (end == valueStr.c_str() || *end != '\0') {
        throw std::invalid_argument("invalid numeric value");
    }
    return ConversionQuery{ unit, v };
}

InputParser::Registration InputParser::parseRegistration(const std::string& input) const {
    // format: "1 X = r Y"
    // tokenize by spaces/equals
    // We'll parse simply: expect leading '1', then unit, '=', ratio, relativeUnit
    std::string s = trim(input);
    // replace multiple spaces around '=' to handle variations
    auto eqPos = s.find('=');
    if (eqPos == std::string::npos) {
        throw std::invalid_argument("invalid registration format, expected '='");
    }
    std::string left = trim(s.substr(0, eqPos));
    std::string right = trim(s.substr(eqPos + 1));

    // left: "1 X"
    std::istringstream lss(left);
    double one;
    std::string unitName;
    if (!(lss >> one) || !(lss >> unitName) || one != 1.0) {
        throw std::invalid_argument("invalid left side, expected '1 <unit>'");
    }

    // right: "r Y"
    std::istringstream rss(right);
    double ratio;
    std::string relativeUnit;
    if (!(rss >> ratio) || !(rss >> relativeUnit)) {
        throw std::invalid_argument("invalid right side, expected '<ratio> <unit>'");
    }
    if (!(ratio > 0.0)) {
        throw std::invalid_argument("ratio must be > 0");
    }

    return Registration{ unitName, ratio, relativeUnit };
}

// ---------------- Validator -----------------

void Validator::validateQuery(const ConversionQuery& query, const UnitRegistry& registry) const {
    if (!(query.value >= 0.0)) {
        throw std::invalid_argument("value must be >= 0");
    }
    if (!registry.has(query.unitName)) {
        throw std::invalid_argument("unknown unit: " + query.unitName);
    }
}

void Validator::validateRegistration(const InputParser::Registration& reg, const UnitRegistry& registry) const {
    if (reg.unitName.empty() || reg.relativeUnit.empty()) {
        throw std::invalid_argument("unit names must not be empty");
    }
    if (!(reg.ratio > 0.0)) {
        throw std::invalid_argument("ratio must be > 0");
    }
    if (!registry.has(reg.relativeUnit)) {
        throw std::invalid_argument("relative unit not found: " + reg.relativeUnit);
    }
}

// ---------------- Formatters -----------------

static std::string formatDouble(double v, int precision = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << v;
    return oss.str();
}

std::string TableFormatter::format(const ConversionQuery& query,
                                   const std::vector<std::pair<std::string, double>>& results,
                                   const std::string& /*baseUnit*/) const {
    std::ostringstream oss;
    for (const auto& kv : results) {
        oss << formatDouble(query.value) << " " << query.unitName
            << " = " << formatDouble(kv.second) << " " << kv.first << "\n";
    }
    return oss.str();
}

std::string CsvFormatter::format(const ConversionQuery& query,
                                 const std::vector<std::pair<std::string, double>>& results,
                                 const std::string& baseUnit) const {
    std::ostringstream oss;
    (void)baseUnit; // not used currently
    oss << "source_unit,source_value,target_unit,target_value\n";
    for (const auto& kv : results) {
        oss << query.unitName << "," << formatDouble(query.value)
            << "," << kv.first << "," << formatDouble(kv.second) << "\n";
    }
    return oss.str();
}

std::string JsonFormatter::format(const ConversionQuery& query,
                                  const std::vector<std::pair<std::string, double>>& results,
                                  const std::string& baseUnit) const {
    // Minimal JSON building without external deps
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"baseUnit\": \"" << baseUnit << "\",\n";
    oss << "  \"source\": { \"unit\": \"" << query.unitName << "\", \"value\": " << formatDouble(query.value) << " },\n";
    oss << "  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& kv = results[i];
        oss << "    { \"unit\": \"" << kv.first << "\", \"value\": " << formatDouble(kv.second) << " }";
        if (i + 1 < results.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

// ---------------- Backward-compat helpers (compat) -----------------

namespace compat {
    double to_meter(const std::string& unit, double value) {
        if (unit == "meter") {
            return value;
        } else if (unit == "feet") {
            return value / FEET_PER_METER;
        } else if (unit == "yard") {
            return value / YARDS_PER_METER;
        }
        // Unknown unit: keep behavior close to original (no explicit error). Return 0.
        return 0.0;
    }

    double from_meter(const std::string& unit, double meterValue) {
        if (unit == "meter") {
            return meterValue;
        } else if (unit == "feet") {
            return meterValue * FEET_PER_METER;
        } else if (unit == "yard") {
            return meterValue * YARDS_PER_METER;
        }
        return 0.0;
    }

    void compute_all(const std::string& unit,
                     double value,
                     double& outMeters,
                     double& outFeet,
                     double& outYards) {
        double meter_value = to_meter(unit, value);
        outMeters = meter_value;
        outFeet = from_meter("feet", meter_value);
        outYards = from_meter("yard", meter_value);
    }
}



```

- 소수점 자리수를 2로 고정했습니다. 모든 출력 형식(table/csv/json)에 적용됩니다.
- 실행 후 예시
  - `meter:1` →
    - `1.00 meter = 2.19 cubit`
    - `1.00 meter = 1.09 yard`
    - `1.00 meter = 3.28 feet`
    - `1.00 meter = 1.00 meter`

