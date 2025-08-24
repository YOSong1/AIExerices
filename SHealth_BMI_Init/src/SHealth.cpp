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
