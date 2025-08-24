#include "config_loader.h"

#include <fstream>
#include <sstream>
#include <cctype>

namespace {
    static inline std::string trim(const std::string& s) {
        size_t b = 0;
        while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
        size_t e = s.size();
        while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
        return s.substr(b, e - b);
    }
}

// Very small hand-rolled parser sufficient for the expected schema.
// Schema:
// {
//   "baseUnit": "meter",
//   "units": { "meter": 1.0, "feet": 0.3048, "yard": 0.9144 }
// }
bool ConfigLoader::loadFromJson(const std::string& path, UnitRegistry& registry, std::string* errorMessage) const {
    std::ifstream ifs(path);
    if (!ifs) {
        if (errorMessage) *errorMessage = "cannot open file: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string json = ss.str();

    // naive search; not robust to arbitrary JSON
    auto findString = [&](const std::string& key, std::string& out) -> bool {
        std::string pat = "\"" + key + "\"";
        auto p = json.find(pat);
        if (p == std::string::npos) return false;
        p = json.find(':', p);
        if (p == std::string::npos) return false;
        p = json.find('"', p);
        if (p == std::string::npos) return false;
        auto q = json.find('"', p + 1);
        if (q == std::string::npos) return false;
        out = json.substr(p + 1, q - (p + 1));
        return true;
    };

    std::string baseUnit;
    if (!findString("baseUnit", baseUnit)) {
        if (errorMessage) *errorMessage = "missing baseUnit";
        return false;
    }

    // Find units object
    std::string unitsKey = "\"units\"";
    auto u = json.find(unitsKey);
    if (u == std::string::npos) {
        if (errorMessage) *errorMessage = "missing units";
        return false;
    }
    auto open = json.find('{', u);
    auto close = json.find('}', open);
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        if (errorMessage) *errorMessage = "invalid units object";
        return false;
    }
    std::string body = json.substr(open + 1, close - open - 1);

    // Parse entries of the form  "name": number
    UnitRegistry newReg(baseUnit);
    newReg.addUnitFromBase(baseUnit, 1.0);

    size_t pos = 0;
    while (pos < body.size()) {
        // find key
        size_t k1 = body.find('"', pos);
        if (k1 == std::string::npos) break;
        size_t k2 = body.find('"', k1 + 1);
        if (k2 == std::string::npos) break;
        std::string name = body.substr(k1 + 1, k2 - (k1 + 1));
        size_t colon = body.find(':', k2);
        if (colon == std::string::npos) break;
        size_t vstart = colon + 1;
        while (vstart < body.size() && std::isspace(static_cast<unsigned char>(body[vstart]))) vstart++;
        size_t vend = vstart;
        while (vend < body.size() && (std::isdigit(static_cast<unsigned char>(body[vend])) || body[vend] == '.' || body[vend] == 'e' || body[vend] == 'E' || body[vend] == '+' || body[vend] == '-')) vend++;
        std::string num = body.substr(vstart, vend - vstart);
        double value = std::strtod(num.c_str(), nullptr);

        if (name != baseUnit) {
            if (!(value > 0.0)) {
                if (errorMessage) *errorMessage = "invalid factor for unit: " + name;
                return false;
            }
            newReg.addUnitFromBase(name, value);
        }
        pos = body.find(',', vend);
        if (pos == std::string::npos) break;
        pos++;
    }

    // Commit to provided registry
    registry = newReg;
    return true;
}


