"""No private-mode option may silently fall back to the development TCP gate."""
import os
import secrets
import subprocess
import sys
env = dict(os.environ, PFME_DEV_SECRET=secrets.token_hex(32))
for args in (
    ['--cloudflare-peer-uid','1001','--cloudflare-peer-gid','981'],
    ['--cloudflare-peer-uid','0'],
    ['--cloudflare-origin','https://game.example.com/'],
    ['--serve-origin','https://game.example.com'],
    ['--serve-socket','/socket/backend.sock','--cloudflare-origin','https://game.example.com'],
    ['--cloudflare-socket','/socket/backend.sock','--port','0'],
):
    result = subprocess.run([sys.argv[1],*args],env=env,capture_output=True,text=True,timeout=4)
    assert result.returncode==2 and 'listening=' not in result.stdout,args
print('PASS private mode cannot fall back to TCP; invalid/mixed modes rejected')
