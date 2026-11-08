#!/bin/sh
# One-shot only. Call hourly after deployment; no game/container restart.
set -eu
cd -- "$(dirname -- "$0")"
python3 verify_images.py .env
# Fail closed if the Samsung HDD is absent, replaced or mounted elsewhere.
test "$(findmnt -n -o UUID --target /mnt/samsung-hdd)" = '47d5e89e-9753-4420-be55-347a950371cd'
test "$(findmnt -n -o TARGET --target /mnt/samsung-hdd)" = '/mnt/samsung-hdd'
python3 - <<'PY'
from pathlib import Path
import subprocess
values=dict(line.split('=',1) for line in Path('.env').read_text().splitlines() if line and not line.startswith('#') and '=' in line)
image=values['PFME_ACCOUNTS_IMAGE_ID'].strip('"\'')
assert image.startswith('sha256:') and len(image)==71
command=['docker','run','--rm','--network','none','--read-only','--user','0:0','--cap-drop','ALL','--cap-add','DAC_OVERRIDE',
 '--security-opt','no-new-privileges','--pids-limit','32','--memory','64m','--cpus','0.20','--log-driver','none',
 '--mount','type=volume,src=pfme-friends_characters,dst=/saves,readonly',
 '--mount','type=bind,src=/mnt/samsung-hdd/pathfinder-backups,dst=/backups',
 '--mount','type=bind,src='+str(Path('character_backup.py').resolve())+',dst=/character_backup.py,readonly',
 '--entrypoint','python',image,'/character_backup.py','backup','/saves','/backups']
subprocess.run(command,check=True)
PY
