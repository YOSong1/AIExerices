#include <iostream>
#include <iomanip>
#include "SHealth.h"

int main() {
    try {
        SHealth shealth;
        
        // 데이터 파일 로드 및 처리
        int recordCount = shealth.CalculateBmi("data/shealth.dat");
        
        if (recordCount == 0) {
            std::cout << "데이터 파일을 찾을 수 없거나 유효한 데이터가 없습니다." << std::endl;
            return 1;
        }
        
        std::cout << "총 " << recordCount << "개의 건강 기록을 처리했습니다." << std::endl;
        std::cout << std::endl;
        
        // 연령대별 BMI 분포 출력
        std::cout << "=== 연령대별 BMI 분포 ===" << std::endl;
        std::cout << std::fixed << std::setprecision(1);
        
        for (int ageGroup : AGE_GROUPS) {
            std::cout << ageGroup << "대:" << std::endl;
            std::cout << "  저체중: " << std::setw(5) << shealth.GetBmiRatio(ageGroup, BmiType::Underweight) << "%" << std::endl;
            std::cout << "  정상:   " << std::setw(5) << shealth.GetBmiRatio(ageGroup, BmiType::Normal) << "%" << std::endl;
            std::cout << "  과체중: " << std::setw(5) << shealth.GetBmiRatio(ageGroup, BmiType::Overweight) << "%" << std::endl;
            std::cout << "  비만:   " << std::setw(5) << shealth.GetBmiRatio(ageGroup, BmiType::Obesity) << "%" << std::endl;
            std::cout << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "오류 발생: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
