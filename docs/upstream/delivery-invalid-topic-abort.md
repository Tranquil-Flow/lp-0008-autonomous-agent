# delivery_module invalid topic abort

Context

LP-0008 Stage C found malformed LIP-23 content topics can kill delivery_module when sent directly through delivery_module.send. LP-0008 currently guards this at the agent boundary, so invalid topics fall back to local/simulated message logging and do not reach delivery_module.

Root cause

DeliveryModulePlugin::send calls outcome.getString() even when callApiRetValue("send", ...) returns success == false.

For invalid content topics, liblogosdelivery reports a failed outcome. Calling getString() on that failed outcome throws LogosResultException, crossing the module boundary uncaught and aborting the module process.

Minimal upstream fix

Return immediately when outcome.success == false before reading the success value.

Patch captured in:

- patches/logos-delivery-module-invalid-topic-nonfatal.patch

Verification evidence

Applied the one-line fix locally in refs/logos-delivery-module, rebuilt with:

    PATH=/opt/homebrew/bin:$HOME/.cargo/bin:$HOME/bin:$PATH nix build .#install --print-build-logs

Build result:

- /nix/store/2rx0gq8zhzl2qfnrjf6gwfqwvfvp07vp-logos-delivery_module-module-lib-install

Direct Logos Core repro after patch:

- loaded delivery_module
- called createNode with Edge config
- called start
- called send with invalid topic /logos/lp0008/stage-c
- send returned method failure (rc=4), but daemon stayed alive and delivery_module remained loaded

Evidence directory:

- $LP0008_TEST_ROOT/delivery_invalid_topic_fix_1781645626

Assertion output:

    ASSERT direct delivery invalid-topic failure is non-fatal: OK rc=4 base=$LP0008_TEST_ROOT/delivery_invalid_topic_fix_1781645626

Current LP-0008 mitigation

src/agent_module_impl.cpp validates LIP-23 topic shape before calling delivery_module.send. Invalid topics stay local and return mode: simulated, preserving delivery module liveness even before the upstream fix lands.
