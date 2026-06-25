#!/usr/bin/env python3
from __future__ import annotations

import argparse, json, os, pathlib, subprocess, sys
from typing import Any

ROOT = pathlib.Path(__file__).resolve().parents[1]

def run(cmd: list[str], *, env: dict[str,str]|None=None, timeout: int=240, check: bool=True) -> subprocess.CompletedProcess[str]:
    cp=subprocess.run(cmd, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    if check and cp.returncode:
        raise SystemExit(f"command failed ({cp.returncode}): {' '.join(cmd)}\n{cp.stdout}")
    return cp

def parse_cabi(stdout: str) -> Any:
    text=stdout.strip()
    starts=[i for i in (text.find('"'), text.find('{'), text.find('[')) if i>=0]
    if starts: text=text[min(starts):]
    if text.startswith('"'):
        text=json.loads(text)
    return json.loads(text)

def call_skill(caller: pathlib.Path, plugin: pathlib.Path, state: pathlib.Path, skill: str, args: list[Any]) -> Any:
    env=os.environ.copy(); env['AGENT_MODULE_STATE_DIR']=str(state)
    payload=json.dumps([skill, json.dumps(args)])
    return parse_cabi(run([str(caller), str(plugin), 'dispatchSkill', payload], env=env, timeout=600).stdout)

def wallet_account(wallet_cli: pathlib.Path, wallet_home: pathlib.Path, account: str) -> tuple[bool, int|None, str]:
    env=os.environ.copy(); env['NSSA_WALLET_HOME_DIR']=str(wallet_home)
    cp=run([str(wallet_cli), 'account', 'get', '--account-id', account], env=env, timeout=180, check=False)
    if cp.returncode != 0:
        return False, None, cp.stdout[:1000]
    # Output contains a leading human line followed by JSON.
    start=cp.stdout.find('{')
    if start < 0:
        return False, None, cp.stdout[:1000]
    data=json.loads(cp.stdout[start:])
    bal=int(data.get('balance', -1))
    return True, bal, cp.stdout[:1000]

def main() -> None:
    ap=argparse.ArgumentParser()
    ap.add_argument('--state-root', required=True)
    ap.add_argument('--wallet-cli', required=True)
    ap.add_argument('--log', default='')
    ns=ap.parse_args()
    state_root=pathlib.Path(ns.state_root).expanduser().resolve()
    wallet_cli=pathlib.Path(ns.wallet_cli).expanduser().resolve()
    agents_doc=state_root/'agents.json'
    if not agents_doc.exists(): raise SystemExit(f'missing {agents_doc}')
    agents=json.loads(agents_doc.read_text())['agents']
    if len(agents) != 3: raise SystemExit(f'expected 3 agents, got {len(agents)}')
    accounts=[a['account'] for a in agents]
    raw_accounts=[a['account'].split('/',1)[1] if '/' in a['account'] else a['account'] for a in agents]
    if len(set(raw_accounts)) != 3: raise SystemExit(f'accounts not distinct: {accounts}')

    caller=ROOT/'.demo-state/cabi_call'
    plugin=ROOT/'result/modules/agent_module/agent_module_plugin.dylib'
    if not plugin.exists(): plugin=ROOT/'result/modules/agent_module/agent_module_plugin.so'
    for p in (caller, plugin, wallet_cli):
        if not p.exists(): raise SystemExit(f'missing executable/artifact: {p}')

    rows=[]
    categories=set()
    topics=set()
    owner_topics=set()
    for agent in agents:
        state=pathlib.Path(agent['state_dir'])
        cfg=json.loads((state/'config.json').read_text())
        acct_cli=agent['account']
        acct=acct_cli.split('/',1)[1] if '/' in acct_cli else acct_cli
        ok, wallet_bal, wallet_excerpt=wallet_account(wallet_cli, state/'wallet', acct_cli)
        if not ok or wallet_bal is None or wallet_bal <= 0:
            raise SystemExit(f"{agent['agent_id']} wallet account not funded/initialized: ok={ok} balance={wallet_bal} excerpt={wallet_excerpt}")
        status=call_skill(caller, plugin, state, 'meta.status', [])
        balance=call_skill(caller, plugin, state, 'wallet.balance', [])
        card=call_skill(caller, plugin, state, 'agent.card', [])
        if balance.get('mode') != 'live': raise SystemExit(f"{agent['agent_id']} not live wallet: {balance}")
        if balance.get('account') != acct: raise SystemExit(f"{agent['agent_id']} mounted wrong account: {balance.get('account')} != {acct}")
        if int(balance.get('balance', -1)) != wallet_bal: raise SystemExit(f"{agent['agent_id']} wallet balance mismatch: plugin={balance} cli={wallet_bal}")
        if card.get('agent_id') != agent['agent_id']: raise SystemExit(f"card id mismatch: {card}")
        if card.get('protocol') != 'a2a': raise SystemExit(f"card protocol mismatch: {card}")
        if card.get('payment',{}).get('recipient') != acct: raise SystemExit(f"payment recipient mismatch: {card.get('payment')}")
        topics.add(card.get('task_topic')); owner_topics.add(card.get('owner_topic')); categories.add(agent['category'])
        rows.append({
            'agent_id': agent['agent_id'],
            'category': agent['category'],
            'state_dir': str(state),
            'account': acct_cli,
            'agent_wallet_account': acct,
            'balance': wallet_bal,
            'task_topic': card.get('task_topic'),
            'owner_topic': card.get('owner_topic'),
            'skills': len(card.get('skills', [])),
            'wallet_mode': balance.get('mode'),
        })
    if len(topics) != 3 or len(owner_topics) != 3: raise SystemExit('task/owner topics must be distinct')
    if categories != {'storage','messaging','chain'}: raise SystemExit(f'missing categories: {categories}')
    if any(r['skills'] < 23 for r in rows): raise SystemExit(f'cards do not expose all default skills: {rows}')

    summary={
        'schema':'lp0008-three-live-agents-v1',
        'repo_sha': run(['git','rev-parse','HEAD']).stdout.strip(),
        'state_root': str(state_root),
        'setup_log': ns.log,
        'agents': rows,
        'assertions': [
            'three distinct public testnet private account identifiers',
            'each account initialized and funded with positive balance',
            'each agent state mounts its configured account in live wallet mode',
            'each agent has distinct A2A task and owner topics',
            'each Agent Card advertises a per-task LEZ payment recipient and >=23 default skills',
            'agents cover storage, messaging, and chain/default-skill categories'
        ]
    }
    out=state_root/'three_live_agents_summary.json'
    out.write_text(json.dumps(summary, indent=2, sort_keys=True))

    doc=ROOT/'docs/three-live-agents.md'
    lines=['# LP-0008 three live agents evidence','',f'Generated from `{out}`.','', '| Agent | Category | Wallet CLI account ID | Agent wallet account | Balance | Task topic | Owner topic | Skills |', '|---|---|---|---|---:|---|---|---:|']
    for r in rows:
        lines.append(f"| {r['agent_id']} | {r['category']} | `{r['account']}` | `{r['agent_wallet_account']}` | {r['balance']} | `{r['task_topic']}` | `{r['owner_topic']}` | {r['skills']} |")
    lines += ['', 'These are public account identifiers on the LEZ testnet, not private keys. Each agent state contains its own wallet-home copy plus `config.json` selecting its account. This proves three distinct live LEZ account identities for the strict evidence gate; later paid-A2A evidence binds task execution to a live transaction hash.','']
    doc.write_text('\n'.join(lines))

    manifest=ROOT/'scripts/write_strict_evidence_manifest.py'
    wallet_args=[]
    for account in accounts:
        wallet_args += ['--wallet-account', account]
    base_cmd=[sys.executable, str(manifest), 'add', '--status', 'pass', '--script', 'scripts/setup_three_live_agents.sh', '--script', 'scripts/verify_three_live_agents.py', '--artifact', str(out), '--artifact', 'docs/three-live-agents.md']
    subprocess.run(base_cmd + ['--criterion', 'F2', *wallet_args, '--note', 'three distinct initialized/funded LEZ testnet agent accounts verified'], cwd=ROOT, check=True)
    subprocess.run(base_cmd + ['--criterion', 'F10', *wallet_args, '--note', 'Alpha/Beta/Gamma deployed as separate configured agents for storage/messaging/chain categories'], cwd=ROOT, check=True)
    subprocess.run(base_cmd + ['--criterion', 'SR4', *wallet_args, '--note', 'reproducible three-agent setup evidence retained'], cwd=ROOT, check=True)
    print('THREE_LIVE_AGENTS_OK')
    print(json.dumps(summary, indent=2, sort_keys=True))

if __name__ == '__main__':
    main()
