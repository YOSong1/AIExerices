#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include "converter.h"
#include "config_loader.h"

// ---------------- InputParser tests ----------------

TEST(InputParserTest, ParseQuery_Success_WithSpaces) {
    InputParser p;
    auto q = p.parseQuery("  meter  :  2.5  ");
    EXPECT_EQ(q.unitName, "meter");
    EXPECT_DOUBLE_EQ(q.value, 2.5);
}

TEST(InputParserTest, ParseQuery_Failure_BadFormat) {
    InputParser p;
    EXPECT_THROW(p.parseQuery("meter-2.5"), std::invalid_argument);
    EXPECT_THROW(p.parseQuery(":2.5"), std::invalid_argument);
    EXPECT_THROW(p.parseQuery("meter:"), std::invalid_argument);
    EXPECT_THROW(p.parseQuery("meter:abc"), std::invalid_argument);
}

TEST(InputParserTest, ParseRegistration_Success_VariousSpaces) {
    InputParser p;
    auto r = p.parseRegistration("  1   cubit   =   0.4572   meter ");
    EXPECT_EQ(r.unitName, "cubit");
    EXPECT_NEAR(r.ratio, 0.4572, 1e-12);
    EXPECT_EQ(r.relativeUnit, "meter");
}

TEST(InputParserTest, ParseRegistration_Failure_Various) {
    InputParser p;
    EXPECT_THROW(p.parseRegistration("2 cubit = 0.4 meter"), std::invalid_argument); // not leading 1
    EXPECT_THROW(p.parseRegistration("1 cubit 0.4 meter"), std::invalid_argument);   // missing '='
    EXPECT_THROW(p.parseRegistration("1 cubit = -0.1 meter"), std::invalid_argument);
}

TEST(InputParserTest, ParseRegistration_Success_NoWhitespaceAfterOne) {
    InputParser p;
    auto r = p.parseRegistration("1cubit = 0.4 meter");
    EXPECT_EQ(r.unitName, "cubit");
    EXPECT_NEAR(r.ratio, 0.4, 1e-12);
    EXPECT_EQ(r.relativeUnit, "meter");
}

// ---------------- Validator tests ----------------

TEST(ValidatorTest, ValidateQuery_SuccessAndFailures) {
    UnitRegistry reg("meter");
    reg.addUnitFromBase("meter", 1.0);
    reg.addUnitFromBase("feet", 0.3048);

    Validator v;
    v.validateQuery(ConversionQuery{"meter", 0.0}, reg); // ok
    v.validateQuery(ConversionQuery{"feet", 1.0}, reg);  // ok

    EXPECT_THROW(v.validateQuery(ConversionQuery{"feet", -1.0}, reg), std::invalid_argument);
    EXPECT_THROW(v.validateQuery(ConversionQuery{"yard", 1.0}, reg), std::invalid_argument); // unknown unit
}

TEST(ValidatorTest, ValidateRegistration_SuccessAndFailures) {
    UnitRegistry reg("meter");
    reg.addUnitFromBase("meter", 1.0);

    Validator v;
    // relativeUnit missing in registry
    InputParser::Registration r1{"cubit", 0.4572, "meter"};
    EXPECT_NO_THROW(v.validateRegistration(r1, reg));

    InputParser::Registration badRatio{"x", 0.0, "meter"};
    EXPECT_THROW(v.validateRegistration(badRatio, reg), std::invalid_argument);

    InputParser::Registration badEmpty{"", 0.1, "meter"};
    EXPECT_THROW(v.validateRegistration(badEmpty, reg), std::invalid_argument);

    InputParser::Registration badRel{"x", 0.1, "unknown"};
    EXPECT_THROW(v.validateRegistration(badRel, reg), std::invalid_argument);
}

// ---------------- UnitRegistry error paths ----------------

TEST(UnitRegistryTest, AddAndGetAndErrors) {
    UnitRegistry reg("meter");
    reg.addUnitFromBase("meter", 1.0);

    // add valid
    reg.addUnitFromBase("feet", 0.3048);
    EXPECT_TRUE(reg.has("feet"));

    // get unknown
    EXPECT_THROW(reg.get("unknown"), std::out_of_range);

    // add invalid
    EXPECT_THROW(reg.addUnitFromBase("", 1.0), std::invalid_argument);
    EXPECT_THROW(reg.addUnitFromBase("bad", 0.0), std::invalid_argument);

    // relative not found
    EXPECT_THROW(reg.addUnitRelative("yard", 0.9144, "unknown"), std::invalid_argument);
}

// ---------------- Formatters ----------------

TEST(FormatterTest, TableCsvJsonContainExpectedFields) {
    UnitRegistry reg("meter");
    reg.addUnitFromBase("meter", 1.0);
    reg.addUnitFromBase("feet", 0.3048);

    ConversionService svc(reg);
    InputParser p;
    auto q = p.parseQuery("meter:2");
    auto results = svc.convertToAll(q.value, q.unitName, true); // include source

    TableFormatter tf;
    CsvFormatter cf;
    JsonFormatter jf;

    auto t = tf.format(q, results, reg.baseUnit());
    auto c = cf.format(q, results, reg.baseUnit());
    auto j = jf.format(q, results, reg.baseUnit());

    EXPECT_NE(t.find("2.00 meter ="), std::string::npos);
    EXPECT_NE(c.find("source_unit,source_value,target_unit,target_value"), std::string::npos);
    EXPECT_NE(j.find("\"baseUnit\": \"meter\""), std::string::npos);
    EXPECT_NE(j.find("\"source\": { \"unit\": \"meter\""), std::string::npos);
}

// ---------------- ConfigLoader ----------------

static std::string writeTempFile(const std::string& name, const std::string& content) {
    std::ofstream ofs(name);
    ofs << content;
    ofs.close();
    return name;
}

TEST(ConfigLoaderTest, LoadSuccessAndBasicValues) {
    // includes exponent and signs to exercise parser
    std::string json = R"({
        "baseUnit": "meter",
        "units": { "meter": 1.0, "feet": 3.048e-1, "yard": 9.144e-1 }
    })";
    auto path = writeTempFile("units_ok.json", json);

    UnitRegistry reg("meter");
    ConfigLoader loader;
    std::string err;
    bool ok = loader.loadFromJson(path, reg, &err);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(err.empty());
    EXPECT_TRUE(reg.has("feet"));
    EXPECT_TRUE(reg.has("yard"));
}

TEST(ConfigLoaderTest, LoadFailures) {
    ConfigLoader loader;
    UnitRegistry reg("meter");
    std::string err;

    // cannot open
    EXPECT_FALSE(loader.loadFromJson("no_such_file.json", reg, &err));
    EXPECT_NE(err.find("cannot open file"), std::string::npos);

    // missing baseUnit
    auto p1 = writeTempFile("units_nobase.json", R"({ "units": {"meter":1.0} })");
    EXPECT_FALSE(loader.loadFromJson(p1, reg, &err));
    EXPECT_NE(err.find("missing baseUnit"), std::string::npos);

    // missing units
    auto p2 = writeTempFile("units_nounits.json", R"({ "baseUnit": "meter" })");
    EXPECT_FALSE(loader.loadFromJson(p2, reg, &err));
    EXPECT_NE(err.find("missing units"), std::string::npos);

    // invalid units object braces
    auto p3 = writeTempFile("units_badobj.json", R"({ "baseUnit": "meter", "units": [1,2,3] })");
    EXPECT_FALSE(loader.loadFromJson(p3, reg, &err));
    EXPECT_NE(err.find("invalid units object"), std::string::npos);

    // invalid factor (<=0)
    auto p4 = writeTempFile("units_badfactor.json", R"({ "baseUnit": "meter", "units": {"meter":1.0, "feet": 0} })");
    EXPECT_FALSE(loader.loadFromJson(p4, reg, &err));
    EXPECT_NE(err.find("invalid factor for unit"), std::string::npos);
}

// ---------------- ConversionService includeSource flag ----------------

TEST(ConversionServiceTestExtra, ConvertToAllIncludeSource) {
    UnitRegistry reg("meter");
    reg.addUnitFromBase("meter", 1.0);
    reg.addUnitFromBase("feet", 0.3048);

    ConversionService svc(reg);
    auto results = svc.convertToAll(1.0, "meter", true);
    bool sawMeter = false;
    for (const auto& kv : results) {
        if (kv.first == "meter") sawMeter = true;
    }
    EXPECT_TRUE(sawMeter);
}

// ---------------- compat unknown handling ----------------

TEST(CompatTestExtra, UnknownUnitsReturnZero) {
    EXPECT_DOUBLE_EQ(compat::to_meter("unknown", 1.23), 0.0);
    EXPECT_DOUBLE_EQ(compat::from_meter("unknown", 1.23), 0.0);
}
