#define APPROVALS_GOOGLETEST
#include "ApprovalTests.hpp"
#include <gtest/gtest.h>
#include "SHealth.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>

using namespace ApprovalTests;

TEST(SHealthApprovalTests, CalculateBmiApproval)
{
    auto approvalsSubdir = Approvals::useApprovalsSubdirectory("approvals");

    std::string input =
        "id,age,weight,height\n"
        "1,25,70,175\n"
        "2,25,48,165\n"
        "3,25,0,165\n"
        "4,27,75,170\n";

    const char* tmp = "temp_input.csv";
    {
        std::ofstream ofs(tmp);
        ofs << input;
    }

    SHealth shealth;
    shealth.CalculateBmi(tmp);

    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss << std::setprecision(1);

    oss << "BMI Ratios for 20s (%)\n";
    oss << "Underweight: " << shealth.GetBmiRatio(20, 100) << "\n";
    oss << "Normal: "      << shealth.GetBmiRatio(20, 200) << "\n";
    oss << "Overweight: "  << shealth.GetBmiRatio(20, 300) << "\n";
    oss << "Obesity: "     << shealth.GetBmiRatio(20, 400) << "\n";

    Approvals::verify(oss.str());

    std::remove(tmp);
}

TEST(SHealthApprovalTests, MultiAgeDistributionApproval)
{
    auto approvalsSubdir = Approvals::useApprovalsSubdirectory("approvals");

    // 20s: 4명 (각 1명씩) -> 25%씩
    // 30s: 4명 (각 1명씩) -> 25%씩
    std::string input =
        "id,age,weight,height\n"
        // 20s (height 170cm)
        "1,25,52.0,170\n"  // BMI ≈ 18.0 -> Underweight
        "2,25,60.7,170\n"  // BMI ≈ 21.0 -> Normal
        "3,25,69.4,170\n"  // BMI ≈ 24.0 -> Overweight
        "4,25,75.1,170\n"  // BMI ≈ 26.0 -> Obesity
        // 30s (height 180cm)
        "5,35,58.3,180\n"  // BMI ≈ 18.0 -> Underweight
        "6,35,68.0,180\n"  // BMI ≈ 21.0 -> Normal
        "7,35,77.8,180\n"  // BMI ≈ 24.0 -> Overweight
        "8,35,84.2,180\n"; // BMI ≈ 26.0 -> Obesity

    const char* tmp = "temp_multi_age.csv";
    {
        std::ofstream ofs(tmp);
        ofs << input;
    }

    SHealth shealth;
    shealth.CalculateBmi(tmp);

    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss << std::setprecision(1);

    oss << "BMI Ratios for 20s (%)\n";
    oss << "Underweight: " << shealth.GetBmiRatio(20, 100) << "\n";
    oss << "Normal: "      << shealth.GetBmiRatio(20, 200) << "\n";
    oss << "Overweight: "  << shealth.GetBmiRatio(20, 300) << "\n";
    oss << "Obesity: "     << shealth.GetBmiRatio(20, 400) << "\n";

    oss << "BMI Ratios for 30s (%)\n";
    oss << "Underweight: " << shealth.GetBmiRatio(30, 100) << "\n";
    oss << "Normal: "      << shealth.GetBmiRatio(30, 200) << "\n";
    oss << "Overweight: "  << shealth.GetBmiRatio(30, 300) << "\n";
    oss << "Obesity: "     << shealth.GetBmiRatio(30, 400) << "\n";

    Approvals::verify(oss.str());
    std::remove(tmp);
}

TEST(SHealthApprovalTests, BoundaryClassificationApproval)
{
    auto approvalsSubdir = Approvals::useApprovalsSubdirectory("approvals");

    // 30s: BMI 경계값 검증 (height 100cm -> BMI == weight)
    // 값: 18.5(저체중 한계), 23.0(과체중 하한 포함), 25.0(어느 범주에도 포함되지 않음)
    std::string input =
        "id,age,weight,height\n"
        "1,35,18.5,100\n"  // Underweight (<=18.5)
        "2,35,23.0,100\n"  // Overweight (>=23 && <25)
        "3,35,25.0,100\n"; // Not counted (obesity is >25 only)

    const char* tmp = "temp_boundary.csv";
    {
        std::ofstream ofs(tmp);
        ofs << input;
    }

    SHealth shealth;
    shealth.CalculateBmi(tmp);

    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss << std::setprecision(1);

    oss << "BMI Ratios for 30s (%)\n";
    oss << "Underweight: " << shealth.GetBmiRatio(30, 100) << "\n";
    oss << "Normal: "      << shealth.GetBmiRatio(30, 200) << "\n";
    oss << "Overweight: "  << shealth.GetBmiRatio(30, 300) << "\n";
    oss << "Obesity: "     << shealth.GetBmiRatio(30, 400) << "\n";

    Approvals::verify(oss.str());
    std::remove(tmp);
}

TEST(SHealthApprovalTests, MissingWeightImputationApproval)
{
    auto approvalsSubdir = Approvals::useApprovalsSubdirectory("approvals");

    // 20s: 체중 0이 평균(비영 0 제외)으로 대체되는지 확인
    // 데이터: (0, 80) -> 평균 80으로 대체되어 모두 비만
    std::string input =
        "id,age,weight,height\n"
        "1,25,0.0,170\n"
        "2,25,80.0,170\n";

    const char* tmp = "temp_imputation.csv";
    {
        std::ofstream ofs(tmp);
        ofs << input;
    }

    SHealth shealth;
    shealth.CalculateBmi(tmp);

    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss << std::setprecision(1);

    oss << "BMI Ratios for 20s (%)\n";
    oss << "Underweight: " << shealth.GetBmiRatio(20, 100) << "\n";
    oss << "Normal: "      << shealth.GetBmiRatio(20, 200) << "\n";
    oss << "Overweight: "  << shealth.GetBmiRatio(20, 300) << "\n";
    oss << "Obesity: "     << shealth.GetBmiRatio(20, 400) << "\n";

    Approvals::verify(oss.str());
    std::remove(tmp);
}


