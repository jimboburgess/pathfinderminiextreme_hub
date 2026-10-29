"""One-shot named-volume initialization; no network or unrelated host mounts."""
import os,secrets
from pathlib import Path
def main():
    assert os.geteuid()==0
    for name,uid,gid,mode in [('state',995,980,0o750),('policy',0,980,0o750),('status',995,980,0o750),
                              ('front',995,979,0o710),('back',999,980,0o710),('gate',0,979,0o750)]:
        p=Path('/'+name);os.chown(p,uid,gid);os.chmod(p,mode)
    p=Path('/state/secret.key')
    if not p.exists():
        fd=os.open(p,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o640)
        with os.fdopen(fd,'w') as f:f.write(secrets.token_urlsafe(64))
        os.chown(p,995,980)
    p=Path('/policy/admission.json')
    if not p.exists():
        fd=os.open(p,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o640)
        with os.fdopen(fd,'w') as f:f.write('{"version":1,"revision":1,"users":[]}')
        os.chown(p,0,980)
    p=Path('/gate/release')
    if p.exists():
        if p.is_symlink() or not p.is_file():raise RuntimeError('Unexpected gate path')
        p.unlink()
    print('Initialized owned volumes only; admission remains administrator-managed.')
if __name__=='__main__':main()
