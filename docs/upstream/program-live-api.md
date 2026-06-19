# LP-0008 program.* live API boundary

Status: investigated and fail-closed.

## Finding

No in-process LEZ program SDK or C ABI is exposed to Logos Core modules in the
LP-0008 integration surface. The working rc3 SPEL path exists outside the module
process through `spel`/`lgs`/Rust harnesses, but that path is a host-side driver,
not a stable module-safe API that `agent_module.lgx` can call from Logos Core.

## Agent behavior

- `program.query` returns `mode=simulated` with an explicit note that live query
  is unavailable through the module ABI.
- program.call fails closed with `submitted=false` and
  `error=live_program_call_not_available`.
- `program.deploy` validates the binary path but fails closed with
  `deployed=false` and `error=live_program_deploy_not_available`.

This replaces the earlier synthetic hash/deployed-success behavior so reviewer
evidence cannot be mistaken for a live LEZ program transaction.

## Compatible external path

The rc3 SPEL path exists: LP-0013 proved that `spel` rc3 plus `lgs deploy` and a
Rust lifecycle harness can deploy and call LEZ programs against a compatible
sequencer. LP-0008 can interoperate with those deployed program IDs at the task
planning/documentation layer, but the agent module does not yet have a safe
in-process call/deploy API.

## Upstream request

Expose a module-safe program client, analogous to the wallet FFI surface, with:

1. program deploy(binary) -> program_id / tx_hash,
2. program call(program_id, instruction bytes/accounts, payer) -> tx_hash,
3. program query(program_id, account/state selector) -> bytes/JSON,
4. deterministic error returns rather than host CLI parsing,
5. version pinning compatible with public testnet rc3.

Until that exists, LP-0008 treats program live execution as an upstream API gap,
not as completed functionality.
