#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

// Core domain types
struct Unit {
    std::string name;             // Unit name, e.g., "meter"
    double toBaseFactor;          // How many base units in 1 of this unit. e.g., 1 feet = 0.3048 meter -> toBaseFactor=0.3048
};

struct ConversionQuery {
    std::string unitName;         // Source unit name
    double value;                 // Source value (>= 0)
};

// Registry of units relative to a base unit (e.g., meter)
class UnitRegistry {
public:
    explicit UnitRegistry(const std::string& baseUnitName);

    const std::string& baseUnit() const;

    // Register a unit by specifying how many base units are in 1 unit
    // Example: addUnitFromBase("feet", 0.3048) means 1 feet = 0.3048 meter
    void addUnitFromBase(const std::string& unitName, double toBaseFactor);

    // Register a unit relative to an existing unit
    // Example: addUnitRelative("cubit", 0.4572, "meter") means 1 cubit = 0.4572 meter
    void addUnitRelative(const std::string& unitName, double ratio, const std::string& relativeUnitName);

    bool has(const std::string& unitName) const;
    const Unit& get(const std::string& unitName) const; // throws std::out_of_range if not found
    std::vector<Unit> list() const;

private:
    std::string baseUnitName_;
    std::unordered_map<std::string, Unit> nameToUnit_;
};

// Performs conversions via the base unit
class ConversionService {
public:
    explicit ConversionService(const UnitRegistry& registry);

    double convert(double value, const std::string& fromUnit, const std::string& toUnit) const;

    // Convert to all registered units. If includeSource=false, exclude the source unit from the result list.
    std::vector<std::pair<std::string, double>> convertToAll(double value, const std::string& fromUnit, bool includeSource = false) const;

private:
    const UnitRegistry& registry_;
};

// Parses user inputs (query: "unit:value", registration: "1 X = r Y")
class InputParser {
public:
    ConversionQuery parseQuery(const std::string& input) const; // throws std::invalid_argument

    struct Registration {
        std::string unitName;     // e.g., "cubit"
        double ratio;             // e.g., 0.4572
        std::string relativeUnit; // e.g., "meter"
    };

    Registration parseRegistration(const std::string& input) const; // throws std::invalid_argument
};

// Validates inputs and registry-related constraints
class Validator {
public:
    void validateQuery(const ConversionQuery& query, const UnitRegistry& registry) const;      // throws std::invalid_argument
    void validateRegistration(const InputParser::Registration& reg, const UnitRegistry& registry) const; // throws std::invalid_argument
};

// Output formatting strategy
class FormatStrategy {
public:
    virtual ~FormatStrategy() = default;
    virtual std::string format(const ConversionQuery& query,
                               const std::vector<std::pair<std::string, double>>& results,
                               const std::string& baseUnit) const = 0;
};

class TableFormatter : public FormatStrategy {
public:
    std::string format(const ConversionQuery& query,
                       const std::vector<std::pair<std::string, double>>& results,
                       const std::string& baseUnit) const override;
};

class CsvFormatter : public FormatStrategy {
public:
    std::string format(const ConversionQuery& query,
                       const std::vector<std::pair<std::string, double>>& results,
                       const std::string& baseUnit) const override;
};

class JsonFormatter : public FormatStrategy {
public:
    std::string format(const ConversionQuery& query,
                       const std::vector<std::pair<std::string, double>>& results,
                       const std::string& baseUnit) const override;
};

// ------------------------------------------------------------
// Backward-compatibility helpers to work with the existing main
// These helpers mirror the current constants and flow used in unit_converter.cpp
namespace compat {
    static constexpr double FEET_PER_METER = 3.28084;  // matches README/main
    static constexpr double YARDS_PER_METER = 1.09361; // matches README/main

    // Convert input value in unit -> meters, supporting {meter, feet, yard}
    double to_meter(const std::string& unit, double value);

    // Convert value in meters -> target unit, supporting {meter, feet, yard}
    double from_meter(const std::string& unit, double meterValue);

    // Compute three outputs exactly like the existing main's print order
    void compute_all(const std::string& unit,
                     double value,
                     double& outMeters,
                     double& outFeet,
                     double& outYards);
}


