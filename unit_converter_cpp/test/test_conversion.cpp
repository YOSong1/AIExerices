#include <gtest/gtest.h>
#include "converter.h"

TEST(ConversionServiceTest, MeterToFeetAndYard) {
    UnitRegistry reg("meter");
    reg.addUnitFromBase("meter", 1.0);
    reg.addUnitFromBase("feet", 0.3048);
    reg.addUnitFromBase("yard", 0.9144);

    ConversionService svc(reg);
    double v = 2.5; // meter
    // Expected values based on ratios
    double feet = svc.convert(v, "meter", "feet");
    double yard = svc.convert(v, "meter", "yard");

    // From registry: 1 yard = 0.9144 meter (single source of truth)
    // Keep feet expectation as in README; for yard, avoid mixed rounding by using 0.9144
    EXPECT_NEAR(feet, 2.5 * 3.28084, 1e-6);
    EXPECT_NEAR(yard, v / 0.9144, 1e-9);
}

TEST(CompatTest, ComputeAllMatchesMain) {
    double m=0, f=0, y=0;
    compat::compute_all("meter", 2.5, m, f, y);
    EXPECT_NEAR(m, 2.5, 1e-9);
    EXPECT_NEAR(f, 2.5 * compat::FEET_PER_METER, 1e-6);
    EXPECT_NEAR(y, 2.5 * compat::YARDS_PER_METER, 1e-6);
}


TEST(ConversionServiceTest, FeetToMeterAndYard) {
    UnitRegistry reg("meter");
    reg.addUnitFromBase("meter", 1.0);
    reg.addUnitFromBase("feet", 0.3048);
    reg.addUnitFromBase("yard", 0.9144);

    ConversionService svc(reg);

    double oneFootInMeter = svc.convert(1.0, "feet", "meter");
    double oneFootInYard  = svc.convert(1.0, "feet", "yard");

    EXPECT_NEAR(oneFootInMeter, 0.3048, 1e-12);
    EXPECT_NEAR(oneFootInYard,  0.3048 / 0.9144, 1e-12); // 1/3
}

TEST(ConversionServiceTest, YardToMeterAndFeet) {
    UnitRegistry reg("meter");
    reg.addUnitFromBase("meter", 1.0);
    reg.addUnitFromBase("feet", 0.3048);
    reg.addUnitFromBase("yard", 0.9144);

    ConversionService svc(reg);

    double oneYardInMeter = svc.convert(1.0, "yard", "meter");
    double oneYardInFeet  = svc.convert(1.0, "yard", "feet");

    EXPECT_NEAR(oneYardInMeter, 0.9144, 1e-12);
    EXPECT_NEAR(oneYardInFeet,  3.0,     1e-12);
}

TEST(ConversionServiceTest, SameUnitReturnsSameValue) {
    UnitRegistry reg("meter");
    reg.addUnitFromBase("meter", 1.0);
    reg.addUnitFromBase("feet", 0.3048);
    reg.addUnitFromBase("yard", 0.9144);

    ConversionService svc(reg);
    double v = 2.5;
    EXPECT_NEAR(svc.convert(v, "meter", "meter"), v, 1e-12);
    EXPECT_NEAR(svc.convert(v, "feet",  "feet"),  v, 1e-12);
    EXPECT_NEAR(svc.convert(v, "yard",  "yard"),  v, 1e-12);
}

TEST(ConversionServiceTest, ConvertToAllFromMeter) {
    UnitRegistry reg("meter");
    reg.addUnitFromBase("meter", 1.0);
    reg.addUnitFromBase("feet", 0.3048);
    reg.addUnitFromBase("yard", 0.9144);

    ConversionService svc(reg);
    double v = 1.0;
    auto results = svc.convertToAll(v, "meter", false);

    bool foundFeet = false, foundYard = false;
    for (const auto& kv : results) {
        if (kv.first == "feet") {
            foundFeet = true;
            EXPECT_NEAR(kv.second, v / 0.3048, 1e-12);
        } else if (kv.first == "yard") {
            foundYard = true;
            EXPECT_NEAR(kv.second, v / 0.9144, 1e-12);
        }
    }
    EXPECT_TRUE(foundFeet);
    EXPECT_TRUE(foundYard);
}

TEST(CompatTest, ToFromMeterRoundTripFeetAndYard) {
    // 1) feet -> meter -> feet (round-trip)
    double vFeet = 7.0;
    double m = compat::to_meter("feet", vFeet);
    double backFeet = compat::from_meter("feet", m);
    EXPECT_NEAR(backFeet, vFeet, 1e-9);

    // 2) yard -> meter -> yard (round-trip)
    double vYard = 11.0;
    m = compat::to_meter("yard", vYard);
    double backYard = compat::from_meter("yard", m);
    EXPECT_NEAR(backYard, vYard, 1e-9);
}


