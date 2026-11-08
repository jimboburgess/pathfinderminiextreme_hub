"""Install/remove only the Pathfinder entry in the current user's crontab."""
import argparse
import os
from pathlib import Path
import subprocess

MARKER = '# PFME_CHARACTER_BACKUP_V1'
def main():
    p=argparse.ArgumentParser();p.add_argument('operation',choices=['install','remove']);args=p.parse_args()
    root=Path(__file__).resolve().parent
    # The reviewed deployment path contains no spaces or shell metacharacters.
    if str(root)!='/home/jimbo/pathfinder-friends':raise SystemExit('Unexpected deployment directory')
    entry=f'17 * * * * /usr/bin/flock -n {root}/.character-backup.lock /usr/bin/timeout 120 /bin/sh {root}/backup-characters.sh > {root}/character-backup.latest.log 2>&1'
    previous=subprocess.run(['crontab','-l'],capture_output=True,text=True)
    if previous.returncode not in (0,1):raise SystemExit('Could not read crontab')
    original=previous.stdout if previous.returncode==0 else ''
    lines=original.splitlines(keepends=True)
    indices=[i for i,line in enumerate(lines) if line.strip()==MARKER]
    if len(indices)>1:raise SystemExit('Duplicate backup markers: review first')
    if indices:
        i=indices[0]
        if i+1>=len(lines) or lines[i+1].strip()!=entry:raise SystemExit('Unexpected existing backup entry')
        del lines[i:i+2]
    if args.operation=='install':
        subprocess.run(['systemctl','is-active','--quiet','cron'],check=True)
        subprocess.run(['/bin/sh',str(root/'backup-characters.sh')],check=True)
        lines.append(('' if not lines or lines[-1].endswith('\n') else '\n')+MARKER+'\n'+entry+'\n')
    replacement=''.join(lines)
    if replacement!=original:subprocess.run(['crontab','-'],input=replacement,text=True,check=True)
    print('Pathfinder hourly backup '+('enabled; latest 336 archives retained.' if args.operation=='install' else 'removed; unrelated cron entries preserved.'))
if __name__=='__main__':
    os.umask(0o077)
    main()
