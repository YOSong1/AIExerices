#include <stdlib.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include "SHealth.h"

using namespace std;

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
