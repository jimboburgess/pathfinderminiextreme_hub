"""Admin terminal only. Tunnel token in hidden input; never in argv/logs/Git."""
import os,sys,getpass,secrets
from pathlib import Path
def main():
    if os.geteuid()!=0 or not sys.stdin.isatty():raise SystemExit('Use the interactive administrator Compose helper.')
    targets=[(Path('/tunnel_secret'),979,'tunnel-token'),(Path('/status_secret'),980,'status-token')]
    for directory,gid,name in targets:
        if (directory/name).exists():raise SystemExit('Secrets already exist; stop for explicit rotation review.')
    token=getpass.getpass('Remotely managed tunnel token (hidden; do not paste into chat): ')
    if not 32<=len(token)<=8192 or not token.startswith('ey') or any(c.isspace() for c in token):raise SystemExit('Unexpected token format; nothing written.')
    for directory,gid,name in targets:
        os.chown(directory,0,gid);directory.chmod(0o750)
        fd=os.open(directory/name,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o440)
        with os.fdopen(fd,'w') as f:f.write(token if name=='tunnel-token' else secrets.token_urlsafe(48))
        os.chown(directory/name,0,gid)
    del token
    print('Tokens stored in their separate private Docker volumes. No connector started.')
if __name__=='__main__':main()
