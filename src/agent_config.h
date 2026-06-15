#pragma once

#include <string>
#include <vector>
#include <mutex>

/// Runtime configuration for the agent module.
/// Loaded from a JSON config file or set via meta.configure.
struct AgentConfig {
    // Identity
    std::string agent_id = "default-agent";
    std::string agent_name = "Default Agent";
    std::string description = "Autonomous AI agent module";

    // Spending thresholds (LEZ amounts as strings for precision)
    std::string per_tx_limit = "1000";
    std::string per_period_limit = "10000";
    std::string period_seconds = "86400"; // 24h rolling window

    // Messaging topics
    std::string discovery_topic = "/logos/agents/v1/discovery";
    std::string owner_topic = "/logos/agents/v1/default/owner";
    std::string task_topic = "/logos/agents/v1/default/tasks";

    // Wallet
    std::string wallet_account_hex = "";

    // A2A heartbeat interval (seconds)
    int heartbeat_interval = 60;

    // Enabled skills (empty = all registered skills enabled)
    std::vector<std::string> enabled_skills;

    /// Get config as JSON string.
    std::string toJson() const;

    /// Update a single config key. Returns true if key recognized.
    bool set(const std::string& key, const std::string& value);

    /// Get a single config value by key.
    std::string get(const std::string& key) const;

    /// Persist config to disk.
    void save() const;

private:
    mutable std::mutex mutex_;
};

// Global config instance (module is single-instance by design)
AgentConfig& agentConfig();
