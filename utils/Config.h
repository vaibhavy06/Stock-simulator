#pragma once
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <stdexcept>

// Lightweight INI-style config file parser
class Config {
public:
    static Config& instance() {
        static Config inst;
        return inst;
    }

    void load(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return; // Use defaults if file not found

        std::string line;
        while (std::getline(file, line)) {
            // Strip comments and whitespace
            auto hash = line.find('#');
            if (hash != std::string::npos) line = line.substr(0, hash);
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            if (line.empty()) continue;

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            key.erase(key.find_last_not_of(" \t") + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            data_[key] = val;
        }
    }

    std::string getString(const std::string& key, const std::string& def = "") const {
        auto it = data_.find(key);
        return it != data_.end() ? it->second : def;
    }

    int getInt(const std::string& key, int def = 0) const {
        auto it = data_.find(key);
        if (it == data_.end()) return def;
        try { return std::stoi(it->second); } catch (...) { return def; }
    }

    double getDouble(const std::string& key, double def = 0.0) const {
        auto it = data_.find(key);
        if (it == data_.end()) return def;
        try { return std::stod(it->second); } catch (...) { return def; }
    }

    bool getBool(const std::string& key, bool def = false) const {
        auto it = data_.find(key);
        if (it == data_.end()) return def;
        return it->second == "true" || it->second == "1" || it->second == "yes";
    }

private:
    Config() = default;
    std::unordered_map<std::string, std::string> data_;
};
