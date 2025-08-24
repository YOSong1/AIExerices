#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include <stdexcept>
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
    
    // 레거시 타입 코드 상수 (기존 호환성 유지)
    constexpr int TYPE_UNDERWEIGHT = static_cast<int>(BmiType::Underweight);
    constexpr int TYPE_NORMAL = static_cast<int>(BmiType::Normal);
    constexpr int TYPE_OVERWEIGHT = static_cast<int>(BmiType::Overweight);
    constexpr int TYPE_OBESITY = static_cast<int>(BmiType::Obesity);
}


// =============================================================================
// 기본 구현체들 (Default Implementations)
// =============================================================================

int StandardAgeBucketer::GetAgeBucket(int age) const {
    if (age < MIN_AGE_GROUP) return MIN_AGE_GROUP;
    if (age >= MAX_AGE_GROUP + AGE_GROUP_SPAN) return MAX_AGE_GROUP;
    return (age / AGE_GROUP_SPAN) * AGE_GROUP_SPAN;
}

BmiType StandardBmiClassifier::ClassifyBmi(double bmi) const {
    if (bmi <= BMI_UNDERWEIGHT_THRESHOLD) return BmiType::Underweight;
    else if (bmi < BMI_NORMAL_UPPER) return BmiType::Normal;
    else if (bmi < BMI_OVERWEIGHT_UPPER) return BmiType::Overweight;
    else if (bmi > BMI_OVERWEIGHT_UPPER) return BmiType::Obesity;
    else return BmiType::Normal; // BMI가 정확히 25.0인 경우 (기존 로직에서는 분류되지 않았지만 안전하게 처리)
}

double StandardBmiClassifier::CalculateBmi(double weight, double height) const {
    if (height <= 0.0) return 0.0; // 안전한 처리
    double heightInM = height / 100.0;
    return weight / (heightInM * heightInM);
}

void AverageWeightImputer::ImputeMissingWeights(std::vector<HealthRecord>& records, const AgeBucketer& bucketer) const {
    for (int ageGroup : AGE_GROUPS) {
        double sum = 0;
        int count = 0;
        
        // 해당 연령대의 유효한 체중 데이터 수집
        for (const auto& record : records) {
            if (bucketer.GetAgeBucket(record.age) == ageGroup && record.weight > 0.0) {
                sum += record.weight;
                count++;
            }
        }
        
        // 평균 체중으로 누락 데이터 보간
        if (count > 0) {
            double average_weight = sum / count;
            for (auto& record : records) {
                if (bucketer.GetAgeBucket(record.age) == ageGroup && record.weight == 0.0) {
                    record.weight = average_weight;
                }
            }
        }
    }
}

std::vector<HealthRecord> CsvHealthDataLoader::LoadHealthData(const std::string& filename) const {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return {}; // 빈 벡터 반환
    }
    return LoadHealthData(file);
}

std::vector<HealthRecord> CsvHealthDataLoader::LoadHealthData(std::istream& input) const {
    std::vector<HealthRecord> records;
    std::string line;
    
    // 헤더 라인 건너뛰기
    if (std::getline(input, line)) {
        // 첫 번째 라인은 헤더로 가정
    }
    
    while (std::getline(input, line)) {
        std::string trimmed_line = Trim(line);
        if (trimmed_line.empty()) continue;
        
        // 주석 라인 스킵
        if (trimmed_line[0] == '#' || trimmed_line.substr(0, 2) == "//") continue;
        
        std::vector<std::string> tokens = Split(trimmed_line, ',');
        if (tokens.size() < 4) continue;
        
        // 안전한 파싱과 유효성 검증
        int age = SafeParseInt(tokens[1]);
        double weight = SafeParseDouble(tokens[2]);
        double height = SafeParseDouble(tokens[3]);
        
        if (!IsValidAge(age) || !IsValidHeight(height)) continue;
        
        double validWeight = IsValidWeight(weight) ? weight : 0.0;
        records.emplace_back(age, validWeight, height);
    }
    
    return records;
}

// =============================================================================
// SHealthCore 구현
// =============================================================================

SHealthCore::SHealthCore() 
    : ageBucketer(std::make_unique<StandardAgeBucketer>()),
      bmiClassifier(std::make_unique<StandardBmiClassifier>()),
      weightImputer(std::make_unique<AverageWeightImputer>()) {
}

SHealthCore::SHealthCore(std::unique_ptr<AgeBucketer> bucketer,
                         std::unique_ptr<BmiClassifier> classifier,
                         std::unique_ptr<WeightImputer> imputer)
    : ageBucketer(std::move(bucketer)),
      bmiClassifier(std::move(classifier)),
      weightImputer(std::move(imputer)) {
}

void SHealthCore::ProcessHealthData(std::vector<HealthRecord>& records) {
    // 1. 결측치 보간
    weightImputer->ImputeMissingWeights(records, *ageBucketer);
    
    // 2. BMI 계산
    for (auto& record : records) {
        record.bmi = bmiClassifier->CalculateBmi(record.weight, record.height);
    }
    
    // 3. 연령대별 비율 계산
    ageGroupRatios.clear();
    
    for (int ageGroup : AGE_GROUPS) {
        int underweight = 0, normal = 0, overweight = 0, obesity = 0, total = 0;
        
        for (const auto& record : records) {
            if (ageBucketer->GetAgeBucket(record.age) == ageGroup) {
                total++;
                BmiType type = bmiClassifier->ClassifyBmi(record.bmi);
                switch (type) {
                    case BmiType::Underweight: underweight++; break;
                    case BmiType::Normal: normal++; break;
                    case BmiType::Overweight: overweight++; break;
                    case BmiType::Obesity: obesity++; break;
                }
            }
        }
        
        BmiRatios ratios;
        if (total > 0) {
            ratios.underweight = (double)underweight * 100 / total;
            ratios.normal = (double)normal * 100 / total;
            ratios.overweight = (double)overweight * 100 / total;
            ratios.obesity = (double)obesity * 100 / total;
        }
        ageGroupRatios[ageGroup] = ratios;
    }
}

double SHealthCore::GetBmiRatio(int age_class, BmiType type) const {
    auto it = ageGroupRatios.find(age_class);
    if (it == ageGroupRatios.end()) {
        return 0.0;
    }
    
    const BmiRatios& ratios = it->second;
    switch (type) {
        case BmiType::Underweight: return ratios.underweight;
        case BmiType::Normal: return ratios.normal;
        case BmiType::Overweight: return ratios.overweight;
        case BmiType::Obesity: return ratios.obesity;
        default: return 0.0;
    }
}

double SHealthCore::GetBmiRatio(int age_class, int type) const {
    switch (type) {
        case TYPE_UNDERWEIGHT: return GetBmiRatio(age_class, BmiType::Underweight);
        case TYPE_NORMAL: return GetBmiRatio(age_class, BmiType::Normal);
        case TYPE_OVERWEIGHT: return GetBmiRatio(age_class, BmiType::Overweight);
        case TYPE_OBESITY: return GetBmiRatio(age_class, BmiType::Obesity);
        default: return 0.0;
    }
}

// =============================================================================
// SHealth 파사드 구현
// =============================================================================

SHealth::SHealth() 
    : dataLoader(std::make_unique<CsvHealthDataLoader>()),
      core(std::make_unique<SHealthCore>()) {
}

SHealth::SHealth(std::unique_ptr<HealthDataLoader> loader,
                 std::unique_ptr<SHealthCore> healthCore)
    : dataLoader(std::move(loader)),
      core(std::move(healthCore)) {
}

int SHealth::CalculateBmi(const std::string& filename) {
    auto records = dataLoader->LoadHealthData(filename);
    return ProcessHealthRecords(std::move(records));
}

int SHealth::CalculateBmi(std::istream& input) {
    auto records = dataLoader->LoadHealthData(input);
    return ProcessHealthRecords(std::move(records));
}

int SHealth::ProcessHealthRecords(std::vector<HealthRecord> records) {
    core->ProcessHealthData(records);
    return static_cast<int>(records.size());
}

double SHealth::GetBmiRatio(int age_class, int type) const {
    return core->GetBmiRatio(age_class, type);
}

double SHealth::GetBmiRatio(int age_class, BmiType type) const {
    return core->GetBmiRatio(age_class, type);
}

const std::map<int, BmiRatios>& SHealth::GetAgeGroupRatios() const {
    return core->GetAgeGroupRatios();
}
