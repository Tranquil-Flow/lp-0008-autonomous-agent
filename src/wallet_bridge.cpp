#include "wallet_bridge.h"

#ifdef __APPLE__

// wallet_ffi.h is a C header without an extern "C" guard; wrap it so the
// wallet_ffi_* symbols keep C linkage and match the exports of libwallet_ffi.
extern "C" {
#include <wallet_ffi.h>
}

#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;

namespace agent {
namespace {

// Convert a base58 account id to FfiBytes32. Returns false on parse failure.
bool idFromBase58(const std::string& b58, FfiBytes32& out) {
    return wallet_ffi_account_id_from_base58(b58.c_str(), &out) == SUCCESS;
}

std::string idToBase58(const FfiBytes32& id) {
    char* s = wallet_ffi_account_id_to_base58(&id);
    if (!s) return {};
    std::string out(s);
    wallet_ffi_free_string(s);
    return out;
}

// Map a WalletFfiError code to a stable string. rc3 has no get_last_error API,
// so this is how callers surface failure detail.
const char* errName(WalletFfiError e) {
    switch (e) {
        case SUCCESS: return "success";
        case NULL_POINTER: return "null_pointer";
        case INVALID_UTF8: return "invalid_utf8";
        case WALLET_NOT_INITIALIZED: return "wallet_not_initialized";
        case CONFIG_ERROR: return "config_error";
        case STORAGE_ERROR: return "storage_error";
        case NETWORK_ERROR: return "network_error";
        case ACCOUNT_NOT_FOUND: return "account_not_found";
        case KEY_NOT_FOUND: return "key_not_found";
        case INSUFFICIENT_FUNDS: return "insufficient_funds";
        case INVALID_ACCOUNT_ID: return "invalid_account_id";
        case RUNTIME_ERROR: return "runtime_error";
        case PASSWORD_REQUIRED: return "password_required";
        case SYNC_ERROR: return "sync_error";
        case SERIALIZATION_ERROR: return "serialization_error";
        case INVALID_TYPE_CONVERSION: return "invalid_type_conversion";
        case INVALID_KEY_VALUE: return "invalid_key_value";
        case INTERNAL_ERROR: return "internal_error";
        default: return "unknown_error";
    }
}

// Build a TxResult from an FFI transfer result, freeing FFI memory.
WalletBridge::TxResult takeTxResult(FfiTransferResult& r, const std::string& errFallback) {
    WalletBridge::TxResult out;
    out.ok = r.success;
    if (r.success && r.tx_hash) {
        out.tx_hash = r.tx_hash;
    } else {
        out.error = errFallback;
    }
    wallet_ffi_free_transfer_result(&r);
    return out;
}

}  // namespace

WalletBridge::~WalletBridge() {
    if (handle_) {
        wallet_ffi_destroy(handle_);
        handle_ = nullptr;
    }
}

std::string WalletBridge::lastError() const {
    return errName(static_cast<WalletFfiError>(last_err_));
}

bool WalletBridge::init(const std::string& state_dir,
                        const std::string& sequencer_addr,
                        const std::string& password) {
    // rc3 auto-initialises its runtime on first wallet open/create — there is
    // no explicit wallet_ffi_init_runtime() in this API version.
    std::error_code ec;
    fs::create_directories(state_dir, ec);

    const std::string configPath = state_dir + "/wallet_config.json";
    // wallet-ffi persists JSON wallet state to the exact storage_path. Using a
    // path without the .json file convention leaves direct C-ABI invocations
    // unable to reopen the same identity across process boundaries.
    const std::string storagePath = state_dir + "/storage.json";

    // Write a minimal wallet config pointing at the sequencer if none exists.
    if (!fs::exists(configPath)) {
        std::ofstream cfg(configPath);
        cfg << "{\n"
            << "  \"sequencer_addr\": \"" << sequencer_addr << "\",\n"
            << "  \"seq_poll_timeout\": \"30s\",\n"
            << "  \"seq_tx_poll_max_blocks\": 15,\n"
            << "  \"seq_poll_max_retries\": 10,\n"
            << "  \"seq_block_poll_max_amount\": 100,\n"
            << "  \"initial_accounts\": []\n"
            << "}\n";
    }

    // Open an existing wallet, or create one on first run.
    if (fs::exists(storagePath)) {
        handle_ = wallet_ffi_open(configPath.c_str(), storagePath.c_str());
    }
    if (!handle_) {
        handle_ = wallet_ffi_create_new(configPath.c_str(), storagePath.c_str(), password.c_str());
    }
    return handle_ != nullptr;
}

std::optional<std::pair<std::string, bool>> WalletBridge::findOwnedAccount(const std::string& preferred_b58) {
    if (!handle_) return std::nullopt;
    FfiAccountList list{};
    if (wallet_ffi_list_accounts(handle_, &list) != SUCCESS) return std::nullopt;

    std::optional<std::pair<std::string, bool>> found_private;
    std::optional<std::pair<std::string, bool>> found_any;
    std::optional<std::pair<std::string, bool>> preferred;
    for (uintptr_t i = 0; i < list.count; ++i) {
        const bool is_public = list.entries[i].is_public;
        auto id = idToBase58(list.entries[i].account_id);
        if (!found_any) found_any = std::make_pair(id, is_public);
        if (!is_public && !found_private) found_private = std::make_pair(id, false);
        if (!preferred_b58.empty() && id == preferred_b58) {
            preferred = std::make_pair(id, is_public);
            break;
        }
    }
    wallet_ffi_free_account_list(&list);
    if (preferred) return preferred;
    if (found_private) return found_private;
    return found_any;
}

std::optional<std::string> WalletBridge::ensureShieldedAccount(const std::string& preferred_b58) {
    if (!handle_) return std::nullopt;

    if (auto acct = findOwnedAccount(preferred_b58)) {
        if (!acct->second) return acct->first;
    }

    // Otherwise mint a fresh shielded account.
    FfiBytes32 id{};
    if (wallet_ffi_create_account_private(handle_, &id) != SUCCESS) return std::nullopt;
    wallet_ffi_save(handle_);
    return idToBase58(id);
}

std::optional<std::string> WalletBridge::balanceDecimal(const std::string& account_b58, bool is_public) {
    if (!handle_) return std::nullopt;
    FfiBytes32 id{};
    if (!idFromBase58(account_b58, id)) return std::nullopt;

    uint8_t bal[16] = {0};
    WalletFfiError e = wallet_ffi_get_balance(handle_, &id, is_public, &bal);
    last_err_ = e;
    if (e != SUCCESS) return std::nullopt;

    std::array<uint8_t, 16> le{};
    for (int i = 0; i < 16; ++i) le[i] = bal[i];
    return decimalFromLe16(le);
}

WalletBridge::TxResult WalletBridge::transferPublic(const std::string& from_b58,
                                                    const std::string& to_b58,
                                                    const std::array<uint8_t, 16>& amount_le) {
    if (!handle_) return {false, "", "wallet_not_live"};
    FfiBytes32 from{}, to{};
    if (!idFromBase58(from_b58, from) || !idFromBase58(to_b58, to))
        return {false, "", "invalid_account_id"};

    uint8_t amt[16];
    for (int i = 0; i < 16; ++i) amt[i] = amount_le[i];

    FfiTransferResult r{};
    WalletFfiError err = wallet_ffi_transfer_public(handle_, &from, &to, &amt, &r);
    last_err_ = err;
    if (err != SUCCESS && !r.success) return {false, "", errName(err)};
    return takeTxResult(r, errName(err));
}

WalletBridge::TxResult WalletBridge::transferShieldedOwned(const std::string& from_b58,
                                                           const std::string& to_b58,
                                                           const std::array<uint8_t, 16>& amount_le) {
    if (!handle_) return {false, "", "wallet_not_live"};
    FfiBytes32 from{}, to{};
    if (!idFromBase58(from_b58, from) || !idFromBase58(to_b58, to))
        return {false, "", "invalid_account_id"};

    uint8_t amt[16];
    for (int i = 0; i < 16; ++i) amt[i] = amount_le[i];

    FfiTransferResult r{};
    WalletFfiError err = wallet_ffi_transfer_shielded_owned(handle_, &from, &to, &amt, &r);
    last_err_ = err;
    if (err != SUCCESS && !r.success) return {false, "", errName(err)};
    return takeTxResult(r, errName(err));
}

WalletBridge::TxResult WalletBridge::transferDeshielded(const std::string& from_b58,
                                                        const std::string& to_b58,
                                                        const std::array<uint8_t, 16>& amount_le) {
    if (!handle_) return {false, "", "wallet_not_live"};
    FfiBytes32 from{}, to{};
    if (!idFromBase58(from_b58, from) || !idFromBase58(to_b58, to))
        return {false, "", "invalid_account_id"};

    uint8_t amt[16];
    for (int i = 0; i < 16; ++i) amt[i] = amount_le[i];

    FfiTransferResult r{};
    WalletFfiError err = wallet_ffi_transfer_deshielded(handle_, &from, &to, &amt, &r);
    last_err_ = err;
    if (err != SUCCESS && !r.success) return {false, "", errName(err)};
    return takeTxResult(r, errName(err));
}

uint64_t WalletBridge::currentBlockHeight() {
    if (!handle_) return 0;
    uint64_t h = 0;
    if (wallet_ffi_get_current_block_height(handle_, &h) != SUCCESS) return 0;
    return h;
}

bool WalletBridge::syncToCurrentBlock() {
    if (!handle_) return false;
    uint64_t h = currentBlockHeight();
    if (h == 0) return false;
    std::fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) dup2(devnull, STDOUT_FILENO);

    WalletFfiError err = wallet_ffi_sync_to_block(handle_, h);

    std::fflush(stdout);
    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (devnull >= 0) close(devnull);
    last_err_ = err;
    return err == SUCCESS;
}

std::string WalletBridge::sequencerAddr() {
    if (!handle_) return {};
    char* s = wallet_ffi_get_sequencer_addr(handle_);
    if (!s) return {};
    std::string out(s);
    wallet_ffi_free_string(s);
    return out;
}

void WalletBridge::save() {
    if (handle_) wallet_ffi_save(handle_);
}

}  // namespace agent

#else

namespace agent {

WalletBridge::~WalletBridge() = default;

std::string WalletBridge::lastError() const {
    return "wallet_ffi_unavailable_on_this_platform";
}

bool WalletBridge::init(const std::string&, const std::string&, const std::string&) {
    last_err_ = -1;
    return false;
}

std::optional<std::pair<std::string, bool>> WalletBridge::findOwnedAccount(const std::string&) {
    return std::nullopt;
}

std::optional<std::string> WalletBridge::ensureShieldedAccount(const std::string&) {
    return std::nullopt;
}

std::optional<std::string> WalletBridge::balanceDecimal(const std::string&, bool) {
    return std::nullopt;
}

WalletBridge::TxResult WalletBridge::transferPublic(const std::string&, const std::string&, const std::array<uint8_t, 16>&) {
    return {false, "", "wallet_ffi_unavailable_on_this_platform"};
}

WalletBridge::TxResult WalletBridge::transferShieldedOwned(const std::string&, const std::string&, const std::array<uint8_t, 16>&) {
    return {false, "", "wallet_ffi_unavailable_on_this_platform"};
}

WalletBridge::TxResult WalletBridge::transferDeshielded(const std::string&, const std::string&, const std::array<uint8_t, 16>&) {
    return {false, "", "wallet_ffi_unavailable_on_this_platform"};
}

uint64_t WalletBridge::currentBlockHeight() {
    return 0;
}

bool WalletBridge::syncToCurrentBlock() {
    return false;
}

std::string WalletBridge::sequencerAddr() {
    return {};
}

void WalletBridge::save() {}

}  // namespace agent

#endif

namespace agent {
std::array<uint8_t, 16> WalletBridge::le16FromHex(const std::string& hex) {
    std::string h = hex;
    if (h.size() > 2 && h[0] == '0' && (h[1] == 'x' || h[1] == 'X')) h = h.substr(2);
    std::array<uint8_t, 16> out{};
    for (size_t i = 0; i < 16 && (i * 2 + 1) < h.size(); ++i) {
        auto nyb = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        out[i] = static_cast<uint8_t>((nyb(h[i * 2]) << 4) | nyb(h[i * 2 + 1]));
    }
    return out;
}

std::string WalletBridge::decimalFromLe16(const std::array<uint8_t, 16>& le) {
    unsigned __int128 v = 0;
    for (int i = 15; i >= 0; --i) v = (v << 8) | le[i];
    if (v == 0) return "0";
    char buf[40];
    int pos = sizeof(buf);
    buf[--pos] = '\0';
    while (v > 0) {
        buf[--pos] = static_cast<char>('0' + static_cast<int>(v % 10));
        v /= 10;
    }
    return std::string(&buf[pos]);
}

}  // namespace agent
