#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

// Opaque FFI handle, forward-declared so this header doesn't leak <wallet_ffi.h>.
struct WalletHandle;

namespace agent {

/**
 * Thin RAII wrapper over the NSSA wallet-ffi C API (logos-execution-zone).
 *
 * Gives the agent module a single shielded LEZ identity plus real on-chain
 * operations (balance, transfer, faucet) against a LEZ sequencer. Amounts are
 * 16-byte little-endian u128 — the same representation the FFI uses and the
 * same "le16" format the agent's spending gate already speaks.
 *
 * If the wallet cannot be brought up (FFI absent, bad config, unreachable
 * sequencer), live() stays false and callers fall back to simulated mode.
 * Construction never throws; failures are reported through live()/lastError().
 */
class WalletBridge {
public:
    struct TxResult {
        bool ok = false;
        std::string tx_hash;  // set on success
        std::string error;    // set on failure
    };

    WalletBridge() = default;
    ~WalletBridge();
    WalletBridge(const WalletBridge&) = delete;
    WalletBridge& operator=(const WalletBridge&) = delete;

    /**
     * Initialise the FFI runtime and open (or create) the wallet under
     * `state_dir`. If `state_dir/wallet_config.json` is absent, one is written
     * pointing at `sequencer_addr`. Returns true iff the handle is live.
     */
    bool init(const std::string& state_dir,
              const std::string& sequencer_addr,
              const std::string& password = "agent");

    bool live() const { return handle_ != nullptr; }
    std::string lastError() const;

    /** Ensure the agent owns a private (shielded) account; return its base58 id. */
    std::optional<std::string> ensureShieldedAccount(const std::string& preferred_b58 = "");

    /** Balance of `account_b58` as a decimal string (nullopt on error). */
    std::optional<std::string> balanceDecimal(const std::string& account_b58, bool is_public);

    /** Public -> public transfer from an owned account. */
    TxResult transferPublic(const std::string& from_b58,
                            const std::string& to_b58,
                            const std::array<uint8_t, 16>& amount_le);

    /** Public -> owned-private transfer (shielding; emits a RISC0 proof). */
    TxResult transferShieldedOwned(const std::string& from_b58,
                                   const std::string& to_b58,
                                   const std::array<uint8_t, 16>& amount_le);

    /**
     * Private -> public transfer (deshielding; emits a RISC0 proof). This is the
     * path wallet.send() uses to pay an arbitrary public recipient from the
     * agent's shielded identity.
     */
    TxResult transferDeshielded(const std::string& from_b58,
                                const std::string& to_b58,
                                const std::array<uint8_t, 16>& amount_le);

    uint64_t currentBlockHeight();  // 0 on error
    bool syncToCurrentBlock();
    std::string sequencerAddr();
    void save();

    // --- pure helpers (shared with skill code) ---

    /** Parse a little-endian hex amount ("0a00..", optional 0x) into 16 bytes. */
    static std::array<uint8_t, 16> le16FromHex(const std::string& hex);

    /** Full 128-bit little-endian -> decimal string (no 64-bit truncation). */
    static std::string decimalFromLe16(const std::array<uint8_t, 16>& le);

private:
    WalletHandle* handle_ = nullptr;
    mutable int last_err_ = 0;  // last WalletFfiError code (rc3 has no get_last_error API)
};

}  // namespace agent
