#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <cctype>
#include "converter.h"
#include "config_loader.h"

using namespace std;

// ------------------------- Helpers for interactive console UI -------------------------
static inline std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

static bool readInt(const std::string& prompt, int& out) {
    while (true) {
        cout << prompt;
        string s;
        if (!getline(cin, s)) return false;
        s = trim(s);
        if (s.empty()) continue;
        try {
            size_t idx = 0;
            long v = std::stol(s, &idx);
            if (idx != s.size()) throw std::invalid_argument("extra");
            out = static_cast<int>(v);
            return true;
        } catch (...) {
            cout << "숫자를 입력하세요.\n";
        }
    }
}

static bool readDouble(const std::string& prompt, double& out) {
    while (true) {
        cout << prompt;
        string s;
        if (!getline(cin, s)) return false;
        s = trim(s);
        if (s.empty()) continue;
        try {
            size_t idx = 0;
            double v = std::stod(s, &idx);
            if (idx != s.size()) throw std::invalid_argument("extra");
            out = v;
            return true;
        } catch (...) {
            cout << "실수 값을 입력하세요.\n";
        }
    }
}

static bool readLine(const std::string& prompt, std::string& out) {
    cout << prompt;
    if (!getline(cin, out)) return false;
    out = trim(out);
    return true;
}

static void listUnits(const UnitRegistry& registry) {
    auto units = registry.list();
    cout << "\n[등록된 단위] (기준 단위: " << registry.baseUnit() << ")\n";
    // 기준 단위를 먼저 표시하고, 나머지는 이름순
    std::vector<Unit> others;
    for (const auto& u : units) {
        if (u.name == registry.baseUnit()) continue;
        others.push_back(u);
    }
    std::sort(others.begin(), others.end(), [](const Unit& a, const Unit& b){ return a.name < b.name; });
    cout << "  1 " << registry.baseUnit() << " = 1 " << registry.baseUnit() << "\n";
    for (const auto& u : others) {
        cout << "  1 " << u.name << " = " << u.toBaseFactor << " " << registry.baseUnit() << "\n";
    }
}

static int chooseUnit(const UnitRegistry& registry, std::string& chosenUnitName) {
    auto units = registry.list();
    // 유닛 이름 목록 생성 (기준 단위 먼저)
    std::vector<std::string> names;
    names.push_back(registry.baseUnit());
    for (const auto& u : units) {
        if (u.name != registry.baseUnit()) names.push_back(u.name);
    }
    // 중복 제거(혹시 모를)
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());

    // 기준 단위가 첫 번째가 되도록 재정렬
    auto it = std::find(names.begin(), names.end(), registry.baseUnit());
    if (it != names.end()) {
        std::rotate(names.begin(), it, it + 1);
    }

    cout << "\n단위를 선택하세요:\n";
    for (size_t i = 0; i < names.size(); ++i) {
        cout << "  " << (i + 1) << ") " << names[i] << "\n";
    }
    int sel = 0;
    while (true) {
        if (!readInt("선택 번호 입력: ", sel)) return -1;
        if (sel >= 1 && sel <= static_cast<int>(names.size())) break;
        cout << "올바른 범위의 번호를 입력하세요.\n";
    }
    chosenUnitName = names[static_cast<size_t>(sel - 1)];
    return sel;
}

static void printMenu(const std::string& format) {
    cout << "\n================ 단위 변환기 ================\n";
    cout << "1) 값 변환 (단위 선택 → 값 입력)\n";
    cout << "2) 새 단위 등록 (1 x = r y)\n";
    cout << "3) 등록된 단위 목록 보기\n";
    cout << "4) 출력 포맷 변경 (현재: " << format << ")\n";
    cout << "5) 구성 파일 로드 (--config와 동일)\n";
    cout << "6) 빠른 명령 입력 (meter:2.5 또는 1 cubit = 0.4572 meter)\n";
    cout << "   - 힌트: 값 변환은 메뉴 1에서 단위를 고르고 숫자만 입력하면 됩니다.\n";
    cout << "   - 힌트: 새 단위 등록은 '1 새단위 = 비율 기준단위' 형태입니다.\n";
    cout << "0) 종료\n";
    cout << "-------------------------------------------\n";
}

int main(int argc, char** argv) {
    // Simple CLI options: --config <path> --format table|json|csv
    string configPath;
    string format = "table";
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            format = argv[++i];
        }
    }

    UnitRegistry registry("meter");
    // Default units (fallback)
    registry.addUnitFromBase("meter", 1.0);
    registry.addUnitFromBase("feet", 0.3048);
    registry.addUnitFromBase("yard", 0.9144);

    if (!configPath.empty()) {
        ConfigLoader loader;
        std::string err;
        if (!loader.loadFromJson(configPath, registry, &err)) {
            std::cerr << "Config load failed: " << err << std::endl;
        }
    }

    ConversionService service(registry);
    TableFormatter tableFmt;
    CsvFormatter csvFmt;
    JsonFormatter jsonFmt;
    FormatStrategy* fmt = &tableFmt;
    if (format == "csv") fmt = &csvFmt;
    else if (format == "json") fmt = &jsonFmt;

    InputParser parser;
    Validator validator;

    // ------------------------- Interactive menu loop -------------------------
    while (true) {
        printMenu(format);
        int choice = -1;
        if (!readInt("메뉴 선택: ", choice)) break;
        if (choice == 0) break;

        try {
            if (choice == 1) {
                // 값 변환
                std::string fromUnit;
                if (chooseUnit(registry, fromUnit) < 0) break;
                double value = 0.0;
                if (!readDouble("변환할 값 입력: ", value)) break;

                ConversionQuery q{ fromUnit, value };
                validator.validateQuery(q, registry);
                auto results = service.convertToAll(q.value, q.unitName, true);
                cout << "\n" << fmt->format(q, results, registry.baseUnit());

            } else if (choice == 2) {
                // 새 단위 등록
                std::string newUnit;
                if (!readLine("새 단위 이름: ", newUnit)) break;
                if (newUnit.empty()) { cout << "단위 이름이 비어있습니다.\n"; continue; }

                std::string relUnit;
                if (chooseUnit(registry, relUnit) < 0) break;
                double ratio = 0.0;
                if (!readDouble("비율 r 입력 (형식: 1 " + newUnit + " = r " + relUnit + "): ", ratio)) break;

                InputParser::Registration reg{ newUnit, ratio, relUnit };
                validator.validateRegistration(reg, registry);
                registry.addUnitRelative(reg.unitName, reg.ratio, reg.relativeUnit);
                cout << "등록 완료: " << reg.unitName << " (1 " << reg.unitName << " = " << reg.ratio << " " << reg.relativeUnit << ")\n";

            } else if (choice == 3) {
                // 단위 목록
                listUnits(registry);

            } else if (choice == 4) {
                // 포맷 변경
                cout << "\n출력 포맷 선택:\n  1) table\n  2) csv\n  3) json\n";
                int fsel = 0;
                if (!readInt("선택 번호 입력: ", fsel)) break;
                if (fsel == 1) { format = "table"; fmt = &tableFmt; }
                else if (fsel == 2) { format = "csv"; fmt = &csvFmt; }
                else if (fsel == 3) { format = "json"; fmt = &jsonFmt; }
                else { cout << "알 수 없는 선택입니다.\n"; }

            } else if (choice == 5) {
                // 구성 파일 로드
                std::string path;
                if (!readLine("구성 파일 경로: ", path)) break;
                ConfigLoader loader;
                std::string err;
                if (!loader.loadFromJson(path, registry, &err)) {
                    std::cerr << "구성 파일 로드 실패: " << err << std::endl;
                } else {
                    std::cout << "구성 파일 로드 완료. 기준 단위: " << registry.baseUnit() << "\n";
                }

            } else if (choice == 6) {
                // 빠른 명령 입력 (기존 호환)
                string input_str;
                if (!readLine("명령 입력 (meter:2.5 / 1 cubit = 0.4572 meter): ", input_str)) break;
                if (input_str.empty()) continue;

                // try registration first
                try {
                    auto reg = parser.parseRegistration(input_str);
                    validator.validateRegistration(reg, registry);
                    registry.addUnitRelative(reg.unitName, reg.ratio, reg.relativeUnit);
                    cout << "등록 완료: " << reg.unitName << "\n";
                } catch (...) {
                    // not a registration, treat as query
                    auto q = parser.parseQuery(input_str);
                    validator.validateQuery(q, registry);
                    auto results = service.convertToAll(q.value, q.unitName, true);
                    cout << fmt->format(q, results, registry.baseUnit());
                }

            } else {
                cout << "알 수 없는 메뉴입니다.\n";
            }
        } catch (const std::exception& ex) {
            cerr << "Error: " << ex.what() << endl;
        }
    }
    return 0;
}
