"""Verify longer CLI lifetimes without waiting for the timer to expire."""
import os,secrets,subprocess,sys
executable=sys.argv[1]
env=dict(os.environ,PFME_DEV_SECRET=secrets.token_hex(32))
for value in ('86400001','4294967296','-1'):
    r=subprocess.run([executable,'--run-for-ms',value],env=env,capture_output=True,text=True,timeout=10)
    assert r.returncode==2 and 'invalid startup option' in r.stderr,(value,r.returncode)
for value in ('0','28800000','86400000'):
    p=subprocess.Popen([executable,'--port','0','--run-for-ms',value],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    try:
        try:p.communicate(timeout=1);raise AssertionError('Long lifetime exited early')
        except subprocess.TimeoutExpired:pass
    finally:
        p.terminate();out,err=p.communicate(timeout=10)
    assert 'listening=' in out,(out,err)
    if sys.platform=='linux':assert p.returncode==0 and 'stopped' in out,(p.returncode,out,err)
r=subprocess.run([executable,'--port','0','--run-for-ms','30'],env=env,capture_output=True,text=True,timeout=10)
assert r.returncode==0 and 'listening=' in r.stdout and 'stopped' in r.stdout
print('Explicit zero runs until stopped; finite timers and graceful shutdown preserved.')
