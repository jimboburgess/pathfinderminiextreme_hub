"""Offline gateway image preparation using installed Ubuntu Python/cryptography.
No package installation, base-image pull, container start or network configuration.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import sysconfig
import cryptography


def copy_python(root):
    def copy(path):
        target = root / str(path).lstrip('/')
        target.parent.mkdir(parents=True, exist_ok=True)
        if path.is_dir(): shutil.copytree(path, target, dirs_exist_ok=True, ignore=shutil.ignore_patterns('__pycache__','test','tests','idlelib','tkinter','turtledemo','ensurepip'))
        else: shutil.copy2(path.resolve(), target)
    executable = Path(sys.executable).resolve();copy(executable)
    stdlib = Path(sysconfig.get_path('stdlib'));copy(stdlib)
    package = Path(cryptography.__file__).parent;copy(package)
    try:
        import _cffi_backend
        copy(Path(_cffi_backend.__file__))
    except ImportError: pass
    # Resolve dependencies of every copied extension using existing system libs.
    inputs = [root / str(executable).lstrip('/'), *root.rglob('*.so'), *root.rglob('*.so.*')]
    for path in inputs:
        result = subprocess.run(['ldd',str(path)],capture_output=True,text=True)
        if 'not found' in result.stdout: raise RuntimeError('missing installed Python dependency')
        for name in set(re.findall(r'(/[\w/+.,-]+)',result.stdout)):
            source = Path(name)
            if source.is_file(): copy(source)
    return str(executable)


def main():
    p=argparse.ArgumentParser();p.add_argument('--destination',type=Path,required=True);p.add_argument('--image',required=True);p.add_argument('--uid',type=int,required=True);p.add_argument('--gid',type=int,required=True)
    a=p.parse_args();assert sys.platform=='linux' and os.getuid()!=0 and a.uid>0 and a.gid>0
    assert re.fullmatch(r'pfme-cf-gateway:[a-z0-9-]+',a.image) and not a.destination.exists()
    assert subprocess.run(['docker','image','inspect',a.image],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode!=0
    a.destination.mkdir(mode=0o700);root=a.destination/'rootfs';root.mkdir();executable=copy_python(root)
    target=root/'adapter';shutil.copytree(Path(__file__).parent,target,ignore=shutil.ignore_patterns('__pycache__'))
    # Include installed package copyright text without copying personal config.
    for package in ('python3.14-minimal','libpython3.14-stdlib','python3-cryptography','libc6','libssl3t64'):
        source=Path('/usr/share/doc')/package/'copyright'
        if source.is_file():
            out=root/'usr/share/licenses'/package/'copyright';out.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source.resolve(),out)
    (a.destination/'Dockerfile').write_text('FROM scratch\nCOPY rootfs/ /\nENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 CRYPTOGRAPHY_OPENSSL_NO_LEGACY=1\nUSER '+str(a.uid)+':'+str(a.gid)+'\nENTRYPOINT ["'+executable+'","/adapter/gateway.py"]\n')
    subprocess.run(['docker','build','--network=none','--pull=false','-q','-t',a.image,str(a.destination)],check=True)
    image=subprocess.check_output(['docker','image','inspect','--format','{{.Id}}',a.image],text=True).strip()
    manifest={'image_id':image,'started':False,'cryptography':cryptography.__version__,'python':sys.version.split()[0],
              'uid':a.uid,'gid':a.gid,'files_sha256':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in root.rglob('*') if f.is_file()}}
    (a.destination/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps({'image_id':image,'started':False,'cryptography':cryptography.__version__}))


if __name__=='__main__':main()
