"""Generated RSA fixtures only; never includes a real token or signing secret."""
import base64
import json
from pathlib import Path
import sys
import unittest
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'apps/cloudflare'))
from access_jwt import Denied, Validator, identity_login, strict_json
from gateway import config_valid, origin_valid
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa

ISSUER = 'https://fixture-team.cloudflareaccess.com'
NOW = 1800000000


def b64(raw): return base64.urlsafe_b64encode(raw).decode().rstrip('=')


def jwk(key, kid='current'):
    public = key.public_key().public_numbers()
    return dict(kty='RSA', use='sig', alg='RS256', kid=kid,
                n=b64(public.n.to_bytes((public.n.bit_length()+7)//8, 'big')),
                e=b64(public.e.to_bytes(3, 'big')))


def token(key, claims=None, header=None, payload=None):
    h = b64(json.dumps(header or dict(alg='RS256', kid='current', typ='JWT')).encode())
    c = dict(iss=ISSUER, sub='fixture-user', email='fixture@example.com', type='app',
             aud=['fixture-audience'], iat=NOW-10, nbf=NOW-10, exp=NOW+900)
    c.update(claims or {})
    body = h + '.' + b64(payload if payload is not None else json.dumps(c).encode())
    return body + '.' + b64(key.sign(body.encode(), padding.PKCS1v15(), hashes.SHA256()))


class AccessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        cls.other = rsa.generate_private_key(public_exponent=65537, key_size=2048)

    def setUp(self):
        self.validator = Validator(ISSUER, 'fixture-audience')
        self.validator.load(dict(issuer=ISSUER, fetched_at=NOW, keys=[jwk(self.key)]), NOW)

    def denied(self, value):
        with self.assertRaises(Denied): self.validator.verify(value, NOW)

    def test_real_rsa_signature_and_stable_subject(self):
        identity = self.validator.verify(token(self.key), NOW)
        self.assertEqual(identity.login, identity_login(ISSUER, 'fixture-user'))
        self.assertEqual(identity.expires, NOW+900)
        same = self.validator.verify(token(self.key, {'email':'changed@example.com'}), NOW)
        self.assertEqual(same.login, identity.login)
        self.assertNotEqual(identity.login, identity_login(ISSUER, 'another-user'))

    def test_invalid_claims(self):
        for claims in ({'iss':'https://wrong.cloudflareaccess.com'}, {'aud':['wrong']}, {'aud':[]},
                       {'exp':NOW}, {'exp':NOW-1}, {'exp':True}, {'exp':'later'},
                       {'iat':NOW+1}, {'iat':False}, {'nbf':NOW+1}, {'nbf':'later'},
                       {'sub':''}, {'sub':'x\r\nheader'}, {'email':''}, {'email':None},
                       {'type':'org'}, {'sub':None}, {'aud':['fixture-audience',True]}):
            with self.subTest(claims=claims): self.denied(token(self.key, claims))

    def test_signature_algorithm_and_header_attacks(self):
        self.denied(token(self.other))
        valid = token(self.key)
        self.denied(valid[:-1] + ('A' if valid[-1] != 'A' else 'B'))
        for header in (dict(alg='none',kid='current',typ='JWT'), dict(alg='HS256',kid='current',typ='JWT'),
                       dict(alg='RS256',kid='unknown',typ='JWT'),
                       dict(alg='RS256',kid='current',typ='JWT',jku='https://evil.invalid/keys'),
                       dict(alg='RS256',kid='current',typ='JWT',crit=['evil'])):
            self.denied(token(self.key, header=header))

    def test_malformed_and_duplicate_fields(self):
        for value in ('', 'a.b', 'a.b.c.d', 'a.'*5000, token(self.key)+'=', 'a.b.c', None): self.denied(value)
        self.denied(token(self.key, payload=b'{"iss":"x","iss":"y"}'))
        with self.assertRaises(Denied): strict_json('{"a":{"b":1,"b":2}}')
        with self.assertRaises(Denied): strict_json('{"a":NaN}')
        with self.assertRaises(Denied): strict_json('['*10+'0'+']'*10)

    def test_rotation_current_and_previous_keys(self):
        rotated = token(self.other, header=dict(alg='RS256',kid='next',typ='JWT'))
        self.denied(rotated)
        self.validator.load(dict(issuer=ISSUER, fetched_at=NOW, keys=[jwk(self.key),jwk(self.other,'next')]), NOW)
        self.validator.verify(rotated, NOW);self.validator.verify(token(self.key), NOW)
        self.validator.load(dict(issuer=ISSUER, fetched_at=NOW, keys=[jwk(self.other,'next')]), NOW)
        self.denied(token(self.key));self.validator.verify(rotated, NOW)

    def test_cache_expiry_and_failed_refresh_fail_closed(self):
        with self.assertRaises(Denied): self.validator.verify(token(self.key,{'exp':NOW+10000}), NOW+3600)
        for value in (dict(issuer=ISSUER,fetched_at=NOW-3600,keys=[jwk(self.key)]),
                      dict(issuer=ISSUER,fetched_at=NOW+1,keys=[jwk(self.key)]),
                      dict(issuer=ISSUER,fetched_at=NOW,keys=[jwk(self.key),jwk(self.key)])):
            with self.assertRaises(Denied): self.validator.load(value, NOW)
            self.denied(token(self.key))

    def test_no_service_tokens_or_email_identity(self):
        self.denied(token(self.key,{'sub':'','email':None,'common_name':'service-token'}))
        self.assertTrue(identity_login(ISSUER,'fixture-user').startswith('cf-'))
        with self.assertRaises(Denied): identity_login('https://evil.invalid','fixture-user')

    def test_exact_production_origin(self):
        self.assertTrue(origin_valid('https://game.example.com'))
        for value in ('http://game.example.com','https://*.example.com','https://game.example.com:443',
                      'https://game.example.com/','https://game.example.com@evil.invalid','https://game..example.com'):
            self.assertFalse(origin_valid(value))


if __name__ == '__main__': unittest.main()
