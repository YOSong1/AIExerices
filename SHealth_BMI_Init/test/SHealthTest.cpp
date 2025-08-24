#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <cmath>
#include <memory>
#include "SHealth.h"

using namespace std;

// =============================================================================
// 테스트용 모의 객체들 (Mock Objects)
// =============================================================================

class MockAgeBucketer : public AgeBucketer {
public:
    int GetAgeBucket(int age) const override {
        // 테스트용 단순한 버킷팅 (10년 단위)
        return (age / 10) * 10;
    }
};

class MockBmiClassifier : public BmiClassifier {
public:
    BmiType ClassifyBmi(double bmi) const override {
        if (bmi < 18.5) return BmiType::Underweight;
        else if (bmi < 25.0) return BmiType::Normal;
        else if (bmi < 30.0) return BmiType::Overweight;
        else return BmiType::Obesity;
    }
    
    double CalculateBmi(double weight, double height) const override {
        if (height <= 0.0) return 0.0;
        double heightInM = height / 100.0;
        return weight / (heightInM * heightInM);
    }
};

class NoOpWeightImputer : public WeightImputer {
public:
    void ImputeMissingWeights(std::vector<HealthRecord>&, const AgeBucketer&) const override {
        // 테스트용: 보간하지 않음
    }
};

class TestHealthDataLoader : public HealthDataLoader {
private:
    std::vector<HealthRecord> testData;
    
public:
    void SetTestData(std::vector<HealthRecord> data) {
        testData = std::move(data);
    }
    
    std::vector<HealthRecord> LoadHealthData(const std::string&) const override {
        return testData;
    }
    
    std::vector<HealthRecord> LoadHealthData(std::istream&) const override {
        return testData;
    }
};

// =============================================================================
// 테스트용 유틸리티 함수들
// =============================================================================

void CreateTestFile(const string& filename, const string& content) {
    ofstream file(filename);
    file << content;
    file.close();
}

void RemoveTestFile(const string& filename) {
    remove(filename.c_str());
}

// =============================================================================
// 1. 기본 구조체 테스트
// =============================================================================

class HealthRecordTest : public ::testing::Test {};

TEST_F(HealthRecordTest, DefaultConstructor) {
    HealthRecord record;
    EXPECT_EQ(record.age, 0);
    EXPECT_DOUBLE_EQ(record.weight, 0.0);
    EXPECT_DOUBLE_EQ(record.height, 0.0);
    EXPECT_DOUBLE_EQ(record.bmi, 0.0);
}

TEST_F(HealthRecordTest, ParameterizedConstructor) {
    HealthRecord record(25, 70.5, 175.0);
    EXPECT_EQ(record.age, 25);
    EXPECT_DOUBLE_EQ(record.weight, 70.5);
    EXPECT_DOUBLE_EQ(record.height, 175.0);
    EXPECT_DOUBLE_EQ(record.bmi, 0.0);
}

class BmiRatiosTest : public ::testing::Test {};

TEST_F(BmiRatiosTest, DefaultConstructor) {
    BmiRatios ratios;
    EXPECT_DOUBLE_EQ(ratios.underweight, 0.0);
    EXPECT_DOUBLE_EQ(ratios.normal, 0.0);
    EXPECT_DOUBLE_EQ(ratios.overweight, 0.0);
    EXPECT_DOUBLE_EQ(ratios.obesity, 0.0);
}

// =============================================================================
// 2. 전략 구현체 테스트
// =============================================================================

class StandardAgeBucketerTest : public ::testing::Test {
protected:
    StandardAgeBucketer bucketer;
};

TEST_F(StandardAgeBucketerTest, NormalAgeRanges) {
    EXPECT_EQ(bucketer.GetAgeBucket(25), 20);
    EXPECT_EQ(bucketer.GetAgeBucket(29), 20);
    EXPECT_EQ(bucketer.GetAgeBucket(30), 30);
    EXPECT_EQ(bucketer.GetAgeBucket(35), 30);
    EXPECT_EQ(bucketer.GetAgeBucket(45), 40);
    EXPECT_EQ(bucketer.GetAgeBucket(65), 60);
    EXPECT_EQ(bucketer.GetAgeBucket(75), 70);
}

TEST_F(StandardAgeBucketerTest, BoundaryValues) {
    EXPECT_EQ(bucketer.GetAgeBucket(20), 20);
    EXPECT_EQ(bucketer.GetAgeBucket(70), 70);
    EXPECT_EQ(bucketer.GetAgeBucket(79), 70);
}

TEST_F(StandardAgeBucketerTest, OutOfRangeValues) {
    EXPECT_EQ(bucketer.GetAgeBucket(15), 20);
    EXPECT_EQ(bucketer.GetAgeBucket(0), 20);
    EXPECT_EQ(bucketer.GetAgeBucket(-5), 20);
    EXPECT_EQ(bucketer.GetAgeBucket(85), 70);
    EXPECT_EQ(bucketer.GetAgeBucket(100), 70);
}

class StandardBmiClassifierTest : public ::testing::Test {
protected:
    StandardBmiClassifier classifier;
};

TEST_F(StandardBmiClassifierTest, BmiCalculation) {
    EXPECT_NEAR(classifier.CalculateBmi(70.0, 175.0), 22.86, 0.01);
    EXPECT_NEAR(classifier.CalculateBmi(60.0, 165.0), 22.04, 0.01);
    EXPECT_NEAR(classifier.CalculateBmi(80.0, 180.0), 24.69, 0.01);
}

TEST_F(StandardBmiClassifierTest, BmiClassification) {
    EXPECT_EQ(classifier.ClassifyBmi(18.0), BmiType::Underweight);
    EXPECT_EQ(classifier.ClassifyBmi(18.5), BmiType::Underweight);
    EXPECT_EQ(classifier.ClassifyBmi(20.0), BmiType::Normal);
    EXPECT_EQ(classifier.ClassifyBmi(22.9), BmiType::Normal);
    EXPECT_EQ(classifier.ClassifyBmi(24.0), BmiType::Overweight);
    EXPECT_EQ(classifier.ClassifyBmi(24.9), BmiType::Overweight);
    EXPECT_EQ(classifier.ClassifyBmi(26.0), BmiType::Obesity);
}

TEST_F(StandardBmiClassifierTest, ZeroHeightHandling) {
    EXPECT_DOUBLE_EQ(classifier.CalculateBmi(70.0, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(classifier.CalculateBmi(70.0, -10.0), 0.0);
}

// =============================================================================
// 3. 데이터 로더 테스트 (행동 중심)
// =============================================================================

class CsvHealthDataLoaderTest : public ::testing::Test {
protected:
    CsvHealthDataLoader loader;
    string testFileName;
    
    void SetUp() override {
        testFileName = "test_data.csv";
    }
    
    void TearDown() override {
        RemoveTestFile(testFileName);
    }
};

TEST_F(CsvHealthDataLoaderTest, FileNotFound) {
    auto records = loader.LoadHealthData("nonexistent_file.csv");
    EXPECT_TRUE(records.empty());
}

TEST_F(CsvHealthDataLoaderTest, EmptyFile) {
    CreateTestFile(testFileName, "");
    auto records = loader.LoadHealthData(testFileName);
    EXPECT_TRUE(records.empty());
}

TEST_F(CsvHealthDataLoaderTest, ValidData) {
    CreateTestFile(testFileName,
        "Name,Age,Weight,Height\n"
        "John,25,70.5,175.0\n"
        "Jane,30,60.0,165.0\n"
    );
    
    auto records = loader.LoadHealthData(testFileName);
    EXPECT_EQ(records.size(), 2);
    EXPECT_EQ(records[0].age, 25);
    EXPECT_DOUBLE_EQ(records[0].weight, 70.5);
    EXPECT_DOUBLE_EQ(records[0].height, 175.0);
}

TEST_F(CsvHealthDataLoaderTest, InvalidData) {
    CreateTestFile(testFileName,
        "Name,Age,Weight,Height\n"
        "Invalid1,-5,70.0,175.0\n"  // 음수 나이
        "Invalid2,200,70.0,175.0\n" // 너무 큰 나이
        "Valid,25,70.0,175.0\n"
    );
    
    auto records = loader.LoadHealthData(testFileName);
    EXPECT_EQ(records.size(), 1); // 유효한 데이터만
    EXPECT_EQ(records[0].age, 25);
}

TEST_F(CsvHealthDataLoaderTest, StreamInput) {
    string csvData = 
        "Name,Age,Weight,Height\n"
        "John,25,70.0,175.0\n";
    
    istringstream stream(csvData);
    auto records = loader.LoadHealthData(stream);
    
    EXPECT_EQ(records.size(), 1);
    EXPECT_EQ(records[0].age, 25);
}

// =============================================================================
// 4. SHealthCore 테스트 (행동 중심)
// =============================================================================

class SHealthCoreTest : public ::testing::Test {
protected:
    unique_ptr<SHealthCore> core;
    
    void SetUp() override {
        core = make_unique<SHealthCore>();
    }
};

TEST_F(SHealthCoreTest, ProcessEmptyData) {
    vector<HealthRecord> records;
    core->ProcessHealthData(records);
    
    // 모든 연령대에서 0% 반환
    EXPECT_DOUBLE_EQ(core->GetBmiRatio(20, BmiType::Normal), 0.0);
    EXPECT_DOUBLE_EQ(core->GetBmiRatio(30, BmiType::Normal), 0.0);
}

TEST_F(SHealthCoreTest, ProcessSingleRecord) {
    vector<HealthRecord> records = {
        HealthRecord(25, 70.0, 175.0)
    };
    
    core->ProcessHealthData(records);
    
    // 20대에 100% 정상 BMI가 있어야 함
    EXPECT_GT(core->GetBmiRatio(20, BmiType::Normal), 90.0);
    EXPECT_DOUBLE_EQ(core->GetBmiRatio(20, BmiType::Underweight), 0.0);
}

TEST_F(SHealthCoreTest, ProcessMultipleRecords) {
    vector<HealthRecord> records = {
        HealthRecord(25, 50.0, 175.0),  // BMI = 16.33 (저체중)
        HealthRecord(26, 70.0, 175.0),  // BMI = 22.86 (정상)
        HealthRecord(27, 78.0, 175.0),  // BMI = 25.47 (비만)
        HealthRecord(28, 74.0, 175.0),  // BMI = 24.16 (과체중)
    };
    
    core->ProcessHealthData(records);
    
    // 각 카테고리에 25%씩 분포되어야 함
    EXPECT_NEAR(core->GetBmiRatio(20, BmiType::Underweight), 25.0, 1.0);
    EXPECT_NEAR(core->GetBmiRatio(20, BmiType::Normal), 25.0, 1.0);
    EXPECT_NEAR(core->GetBmiRatio(20, BmiType::Overweight), 25.0, 1.0);
    EXPECT_NEAR(core->GetBmiRatio(20, BmiType::Obesity), 25.0, 1.0);
}

TEST_F(SHealthCoreTest, WeightImputationBehavior) {
    vector<HealthRecord> records = {
        HealthRecord(25, 70.0, 175.0),  // 유효한 체중
        HealthRecord(26, 0.0, 170.0),   // 누락된 체중 (보간됨)
    };
    
    core->ProcessHealthData(records);
    
    // 두 레코드 모두 처리되어 20대에 데이터가 있어야 함
    double totalRatio = core->GetBmiRatio(20, BmiType::Underweight) +
                       core->GetBmiRatio(20, BmiType::Normal) +
                       core->GetBmiRatio(20, BmiType::Overweight) +
                       core->GetBmiRatio(20, BmiType::Obesity);
    EXPECT_NEAR(totalRatio, 100.0, 0.1);
}

TEST_F(SHealthCoreTest, LegacyInterfaceCompatibility) {
    vector<HealthRecord> records = {
        HealthRecord(25, 70.0, 175.0)
    };
    
    core->ProcessHealthData(records);
    
    // 레거시 int 타입 인터페이스 테스트
    EXPECT_GT(core->GetBmiRatio(20, static_cast<int>(BmiType::Normal)), 90.0);
    EXPECT_DOUBLE_EQ(core->GetBmiRatio(20, 999), 0.0); // 잘못된 타입
}

// =============================================================================
// 5. SHealth 파사드 테스트 (행동 중심)
// =============================================================================

class SHealthTest : public ::testing::Test {
protected:
    SHealth health;
    string testFileName;
    
    void SetUp() override {
        testFileName = "shealth_test.csv";
    }
    
    void TearDown() override {
        RemoveTestFile(testFileName);
    }
};

TEST_F(SHealthTest, FileBasedWorkflow) {
    CreateTestFile(testFileName,
        "Name,Age,Weight,Height\n"
        "Person1,25,70.0,175.0\n"
        "Person2,30,65.0,170.0\n"
    );
    
    int recordCount = health.CalculateBmi(testFileName);
    EXPECT_EQ(recordCount, 2);
    
    // 데이터가 처리되었는지 확인
    EXPECT_GT(health.GetBmiRatio(20, BmiType::Normal), 0.0);
    EXPECT_GT(health.GetBmiRatio(30, BmiType::Normal), 0.0);
}

TEST_F(SHealthTest, StreamBasedWorkflow) {
    string csvData = 
        "Name,Age,Weight,Height\n"
        "Person1,25,70.0,175.0\n";
    
    istringstream stream(csvData);
    int recordCount = health.CalculateBmi(stream);
    
    EXPECT_EQ(recordCount, 1);
    EXPECT_GT(health.GetBmiRatio(20, BmiType::Normal), 0.0);
}

TEST_F(SHealthTest, DirectDataInjection) {
    vector<HealthRecord> records = {
        HealthRecord(25, 70.0, 175.0),  // BMI = 22.86 (정상)
        HealthRecord(30, 65.0, 170.0)   // BMI = 22.49 (정상)
    };
    
    int recordCount = health.ProcessHealthRecords(std::move(records));
    EXPECT_EQ(recordCount, 2);
    
    // 각 연령대에 데이터가 있는지 확인
    EXPECT_GT(health.GetBmiRatio(20, BmiType::Normal), 0.0);
    EXPECT_GT(health.GetBmiRatio(30, BmiType::Normal), 0.0);
}

TEST_F(SHealthTest, NonExistentFile) {
    int recordCount = health.CalculateBmi("nonexistent_file.csv");
    EXPECT_EQ(recordCount, 0);
    
    // 데이터가 없으면 모든 비율이 0이어야 함
    EXPECT_DOUBLE_EQ(health.GetBmiRatio(20, BmiType::Normal), 0.0);
}

// =============================================================================
// 6. 전략 주입 테스트
// =============================================================================

class SHealthStrategyInjectionTest : public ::testing::Test {};

TEST_F(SHealthStrategyInjectionTest, CustomStrategies) {
    // 커스텀 전략들을 주입한 SHealthCore 생성
    auto core = make_unique<SHealthCore>(
        make_unique<MockAgeBucketer>(),
        make_unique<MockBmiClassifier>(),
        make_unique<NoOpWeightImputer>()
    );
    
    vector<HealthRecord> records = {
        HealthRecord(25, 70.0, 175.0)
    };
    
    core->ProcessHealthData(records);
    
    // MockAgeBucketer는 (age/10)*10 버킷을 사용하므로 20대 대신 20에 저장
    EXPECT_GT(core->GetBmiRatio(20, BmiType::Normal), 0.0);
}

TEST_F(SHealthStrategyInjectionTest, CustomDataLoader) {
    auto testLoader = make_unique<TestHealthDataLoader>();
    testLoader->SetTestData({
        HealthRecord(25, 70.0, 175.0)
    });
    
    SHealth health(std::move(testLoader), make_unique<SHealthCore>());
    
    int recordCount = health.CalculateBmi("dummy_filename");
    EXPECT_EQ(recordCount, 1);
    EXPECT_GT(health.GetBmiRatio(20, BmiType::Normal), 0.0);
}

// =============================================================================
// 7. 통합 테스트 및 시나리오 테스트
// =============================================================================

class SHealthIntegrationTest : public ::testing::Test {
protected:
    SHealth health;
    string testFileName;
    
    void SetUp() override {
        testFileName = "integration_test.csv";
    }
    
    void TearDown() override {
        RemoveTestFile(testFileName);
    }
};

TEST_F(SHealthIntegrationTest, RealisticDataScenario) {
    CreateTestFile(testFileName,
        "Name,Age,Weight,Height\n"
        // 20대 데이터 (다양한 BMI)
        "Person1,22,55.0,160.0\n"  // 저체중
        "Person2,25,65.0,170.0\n"  // 정상
        "Person3,28,75.0,175.0\n"  // 정상
        "Person4,29,85.0,175.0\n"  // 과체중
        // 30대 데이터
        "Person5,32,70.0,170.0\n"  // 정상
        "Person6,35,80.0,175.0\n"  // 과체중
        "Person7,38,95.0,175.0\n"  // 비만
        // 결측치 데이터
        "Person8,26,0,165.0\n"     // 체중 누락
    );
    
    int recordCount = health.CalculateBmi(testFileName);
    EXPECT_EQ(recordCount, 8);
    
    // 각 연령대별로 합리적인 분포가 있는지 확인
    double total20 = health.GetBmiRatio(20, BmiType::Underweight) +
                    health.GetBmiRatio(20, BmiType::Normal) +
                    health.GetBmiRatio(20, BmiType::Overweight) +
                    health.GetBmiRatio(20, BmiType::Obesity);
    EXPECT_NEAR(total20, 100.0, 0.1);
    
    double total30 = health.GetBmiRatio(30, BmiType::Underweight) +
                    health.GetBmiRatio(30, BmiType::Normal) +
                    health.GetBmiRatio(30, BmiType::Overweight) +
                    health.GetBmiRatio(30, BmiType::Obesity);
    EXPECT_NEAR(total30, 100.0, 0.1);
}

TEST_F(SHealthIntegrationTest, EdgeCasesHandling) {
    CreateTestFile(testFileName,
        "Name,Age,Weight,Height\n"
        "Person1,25,70.0,175.0\n"        // 정상
        "Person2,abc,70.0,175.0\n"       // 잘못된 나이
        "Person3,25,xyz,175.0\n"         // 잘못된 체중
        "Person4,25,70.0,abc\n"          // 잘못된 키
        "Person5,,70.0,175.0\n"          // 빈 나이
        "Person6,25,,175.0\n"            // 빈 체중 (보간됨)
        "Person7,25,70.0,\n"             // 빈 키
    );
    
    int recordCount = health.CalculateBmi(testFileName);
    EXPECT_GE(recordCount, 1); // 최소 1개는 처리되어야 함
    
    // 처리된 데이터에 대해서는 합리적인 결과가 나와야 함
    if (recordCount > 0) {
        double totalRatio = health.GetBmiRatio(20, BmiType::Underweight) +
                           health.GetBmiRatio(20, BmiType::Normal) +
                           health.GetBmiRatio(20, BmiType::Overweight) +
                           health.GetBmiRatio(20, BmiType::Obesity);
        EXPECT_GT(totalRatio, 0.0);
    }
}

// =============================================================================
// 8. 성능 및 확장성 테스트
// =============================================================================

class SHealthPerformanceTest : public ::testing::Test {
protected:
    SHealth health;
    string testFileName;
    
    void SetUp() override {
        testFileName = "performance_test.csv";
    }
    
    void TearDown() override {
        RemoveTestFile(testFileName);
    }
};

TEST_F(SHealthPerformanceTest, LargeDataSet) {
    // 큰 데이터셋 생성 (실제로는 더 클 수 있지만 테스트 속도를 위해 제한)
    ofstream file(testFileName);
    file << "Name,Age,Weight,Height\n";
    
    for (int i = 0; i < 1000; ++i) {
        int age = 20 + (i % 50);
        double weight = 50.0 + (i % 50);
        double height = 150.0 + (i % 50);
        file << "Person" << i << "," << age << "," << weight << "," << height << "\n";
    }
    file.close();
    
    int recordCount = health.CalculateBmi(testFileName);
    EXPECT_EQ(recordCount, 1000);
    
    // 결과의 일관성 확인
    for (int ageGroup : AGE_GROUPS) {
        double totalRatio = health.GetBmiRatio(ageGroup, BmiType::Underweight) +
                           health.GetBmiRatio(ageGroup, BmiType::Normal) +
                           health.GetBmiRatio(ageGroup, BmiType::Overweight) +
                           health.GetBmiRatio(ageGroup, BmiType::Obesity);
        
        if (totalRatio > 0) { // 해당 연령대에 데이터가 있는 경우
            EXPECT_NEAR(totalRatio, 100.0, 0.1);
        }
    }
}

// =============================================================================
// 9. 실제 데이터 파일 테스트
// =============================================================================

class SHealthRealDataTest : public ::testing::Test {
protected:
    SHealth health;
};

TEST_F(SHealthRealDataTest, LoadRealDataFile) {
    string realDataFile = "data/shealth.dat";
    
    int recordCount = health.CalculateBmi(realDataFile);
    
    if (recordCount > 0) {
        // 실제 데이터가 로드된 경우 기본 검증
        EXPECT_GT(recordCount, 0);
        
        // 각 연령대에 대한 비율 검증
        for (int ageGroup : AGE_GROUPS) {
            double underweight = health.GetBmiRatio(ageGroup, BmiType::Underweight);
            double normal = health.GetBmiRatio(ageGroup, BmiType::Normal);
            double overweight = health.GetBmiRatio(ageGroup, BmiType::Overweight);
            double obesity = health.GetBmiRatio(ageGroup, BmiType::Obesity);
            
            // 각 비율이 유효한 범위에 있는지 확인
            EXPECT_GE(underweight, 0.0);
            EXPECT_LE(underweight, 100.0);
            EXPECT_GE(normal, 0.0);
            EXPECT_LE(normal, 100.0);
            EXPECT_GE(overweight, 0.0);
            EXPECT_LE(overweight, 100.0);
            EXPECT_GE(obesity, 0.0);
            EXPECT_LE(obesity, 100.0);
            
            // 비율의 합 검증 (데이터가 있는 경우)
            double total = underweight + normal + overweight + obesity;
            if (total > 0) {
                EXPECT_NEAR(total, 100.0, 1.0);
            }
        }
        
        // 레거시 인터페이스 호환성 확인
        EXPECT_EQ(health.GetBmiRatio(20, static_cast<int>(BmiType::Normal)),
                  health.GetBmiRatio(20, BmiType::Normal));
    } else {
        // 파일이 없거나 로드에 실패한 경우
        std::cout << "실제 데이터 파일을 찾을 수 없습니다: " << realDataFile << std::endl;
    }
}