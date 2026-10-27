"""Foreground private playtest launcher. No files, services or settings are changed."""
import argparse
import os
from pathlib import Path
import secrets
import signal
import subprocess
import sys

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',required=True,type=Path,help='Path to this checkout’s compiled pfme_server')
    parser.add_argument('--port',type=int,default=18765)
    parser.add_argument('--seed',type=int,default=2,help='Production RNG seed; 2 is the documented playtest route')
    args=parser.parse_args()
    if not (1024<=args.port<=65535 and 0<=args.seed<=0xffffffff):parser.error('Invalid port or seed')
    executable=args.server.resolve(strict=True)
    if not executable.is_file():parser.error('Server executable is not a file')
    gate=secrets.token_hex(32)
    env=os.environ.copy();env['PFME_DEV_SECRET']=gate
    print(f'Private browser URL: http://127.0.0.1:{args.port}/play',flush=True)
    print(f'Development secret for this run: {gate}',flush=True)
    print('Copy the secret into the browser. Keep this terminal open; Ctrl+C stops the game.',flush=True)
    child=subprocess.Popen([str(executable),'--port',str(args.port),'--seed',str(args.seed)],env=env,
                           creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name=='nt' else 0)
    try:
        return child.wait()
    except KeyboardInterrupt:
        if child.poll() is None:
            child.send_signal(signal.CTRL_BREAK_EVENT if os.name=='nt' else signal.SIGTERM)
        try:return child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            child.kill();child.wait(timeout=5)
            print('Server did not stop within 5 seconds; terminated the owned child process.',file=sys.stderr)
            return 1

if __name__=='__main__':sys.exit(main())
