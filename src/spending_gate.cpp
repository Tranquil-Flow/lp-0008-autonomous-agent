#include "spending_gate.h"
#include "agent_config.h"
#include "persistence.h"
#include <nlohmann/json.hpp>

SpendingGate& spendingGate()
{
    static SpendingGate gate;
    return gate;
}

std::string SpendingGate::le16ToDecimal(const std::string& le16_hex)
{
    // LE16 = 32 hex chars, little-endian 128-bit.
    // For simplicity we handle up to 64-bit values (first 16 hex chars = 8 bytes LE).
    // Full 128-bit would need bigint; for demo this is sufficient.
    std::string hex = le16_hex;
    // Remove 0x prefix if present
    if (hex.size() > 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))
        hex = hex.substr(2);

    // Pad to 32 chars
    while (hex.size() < 32) hex += '0';

    // Convert LE hex to bytes, then to uint64
    // LE means first byte is least significant
    uint64_t value = 0;
    for (int i = 0; i < 8 && i * 2 < (int)hex.size(); i++) {
        // Each pair of hex chars = 1 byte, LE order
        std::string byteStr = hex.substr(i * 2, 2);
        uint64_t byteVal = std::stoul(byteStr, nullptr, 16);
        value |= (byteVal << (i * 8));
    }

    return std::to_string(value);
}

int SpendingGate::compareDecimal(const std::string& a, const std::string& b)
{
    // Simple decimal string comparison (both are non-negative integers)
    // Strip leading zeros
    std::string sa = a; while (sa.size() > 1 && sa[0] == '0') sa = sa.substr(1);
    std::string sb = b; while (sb.size() > 1 && sb[0] == '0') sb = sb.substr(1);

    if (sa.size() != sb.size()) return sa.size() < sb.size() ? -1 : 1;
    if (sa < sb) return -1;
    if (sa > sb) return 1;
    return 0;
}

std::string SpendingGate::sumCurrentPeriod() const
{
    // Sum all records within the rolling window
    auto& cfg = agentConfig();
    int64_t period_sec = std::stoll(cfg.period_seconds);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    uint64_t total = 0;
    for (const auto& r : records_) {
        if (now - r.timestamp < period_sec) {
            uint64_t amt = std::stoull(le16ToDecimal(r.amount_le16));
            total += amt;
        }
    }
    return std::to_string(total);
}

void SpendingGate::loadPersistedLocked() const
{
    if (loaded_) return;
    records_.clear();
    auto data = agent_persistence::loadJsonFile(
        agent_persistence::spendHistoryPath(),
        nlohmann::json{{"records", nlohmann::json::array()}}
    );
    if (data.contains("records") && data["records"].is_array()) {
        for (const auto& item : data["records"]) {
            SpendRecord r;
            r.amount_le16 = item.value("amount_le16", "");
            r.timestamp = item.value("timestamp", int64_t{0});
            if (!r.amount_le16.empty() && r.timestamp > 0) records_.push_back(r);
        }
    }
    loaded_ = true;
}

void SpendingGate::savePersistedLocked() const
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& r : records_) {
        arr.push_back({{"amount_le16", r.amount_le16}, {"amount", le16ToDecimal(r.amount_le16)}, {"timestamp", r.timestamp}});
    }
    agent_persistence::saveJsonFile(agent_persistence::spendHistoryPath(), nlohmann::json{{"records", arr}});
}

void SpendingGate::pruneExpired()
{
    std::lock_guard<std::mutex> lock(mutex_);
    loadPersistedLocked();
    auto& cfg = agentConfig();
    int64_t period_sec = std::stoll(cfg.period_seconds);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::vector<SpendRecord> kept;
    for (const auto& r : records_) {
        if (now - r.timestamp < period_sec) {
            kept.push_back(r);
        }
    }
    records_ = std::move(kept);
    savePersistedLocked();
}

std::string SpendingGate::checkSpend(const std::string& amount_le16) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    loadPersistedLocked();
    auto& cfg = agentConfig();

    std::string amount_dec = le16ToDecimal(amount_le16);

    // Check per-tx limit
    if (compareDecimal(amount_dec, cfg.per_tx_limit) > 0) {
        nlohmann::json r;
        r["allowed"] = false;
        r["reason"] = "exceeds_per_tx_limit";
        r["amount"] = amount_dec;
        r["per_tx_limit"] = cfg.per_tx_limit;
        r["current_period_total"] = sumCurrentPeriod();
        return r.dump();
    }

    // Check per-period limit (current total + proposed)
    std::string current = sumCurrentPeriod();
    uint64_t current_val = std::stoull(current);
    uint64_t amount_val = std::stoull(amount_dec);
    uint64_t projected = current_val + amount_val;
    std::string projected_str = std::to_string(projected);

    if (compareDecimal(projected_str, cfg.per_period_limit) > 0) {
        nlohmann::json r;
        r["allowed"] = false;
        r["reason"] = "exceeds_per_period_limit";
        r["amount"] = amount_dec;
        r["current_period_total"] = current;
        r["projected_total"] = projected_str;
        r["per_period_limit"] = cfg.per_period_limit;
        return r.dump();
    }

    nlohmann::json r;
    r["allowed"] = true;
    r["reason"] = "within_limits";
    r["amount"] = amount_dec;
    r["current_period_total"] = current;
    return r.dump();
}

void SpendingGate::recordSpend(const std::string& amount_le16)
{
    std::lock_guard<std::mutex> lock(mutex_);
    loadPersistedLocked();
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    records_.push_back(SpendRecord{amount_le16, now});
    savePersistedLocked();
}

std::string SpendingGate::getHistory() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    loadPersistedLocked();
    auto& cfg = agentConfig();
    int64_t period_sec = std::stoll(cfg.period_seconds);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& r : records_) {
        if (now - r.timestamp < period_sec) {
            nlohmann::json obj;
            obj["amount"] = le16ToDecimal(r.amount_le16);
            obj["timestamp"] = r.timestamp;
            arr.push_back(obj);
        }
    }

    nlohmann::json result;
    result["records"] = arr;
    result["period_total"] = sumCurrentPeriod();
    result["period_limit"] = cfg.per_period_limit;
    result["period_seconds"] = cfg.period_seconds;
    return result.dump();
}

std::string SpendingGate::getThresholds() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    loadPersistedLocked();
    auto& cfg = agentConfig();
    nlohmann::json r;
    r["per_tx_limit"] = cfg.per_tx_limit;
    r["per_period_limit"] = cfg.per_period_limit;
    r["period_seconds"] = cfg.period_seconds;
    return r.dump();
}
