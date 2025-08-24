#ifndef SHEALTH_H
#define SHEALTH_H

#include <string>
#include <vector>
#include <map>
#include <array>

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

class SHealth {
private:
    std::vector<HealthRecord> records;
    std::map<int, BmiRatios> ageGroupRatios;

    // Helper functions
    void ImputeMissingWeights();
    void CalculateBmiValues();
    void CalculateAgeGroupRatios();
    int GetAgeBucket(int age) const;

public:
    int CalculateBmi(std::string filename);
    double GetBmiRatio(int age_class, int type) const;  // 기존 호환성 유지
    double GetBmiRatio(int age_class, BmiType type) const;  // 개선된 인터페이스
};

#endif
