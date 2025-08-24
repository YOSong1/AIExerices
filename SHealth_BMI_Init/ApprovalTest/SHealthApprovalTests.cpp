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


