#pragma once

#include <string>
#include <memory>
#include <logos_module_context.h>
#include <logos_lp_client.h>   // logos::LpClient — inter-module calls

#include "wallet_bridge.h"     // agent::WalletBridge — real LEZ wallet

/**
 * @brief Autonomous AI agent module for Logos Core (LP-0008).
 *
 * Implements an autonomous AI agent with wallet, storage, and messaging
 * capabilities. Skills match the LP-0008 spec exactly:
 *
 *   storage.*  — file upload/download/list/share via storage_module
 *   messaging.*— send/join/create_group via delivery_module
 *   wallet.*   — balance/send/history via logos_execution_zone
 *   program.*  — query/call/deploy LEZ programs
 *   agent.*    — A2A-compatible agent coordination
 *   meta.*     — introspection, config, status
 *
 * Inherits LogosModuleContext for:
 *   - instancePersistencePath() — canonical state directory
 *   - modules() — typed access to co-loaded dependency modules
 *   - onContextReady() — one-time initialization hook
 *
 * Uses logos::LpClient for direct inter-module calls to storage_module
 * and delivery_module when they are co-loaded. Falls back to simulated
 * mode when sibling modules are not available.
 */
class AgentModuleImpl : public LogosModuleContext
{
public:
    AgentModuleImpl();

    // === Lifecycle ===
    std::string greet(const std::string& name);

    // === Skill Dispatch ===
    std::string dispatchSkill(const std::string& skill_name, const std::string& args_json);

    // === Meta Skills ===
    std::string metaSkills();
    std::string metaStatus();
    std::string metaConfigure(const std::string& key, const std::string& value);

    // === Storage Skills ===
    std::string storageUpload(const std::string& path, const std::string& label);
    std::string storageDownload(const std::string& address, const std::string& path);
    std::string storageList();
    std::string storageShare(const std::string& address, const std::string& recipient);

    // === Messaging Skills ===
    std::string messagingSend(const std::string& recipient, const std::string& message);
    std::string messagingJoin(const std::string& groupId);
    std::string messagingCreateGroup(const std::string& members);

    // === Wallet Skills ===
    std::string walletBalance();
    std::string walletSend(const std::string& recipient, const std::string& amountLe16);
    std::string walletHistory();
    std::string approvalList();
    std::string approvalApprove(const std::string& approvalId);
    std::string approvalReject(const std::string& approvalId, const std::string& reason);
    std::string approvalRetry(const std::string& approvalId);

    // === Program Skills ===
    std::string programQuery(const std::string& programId, const std::string& params);
    std::string programCall(const std::string& programId, const std::string& instruction, const std::string& params);
    std::string programDeploy(const std::string& binaryPath);

    // === Agent Coordination (A2A) ===
    std::string agentCard();
    std::string agentDiscover(const std::string& topic);
    std::string agentTask(const std::string& agentAddress, const std::string& skill, const std::string& params);
    std::string agentComplete(const std::string& taskId, const std::string& resultJson);
    std::string agentReceive();
    std::string agentSubscribe(const std::string& agentAddress, const std::string& taskId);
    std::string agentCancel(const std::string& agentAddress, const std::string& taskId);

protected:
    void onContextReady() override;

private:
    bool m_contextReady = false;
    bool m_storageReady = false;
    bool m_deliveryReady = false;

    // Inter-module clients (lazy-init in onContextReady)
    std::unique_ptr<logos::LpClient> m_storageClient;   // → storage_module
    std::unique_ptr<logos::LpClient> m_deliveryClient;  // → delivery_module

    // Real LEZ wallet (logos-execution-zone wallet-ffi). Falls back to
    // simulated mode when the wallet cannot be brought up.
    agent::WalletBridge m_wallet;
    std::string m_agentAccount;  // base58 of the agent wallet account ("" if simulated)
    bool m_agentAccountIsPublic = false;  // true when configured wallet_account_hex is a public owned account
    bool m_walletTried = false;  // guards one-shot lazy init

    // Helpers
    void ensureWallet();  // one-shot lazy bring-up of the LEZ wallet
    bool ensureStorageReady();
    bool ensureDeliveryReady();
    bool tryStorageCall(const std::string& method, const nlohmann::json& args, nlohmann::json& out);
    bool tryDeliveryCall(const std::string& method, const nlohmann::json& args, nlohmann::json& out);
    nlohmann::json executeWalletTransfer(const std::string& recipient, const std::string& amountLe16, const std::string& approvalId = "");
    nlohmann::json notifyOwnerApproval(const nlohmann::json& approval, const std::string& event);
};
