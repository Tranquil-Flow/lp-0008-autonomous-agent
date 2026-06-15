#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <mutex>

/// A single spending transaction record for threshold tracking.
struct SpendRecord {
    std::string amount_le16;  // amount in LE 16-byte hex
    int64_t timestamp;
};

/// Spending threshold gate. Blocks transactions that exceed configured limits.
/// Tracks a rolling window of spend records.
class SpendingGate {
public:
    /// Check if a proposed spend is allowed under current thresholds.
    /// Returns JSON: {"allowed": bool, "reason": "...", "current_period_total": "..."}
    std::string checkSpend(const std::string& amount_le16) const;

    /// Record a completed spend (called after a transfer succeeds).
    void recordSpend(const std::string& amount_le16);

    /// Get spending history for the current period.
    std::string getHistory() const;

    /// Get threshold config as JSON.
    std::string getThresholds() const;

    /// Clear expired records (older than period_seconds).
    void pruneExpired();

private:
    mutable std::mutex mutex_;
    mutable std::vector<SpendRecord> records_;
    mutable bool loaded_ = false;

    /// Load persisted records once per process. Caller must hold mutex_.
    void loadPersistedLocked() const;

    /// Save current records to disk. Caller must hold mutex_.
    void savePersistedLocked() const;

    /// Sum all spend amounts in the current rolling window.
    /// Returns the sum as a decimal string.
    std::string sumCurrentPeriod() const;

    /// Convert LE16 hex amount to a decimal string for comparison.
    /// LE16 = little-endian 128-bit integer in hex (32 chars).
    static std::string le16ToDecimal(const std::string& le16_hex);

    /// Compare two decimal amount strings.
    /// Returns: -1 if a < b, 0 if a == b, 1 if a > b
    static int compareDecimal(const std::string& a, const std::string& b);
};

// Global gate instance
SpendingGate& spendingGate();
