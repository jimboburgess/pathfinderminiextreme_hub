"""Operator-only append of ONE Pathfinder section; token read from stdin."""
import sys,hashlib,os,stat
from pathlib import Path
def main():
    if len(sys.argv)!=2:raise SystemExit('Provide the reviewed Homepage services.yaml path.')
    path=Path(sys.argv[1]);token=sys.stdin.read().strip()
    if not 32<=len(token)<=128 or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-' for c in token):raise SystemExit('Invalid private status token.')
    if path.is_symlink() or not path.is_file():raise SystemExit('Expected regular Homepage services.yaml.')
    old=path.read_text()
    if '\n- Pathfinder:' in '\n'+old:raise SystemExit('Pathfinder section already exists; review instead of duplicating.')
    backup=path.with_name(path.name+'.before-pathfinder')
    if backup.exists():raise SystemExit('Backup exists; inspect before modifying.')
    fd=os.open(backup,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'w') as f:f.write(old)
    section='''
- Pathfinder:
    - Pathfinder Mini EXTREME:
        href: https://game.noburdenmusics.com/play
        description: Four-player shared dungeon; individual accounts
        widget:
          type: customapi
          url: http://192.168.1.216:18766/status
          headers:
            Authorization: "Bearer TOKEN"
          mappings:
            - field: state
              label: Game
            - field: active_players
              label: Playing
            - field: capacity
              label: Slots
            - field: usernames
              label: Players
            - field: connected_accounts
              label: Signed in
'''.replace('TOKEN',token)
    temporary=path.with_name(path.name+'.pathfinder-new')
    fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o640)
    with os.fdopen(fd,'w') as f:f.write(old.rstrip()+'\n'+section)
    temporary.replace(path)
    print('Appended only Pathfinder. Backup kept privately; no container restart. Prior SHA256='+hashlib.sha256(old.encode()).hexdigest())
if __name__=='__main__':main()
