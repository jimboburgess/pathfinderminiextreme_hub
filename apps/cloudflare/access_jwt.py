"""Cloudflare Access application JWT validation. No network/token logging.

Only vetted cryptography primitives perform RSA/SHA256 verification. Trusted
issuer/audience and the bounded root-managed rotating JWK cache are supplied by
the operator, never by token headers/claims. Pure validation also runs on Windows.
"""
import base64
import hashlib
import json
import os
import re
import stat
import time
from dataclasses import dataclass
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa


class Denied(Exception):
    pass


def strict_json(raw, maximum=131072):
    if not isinstance(raw, (bytes, str)) or len(raw) > maximum:
        raise Denied('invalid document')
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise Denied('duplicate field')
            result[key] = value
        return result
    def invalid(_):
        raise Denied('invalid number')
    try:
        result = json.loads(raw, object_pairs_hook=pairs, parse_constant=invalid)
        def depth(value, level=0):
            if level > 8:
                raise Denied('document depth')
            if isinstance(value, dict):
                for item in value.values(): depth(item, level + 1)
            elif isinstance(value, list):
                for item in value: depth(item, level + 1)
        depth(result)
        return result
    except (ValueError, TypeError, RecursionError, UnicodeError) as error:
        raise Denied('invalid document') from error


def root_document(path):
    flags = os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW | os.O_NONBLOCK
    fd = os.open(path, flags)
    try:
        before = os.fstat(fd)
        if (not stat.S_ISREG(before.st_mode) or before.st_uid != 0 or
                before.st_mode & 0o022 or before.st_nlink != 1 or
                not 0 < before.st_size <= 131072):
            raise Denied('unsafe document')
        raw = bytearray()
        while len(raw) < before.st_size:
            part = os.read(fd, before.st_size - len(raw))
            if not part: raise Denied('incomplete document')
            raw.extend(part)
        after = os.fstat(fd)
        if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
            raise Denied('changed document')
        return strict_json(bytes(raw))
    finally:
        os.close(fd)


def integer(value):
    return type(value) is int and 0 <= value <= 2**53 - 1


def printable(value, maximum):
    return isinstance(value, str) and 0 < len(value) <= maximum and all(33 <= ord(c) <= 126 for c in value)


def issuer_valid(value):
    return isinstance(value, str) and re.fullmatch(r'https://[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.cloudflareaccess\.com', value) is not None


def unbase64(value):
    if not isinstance(value, str) or not value or len(value) > 8192 or re.fullmatch(r'[A-Za-z0-9_-]+', value) is None:
        raise Denied('invalid base64url')
    try:
        raw = base64.urlsafe_b64decode(value + '=' * (-len(value) % 4))
        if base64.urlsafe_b64encode(raw).decode().rstrip('=') != value:
            raise Denied('noncanonical base64url')
        return raw
    except ValueError as error:
        raise Denied('invalid base64url') from error


@dataclass(frozen=True)
class Identity:
    login: str
    expires: int


def identity_login(issuer, subject):
    if not issuer_valid(issuer) or not printable(subject, 128):
        raise Denied('invalid subject')
    return 'cf-' + hashlib.sha256((issuer + '\0' + subject).encode()).hexdigest()


class Validator:
    def __init__(self, issuer, audience):
        if not issuer_valid(issuer) or not printable(audience, 128):
            raise Denied('invalid trusted configuration')
        self.issuer, self.audience = issuer, audience
        self.keys, self.cache_deadline = {}, 0

    def load(self, document, now=None):
        now = int(time.time()) if now is None else now
        # Invalidate before parsing so a failed rotation never preserves stale trust.
        self.keys, self.cache_deadline = {}, 0
        if (not isinstance(document, dict) or set(document) != {'issuer', 'fetched_at', 'keys'} or
                document['issuer'] != self.issuer or not integer(document['fetched_at']) or
                not document['fetched_at'] <= now < document['fetched_at'] + 3600 or
                not isinstance(document['keys'], list) or not 1 <= len(document['keys']) <= 16):
            raise Denied('invalid key cache')
        keys = {}
        try:
            for item in document['keys']:
                if (not isinstance(item, dict) or item.get('kty') != 'RSA' or
                        item.get('alg', 'RS256') != 'RS256' or item.get('use', 'sig') != 'sig' or
                        not printable(item.get('kid'), 128) or item['kid'] in keys or
                        any(k in item for k in ('d', 'p', 'q', 'dp', 'dq', 'qi'))):
                    raise Denied('invalid signing key')
                n, e = unbase64(item['n']), unbase64(item['e'])
                if not 256 <= len(n) <= 512 or n[0] < 128 or not n[-1] & 1 or e != b'\x01\x00\x01':
                    raise Denied('unsupported RSA key')
                keys[item['kid']] = rsa.RSAPublicNumbers(65537, int.from_bytes(n, 'big')).public_key()
        except (KeyError, ValueError, TypeError) as error:
            raise Denied('invalid signing key') from error
        self.keys, self.cache_deadline = keys, document['fetched_at'] + 3600

    def verify(self, token, now=None):
        now = int(time.time()) if now is None else now
        if not printable(token, 8192) or not now < self.cache_deadline:
            raise Denied('invalid assertion')
        parts = token.split('.')
        if len(parts) != 3:
            raise Denied('invalid assertion')
        try:
            header = strict_json(unbase64(parts[0]), 1024)
            claims = strict_json(unbase64(parts[1]), 6144)
            if (not isinstance(header, dict) or set(header) not in ({'alg', 'kid'}, {'alg', 'kid', 'typ'}) or
                    header['alg'] != 'RS256' or header.get('typ', 'JWT') != 'JWT' or
                    not printable(header['kid'], 128) or header['kid'] not in self.keys or
                    not isinstance(claims, dict)):
                raise Denied('unsupported assertion')
            signature = unbase64(parts[2]);key = self.keys[header['kid']]
            if len(signature) != key.key_size // 8:
                raise Denied('invalid signature length')
            key.verify(signature, (parts[0] + '.' + parts[1]).encode(), padding.PKCS1v15(), hashes.SHA256())
            aud = claims.get('aud')
            audiences = [aud] if isinstance(aud, str) else aud
            if (claims.get('iss') != self.issuer or claims.get('type') != 'app' or
                    not isinstance(audiences, list) or not 1 <= len(audiences) <= 8 or
                    not all(printable(a, 128) for a in audiences) or self.audience not in audiences or
                    not integer(claims.get('exp')) or not integer(claims.get('iat')) or
                    not claims['iat'] <= now < claims['exp'] or
                    ('nbf' in claims and (not integer(claims['nbf']) or now < claims['nbf'])) or
                    not printable(claims.get('email'), 254) or '@' not in claims['email']):
                raise Denied('invalid claims')
            return Identity(identity_login(self.issuer, claims.get('sub')), claims['exp'])
        except Denied:
            raise
        except Exception as error:
            raise Denied('invalid assertion') from error
