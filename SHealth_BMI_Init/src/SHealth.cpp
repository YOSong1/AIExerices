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
    
    // 레거시 타입 코드 상수 (기존 호환성 유지)
    constexpr int TYPE_UNDERWEIGHT = static_cast<int>(BmiType::Underweight);
    constexpr int TYPE_NORMAL = static_cast<int>(BmiType::Normal);
    constexpr int TYPE_OVERWEIGHT = static_cast<int>(BmiType::Overweight);
    constexpr int TYPE_OBESITY = static_cast<int>(BmiType::Obesity);
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
    auto it = ageGroupRatios.find(age_class);
    if (it == ageGroupRatios.end()) {
        return 0.0; // 해당 연령대 데이터가 없음
    }
    
    const BmiRatios& ratios = it->second;
    
    switch (type) {
        case TYPE_UNDERWEIGHT: return ratios.underweight;
        case TYPE_NORMAL:      return ratios.normal;
        case TYPE_OVERWEIGHT:  return ratios.overweight;
        case TYPE_OBESITY:     return ratios.obesity;
        default:               return 0.0;
    }
}

// 개선된 인터페이스 (BmiType enum 사용)
double SHealth::GetBmiRatio(int age_class, BmiType type) const
{
    return GetBmiRatio(age_class, static_cast<int>(type));
}

int SHealth::GetAgeBucket(int age) const
{
    // 연령대를 10년 단위로 구분
    if (age < MIN_AGE_GROUP) return MIN_AGE_GROUP;
    if (age >= MAX_AGE_GROUP + AGE_GROUP_SPAN) return MAX_AGE_GROUP;
    return (age / AGE_GROUP_SPAN) * AGE_GROUP_SPAN;
}

void SHealth::ImputeMissingWeights()
{
    // 데이터 수집 중 누락된 체중에 나이대(ex. 20대, 30대, 40대 등)의 평균 체중을 적용
    for (int ageGroup : AGE_GROUPS)
    {
        double sum = 0;
        int age_count = 0;
        
        // 해당 연령대의 유효한 체중 데이터 수집
        for (const auto& record : records) 
        {
            if (record.age >= ageGroup && record.age < ageGroup + AGE_GROUP_SPAN)
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
                if (record.age >= ageGroup && record.age < ageGroup + AGE_GROUP_SPAN)
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
    ageGroupRatios.clear();
    
    for (int ageGroup : AGE_GROUPS)
    {
        int underweight = 0;
        int normalweight = 0;
        int overweight = 0;
        int obesity = 0;
        int sum = 0;
        
        for (const auto& record : records)
        {
            if (record.age >= ageGroup && record.age < ageGroup + AGE_GROUP_SPAN)
            {
                sum++;
                // 기존 로직 유지 (25.0은 어떤 카테고리에도 포함되지 않음)
                if (record.bmi <= BMI_UNDERWEIGHT_THRESHOLD) underweight++;
                else if (record.bmi < BMI_NORMAL_UPPER) normalweight++;
                else if (record.bmi < BMI_OVERWEIGHT_UPPER) overweight++;
                else if (record.bmi > BMI_OVERWEIGHT_UPPER) obesity++;
            }
        }
        
        // 비율 계산 및 저장
        BmiRatios ratios;
        if (sum > 0) {
            ratios.underweight = (double)underweight * 100 / sum;
            ratios.normal = (double)normalweight * 100 / sum;
            ratios.overweight = (double)overweight * 100 / sum;
            ratios.obesity = (double)obesity * 100 / sum;
        }
        // sum이 0인 경우 ratios는 기본값(0.0)으로 초기화됨
        
        ageGroupRatios[ageGroup] = ratios;
    }
}
