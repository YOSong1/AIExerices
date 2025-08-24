// converter.cpp: core implementation for unit conversion

#include "converter.h"

#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdlib>

// ---------------- UnitRegistry -----------------

UnitRegistry::UnitRegistry(const std::string& baseUnitName)
    : baseUnitName_(baseUnitName) {
    nameToUnit_[baseUnitName_] = Unit{ baseUnitName_, 1.0 };
}

const std::string& UnitRegistry::baseUnit() const {
    return baseUnitName_;
}

void UnitRegistry::addUnitFromBase(const std::string& unitName, double toBaseFactor) {
    if (unitName.empty()) {
        throw std::invalid_argument("unitName must not be empty");
    }
    if (!(toBaseFactor > 0.0)) {
        throw std::invalid_argument("toBaseFactor must be > 0");
    }
    nameToUnit_[unitName] = Unit{ unitName, toBaseFactor };
}

void UnitRegistry::addUnitRelative(const std::string& unitName, double ratio, const std::string& relativeUnitName) {
    if (!has(relativeUnitName)) {
        throw std::invalid_argument("relative unit not found: " + relativeUnitName);
    }
    const Unit& rel = get(relativeUnitName);
    // ratio: 1 unitName = ratio relativeUnitName
    double toBase = ratio * rel.toBaseFactor;
    addUnitFromBase(unitName, toBase);
}

bool UnitRegistry::has(const std::string& unitName) const {
    return nameToUnit_.find(unitName) != nameToUnit_.end();
}

const Unit& UnitRegistry::get(const std::string& unitName) const {
    auto it = nameToUnit_.find(unitName);
    if (it == nameToUnit_.end()) {
        throw std::out_of_range("unit not found: " + unitName);
    }
    return it->second;
}

std::vector<Unit> UnitRegistry::list() const {
    std::vector<Unit> units;
    units.reserve(nameToUnit_.size());
    for (const auto& kv : nameToUnit_) {
        units.push_back(kv.second);
    }
    return units;
}

// ---------------- ConversionService -----------------

ConversionService::ConversionService(const UnitRegistry& registry)
    : registry_(registry) {}

double ConversionService::convert(double value, const std::string& fromUnit, const std::string& toUnit) const {
    if (fromUnit == toUnit) {
        return value;
    }
    const Unit& from = registry_.get(fromUnit);
    const Unit& to = registry_.get(toUnit);
    // Convert to base: value * from.toBaseFactor
    // Then to target: baseValue / to.toBaseFactor
    double baseValue = value * from.toBaseFactor;
    double result = baseValue / to.toBaseFactor;
    return result;
}

std::vector<std::pair<std::string, double>> ConversionService::convertToAll(double value, const std::string& fromUnit, bool includeSource) const {
    std::vector<std::pair<std::string, double>> out;
    for (const auto& u : registry_.list()) {
        if (!includeSource && u.name == fromUnit) {
            continue;
        }
        out.emplace_back(u.name, convert(value, fromUnit, u.name));
    }
    return out;
}

// ---------------- InputParser -----------------

static inline std::string trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

ConversionQuery InputParser::parseQuery(const std::string& input) const {
    auto pos = input.find(":");
    if (pos == std::string::npos) {
        throw std::invalid_argument("invalid query format, expected 'unit:value'");
    }
    std::string unit = trim(input.substr(0, pos));
    std::string valueStr = trim(input.substr(pos + 1));
    if (unit.empty() || valueStr.empty()) {
        throw std::invalid_argument("invalid query tokens");
    }
    char* end = nullptr;
    double v = std::strtod(valueStr.c_str(), &end);
    if (end == valueStr.c_str() || *end != '\0') {
        throw std::invalid_argument("invalid numeric value");
    }
    return ConversionQuery{ unit, v };
}

InputParser::Registration InputParser::parseRegistration(const std::string& input) const {
    // format: "1 X = r Y"
    // tokenize by spaces/equals
    // We'll parse simply: expect leading '1', then unit, '=', ratio, relativeUnit
    std::string s = trim(input);
    // replace multiple spaces around '=' to handle variations
    auto eqPos = s.find('=');
    if (eqPos == std::string::npos) {
        throw std::invalid_argument("invalid registration format, expected '='");
    }
    std::string left = trim(s.substr(0, eqPos));
    std::string right = trim(s.substr(eqPos + 1));

    // left: "1 X"
    std::istringstream lss(left);
    double one;
    std::string unitName;
    if (!(lss >> one) || !(lss >> unitName) || one != 1.0) {
        throw std::invalid_argument("invalid left side, expected '1 <unit>'");
    }

    // right: "r Y"
    std::istringstream rss(right);
    double ratio;
    std::string relativeUnit;
    if (!(rss >> ratio) || !(rss >> relativeUnit)) {
        throw std::invalid_argument("invalid right side, expected '<ratio> <unit>'");
    }
    if (!(ratio > 0.0)) {
        throw std::invalid_argument("ratio must be > 0");
    }

    return Registration{ unitName, ratio, relativeUnit };
}

// ---------------- Validator -----------------

void Validator::validateQuery(const ConversionQuery& query, const UnitRegistry& registry) const {
    if (!(query.value >= 0.0)) {
        throw std::invalid_argument("value must be >= 0");
    }
    if (!registry.has(query.unitName)) {
        throw std::invalid_argument("unknown unit: " + query.unitName);
    }
}

void Validator::validateRegistration(const InputParser::Registration& reg, const UnitRegistry& registry) const {
    if (reg.unitName.empty() || reg.relativeUnit.empty()) {
        throw std::invalid_argument("unit names must not be empty");
    }
    if (!(reg.ratio > 0.0)) {
        throw std::invalid_argument("ratio must be > 0");
    }
    if (!registry.has(reg.relativeUnit)) {
        throw std::invalid_argument("relative unit not found: " + reg.relativeUnit);
    }
}

// ---------------- Formatters -----------------

static std::string formatDouble(double v, int precision = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << v;
    return oss.str();
}

std::string TableFormatter::format(const ConversionQuery& query,
                                   const std::vector<std::pair<std::string, double>>& results,
                                   const std::string& /*baseUnit*/) const {
    std::ostringstream oss;
    for (const auto& kv : results) {
        oss << formatDouble(query.value) << " " << query.unitName
            << " = " << formatDouble(kv.second) << " " << kv.first << "\n";
    }
    return oss.str();
}

std::string CsvFormatter::format(const ConversionQuery& query,
                                 const std::vector<std::pair<std::string, double>>& results,
                                 const std::string& baseUnit) const {
    std::ostringstream oss;
    (void)baseUnit; // not used currently
    oss << "source_unit,source_value,target_unit,target_value\n";
    for (const auto& kv : results) {
        oss << query.unitName << "," << formatDouble(query.value)
            << "," << kv.first << "," << formatDouble(kv.second) << "\n";
    }
    return oss.str();
}

std::string JsonFormatter::format(const ConversionQuery& query,
                                  const std::vector<std::pair<std::string, double>>& results,
                                  const std::string& baseUnit) const {
    // Minimal JSON building without external deps
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"baseUnit\": \"" << baseUnit << "\",\n";
    oss << "  \"source\": { \"unit\": \"" << query.unitName << "\", \"value\": " << formatDouble(query.value) << " },\n";
    oss << "  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& kv = results[i];
        oss << "    { \"unit\": \"" << kv.first << "\", \"value\": " << formatDouble(kv.second) << " }";
        if (i + 1 < results.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

// ---------------- Backward-compat helpers (compat) -----------------

namespace compat {
    double to_meter(const std::string& unit, double value) {
        if (unit == "meter") {
            return value;
        } else if (unit == "feet") {
            return value / FEET_PER_METER;
        } else if (unit == "yard") {
            return value / YARDS_PER_METER;
        }
        // Unknown unit: keep behavior close to original (no explicit error). Return 0.
        return 0.0;
    }

    double from_meter(const std::string& unit, double meterValue) {
        if (unit == "meter") {
            return meterValue;
        } else if (unit == "feet") {
            return meterValue * FEET_PER_METER;
        } else if (unit == "yard") {
            return meterValue * YARDS_PER_METER;
        }
        return 0.0;
    }

    void compute_all(const std::string& unit,
                     double value,
                     double& outMeters,
                     double& outFeet,
                     double& outYards) {
        double meter_value = to_meter(unit, value);
        outMeters = meter_value;
        outFeet = from_meter("feet", meter_value);
        outYards = from_meter("yard", meter_value);
    }
}


