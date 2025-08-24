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

TEST(SHealthApprovalTests, EmptyAgeGroupApproval)
{
    auto approvalsSubdir = Approvals::useApprovalsSubdirectory("approvals");

    // 특정 연령대(40대)에 데이터가 없는 경우 테스트
    std::string input =
        "id,age,weight,height\n"
        "1,25,70,170\n"  // 20대
        "2,35,65,175\n"  // 30대
        "3,55,80,180\n"; // 50대 (40대 데이터 없음)

    const char* tmp = "temp_empty_age_group.csv";
    {
        std::ofstream ofs(tmp);
        ofs << input;
    }

    SHealth shealth;
    shealth.CalculateBmi(tmp);

    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss << std::setprecision(1);

    oss << "BMI Ratios for 40s (%) - Should be all zeros\n";
    oss << "Underweight: " << shealth.GetBmiRatio(40, 100) << "\n";
    oss << "Normal: "      << shealth.GetBmiRatio(40, 200) << "\n";
    oss << "Overweight: "  << shealth.GetBmiRatio(40, 300) << "\n";
    oss << "Obesity: "     << shealth.GetBmiRatio(40, 400) << "\n";

    Approvals::verify(oss.str());
    std::remove(tmp);
}

TEST(SHealthApprovalTests, AllZeroWeightsApproval)
{
    auto approvalsSubdir = Approvals::useApprovalsSubdirectory("approvals");

    // 모든 체중이 0인 경우 (보간 불가능)
    std::string input =
        "id,age,weight,height\n"
        "1,25,0,170\n"
        "2,25,0,175\n"
        "3,25,0,180\n";

    const char* tmp = "temp_all_zero_weights.csv";
    {
        std::ofstream ofs(tmp);
        ofs << input;
    }

    SHealth shealth;
    shealth.CalculateBmi(tmp);

    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss << std::setprecision(1);

    oss << "BMI Ratios for 20s (%) - All zero weights case\n";
    oss << "Underweight: " << shealth.GetBmiRatio(20, 100) << "\n";
    oss << "Normal: "      << shealth.GetBmiRatio(20, 200) << "\n";
    oss << "Overweight: "  << shealth.GetBmiRatio(20, 300) << "\n";
    oss << "Obesity: "     << shealth.GetBmiRatio(20, 400) << "\n";

    Approvals::verify(oss.str());
    std::remove(tmp);
}

TEST(SHealthApprovalTests, InvalidDataHandlingApproval)
{
    auto approvalsSubdir = Approvals::useApprovalsSubdirectory("approvals");

    // 유효하지 않은 데이터가 포함된 경우
    std::string input =
        "id,age,weight,height\n"
        "1,25,70,170\n"      // 정상 데이터
        "2,-5,65,175\n"      // 음수 나이 (무시됨)
        "3,200,80,180\n"     // 비현실적 나이 (무시됨)
        "4,30,65,0\n"        // 키 0 (무시됨)
        "5,35,1500,175\n"    // 비현실적 체중 (0으로 처리 후 보간)
        "6,35,75,175\n";     // 정상 데이터

    const char* tmp = "temp_invalid_data.csv";
    {
        std::ofstream ofs(tmp);
        ofs << input;
    }

    SHealth shealth;
    int count = shealth.CalculateBmi(tmp);

    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss << std::setprecision(1);

    oss << "Valid records processed: " << count << "\n";
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

TEST(SHealthApprovalTests, ExactBoundaryValuesApproval)
{
    auto approvalsSubdir = Approvals::useApprovalsSubdirectory("approvals");

    // BMI 경계값들의 정확한 분류 테스트
    std::string input =
        "id,age,weight,height\n"
        "1,25,18.5,100\n"   // BMI = 18.5 (저체중 경계)
        "2,25,18.6,100\n"   // BMI = 18.6 (정상체중 하한)
        "3,25,22.9,100\n"   // BMI = 22.9 (정상체중 상한)
        "4,25,23.0,100\n"   // BMI = 23.0 (과체중 하한)
        "5,25,24.9,100\n"   // BMI = 24.9 (과체중 상한)
        "6,25,25.0,100\n"   // BMI = 25.0 (어떤 카테고리에도 포함되지 않음)
        "7,25,25.1,100\n";  // BMI = 25.1 (비만 하한)

    const char* tmp = "temp_exact_boundaries.csv";
    {
        std::ofstream ofs(tmp);
        ofs << input;
    }

    SHealth shealth;
    int count = shealth.CalculateBmi(tmp);

    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss << std::setprecision(1);

    oss << "Total records: " << count << "\n";
    oss << "BMI Ratios for 20s (%)\n";
    oss << "Underweight: " << shealth.GetBmiRatio(20, 100) << "\n";
    oss << "Normal: "      << shealth.GetBmiRatio(20, 200) << "\n";
    oss << "Overweight: "  << shealth.GetBmiRatio(20, 300) << "\n";
    oss << "Obesity: "     << shealth.GetBmiRatio(20, 400) << "\n";

    Approvals::verify(oss.str());
    std::remove(tmp);
}

