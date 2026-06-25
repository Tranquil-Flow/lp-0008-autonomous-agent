#!/usr/bin/env python3
from __future__ import annotations

import json, os, pathlib, subprocess, time
from typing import Any

HOME=pathlib.Path.home()
AGENT=HOME/'Projects/logos-basecamp/lp-0008-autonomous-agent'
OUT=HOME/'lp0008-phase0'
BASE=OUT/f'program_cu_boundary_{time.strftime("%Y%m%d_%H%M%S", time.gmtime())}'
BASE.mkdir(parents=True, exist_ok=True)
ENV=os.environ.copy(); ENV['PATH']=f"/opt/homebrew/bin:{HOME}/.cargo/bin:{HOME}/bin:"+ENV.get('PATH',''); ENV['LP0008_DISABLE_WALLET_FFI']='1'

def run(name: str, cmd: list[Any], cwd: pathlib.Path|None=None, timeout:int=240, check:bool=True):
    print(f"\n=== {name} ===")
    print('$ '+' '.join(map(str,cmd)))
    cp=subprocess.run([str(x) for x in cmd], cwd=str(cwd) if cwd else str(AGENT), env=ENV, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    (BASE/f'{name}.stdout').write_text(cp.stdout)
    print(cp.stdout, end='')
    print(f"[exit {cp.returncode}]")
    if check and cp.returncode:
        raise SystemExit(f'{name} failed rc={cp.returncode}')
    return cp

def call_payload(skill: str, args_json: str)->str:
    return json.dumps([skill, args_json])

def parse_cabi(name: str)->Any:
    text=(BASE/f'{name}.stdout').read_text().strip()
    if text.startswith('"'):
        text=json.loads(text)
    return json.loads(text[text.find('{'):])

run('nix-build-install', ['nix','build','.#install','--out-link','result'], timeout=600)
caller=BASE/'cabi_call'
run('build-cabi-caller', ['nix','develop','--command','c++','-std=c++17','-O2','-o',caller,'tests/cabi_call.cpp'], timeout=600)
plugin=AGENT/'result/modules/agent_module/agent_module_plugin.dylib'
if not plugin.exists(): plugin=AGENT/'result/modules/agent_module/agent_module_plugin.so'
if not plugin.exists(): raise SystemExit('missing agent_module_plugin')

# Exercise program surfaces through real plugin entrypoint. They must fail closed, never fake tx/program IDs.
run('program-query-boundary', [caller, plugin, 'dispatchSkill', call_payload('program.query', json.dumps(['prog-test','{"key":"value"}']))])
run('program-call-fail-closed', [caller, plugin, 'dispatchSkill', call_payload('program.call', json.dumps(['prog-test','vote','{"choice":"yes"}']))])
missing=BASE/'missing-program.bin'
run('program-deploy-missing-binary', [caller, plugin, 'dispatchSkill', call_payload('program.deploy', json.dumps([str(missing)]))])
binary=BASE/'dummy-program.bin'; binary.write_bytes(b'lp0008 dummy program artifact')
run('program-deploy-fail-closed', [caller, plugin, 'dispatchSkill', call_payload('program.deploy', json.dumps([str(binary)]))])

query=parse_cabi('program-query-boundary')
call=parse_cabi('program-call-fail-closed')
deploy_missing=parse_cabi('program-deploy-missing-binary')
deploy=parse_cabi('program-deploy-fail-closed')
assert query.get('mode')=='simulated' and 'live_program_query_unavailable' in query.get('note',''), query
assert call.get('mode')=='unsupported' and call.get('submitted') is False and call.get('error')=='live_program_call_not_available', call
assert deploy_missing.get('deployed') is False and deploy_missing.get('error')=='binary_not_found', deploy_missing
assert deploy.get('mode')=='unsupported' and deploy.get('deployed') is False and deploy.get('error')=='live_program_deploy_not_available', deploy

# Probe docs/source for explicit CU/proof boundary claims; fail if overclaiming fake CU or RISC0 proof.
cu=(AGENT/'docs/cu-costs.md').read_text()
api=(AGENT/'docs/upstream/program-live-api.md').read_text()
required=[
  'CU field not exposed',
  'program.call', 'live_program_call_not_available',
  'program.deploy', 'live_program_deploy_not_available',
  'RISC0_DEV_MODE=0 boundary',
  'not proven by this module',
]
missing_terms=[x for x in required if x not in cu]
if missing_terms: raise AssertionError({'missing_cu_terms':missing_terms})
if 'module-safe' not in api or 'fails closed' not in api:
    raise AssertionError('program-live-api.md missing module-safe/fail-closed boundary wording')

# Environment proof: this script itself does not rely on dev-mode proof. Program txs are not submitted.
run('env-risc0-dev-mode', ['bash','-lc','printf "RISC0_DEV_MODE=%s\\n" "${RISC0_DEV_MODE-<unset>}"'])

summary={
  'ok': True,
  'repo_sha': subprocess.check_output(['git','rev-parse','HEAD'], cwd=AGENT, text=True).strip(),
  'command_line': 'scripts/run_program_cu_boundary_evidence.py',
  'evidence_dir': str(BASE),
  'program_query': query,
  'program_call': call,
  'program_deploy_missing': deploy_missing,
  'program_deploy': deploy,
  'cu_boundary': 'rc3 wallet output currently exposes tx hash/balance but no stable CU field; program txs are not submitted by module, so CU is not applicable for program.call/deploy.',
  'proof_boundary': 'No fake RISC0_DEV_MODE=0 proof is claimed for module-level program.*; requires upstream module-safe LEZ program SDK/C ABI or external SPEL/lgs harness.',
  'raw_logs': sorted(str(p) for p in BASE.glob('*.stdout')),
}
(BASE/'program_cu_boundary_summary.json').write_text(json.dumps(summary, indent=2, sort_keys=True))
doc=AGENT/'docs'/'program-cu-boundary-evidence.md'
doc.write_text('\n'.join([
  '# Program/CU boundary evidence', '',
  'This evidence closes the module-level program skill boundary without overclaiming live program submission.', '',
  f'- Evidence dir: `{BASE}`',
  '- `program.query` returns simulated mode with explicit unavailable-live-query note.',
  '- `program.call` fails closed with `submitted=false` and `live_program_call_not_available`.',
  '- `program.deploy` fails closed for existing binaries and reports `binary_not_found` for missing binaries.',
  '- CU/proof documentation remains honest: current rc3 wallet output does not expose stable CU cost, and module-level program tx proof is not claimed.', '',
]))
print('PROGRAM_CU_BOUNDARY_OK '+str(BASE/'program_cu_boundary_summary.json'))
