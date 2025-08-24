#pragma once

#include <string>
#include "converter.h"

// Minimal JSON loader without external dependency: expects a very simple JSON schema
// For robustness in production, switch to nlohmann::json
class ConfigLoader {
public:
    // Returns true on success; on failure, leaves registry unchanged and returns false
    bool loadFromJson(const std::string& path, UnitRegistry& registry, std::string* errorMessage = nullptr) const;
};


