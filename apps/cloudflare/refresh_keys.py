"""Fetch public JWKs to a NEW pending file, never change active/root trust.
No tokens/credentials. Operator reviews and atomically installs as root later.
"""
import argparse
import hashlib
import json
import os
import ssl
import time
import urllib.request
from access_jwt import Denied, Validator, issuer_valid, strict_json


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *_args, **_kwargs):
        raise Denied('key endpoint redirect rejected')


def fetch(issuer):
    if not issuer_valid(issuer): raise Denied('invalid trusted issuer')
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect(),
        urllib.request.HTTPSHandler(context=ssl.create_default_context()))
    request = urllib.request.Request(issuer + '/cdn-cgi/access/certs', headers={'Accept':'application/json'})
    with opener.open(request, timeout=5) as response:
        if response.status != 200: raise Denied('key endpoint failed')
        raw = response.read(131073)
    document = strict_json(raw)
    if not isinstance(document, dict) or 'keys' not in document: raise Denied('invalid public key response')
    cache = dict(issuer=issuer, fetched_at=int(time.time()), keys=document['keys'])
    Validator(issuer, 'key-validation-only').load(cache)
    return cache


if __name__ == '__main__':
    parser = argparse.ArgumentParser();parser.add_argument('--issuer', required=True);parser.add_argument('--pending-output', required=True)
    args = parser.parse_args()
    try:
        raw = (json.dumps(fetch(args.issuer), separators=(',',':'))+'\n').encode()
        fd = os.open(args.pending_output, os.O_WRONLY|os.O_CREAT|os.O_EXCL, 0o600)
        with os.fdopen(fd,'wb') as stream: stream.write(raw);stream.flush();os.fsync(stream.fileno())
        print(json.dumps({'event':'pending_keys_written','sha256':hashlib.sha256(raw).hexdigest()}))
    except Exception:
        print('{"event":"key_refresh_failed"}')
        raise SystemExit(1) from None
