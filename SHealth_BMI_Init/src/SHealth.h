#ifndef SHEALTH_H
#define SHEALTH_H

#include <string>
#include <vector>
#include <map>

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
    double GetBmiRatio(int age_class, int type) const;
};

#endif
