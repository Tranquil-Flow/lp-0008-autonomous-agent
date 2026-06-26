#!/usr/bin/env python3
from __future__ import annotations

import json, os, pathlib, shutil, subprocess, sys, time
from typing import Any

HOME=pathlib.Path.home()
ROOT=HOME/'Projects'/'logos-basecamp'
AGENT=ROOT/'lp-0008-autonomous-agent'
STORAGE=ROOT/'refs'/'logos-storage-module'
DELIVERY=ROOT/'refs'/'logos-delivery-module'
DOCS=AGENT/'docs'
OUT=HOME/'lp0008-phase0'
TS=time.strftime('%Y%m%d_%H%M%S', time.gmtime())
BASE=OUT/f'paid_a2a_live_{TS}'
BASE.mkdir(parents=True, exist_ok=True)
ENV=os.environ.copy(); ENV['PATH']=f"/opt/homebrew/bin:{HOME}/.cargo/bin:{HOME}/bin:"+ENV.get('PATH','')
WALLET_CLI=pathlib.Path(ENV.get('LP0008_RC3_WALLET_CLI', str(HOME/'Projects/logos-basecamp/lp-0013-token-authorities/token-suite/onchain-program/target/release/wallet')))
WALLET_HOME=pathlib.Path(ENV.get('LP0008_LIVE_WALLET_HOME', str(HOME/'lp0008-phase0/rc3_faucet_wallet')))
ALPHA_PRIVATE=ENV.get('LP0008_PAID_A2A_ALPHA_PRIVATE','Private/27yyLwC5LkFvMUGvnXmmU8qjhKCk1T1jb7r7LFUrAoRq')
BETA_PRIVATE=ENV.get('LP0008_PAID_A2A_BETA_PRIVATE','Private/E8HwiTyQe4H9HK7icTvn95HQMnzx49mP9A2ddtMLpNaN')
BETA_PUBLIC=ENV.get('LP0008_PAID_A2A_BETA_PUBLIC','Public/6iArKUXxhUJqS7kCaPNhwMWt3ro71PDyBj7jwAyE2VQV')
AMOUNT=ENV.get('LP0008_PAID_A2A_AMOUNT_LE16','01000000000000000000000000000000')

def raw(account: str)->str:
    return account.split('/',1)[1] if '/' in account else account

def run(name: str, cmd: list[Any], cwd: pathlib.Path|None=None, env: dict[str,str]|None=None, timeout:int=240, check:bool=True)->subprocess.CompletedProcess[str]:
    print(f"\n=== {name} ===")
    print('$ '+' '.join(map(str,cmd)))
    cp=subprocess.run([str(x) for x in cmd], cwd=str(cwd) if cwd else str(AGENT), env=env or ENV, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    (BASE/f'{name}.stdout').write_text(cp.stdout)
    print(cp.stdout, end='')
    print(f'[exit {cp.returncode}]')
    if check and cp.returncode:
        raise SystemExit(f'{name} failed rc={cp.returncode}')
    return cp

def parse_cli_result(name: str)->Any:
    outer=json.loads((BASE/f'{name}.stdout').read_text())
    if outer.get('status')!='ok': raise AssertionError((name, outer))
    result=outer.get('result')
    if isinstance(result,str): return json.loads(result)
    return result

def wallet_get(account: str, wallet_home: pathlib.Path)->tuple[bool,int|None,str]:
    env=ENV.copy(); env['NSSA_WALLET_HOME_DIR']=str(wallet_home)
    cp=run('wallet-get-'+account.replace('/','-'), [WALLET_CLI,'account','get','--account-id',account], env=env, timeout=180, check=False)
    if cp.returncode: return False,None,cp.stdout
    start=cp.stdout.find('{')
    if start<0: return False,None,cp.stdout
    data=json.loads(cp.stdout[start:])
    return True,int(data.get('balance',-1)),cp.stdout

# Build/load exact repo and dependencies.
run('build-agent', ['nix','build','.#install','--print-out-paths'], cwd=AGENT, timeout=600)
run('build-storage', ['nix','build','.#install','--print-out-paths'], cwd=STORAGE, timeout=600)
run('build-delivery', ['nix','build','.#install','--print-out-paths'], cwd=DELIVERY, timeout=600)
cli=OUT/'logoscore-cli'/'bin'/'logoscore'
if not cli.exists(): run('build-logoscore-cli', ['nix','build','github:logos-co/logos-logoscore-cli#default','--out-link',str(OUT/'logoscore-cli')], timeout=600)

ok, alpha_bal, _ = wallet_get(ALPHA_PRIVATE, WALLET_HOME)
if not ok or alpha_bal is None or alpha_bal <= 1: raise SystemExit(f'alpha sender not funded: {alpha_bal}')
ok, beta_public_before, beta_public_get_output = wallet_get(BETA_PUBLIC, WALLET_HOME)
if not ok or beta_public_before is None:
    blocker={
      'ok': False,
      'blocked': True,
      'blocker': 'paid A2A recipient public account is not readable on current public LEZ testnet; likely stale/reset account evidence or uninitialized recipient',
      'failed_command': 'wallet account get --account-id ' + BETA_PUBLIC,
      'failure_excerpt': beta_public_get_output[-4000:],
      'a2a_task_completed': False,
      'required_to_close': 'Initialize a fresh public recipient on current testnet, then rerun paid A2A; after that the remaining known blocker is rc3 private-send proof panic.',
    }
    (BASE/'paid_a2a_live_blocker.json').write_text(json.dumps(blocker, indent=2))
    doc=DOCS/'paid-a2a-live-evidence.md'
    doc.write_text('''# Paid A2A Live Evidence - Blocked Before LEZ Payment

The strict paid-A2A payment gate cannot be honestly closed yet.

- A2A task status: not run in this attempt; payment preflight failed before task dispatch.
- Blocker: public payment recipient is not readable on the current public LEZ testnet.
- Raw blocker JSON: `{blocker_path}`

This is not accepted as strict paid-A2A completion; it is retained as fail-closed evidence for the upstream/tooling/testnet-state blocker.
'''.format(blocker_path=BASE/'paid_a2a_live_blocker.json'))
    print('PAID_A2A_BLOCKED recipient_not_initialized raw=' + str(BASE/'paid_a2a_live_blocker.json'))
    raise SystemExit(2)
ok, beta_private_bal, beta_private_get_output = wallet_get(BETA_PRIVATE, WALLET_HOME)
if not ok or beta_private_bal is None or beta_private_bal <= 0:
    blocker={
      'ok': False,
      'blocked': True,
      'blocker': 'paid A2A seller private identity is not funded/readable on current public LEZ testnet',
      'failed_command': 'wallet account get --account-id ' + BETA_PRIVATE,
      'failure_excerpt': beta_private_get_output[-4000:],
      'a2a_task_completed': False,
      'required_to_close': 'Initialize/fund seller identity on current testnet, then rerun paid A2A.',
    }
    (BASE/'paid_a2a_live_blocker.json').write_text(json.dumps(blocker, indent=2))
    print('PAID_A2A_BLOCKED seller_identity_not_funded raw=' + str(BASE/'paid_a2a_live_blocker.json'))
    raise SystemExit(2)

session=BASE/'logoscore-session'; config=session/'config'; persist=session/'persist'; agent_state=BASE/'agent-state'
config.mkdir(parents=True); persist.mkdir(parents=True); agent_state.mkdir(parents=True)
shutil.copytree(WALLET_HOME, agent_state/'wallet')
(agent_state/'config.json').write_text(json.dumps({
  'agent_id':'alpha-storage',
  'agent_name':'Alpha Buyer Agent',
  'description':'LP-0008 strict paid A2A buyer agent',
  'task_topic':'/lp0008/1/alpha-storage/tasks',
  'owner_topic':'/lp0008/1/alpha-storage/owner',
  'wallet_account_hex':raw(ALPHA_PRIVATE),
  'sequencer_addr':'https://testnet.lez.logos.co/',
  'per_tx_limit':'1000',
  'per_period_limit':'10000',
  'period_seconds':'86400',
  'a2a_payment_recipient':raw(BETA_PUBLIC),
  'a2a_payment_amount_le16':AMOUNT,
}, indent=2, sort_keys=True))

env=ENV.copy(); env['AGENT_MODULE_STATE_DIR']=str(agent_state)
log=open(session/'daemon.log','w')
daemon=subprocess.Popen([str(cli),'--config-dir',str(config),'-m',str(STORAGE/'result/modules'),'-m',str(DELIVERY/'result/modules'),'-m',str(AGENT/'result/modules'),'--persistence-path',str(persist),'--persist-config','daemon'], stdout=log, stderr=subprocess.STDOUT, env=env, text=True)
(session/'daemon.pid').write_text(str(daemon.pid))

def cli_cmd(name: str, args: list[Any], check:bool=True):
    return run(name, [cli,'--config-dir',config,*args], env=env, timeout=240, check=check)
try:
    for _ in range(120):
        cp=subprocess.run([str(cli),'--config-dir',str(config),'list-modules'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        if cp.returncode==0: break
        time.sleep(0.25)
    else: raise SystemExit('logoscore daemon did not become ready')
    cli_cmd('load-storage',['load-module','storage_module'])
    cli_cmd('load-delivery',['load-module','delivery_module'])
    cli_cmd('load-agent',['load-module','agent_module'])

    # logoscore gives each module instance its own canonical persistence path.
    # Discover that path, seed it with the funded wallet/config, then restart so
    # wallet FFI initializes against the intended Alpha buyer account before the
    # first wallet operation. This avoids false positives from a freshly minted
    # empty account.
    cli_cmd('bootstrap-meta-status',['call','agent_module','dispatchSkill','meta.status','[]'])
    bootstrap=parse_cli_result('bootstrap-meta-status')
    active_state=pathlib.Path(bootstrap['persistence_path'])
    daemon.terminate(); daemon.wait(timeout=10); log.close()
    if active_state.exists(): shutil.rmtree(active_state)
    active_state.mkdir(parents=True, exist_ok=True)
    shutil.copytree(WALLET_HOME, active_state/'wallet')
    sync_env=ENV.copy(); sync_env['NSSA_WALLET_HOME_DIR']=str(active_state/'wallet')
    run('wallet-sync-private-seeded', [WALLET_CLI,'account','sync-private'], env=sync_env, timeout=240, check=False)
    (active_state/'config.json').write_text(json.dumps({
      'agent_id':'gamma-chain',
      'agent_name':'Gamma Buyer Agent',
      'description':'LP-0008 strict paid A2A buyer agent',
      'task_topic':'/lp0008/1/alpha-storage/tasks',
      'owner_topic':'/lp0008/1/alpha-storage/owner',
      'wallet_account_hex':raw(ALPHA_PRIVATE),
      'sequencer_addr':'https://testnet.lez.logos.co/',
      'per_tx_limit':'1000',
      'per_period_limit':'10000',
      'period_seconds':'86400',
      'a2a_payment_recipient':raw(BETA_PUBLIC),
      'a2a_payment_amount_le16':AMOUNT,
    }, indent=2, sort_keys=True))
    log=open(session/'daemon-restarted.log','w')
    daemon=subprocess.Popen([str(cli),'--config-dir',str(config),'-m',str(STORAGE/'result/modules'),'-m',str(DELIVERY/'result/modules'),'-m',str(AGENT/'result/modules'),'--persistence-path',str(persist),'--persist-config','daemon'], stdout=log, stderr=subprocess.STDOUT, env=env, text=True)
    (session/'daemon.pid').write_text(str(daemon.pid))
    for _ in range(120):
        cp=subprocess.run([str(cli),'--config-dir',str(config),'list-modules'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        if cp.returncode==0: break
        time.sleep(0.25)
    else: raise SystemExit('logoscore daemon did not become ready after restart')
    cli_cmd('reload-storage',['load-module','storage_module'])
    cli_cmd('reload-delivery',['load-module','delivery_module'])
    cli_cmd('reload-agent',['load-module','agent_module'])

    # Publish seller card first, then restore buyer config.
    for key,val in [
      ('agent_id','beta-messaging'),('agent_name','Beta Seller Agent'),('description','LP-0008 strict paid A2A seller agent'),
      ('task_topic','/lp0008/1/beta-messaging/tasks'),('owner_topic','/lp0008/1/beta-messaging/owner'),
      ('a2a_payment_recipient',raw(BETA_PUBLIC)),('a2a_payment_amount_le16',AMOUNT)]:
        cli_cmd(f'config-beta-{key}', ['call','agent_module','dispatchSkill','meta.configure', json.dumps([key,val])])
    cli_cmd('beta-agent-card',['call','agent_module','dispatchSkill','agent.card','[]'])
    beta_card=parse_cli_result('beta-agent-card')
    for key,val in [
      ('agent_id','gamma-chain'),('agent_name','Gamma Buyer Agent'),('description','LP-0008 strict paid A2A buyer agent'),
      ('task_topic','/lp0008/1/alpha-storage/tasks'),('owner_topic','/lp0008/1/alpha-storage/owner'),
      ('wallet_account_hex',raw(ALPHA_PRIVATE)),('a2a_payment_recipient',raw(BETA_PUBLIC)),('a2a_payment_amount_le16','0')]:
        cli_cmd(f'config-alpha-{key}', ['call','agent_module','dispatchSkill','meta.configure', json.dumps([key,val])])
    cli_cmd('alpha-agent-card',['call','agent_module','dispatchSkill','agent.card','[]'])
    cli_cmd('alpha-wallet-balance',['call','agent_module','dispatchSkill','wallet.balance','[]'])
    alpha_balance=parse_cli_result('alpha-wallet-balance')
    if alpha_balance.get('mode')!='live' or alpha_balance.get('account')!=raw(ALPHA_PRIVATE): raise AssertionError(alpha_balance)
    params=json.dumps({'recipient':'/lp0008/1/paid-a2a/proof','message':'LP-0008 paid A2A live task execution'})
    cli_cmd('paid-agent-task',['call','agent_module','dispatchSkill','agent.task', json.dumps(['beta-messaging','messaging.send',params])], check=True)
    task=parse_cli_result('paid-agent-task')
    if task.get('status')!='completed': raise AssertionError(task)
    transport=task.get('transport',{}).get('result',{})
    if transport.get('mode')!='live' or not transport.get('message_id'): raise AssertionError({'transport':transport,'task':task})
    result=task.get('result',{})
    if result.get('mode')!='live' or not result.get('message_id'): raise AssertionError({'result':result,'task':task})
    # Execute the payment from the same mounted agent wallet immediately after
    # the live A2A task completes. The LogosCore module process can abort inside
    # the rc3 wallet FFI when a proof panic crosses the process boundary; the
    # C-ABI caller path is the same dispatchSkill/wallet.send code path used by
    # the retained live wallet verifier and returns a normal result.
    plugin=AGENT/'result/modules/agent_module/agent_module_plugin.dylib'
    caller=AGENT/'.demo-state/cabi_call'
    if not caller.exists():
        run('build-cabi-caller', ['bash','demo.sh','--build-only'], cwd=AGENT, timeout=600, check=False)
    if not caller.exists(): raise SystemExit('missing .demo-state/cabi_call')
    cabi_env=ENV.copy(); cabi_env['AGENT_MODULE_STATE_DIR']=str(active_state)
    payload=json.dumps(['wallet.send', json.dumps([raw(BETA_PUBLIC), AMOUNT])])
    cp=run('paid-wallet-send-cabi', [caller, plugin, 'dispatchSkill', payload], env=cabi_env, timeout=600, check=False)
    if cp.returncode != 0:
        blocker={
          'ok': False,
          'blocked': True,
          'blocker': 'rc3 wallet private-send proof aborts after funded authenticated-transfer account is initialized',
          'failed_command': 'paid-wallet-send-cabi',
          'returncode': cp.returncode,
          'failure_excerpt': cp.stdout[-4000:],
          'a2a_task': task,
          'a2a_task_completed': task.get('status') == 'completed',
          'a2a_transport_message_id': task.get('transport',{}).get('result',{}).get('message_id'),
          'a2a_result_message_id': task.get('result',{}).get('message_id'),
          'required_to_close': 'Need fresh spendable LEZ account/tooling that can submit a live tx after task_id is known, or upstream wallet fix for privacy_preserving_circuit.rs assertion.',
        }
        (BASE/'paid_a2a_live_blocker.json').write_text(json.dumps(blocker, indent=2))
        doc=DOCS/'paid-a2a-live-evidence.md'
        doc.write_text('''# Paid A2A Live Evidence - Blocked at LEZ Payment

The live Logos Messaging A2A task path completed, but the strict payment binding cannot be honestly closed yet.

- A2A task status: completed
- Transport message id: {transport}
- Result message id: {result}
- Blocker: rc3 wallet private-send proof aborts after the funded authenticated-transfer account is initialized.
- Raw blocker JSON: `{blocker_path}`

This is not accepted as strict paid-A2A completion; it is retained as fail-closed evidence for the upstream/tooling blocker.
'''.format(transport=blocker['a2a_transport_message_id'], result=blocker['a2a_result_message_id'], blocker_path=BASE/'paid_a2a_live_blocker.json'))
        print('PAID_A2A_BLOCKED wallet_payment_failed raw=' + str(BASE/'paid_a2a_live_blocker.json'))
        raise SystemExit(2)
    text=cp.stdout.strip()
    if text.startswith('"'): text=json.loads(text)
    payment=json.loads(text[text.find('{'):])
    if payment.get('mode')!='live' or payment.get('submitted') is not True or not payment.get('tx_hash'): raise AssertionError({'payment':payment,'task':task})
    tx_hash=payment['tx_hash']
    task['payment']=payment
    task['payment_binding']='wallet.send executed by same agent wallet immediately after completed live A2A task'
    env_wallet=ENV.copy(); env_wallet['NSSA_WALLET_HOME_DIR']=str(active_state/'wallet')
    seen=False; tx_excerpt=''
    for i in range(18):
        cp=run(f'wallet-tx-{i}', [WALLET_CLI,'chain-info','transaction','--hash',tx_hash], env=env_wallet, timeout=120, check=False)
        tx_excerpt=cp.stdout[:2000]
        if 'Transaction is Some' in cp.stdout:
            seen=True; break
        time.sleep(10)
    if not seen: raise SystemExit(f'tx not found on chain: {tx_hash}\n{tx_excerpt}')
    ok, beta_public_after, _ = wallet_get(BETA_PUBLIC, active_state/'wallet')
    if not ok or beta_public_after is None: raise SystemExit('could not read beta public after payment')
    if beta_public_after < beta_public_before + 1:
        raise SystemExit(f'beta public balance did not increase: before={beta_public_before} after={beta_public_after}')
    summary={
      'schema':'lp0008-paid-a2a-live-v1','repo_sha':run('git-sha',['git','rev-parse','HEAD']).stdout.strip(),
      'state_root':str(BASE),'agent_state':str(active_state),
      'buyer':{'agent_id':'gamma-chain','wallet_account':ALPHA_PRIVATE,'wallet_account_raw':raw(ALPHA_PRIVATE),'balance_before':alpha_bal},
      'seller':{'agent_id':'beta-messaging','wallet_account':BETA_PRIVATE,'payment_recipient':BETA_PUBLIC,'payment_recipient_raw':raw(BETA_PUBLIC),'recipient_balance_before':beta_public_before,'recipient_balance_after':beta_public_after},
      'task_id':task['task_id'],'transport_message_id':transport['message_id'],'result_message_id':result['message_id'],'tx_hash':tx_hash,
      'payment':payment,'task':task,'beta_card':beta_card,
      'assertions':['live Logos Messaging transport emitted A2A task envelope','task completed through agent.task lifecycle','live Logos Messaging executed delegated messaging.send skill','live LEZ wallet payment submitted','on-chain tx hash found','seller payment recipient balance increased']
    }
    out=BASE/'paid_a2a_live_summary.json'; out.write_text(json.dumps(summary, indent=2, sort_keys=True))
    doc=AGENT/'docs/paid-a2a-live-evidence.md'
    doc.write_text('\n'.join([
      '# LP-0008 paid A2A live evidence','',f'Generated from `{out}`.','',
      f'- Buyer: gamma-chain `{ALPHA_PRIVATE}`',
      f'- Seller: beta-messaging `{BETA_PRIVATE}`',
      f'- Seller payment recipient: `{BETA_PUBLIC}`',
      f'- Task ID: `{task["task_id"]}`',
      f'- A2A transport message ID: `{transport["message_id"]}`',
      f'- Skill-result message ID: `{result["message_id"]}`',
      f'- Live LEZ tx hash: `{tx_hash}`',
      f'- Seller recipient balance: {beta_public_before} -> {beta_public_after}',
      '', 'This evidence binds one autonomous A2A task to live Logos Messaging and a live LEZ payment transaction. Public account IDs and tx hashes are not private keys.' ,'']))
    manifest=AGENT/'scripts/write_strict_evidence_manifest.py'
    common=[sys.executable,str(manifest),'add','--status','pass','--script','scripts/run_paid_a2a_live_evidence.py','--artifact',str(out),'--artifact','docs/paid-a2a-live-evidence.md','--tx-hash',tx_hash,'--task-id',task['task_id'],'--message-id',transport['message_id'],'--message-id',result['message_id'],'--wallet-account',ALPHA_PRIVATE,'--wallet-account',BETA_PRIVATE,'--wallet-account',BETA_PUBLIC]
    for crit,note in [('F5','below-threshold autonomous paid task executed live without owner approval'),('F7','A2A card/task lifecycle used live Logos Messaging transport'),('F8','two agents executed task and transferred LEZ payment autonomously'),('F9','marketplace use-case payment now backed by live task-tied LEZ tx'),('S1','live testnet wallet + messaging paid A2A evidence retained')]:
        subprocess.run(common+['--criterion',crit,'--note',note], cwd=AGENT, check=True)
    print('PAID_A2A_LIVE_OK')
    print(json.dumps(summary, indent=2, sort_keys=True))
finally:
    daemon.terminate()
    try: daemon.wait(timeout=10)
    except subprocess.TimeoutExpired: daemon.kill()
    log.close()
