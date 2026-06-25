#!/usr/bin/env python3
from __future__ import annotations

import concurrent.futures
import json
import os
import pathlib
import shutil
import subprocess
import sys
import time
from typing import Any, Callable

HOME=pathlib.Path.home()
ROOT=HOME/'Projects'/'logos-basecamp'
AGENT=ROOT/'lp-0008-autonomous-agent'
STORAGE=ROOT/'refs'/'logos-storage-module'
DELIVERY=ROOT/'refs'/'logos-delivery-module'
OUT=HOME/'lp0008-phase0'
TS=time.strftime('%Y%m%d_%H%M%S', time.gmtime())
BASE=OUT/f'strict_skill_matrix_{TS}'
BASE.mkdir(parents=True, exist_ok=True)
ENV=os.environ.copy()
ENV['PATH']=f"/opt/homebrew/bin:{HOME}/.cargo/bin:{HOME}/bin:"+ENV.get('PATH','')
# Matrix focuses behavior and failure semantics; live LEZ wallet proof is retained in separate phase evidence.
ENV['LP0008_DISABLE_WALLET_FFI']='1'
EXPECTED_SKILLS={
 'meta.skills','meta.status','meta.configure',
 'storage.upload','storage.download','storage.list','storage.share',
 'messaging.send','messaging.join','messaging.create_group',
 'wallet.balance','wallet.send','wallet.history','approval.list','approval.approve','approval.reject','approval.retry',
 'program.query','program.call','program.deploy',
 'agent.card','agent.discover','agent.task','agent.complete','agent.receive','agent.subscribe','agent.cancel',
}
# Upstream text said 23 earlier; current implementation registers 27 including approval.* and receive/complete.

records=[]
artifacts={}

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

def parse_outer(name: str)->Any:
    txt=(BASE/f'{name}.stdout').read_text()
    return json.loads(txt)

def parse_skill(name: str)->Any:
    outer=parse_outer(name)
    if outer.get('status')!='ok': raise AssertionError((name, outer))
    result=outer.get('result')
    if isinstance(result,str) and result[:1] in '{[':
        return json.loads(result)
    return result

def record(skill: str, case: str, ok: bool, detail: Any, mode: str|None=None):
    rec={'skill':skill,'case':case,'ok':bool(ok),'mode':mode,'detail':detail}
    records.append(rec)
    return rec

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

def cli_cmd(name: str, args: list[Any], check:bool=True):
    return run(name, [cli,'--config-dir',config,*args], check=check, timeout=180)

def call(name: str, skill: str, args: list[Any]):
    cli_cmd(name, ['call','agent_module','dispatchSkill',skill,json.dumps(args)])
    return parse_skill(name)

def assert_no_error(obj: Any):
    if isinstance(obj,dict) and obj.get('error'):
        raise AssertionError(obj)
    return obj

try:
    for _ in range(120):
        cp=subprocess.run([str(cli),'--config-dir',str(config),'list-modules'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=ENV)
        if cp.returncode==0: break
        time.sleep(0.25)
    else: raise SystemExit('logoscore daemon did not become ready')
    cli_cmd('load-storage',['load-module','storage_module'])
    cli_cmd('load-delivery',['load-module','delivery_module'])
    cli_cmd('load-agent',['load-module','agent_module'])
    cli_cmd('list-modules-initial',['list-modules'])

    # Configure stable agent identity/topics and approval threshold.
    for k,v in [('agent_id','strict-matrix-agent'),('agent_name','Strict Matrix Agent'),('task_topic','/lp0008/1/strictmatrix/tasks'),('owner_topic','/lp0008/1/strictmatrix/owner'),('per_tx_limit','1'),('period_seconds','86400')]:
        r=call(f'config-{k}','meta.configure',[k,v])
        record('meta.configure',f'success-{k}', r.get('updated') is True, r, r.get('mode'))
    badconf=call('config-invalid-key','meta.configure',['no_such_config','x'])
    record('meta.configure','failure-invalid-key', badconf.get('error')=='unknown_config_key' or badconf.get('updated') is False, badconf)

    # Meta/card/discovery.
    skills=call('meta-skills','meta.skills',[])
    names={x.get('name') for x in skills}
    record('meta.skills','success-list-all', EXPECTED_SKILLS.issubset(names) and len(names)>=len(EXPECTED_SKILLS), {'count':len(names),'missing':sorted(EXPECTED_SKILLS-names)})
    status=call('meta-status','meta.status',[])
    record('meta.status','success-status', isinstance(status,dict) and 'wallet' in status and 'modules' in status and 'skills_count' in status, status)
    card=call('agent-card','agent.card',[])
    record('agent.card','success-card', card.get('agent_id')=='strict-matrix-agent' and len(card.get('skills',[]))>=len(EXPECTED_SKILLS), {'agent_id':card.get('agent_id'),'skill_count':len(card.get('skills',[]))})
    disc=call('agent-discover','agent.discover',['/logos/agents/v1/discovery'])
    record('agent.discover','success-self-published', any(a.get('agent_id')=='strict-matrix-agent' for a in disc.get('agents',[])), {'count':disc.get('count')})

    # Storage success/fail/share semantics.
    infile=session/'matrix-input.txt'; infile.write_text('LP-0008 strict skill matrix payload\n')
    upload=call('storage-upload','storage.upload',[str(infile),'matrix-file'])
    record('storage.upload','success-live-upload', upload.get('stored') is True and upload.get('mode')=='live' and upload.get('address'), upload, upload.get('mode'))
    missup=call('storage-upload-missing','storage.upload',[str(session/'missing.txt'),'missing'])
    record('storage.upload','failure-missing-file', missup.get('error')=='file_not_found', missup)
    listing=call('storage-list','storage.list',[])
    record('storage.list','success-list', listing.get('count',0)>=1 and listing.get('mode') in {'live','simulated'}, {'count':listing.get('count'),'mode':listing.get('mode')}, listing.get('mode'))
    outpath=session/'matrix-output.txt'
    down=call('storage-download','storage.download',[upload['address'],str(outpath)])
    record('storage.download','success-download', down.get('downloaded') is True and outpath.exists(), down, down.get('mode'))
    baddown=call('storage-download-missing','storage.download',['0xmissing-address',str(session/'bad.txt')])
    record('storage.download','failure-address-not-found', baddown.get('error')=='address_not_found', baddown)
    share=call('storage-share','storage.share',[upload['address'],'recipient-agent-1'])
    record('storage.share','success-share-recorded', share.get('shared') is True and share.get('recipient')=='recipient-agent-1', share)
    badshare=call('storage-share-missing','storage.share',['0xmissing-address','recipient-agent-1'])
    record('storage.share','failure-address-not-found', badshare.get('error')=='address_not_found', badshare)

    # Messaging/group semantics.
    msg=call('messaging-send-live','messaging.send',['/lp0008/1/strictmatrix/general','hello matrix'])
    record('messaging.send','success-live-send', msg.get('sent') is True and msg.get('mode')=='live' and msg.get('message_id'), msg, msg.get('mode'))
    badmsg=call('messaging-send-invalid-topic','messaging.send',['/not-lip23','should not crash'])
    record('messaging.send','failure-invalid-topic-fallback', badmsg.get('sent') is True and badmsg.get('mode')=='simulated' and 'valid LIP-23' in badmsg.get('note',''), badmsg, badmsg.get('mode'))
    join=call('messaging-join','messaging.join',['/lp0008/1/strictmatrix/group'])
    record('messaging.join','success-group-join', join.get('joined') is True, join, join.get('mode'))
    badjoin=call('messaging-join-invalid','messaging.join',['/not-lip23-group'])
    record('messaging.join','failure-invalid-group-fallback', badjoin.get('joined') is True and badjoin.get('mode')=='simulated', badjoin, badjoin.get('mode'))
    group=call('messaging-create-group','messaging.create_group',[json.dumps(['alice','bob'])])
    record('messaging.create_group','success-create-group', group.get('created') is True and isinstance(group.get('members'),list), group, group.get('mode'))
    weirdgroup=call('messaging-create-group-string','messaging.create_group',['not-json-member'])
    record('messaging.create_group','failure-non-json-members-normalized', weirdgroup.get('created') is True and weirdgroup.get('members')==['not-json-member'], weirdgroup, weirdgroup.get('mode'))

    # Wallet/approval semantics (deterministic local wallet path; live tx proof is separate Phase 2/3 evidence).
    bal=call('wallet-balance','wallet.balance',[])
    record('wallet.balance','success-balance', 'balance' in bal and bal.get('mode') in {'simulated','live'}, bal, bal.get('mode'))
    wh=call('wallet-history','wallet.history',[])
    record('wallet.history','success-history', isinstance(wh.get('transactions'),list), wh)
    small=call('wallet-send-small','wallet.send',['recipient-sim','01000000000000000000000000000000'])
    record('wallet.send','success-below-limit', small.get('submitted') is True and small.get('approved') is True, small, small.get('mode'))
    approval=call('wallet-send-approval','wallet.send',['recipient-sim','0a000000000000000000000000000000'])
    aid=approval.get('approval_id')
    record('wallet.send','failure-above-limit-requires-approval', approval.get('action')=='owner_approval_required' and aid, approval)
    alist=call('approval-list','approval.list',[])
    record('approval.list','success-list', any(a.get('approval_id')==aid for a in alist.get('approvals',[])), {'count':alist.get('count'),'approval_id':aid})
    retry=call('approval-retry','approval.retry',[aid])
    record('approval.retry','success-retry-notifies', retry.get('status')=='pending' and retry.get('notification_attempts',0)>=2, retry)
    rejected=call('approval-reject','approval.reject',[aid,'matrix_reject'])
    record('approval.reject','success-reject', rejected.get('rejected') is True and rejected.get('status')=='rejected', rejected)
    retry_bad=call('approval-retry-rejected','approval.retry',[aid])
    record('approval.retry','failure-not-pending', retry_bad.get('notified') is False and retry_bad.get('error')=='approval_not_pending', retry_bad)
    approve_bad=call('approval-approve-rejected','approval.approve',[aid])
    record('approval.approve','failure-not-pending', approve_bad.get('approved') is False and approve_bad.get('error')=='approval_not_pending', approve_bad)
    approve_missing=call('approval-approve-missing','approval.approve',['appr-missing'])
    record('approval.approve','failure-not-found', approve_missing.get('approved') is False and approve_missing.get('error')=='approval_not_found', approve_missing)
    approval2=call('wallet-send-approval-approve','wallet.send',['recipient-sim','0b000000000000000000000000000000'])
    aid2=approval2.get('approval_id')
    approved=call('approval-approve','approval.approve',[aid2])
    record('approval.approve','success-execute-approved', approved.get('approved') is True and approved.get('executed') is True, approved)
    reject_bad=call('approval-reject-executed','approval.reject',[aid2,'too-late'])
    record('approval.reject','failure-not-pending', reject_bad.get('rejected') is False and reject_bad.get('error')=='approval_not_pending', reject_bad)

    # Program boundaries.
    pq=call('program-query','program.query',['prog','{}'])
    record('program.query','success-bounded-query', pq.get('mode')=='simulated' and 'live_program_query_unavailable' in pq.get('note',''), pq, pq.get('mode'))
    pc=call('program-call','program.call',['prog','vote','{}'])
    record('program.call','failure-live-call-unavailable', pc.get('submitted') is False and pc.get('error')=='live_program_call_not_available', pc, pc.get('mode'))
    pdmiss=call('program-deploy-missing','program.deploy',[str(session/'nope.bin')])
    record('program.deploy','failure-missing-binary', pdmiss.get('deployed') is False and pdmiss.get('error')=='binary_not_found', pdmiss)
    dummy=session/'dummy.bin'; dummy.write_bytes(b'dummy')
    pd=call('program-deploy-existing','program.deploy',[str(dummy)])
    record('program.deploy','failure-live-deploy-unavailable', pd.get('deployed') is False and pd.get('error')=='live_program_deploy_not_available', pd, pd.get('mode'))

    # A2A lifecycle semantics.
    task=call('agent-task','agent.task',['strict-matrix-agent','messaging.send',json.dumps({'recipient':'/lp0008/1/strictmatrix/general','message':'task hello'})])
    tid=task.get('task_id')
    record('agent.task','success-completed-live-transport', task.get('status')=='completed' and task.get('transport',{}).get('result',{}).get('mode')=='live', task)
    sub=call('agent-subscribe','agent.subscribe',['strict-matrix-agent',tid])
    record('agent.subscribe','success-subscribe-completed', sub.get('subscribed') is True and sub.get('current_status')=='completed', sub)
    cancel=call('agent-cancel-completed','agent.cancel',['strict-matrix-agent',tid])
    record('agent.cancel','success-terminal-task-refused-safely', cancel.get('cancelled') is False and cancel.get('current_status')=='completed', cancel)
    failed=call('agent-task-fail','agent.task',['strict-matrix-agent','agent.no_such_skill','{}'])
    ftid=failed.get('task_id')
    record('agent.task','failure-unknown-skill', failed.get('status')=='failed' and failed.get('error',{}).get('error')=='unknown_skill', failed)
    comp=call('agent-complete','agent.complete',[ftid,json.dumps({'manual':'done'})])
    record('agent.complete','success-complete-existing', comp.get('completed') is True and comp.get('status')=='completed', comp)
    compbad=call('agent-complete-missing','agent.complete',['task-missing','{}'])
    record('agent.complete','failure-not-found', compbad.get('completed') is False and compbad.get('error')=='task_not_found', compbad)
    # Inbound receive and malformed fail-closed.
    inbound=json.dumps({'from':'tester','skill':'messaging.send','params':{'recipient':'/lp0008/1/strictmatrix/general','message':'inbound hello'}})
    call('inbound-message','messaging.send',['/lp0008/1/strictmatrix/tasks',inbound])
    recv=call('agent-receive','agent.receive',[])
    record('agent.receive','success-process-inbound', recv.get('processed',0)>=1 and any(t.get('status')=='completed' for t in recv.get('tasks',[])), recv)
    # Inject malformed envelope by raw messaging to task topic, then receive; invalid envelope should be marked processed with no crash.
    call('inbound-malformed','messaging.send',['/lp0008/1/strictmatrix/tasks','not-json'])
    recv2=call('agent-receive-malformed','agent.receive',[])
    record('agent.receive','failure-malformed-envelope-processed', recv2.get('processed',0)>=1, recv2)
    subbad=call('agent-subscribe-missing','agent.subscribe',['strict-matrix-agent','task-missing'])
    record('agent.subscribe','failure-not-found', subbad.get('subscribed') is False and subbad.get('current_status')=='not_found', subbad)
    cancelbad=call('agent-cancel-missing','agent.cancel',['strict-matrix-agent','task-missing'])
    record('agent.cancel','failure-not-found', cancelbad.get('cancelled') is False and cancelbad.get('error')=='task_not_found', cancelbad)

    # Concurrency/resilience: parallel sends + persistence survives fresh CLI calls and invalid topic did not unload delivery.
    def par_send(i:int):
        cp=subprocess.run([str(cli),'--config-dir',str(config),'call','agent_module','dispatchSkill','messaging.send',json.dumps([f'/lp0008/1/strictmatrix/concurrent{i}',f'parallel {i}'])], env=ENV, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
        (BASE/f'concurrent-send-{i}.stdout').write_text(cp.stdout)
        if cp.returncode: return {'i':i,'ok':False,'stdout':cp.stdout}
        outer=json.loads(cp.stdout); res=json.loads(outer['result'])
        return {'i':i,'ok':res.get('sent') is True and res.get('mode')=='live','message_id':res.get('message_id')}
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as ex:
        cres=list(ex.map(par_send, range(6)))
    record('messaging.send','concurrency-six-live-sends', all(x.get('ok') for x in cres) and len({x.get('message_id') for x in cres})==6, cres)
    cli_cmd('list-modules-after-matrix',['list-modules'])
    mods=parse_outer('list-modules-after-matrix')
    loaded={m.get('name'):m.get('status') for m in mods}
    record('meta.status','resilience-modules-loaded-after-failures', loaded.get('agent_module')=='loaded' and loaded.get('delivery_module')=='loaded' and loaded.get('storage_module')=='loaded', loaded)

    # Validate every registered skill has one success/bounded case and one negative/boundary case.
    by_skill={s:{'success':0,'failure':0,'cases':[]} for s in EXPECTED_SKILLS}
    for r in records:
        if r['skill'] in by_skill:
            if r['case'].startswith('success') or 'success' in r['case'] or 'concurrency' in r['case'] or 'resilience' in r['case']:
                by_skill[r['skill']]['success'] += int(r['ok'])
            if r['case'].startswith('failure') or 'invalid' in r['case'] or 'missing' in r['case'] or 'unavailable' in r['case'] or 'not-found' in r['case'] or 'not-pending' in r['case'] or 'bounded' in r['case']:
                by_skill[r['skill']]['failure'] += int(r['ok'])
            by_skill[r['skill']]['cases'].append({'case':r['case'],'ok':r['ok'],'mode':r.get('mode')})
    # Some inherently read-only skills have bounded only or discovery success instead of separate failure.
    read_only_ok={'meta.skills','meta.status','agent.card','agent.discover','wallet.balance','wallet.history','approval.list','program.query','program.call','program.deploy','storage.list'}
    missing=[]
    for skill, st in sorted(by_skill.items()):
        if st['success'] < 1 and skill not in {'program.call','program.deploy'}:
            missing.append((skill,'success',st))
        if st['failure'] < 1 and skill not in read_only_ok:
            missing.append((skill,'failure',st))
    failed_cases=[r for r in records if not r['ok']]
    if missing or failed_cases:
        (BASE/'debug-missing.json').write_text(json.dumps({'missing':missing,'failed_cases':failed_cases,'by_skill':by_skill,'records':records}, indent=2, sort_keys=True, default=str))
        raise AssertionError({'missing':missing,'failed_cases':failed_cases[:5]})

    matrix=[]
    for skill in sorted(EXPECTED_SKILLS):
        cases=[r for r in records if r['skill']==skill]
        matrix.append({'skill':skill,'success_or_bounded_cases':[r for r in cases if r['ok'] and not r['case'].startswith('failure')],'failure_or_fail_closed_cases':[r for r in cases if r['ok'] and (r['case'].startswith('failure') or 'invalid' in r['case'] or 'missing' in r['case'] or 'unavailable' in r['case'] or 'not-found' in r['case'] or 'not-pending' in r['case'])]})
    summary={
      'ok': True,
      'repo_sha': subprocess.check_output(['git','rev-parse','HEAD'], cwd=AGENT, text=True).strip(),
      'command_line': 'scripts/run_strict_skill_matrix.py',
      'evidence_dir': str(BASE),
      'registered_skill_count': len(names),
      'expected_skill_count': len(EXPECTED_SKILLS),
      'all_expected_skills_present': EXPECTED_SKILLS.issubset(names),
      'records_count': len(records),
      'matrix': matrix,
      'concurrency': {'parallel_sends': cres},
      'resilience': {'modules_after_failures': loaded},
      'notes': ['Wallet matrix uses deterministic non-FFI mode; live LEZ wallet/paid-A2A evidence is retained in phase-specific evidence.', 'Read-only skills are counted by successful bounded behavior; destructive/error cases are tested where semantically applicable.'],
      'raw_logs': sorted(str(p) for p in BASE.glob('*.stdout')),
    }
    (BASE/'strict_skill_matrix_summary.json').write_text(json.dumps(summary, indent=2, sort_keys=True, default=str))
    # Docs table.
    lines=['# Strict default-skill matrix evidence','',f'- Evidence dir: `{BASE}`',f'- Registered skills: {len(names)}',f'- Expected skills tested: {len(EXPECTED_SKILLS)}','', '| Skill | Success / bounded behavior | Fail-closed / negative behavior |', '|---|---|---|']
    for skill in sorted(EXPECTED_SKILLS):
        cases=[r for r in records if r['skill']==skill and r['ok']]
        succ=[r['case'] for r in cases if not r['case'].startswith('failure')]
        fail=[r['case'] for r in cases if r['case'].startswith('failure') or 'invalid' in r['case'] or 'missing' in r['case'] or 'unavailable' in r['case'] or 'not-found' in r['case'] or 'not-pending' in r['case']]
        lines.append(f"| `{skill}` | {', '.join(succ) or 'read-only covered by bounded status'} | {', '.join(fail) or 'not applicable / read-only'} |")
    lines += ['', '## Concurrency and resilience', '', '- Six parallel live `messaging.send` calls returned distinct live message IDs.', '- Invalid topic and fail-closed cases did not unload `agent_module`, `storage_module`, or `delivery_module`.', '- Storage share/group semantics are explicitly covered by success and missing/invalid cases.', '']
    (AGENT/'docs'/'strict-default-skill-matrix.md').write_text('\n'.join(lines))
    print('STRICT_SKILL_MATRIX_OK '+str(BASE/'strict_skill_matrix_summary.json'))
finally:
    daemon.terminate()
    try: daemon.wait(timeout=10)
    except subprocess.TimeoutExpired: daemon.kill()
    log.close()
