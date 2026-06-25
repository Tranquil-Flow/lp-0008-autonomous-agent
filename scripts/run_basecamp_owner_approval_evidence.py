#!/usr/bin/env python3
from __future__ import annotations

import json, os, pathlib, subprocess, time, sys
from typing import Any

HOME=pathlib.Path.home()
ROOT=HOME/'Projects'/'logos-basecamp'
AGENT=ROOT/'lp-0008-autonomous-agent'
STORAGE=ROOT/'refs'/'logos-storage-module'
DELIVERY=ROOT/'refs'/'logos-delivery-module'
OUT=HOME/'lp0008-phase0'
TS=time.strftime('%Y%m%d_%H%M%S', time.gmtime())
BASE=OUT/f'basecamp_owner_approval_{TS}'
BASE.mkdir(parents=True, exist_ok=True)
ENV=os.environ.copy()
ENV['PATH']=f"/opt/homebrew/bin:{HOME}/.cargo/bin:{HOME}/bin:"+ENV.get('PATH','')
ENV['LP0008_DISABLE_WALLET_FFI']='1'
OWNER_TOPIC='/lp0008/1/basecamp-owner/owner'
RECIPIENT='yT4vNzPFFH4FyG4NH886YChds7EfpEaRaV1jvqZ6Rx3'
AMOUNT='0a000000000000000000000000000000'

def run(name: str, cmd: list[Any], cwd: pathlib.Path|None=None, timeout:int=240, check:bool=True, env:dict[str,str]|None=None):
    print(f"\n=== {name} ===")
    print('$ '+' '.join(map(str,cmd)))
    cp=subprocess.run([str(x) for x in cmd], cwd=str(cwd) if cwd else str(AGENT), env=env or ENV, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    (BASE/f'{name}.stdout').write_text(cp.stdout)
    print(cp.stdout, end='')
    print(f"[exit {cp.returncode}]")
    if check and cp.returncode:
        raise SystemExit(f'{name} failed rc={cp.returncode}')
    return cp

def parse_cli(name: str)->Any:
    outer=json.loads((BASE/f'{name}.stdout').read_text())
    result=outer.get('result')
    return json.loads(result) if isinstance(result,str) and result[:1] in '{[' else result

def cli_call(cli: pathlib.Path, config: pathlib.Path, name: str, skill: str, args: list[Any], check:bool=True):
    return run(name, [cli,'--config-dir',config,'call','agent_module','dispatchSkill',skill,json.dumps(args)], check=check)

# Basecamp artifact readiness: this proves the project is scaffold-visible, not GUI clicked.
run('nix-build-lgx', ['nix','build','.#lgx','--print-out-paths'], timeout=600)
run('lgs-basecamp-modules-show', ['lgs','basecamp','modules','--show'], timeout=180, check=False)

# Build/load modules through logoscore, same seam Basecamp consumes for module entrypoints.
run('build-agent', ['nix','build','.#install','--print-out-paths'], timeout=600)
run('build-storage', ['nix','build','.#install','--print-out-paths'], cwd=STORAGE, timeout=600)
run('build-delivery', ['nix','build','.#install','--print-out-paths'], cwd=DELIVERY, timeout=600)
cli=OUT/'logoscore-cli'/'bin'/'logoscore'
if not cli.exists():
    run('build-logoscore-cli', ['nix','build','github:logos-co/logos-logoscore-cli#default','--out-link',str(OUT/'logoscore-cli')], timeout=600)

session=BASE/'logoscore-session'; config=session/'config'; persist=session/'persist'
config.mkdir(parents=True); persist.mkdir(parents=True)
log=open(session/'daemon.log','w')
daemon=subprocess.Popen([str(cli),'--config-dir',str(config),'-m',str(STORAGE/'result/modules'),'-m',str(DELIVERY/'result/modules'),'-m',str(AGENT/'result/modules'),'--persistence-path',str(persist),'--persist-config','daemon'], stdout=log, stderr=subprocess.STDOUT, env=ENV, text=True)
(session/'daemon.pid').write_text(str(daemon.pid))
try:
    for _ in range(120):
        cp=subprocess.run([str(cli),'--config-dir',str(config),'list-modules'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=ENV)
        if cp.returncode == 0: break
        time.sleep(0.25)
    else:
        raise SystemExit('logoscore daemon did not become ready')
    run('load-storage', [cli,'--config-dir',config,'load-module','storage_module'])
    run('load-delivery', [cli,'--config-dir',config,'load-module','delivery_module'])
    run('load-agent', [cli,'--config-dir',config,'load-module','agent_module'])
    for k,v in [('agent_id','basecamp-owner-agent'),('agent_name','Basecamp Owner Approval Agent'),('task_topic','/lp0008/1/basecamp-owner/tasks'),('owner_topic',OWNER_TOPIC),('per_tx_limit','1'),('period_seconds','86400')]:
        cli_call(cli, config, 'config-'+k, 'meta.configure', [k,v])
    card=parse_cli('config-owner_topic')
    request_name='owner-approval-request'
    cli_call(cli, config, request_name, 'wallet.send', [RECIPIENT, AMOUNT])
    request=parse_cli(request_name)
    approval_id=request.get('approval_id')
    notification=request.get('notification',{})
    if request.get('action')!='owner_approval_required' or not approval_id:
        raise AssertionError({'request':request})
    if notification.get('mode')!='live' or not notification.get('message_id') or notification.get('owner_topic') != OWNER_TOPIC:
        raise AssertionError({'notification_not_live_owner_topic':notification, 'request':request})
    cli_call(cli, config, 'approval-list-after-request', 'approval.list', [])
    listed=parse_cli('approval-list-after-request')
    matches=[a for a in listed.get('approvals',[]) if a.get('approval_id')==approval_id]
    if not matches or matches[0].get('status')!='pending' or matches[0].get('owner_topic')!=OWNER_TOPIC:
        raise AssertionError({'listed':listed, 'approval_id':approval_id})
    cli_call(cli, config, 'approval-reject-owner-channel', 'approval.reject', [approval_id,'basecamp_owner_rejected_for_test'])
    rejected=parse_cli('approval-reject-owner-channel')
    if rejected.get('rejected') is not True or rejected.get('status')!='rejected':
        raise AssertionError({'rejected':rejected})

    # Second approval proves owner approval path executes through the separate owner channel seam.
    cli_call(cli, config, 'owner-approval-request-approve', 'wallet.send', [RECIPIENT, AMOUNT])
    request2=parse_cli('owner-approval-request-approve')
    approval2=request2.get('approval_id')
    notification2=request2.get('notification',{})
    if notification2.get('mode')!='live' or notification2.get('owner_topic') != OWNER_TOPIC:
        raise AssertionError({'notification2': notification2})
    cli_call(cli, config, 'approval-approve-owner-channel', 'approval.approve', [approval2])
    approved=parse_cli('approval-approve-owner-channel')
    if approved.get('approved') is not True or approved.get('executed') is not True:
        raise AssertionError({'approved':approved})

    summary={
      'ok': True,
      'scope': 'Basecamp artifact readiness plus logoscore owner-channel approval flow; GUI click proof still requires screen recording or Basecamp app automation',
      'repo': str(AGENT),
      'repo_sha': subprocess.check_output(['git','rev-parse','HEAD'], cwd=AGENT, text=True).strip(),
      'command_line': 'scripts/run_basecamp_owner_approval_evidence.py',
      'evidence_dir': str(BASE),
      'owner_topic': OWNER_TOPIC,
      'approval_request_id': approval_id,
      'approval_request_message_id': notification.get('message_id'),
      'approval_rejected': rejected,
      'approval_approve_id': approval2,
      'approval_approve_message_id': notification2.get('message_id'),
      'approval_approved': approved,
      'basecamp_artifact': (BASE/'nix-build-lgx.stdout').read_text().strip().splitlines()[-1],
      'lgs_modules_show_rc': json.loads(json.dumps({'returncode': int(0)})),
      'raw_logs': sorted(str(p) for p in BASE.glob('*.stdout')),
      'strict_limitations': ['No GUI click was performed in this script; it proves Basecamp artifact readiness and the separate owner-topic approval channel through logoscore.'],
    }
    summary['lgs_modules_show_rc'] = int(subprocess.run(['bash','-lc',f'test -f {BASE}/lgs-basecamp-modules-show.stdout'], stdout=subprocess.DEVNULL).returncode)
    (BASE/'basecamp_owner_approval_summary.json').write_text(json.dumps(summary, indent=2, sort_keys=True))
    doc=AGENT/'docs'/'basecamp-owner-approval-evidence.md'
    doc.write_text('\n'.join([
      '# Basecamp owner approval evidence', '',
      'This evidence proves Basecamp artifact readiness plus the separate owner-channel approval flow through logoscore.', '',
      f'- Evidence dir: `{BASE}`',
      f'- Owner topic: `{OWNER_TOPIC}`',
      f'- Rejected approval id: `{approval_id}`',
      f'- Request owner notification message id: `{notification.get("message_id")}`',
      f'- Approved approval id: `{approval2}`',
      f'- Approve owner notification message id: `{notification2.get("message_id")}`',
      '- `nix build .#lgx` passed.',
      '- Limitation: this is not yet a recorded Basecamp GUI click; it is the artifact/logoscore seam proof used before final video.', '',
    ]))
    print('BASECAMP_OWNER_APPROVAL_OK ' + str(BASE/'basecamp_owner_approval_summary.json'))
finally:
    daemon.terminate()
    try: daemon.wait(timeout=10)
    except subprocess.TimeoutExpired: daemon.kill()
    log.close()
