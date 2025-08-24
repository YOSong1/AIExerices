# OCP의 의미 질문
_Exported on 8/24/2025 at 00:49:50 GMT+9 from Cursor (1.5.4)_

---

**User**

OCP 뜻은?

---

**Cursor**

**OCP (Open-Closed Principle, 개방-폐쇄 원칙)**: 소프트웨어 요소는 확장에는 열려 있고 변경에는 닫혀 있어야 한다는 설계 원칙입니다.

---

**User**

이 내용은 어떻게 의미일까? 

---

**Cursor**

잠깐만요, README 주변 문맥과 핵심 소스 파일들을 열어 실제로 어떤 방식으로 OCP를 충족시키는지 확인해볼게요.

README의 해당 항목이 실제 코드에서 어떻게 OCP를 충족하는지 확인하기 위해 핵심 소스 파일들을 빠르게 열어볼게요.

```plaintext
class FormatStrategy {...
...
}
class TableFormatter : public FormatStrategy { ... }
```

코드를 확인했습니다. README의 “OCP를 만족하는 인터페이스 구현”이 프로젝트에서 어떻게 쓰였는지 핵심만 정리해 드릴게요.

### 이 문장의 의미
- **핵심 의미**: 기능을 “추가”할 때는 새 코드를 “추가”해서 확장하고, 기존 코드는 “수정하지 않도록” 인터페이스(추상화)로 확장점을 만들어 두라는 뜻입니다.

### 이 프로젝트에서의 적용 포인트
- **출력 포맷 확장(OCP)**: 출력은 `FormatStrategy` 인터페이스로 추상화되어 있어, 새 포맷을 추가하려면 새 클래스를 “추가”만 하면 됩니다.
```79:92:src/converter.h
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
```
  - 예: `YamlFormatter`를 새로 만들어 붙이면 기존 변환 로직 수정 없이 포맷 기능이 확장됩니다.

- **단위 추가 확장(OCP)**: 변환은 `UnitRegistry`에 등록된 단위 집합을 기반으로 동작합니다. 새 단위를 “등록”만 하면 `ConversionService`는 수정 없이 그대로 동작합니다.
```26:33:src/converter.h
// Register a unit by specifying how many base units are in 1 unit
// Example: addUnitFromBase("feet", 0.3048) means 1 feet = 0.3048 meter
void addUnitFromBase(const std::string& unitName, double toBaseFactor);

// Register a unit relative to an existing unit
// Example: addUnitRelative("cubit", 0.4572, "meter") means 1 cubit = 0.4572 meter
void addUnitRelative(const std::string& unitName, double ratio, const std::string& relativeUnitName);
```
  - JSON 설정(`ConfigLoader`)이나 런타임 명령으로 단위를 등록하면 기존 코드 변경 없이 확장됩니다.

### 한 줄 요약
- “OCP를 만족하는 인터페이스 구현”이란, 이 프로젝트에서는 출력 포맷은 `FormatStrategy`, 단위 추가는 `UnitRegistry`/설정으로 확장되게 만들어 “수정 없이 확장”할 수 있게 해두라는 뜻입니다.

```plaintext
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
```

```plaintext
// Register a unit by specifying how many base units are in 1 unit
// Example: addUnitFromBase("feet", 0.3048) means 1 feet = 0.3048 meter
void addUnitFromBase(const std::string& unitName, double toBaseFactor);

// Register a unit relative to an existing unit
// Example: addUnitRelative("cubit", 0.4572, "meter") means 1 cubit = 0.4572 meter
void addUnitRelative(const std::string& unitName, double ratio, const std::string& relativeUnitName);
```

