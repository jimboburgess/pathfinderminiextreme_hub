"""Build a non-running private runtime image from the verified Ubuntu executable.
No downloads, package changes, container starts or live networking configuration.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(*args):return subprocess.check_output(args,text=True).strip()
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--binary',type=Path,required=True);parser.add_argument('--destination',type=Path,required=True);parser.add_argument('--image',required=True)
    parser.add_argument('--runtime-uid',type=int,required=True);parser.add_argument('--runtime-gid',type=int,required=True)
    args=parser.parse_args();assert 0<args.runtime_uid<2147483647 and args.runtime_uid!=1000 and 0<args.runtime_gid<2147483647,'Dedicated unprivileged runtime identity required'
    binary=args.binary.resolve();destination=args.destination.absolute()
    assert os.getuid()==1000 and binary.is_file(),'Run offline packaging as the existing unprivileged operator; runtime identity is separate'
    assert re.fullmatch(r'pfme-private-stage1:[a-z0-9-]+',args.image)
    assert not destination.exists(),'Refuse to overwrite an existing preparation directory'
    assert subprocess.run(['docker','image','inspect',args.image],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode!=0,'Refuse to replace an existing image tag'
    security=json.loads(run('docker','info','--format','{{json .SecurityOptions}}'))
    assert not any('userns' in item or 'rootless' in item for item in security),'Kernel UID trust requires explicit remapping review'
    destination.mkdir(mode=0o700);context=destination/'context';rootfs=context/'rootfs';(rootfs/'app').mkdir(parents=True)
    shutil.copy2(binary,rootfs/'app/pfme_server');dependencies=run('ldd',str(binary));assert 'not found' not in dependencies
    manifest={'binary_sha256':sha(binary),'libraries':{},'image_tag':args.image,'prepared_only':True,'started':False,'runtime_uid':args.runtime_uid,'runtime_gid':args.runtime_gid}
    for name in sorted(set(re.findall(r'(/[\w/+.,-]+)',dependencies))):
        path=Path(name)
        if path.is_file():
            target=rootfs/name.lstrip('/');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path.resolve(),target);manifest['libraries'][name]=sha(target)
    for package in ('libc6','libstdc++6','libgcc-s1'):
        license=Path('/usr/share/doc')/package/'copyright'
        if license.exists():
            target=rootfs/'usr/share/licenses'/package/'copyright';target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(license.resolve(),target)
    source=Path(__file__).resolve().parents[2]
    cache=(binary.parents[2]/'CMakeCache.txt').read_text()
    def cached(key,fallback):
        match=re.search(r'^'+re.escape(key)+r':[^=]*=(.*)$',cache,re.MULTILINE)
        return Path(match[1]) if match and match[1] else fallback
    licenses={
        'adafruit_gfx.txt':source/'apps/browser/third_party/adafruit_gfx/license.txt',
        'boost.txt':cached('FETCHCONTENT_SOURCE_DIR_PFME_BOOST',binary.parents[2]/'_deps/pfme_boost-src')/'LICENSE_1_0.txt',
        'monocypher.txt':cached('PFME_MONOCYPHER_DIR',binary.parents[2]/'_deps/monocypher')/'LICENCE.md',
    }
    for name,path in licenses.items():
        assert path.is_file(),path
        target=rootfs/'usr/share/licenses/pfme'/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
    dockerfile=Path(__file__).with_name('Dockerfile').read_text()
    assert dockerfile.count('USER 1000:1000')==1
    (context/'Dockerfile').write_text(dockerfile.replace('USER 1000:1000',f'USER {args.runtime_uid}:{args.runtime_gid}'))
    subprocess.run(['docker','build','--network=none','--pull=false','-q','-t',args.image,str(context)],check=True)
    manifest['image_id']=run('docker','image','inspect','--format','{{.Id}}',args.image)
    (destination/'prepared-runtime.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({'destination':str(destination),**manifest},sort_keys=True))
if __name__=='__main__':main()
