"""Optional owner-operated local verification; never paste assertions into chat.
Does not admit anyone, change a file, fetch keys or expose a registration route.
"""
import argparse
import getpass
import json
import sys
from access_jwt import Validator, root_document

if __name__ == '__main__':
    p=argparse.ArgumentParser();p.add_argument('--config',required=True);a=p.parse_args()
    try:
        if not sys.stdin.isatty(): raise RuntimeError('interactive terminal required')
        config=root_document(a.config);v=Validator(config['issuer'],config['audience']);v.load(root_document(config['keys']))
        identity=v.verify(getpass.getpass('Access assertion (local terminal only; not echoed): '))
        print(json.dumps({'verified_identity_login':identity.login,'expires':identity.expires}))
    except Exception:
        print('{"event":"assertion_verification_failed"}')
        raise SystemExit(1) from None
