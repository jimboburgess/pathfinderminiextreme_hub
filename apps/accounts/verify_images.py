"""Refuse image drift or accidental registry pulls. .env is data, never executed."""
import json,re,subprocess,sys
from pathlib import Path
def main():
    text=Path(sys.argv[1] if len(sys.argv)>1 else '.env').read_text()
    if len(text)>8192:raise SystemExit('Unexpected environment file size')
    values={}
    for line in text.splitlines():
        if not line.strip() or line.lstrip().startswith('#'):continue
        name,sep,value=line.partition('=')
        if not sep or name in values:raise SystemExit('Malformed environment file')
        values[name]=value
    for role in ('GAME','ACCOUNTS','CONNECTOR'):
        name=values.get('PFME_'+role+'_IMAGE','');expected=values.get('PFME_'+role+'_IMAGE_ID','')
        if not re.fullmatch(r'pfme-[a-z0-9-]+:[a-z0-9][a-z0-9_.-]{0,127}',name) or not re.fullmatch(r'sha256:[0-9a-f]{64}',expected):raise SystemExit('Missing pinned local image '+role)
        try:actual=subprocess.check_output(['docker','image','inspect','--format','{{.Id}}',name],text=True,stderr=subprocess.DEVNULL).strip()
        except subprocess.CalledProcessError:raise SystemExit('Missing local image '+role+'; no registry pull attempted')
        if actual!=expected:raise SystemExit('Image drift: '+role+'; stop for review')
    print('Pinned local game/account/connector images verified.')
if __name__=='__main__':main()
