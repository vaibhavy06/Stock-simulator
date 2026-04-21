#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

struct PriceRecord {
    int64_t     timestamp;
    std::string symbol;
    double      price;
};

class CSVReader {
public:
    // Load CSV with header: timestamp,symbol,price
    // Returns records sorted by timestamp then symbol
    static std::vector<PriceRecord> load(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open())
            throw std::runtime_error("Cannot open CSV: " + filepath);

        std::vector<PriceRecord> records;
        std::string line;
        bool firstLine = true;

        while (std::getline(file, line)) {
            if (firstLine) { firstLine = false; continue; } // skip header
            if (line.empty()) continue;

            std::istringstream ss(line);
            std::string tok;
            PriceRecord rec;

            try {
                std::getline(ss, tok, ','); rec.timestamp = std::stoll(tok);
                std::getline(ss, tok, ','); rec.symbol    = tok;
                // Trim whitespace from symbol
                rec.symbol.erase(0, rec.symbol.find_first_not_of(" \t\r\n"));
                rec.symbol.erase(rec.symbol.find_last_not_of(" \t\r\n") + 1);
                std::getline(ss, tok, ','); rec.price     = std::stod(tok);
                records.push_back(std::move(rec));
            } catch (...) {
                // Skip malformed lines silently
            }
        }

        std::sort(records.begin(), records.end(),
                  [](const PriceRecord& a, const PriceRecord& b){
                      return a.timestamp < b.timestamp ||
                             (a.timestamp == b.timestamp && a.symbol < b.symbol);
                  });
        return records;
    }

    // Group records by symbol for per-stock time series
    static std::unordered_map<std::string, std::vector<PriceRecord>>
    groupBySymbol(const std::vector<PriceRecord>& records) {
        std::unordered_map<std::string, std::vector<PriceRecord>> grouped;
        for (auto& r : records) grouped[r.symbol].push_back(r);
        return grouped;
    }
};
