#ifndef SHEALTH_H
#define SHEALTH_H

#include <string>
#include <vector>
#include <map>
#include <array>
#include <memory>
#include <istream>

// BMI 분류 타입
enum class BmiType {
    Underweight = 100,
    Normal = 200,
    Overweight = 300,
    Obesity = 400
};

// 연령대 상수
constexpr std::array<int, 6> AGE_GROUPS = {20, 30, 40, 50, 60, 70};
constexpr int MIN_AGE_GROUP = 20;
constexpr int MAX_AGE_GROUP = 70;
constexpr int AGE_GROUP_SPAN = 10;

struct BmiRatios {
    double underweight;
    double normal;
    double overweight;
    double obesity;
    
    BmiRatios() : underweight(0.0), normal(0.0), overweight(0.0), obesity(0.0) {}
};

struct HealthRecord {
    int age;
    double weight;
    double height;
    double bmi;
    
    HealthRecord() : age(0), weight(0.0), height(0.0), bmi(0.0) {}
    HealthRecord(int a, double w, double h) : age(a), weight(w), height(h), bmi(0.0) {}
};

// =============================================================================
// 전략 인터페이스들 (Strategy Interfaces)
// =============================================================================

// 연령 버킷 계산 전략
class AgeBucketer {
public:
    virtual ~AgeBucketer() = default;
    virtual int GetAgeBucket(int age) const = 0;
};

// BMI 분류 전략
class BmiClassifier {
public:
    virtual ~BmiClassifier() = default;
    virtual BmiType ClassifyBmi(double bmi) const = 0;
    virtual double CalculateBmi(double weight, double height) const = 0;
};

// 결측치 보간 전략
class WeightImputer {
public:
    virtual ~WeightImputer() = default;
    virtual void ImputeMissingWeights(std::vector<HealthRecord>& records, const AgeBucketer& bucketer) const = 0;
};

// 데이터 로더 인터페이스
class HealthDataLoader {
public:
    virtual ~HealthDataLoader() = default;
    virtual std::vector<HealthRecord> LoadHealthData(const std::string& source) const = 0;
    virtual std::vector<HealthRecord> LoadHealthData(std::istream& input) const = 0;
};

// =============================================================================
// 기본 구현체들 (Default Implementations)
// =============================================================================

class StandardAgeBucketer : public AgeBucketer {
public:
    int GetAgeBucket(int age) const override;
};

class StandardBmiClassifier : public BmiClassifier {
public:
    BmiType ClassifyBmi(double bmi) const override;
    double CalculateBmi(double weight, double height) const override;
};

class AverageWeightImputer : public WeightImputer {
public:
    void ImputeMissingWeights(std::vector<HealthRecord>& records, const AgeBucketer& bucketer) const override;
};

class CsvHealthDataLoader : public HealthDataLoader {
public:
    std::vector<HealthRecord> LoadHealthData(const std::string& filename) const override;
    std::vector<HealthRecord> LoadHealthData(std::istream& input) const override;
};

// =============================================================================
// 핵심 계산 클래스 (Core Computation)
// =============================================================================

class SHealthCore {
private:
    std::unique_ptr<AgeBucketer> ageBucketer;
    std::unique_ptr<BmiClassifier> bmiClassifier;
    std::unique_ptr<WeightImputer> weightImputer;
    
    std::map<int, BmiRatios> ageGroupRatios;

public:
    // 생성자 - 기본 전략들 사용
    SHealthCore();
    
    // 생성자 - 커스텀 전략들 주입
    SHealthCore(std::unique_ptr<AgeBucketer> bucketer,
                std::unique_ptr<BmiClassifier> classifier,
                std::unique_ptr<WeightImputer> imputer);
    
    // 핵심 계산 메서드
    void ProcessHealthData(std::vector<HealthRecord>& records);
    double GetBmiRatio(int age_class, BmiType type) const;
    double GetBmiRatio(int age_class, int type) const;  // 기존 호환성 유지
    
    // 테스트 지원 메서드
    const std::map<int, BmiRatios>& GetAgeGroupRatios() const { return ageGroupRatios; }
};

// =============================================================================
// 메인 파사드 클래스 (Main Facade)
// =============================================================================

class SHealth {
private:
    std::unique_ptr<HealthDataLoader> dataLoader;
    std::unique_ptr<SHealthCore> core;

public:
    // 생성자 - 기본 구현체들 사용
    SHealth();
    
    // 생성자 - 커스텀 구현체들 주입
    SHealth(std::unique_ptr<HealthDataLoader> loader,
            std::unique_ptr<SHealthCore> healthCore);
    
    // 기존 API 호환성 유지
    int CalculateBmi(const std::string& filename);
    
    // 새로운 스트림 기반 API
    int CalculateBmi(std::istream& input);
    
    // 데이터 직접 주입 API (테스트 친화적)
    int ProcessHealthRecords(std::vector<HealthRecord> records);
    
    // 결과 조회 API
    double GetBmiRatio(int age_class, int type) const;
    double GetBmiRatio(int age_class, BmiType type) const;
    
    // 테스트 지원 메서드
    const std::map<int, BmiRatios>& GetAgeGroupRatios() const;
};

#endif
