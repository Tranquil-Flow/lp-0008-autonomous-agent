#include "spending_gate.h"
#include "agent_config.h"
#include "persistence.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {

std::string normalizeHexLe16(std::string hex)
{
    if (hex.size() > 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex = hex.substr(2);
    }
    if (hex.size() > 32) {
        throw std::invalid_argument("amount_le16_too_long");
    }
    for (char c : hex) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            throw std::invalid_argument("amount_le16_invalid_hex");
        }
    }
    while (hex.size() < 32) hex += '0';
    return hex;
}

uint64_t parsePositiveInt64(const std::string& text, const char* field)
{
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); })) {
        throw std::invalid_argument(std::string("invalid_") + field);
    }
    unsigned long long v = std::stoull(text);
    if (v == 0) {
        throw std::invalid_argument(std::string("invalid_") + field);
    }
    return static_cast<uint64_t>(v);
}

std::string stripLeadingZeros(std::string s)
{
    while (s.size() > 1 && s[0] == '0') s.erase(s.begin());
    return s;
}

std::string addDecimal(const std::string& a, const std::string& b)
{
    if (!std::all_of(a.begin(), a.end(), [](unsigned char c) { return std::isdigit(c); }) ||
        !std::all_of(b.begin(), b.end(), [](unsigned char c) { return std::isdigit(c); })) {
        throw std::invalid_argument("invalid_decimal_amount");
    }
    std::string out;
    int i = static_cast<int>(a.size()) - 1;
    int j = static_cast<int>(b.size()) - 1;
    int carry = 0;
    while (i >= 0 || j >= 0 || carry) {
        int digit = carry;
        if (i >= 0) digit += a[static_cast<size_t>(i--)] - '0';
        if (j >= 0) digit += b[static_cast<size_t>(j--)] - '0';
        out.push_back(static_cast<char>('0' + (digit % 10)));
        carry = digit / 10;
    }
    std::reverse(out.begin(), out.end());
    return stripLeadingZeros(out);
}

nlohmann::json failClosed(const std::string& reason, const std::string& detail = {})
{
    nlohmann::json r;
    r["allowed"] = false;
    r["reason"] = reason;
    if (!detail.empty()) r["detail"] = detail;
    return r;
}

} // namespace

SpendingGate& spendingGate()
{
    static SpendingGate gate;
    return gate;
}

std::string SpendingGate::le16ToDecimal(const std::string& le16_hex)
{
    const std::string hex = normalizeHexLe16(le16_hex);

    // LE16 = 32 hex chars, little-endian 128-bit. The current spending gate
    // intentionally supports amounts that fit in uint64_t, because the config
    // threshold fields are decimal strings for small autonomous-agent budgets.
    // If the high 64 bits are non-zero, fail closed rather than truncating and
    // disagreeing with the wallet FFI's full 128-bit transfer amount.
    for (size_t i = 16; i < 32; ++i) {
        if (hex[i] != '0') {
            throw std::out_of_range("amount_le16_exceeds_supported_64bit_range");
        }
    }

    uint64_t value = 0;
    for (int i = 0; i < 8; i++) {
        std::string byteStr = hex.substr(static_cast<size_t>(i * 2), 2);
        uint64_t byteVal = std::stoul(byteStr, nullptr, 16);
        value |= (byteVal << (i * 8));
    }

    return std::to_string(value);
}

int SpendingGate::compareDecimal(const std::string& a, const std::string& b)
{
    if (!std::all_of(a.begin(), a.end(), [](unsigned char c) { return std::isdigit(c); }) ||
        !std::all_of(b.begin(), b.end(), [](unsigned char c) { return std::isdigit(c); })) {
        throw std::invalid_argument("invalid_decimal_amount");
    }
    std::string sa = stripLeadingZeros(a);
    std::string sb = stripLeadingZeros(b);

    if (sa.size() != sb.size()) return sa.size() < sb.size() ? -1 : 1;
    if (sa < sb) return -1;
    if (sa > sb) return 1;
    return 0;
}

std::string SpendingGate::sumCurrentPeriod() const
{
    auto& cfg = agentConfig();
    int64_t period_sec = static_cast<int64_t>(parsePositiveInt64(cfg.period_seconds, "period_seconds"));
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::string total = "0";
    for (const auto& r : records_) {
        if (now - r.timestamp < period_sec) {
            total = addDecimal(total, le16ToDecimal(r.amount_le16));
        }
    }
    return total;
}

void SpendingGate::loadPersistedLocked() const
{
    if (loaded_) return;
    records_.clear();
    auto data = agent_persistence::loadJsonFile(
        agent_persistence::spendHistoryPath()
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
        try {
            arr.push_back({{"amount_le16", r.amount_le16}, {"amount", le16ToDecimal(r.amount_le16)}, {"timestamp", r.timestamp}});
        } catch (const std::exception& e) {
            arr.push_back({{"amount_le16", r.amount_le16}, {"amount_error", e.what()}, {"timestamp", r.timestamp}});
        }
    }
    agent_persistence::saveJsonFile(agent_persistence::spendHistoryPath(), nlohmann::json{{"records", arr}});
}

void SpendingGate::pruneExpired()
{
    std::lock_guard<std::mutex> lock(mutex_);
    loadPersistedLocked();
    auto& cfg = agentConfig();
    int64_t period_sec = static_cast<int64_t>(parsePositiveInt64(cfg.period_seconds, "period_seconds"));
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

    try {
        std::string amount_dec = le16ToDecimal(amount_le16);

        if (compareDecimal(amount_dec, cfg.per_tx_limit) > 0) {
            nlohmann::json r;
            r["allowed"] = false;
            r["reason"] = "exceeds_per_tx_limit";
            r["amount"] = amount_dec;
            r["per_tx_limit"] = cfg.per_tx_limit;
            r["current_period_total"] = sumCurrentPeriod();
            return r.dump();
        }

        std::string current = sumCurrentPeriod();
        std::string projected_str = addDecimal(current, amount_dec);

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
    } catch (const std::exception& e) {
        auto r = failClosed("invalid_spending_gate_state", e.what());
        r["amount_le16"] = amount_le16;
        return r.dump();
    }
}

void SpendingGate::recordSpend(const std::string& amount_le16)
{
    std::lock_guard<std::mutex> lock(mutex_);
    loadPersistedLocked();
    // Validate before persisting so malformed amounts cannot poison later checks.
    (void)le16ToDecimal(amount_le16);
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

    try {
        int64_t period_sec = static_cast<int64_t>(parsePositiveInt64(cfg.period_seconds, "period_seconds"));
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
    } catch (const std::exception& e) {
        nlohmann::json result;
        result["records"] = nlohmann::json::array();
        result["period_total"] = "0";
        result["period_limit"] = cfg.per_period_limit;
        result["period_seconds"] = cfg.period_seconds;
        result["error"] = "invalid_spending_gate_state";
        result["detail"] = e.what();
        return result.dump();
    }
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
