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
