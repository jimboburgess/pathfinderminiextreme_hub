"""Unprivileged, network-isolated Access adapter; Unix sockets only.
cloudflared -> protected front socket -> JWT verification -> protected C++ hop.
No browser email/header is accepted as identity without signature verification.
"""
import argparse
import asyncio
import base64
import contextlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import stat
import struct
import time
from access_jwt import Denied, Validator, root_document


def origin_valid(origin):
    if not isinstance(origin, str) or not origin.startswith('https://'):
        return False
    host = origin[8:]
    return (len(host) <= 253 and '.' in host and all(
        re.fullmatch(r'[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?', label)
        for label in host.split('.')))


def config_valid(config):
    if (not isinstance(config, dict) or set(config) != {
            'version', 'issuer', 'audience', 'origin', 'frontend', 'backend',
            'connector_uid', 'frontend_gid', 'keys'} or config['version'] != 1 or
            not origin_valid(config['origin']) or
            any(type(config[k]) is not int or not 0 < config[k] < 2**31 for k in ('connector_uid', 'frontend_gid'))):
        raise Denied('invalid configuration')
    for name in ('frontend', 'backend', 'keys'):
        path = Path(config[name])
        if not path.is_absolute() or str(path) != config[name] or '..' in path.parts or len(str(path)) >= 100:
            raise Denied('invalid configured path')
    Validator(config['issuer'], config['audience'])
    return config


class Gateway:
    def __init__(self, config):
        self.config = config_valid(config)
        self.validator = Validator(config['issuer'], config['audience'])
        self.reload_keys()
        self.active = 0
        self.window, self.accepts = time.monotonic(), 0
        self.serial = 0

    def reload_keys(self):
        try:
            self.validator.load(root_document(self.config['keys']))
        except Exception:
            self.validator.keys, self.validator.cache_deadline = {}, 0
            raise Denied('key cache unavailable') from None

    def event(self, name, serial):
        # Codes/counters only; never JWTs, cookies, claims, emails or request bytes.
        print(json.dumps({'event': name, 'connection': serial}), flush=True)

    async def write(self, writer, data):
        writer.write(data)
        await asyncio.wait_for(writer.drain(), 5)

    async def pipe(self, reader, writer):
        while True:
            data = await asyncio.wait_for(reader.read(16384), 20)
            if not data: return
            await self.write(writer, data)

    async def lease(self, token, deadline):
        while time.monotonic() < deadline:
            await asyncio.sleep(.5)
            self.reload_keys()
            self.validator.verify(token)

    async def handle(self, reader, writer):
        serial, counted, upstream, tasks = 0, False, None, []
        try:
            peer = writer.get_extra_info('socket')
            _, uid, _ = struct.unpack('3i', peer.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12))
            if uid != self.config['connector_uid']: return
            now = time.monotonic()
            if now - self.window >= 1: self.window, self.accepts = now, 0
            self.accepts += 1
            if self.active >= 8 or self.accepts > 32: return
            self.active += 1;counted = True;self.serial += 1;serial = self.serial
            writer.transport.set_write_buffer_limits(high=32768, low=16384)
            request = await asyncio.wait_for(reader.readuntil(b'\r\n\r\n'), 3)
            if len(request) > 32768: raise Denied('header bound')
            lines = request[:-4].decode('ascii').split('\r\n')
            if len(lines) > 65: raise Denied('header count')
            method, target, version = lines[0].split(' ')
            if method != 'GET' or version != 'HTTP/1.1' or target not in ('/', '/play', '/play.js', '/play.css', '/audio.js'):
                raise Denied('unsupported route')
            headers = {}
            for line in lines[1:]:
                name, value = line.split(':', 1);name = name.lower()
                if not re.fullmatch(r'[a-z0-9-]+', name) or name in headers or any(ord(c) < 32 or ord(c) > 126 for c in value):
                    raise Denied('invalid headers')
                headers[name] = value.strip(' ')
            if (headers.get('host') != self.config['origin'][8:] or
                    'transfer-encoding' in headers or headers.get('content-length', '0') != '0' or
                    ('origin' in headers and headers['origin'] != self.config['origin'])):
                raise Denied('invalid origin or framing')
            self.reload_keys()
            token = headers.get('cf-access-jwt-assertion', '')
            identity = self.validator.verify(token)
            upgrade = headers.get('upgrade', '').lower() == 'websocket'
            forwarded = ['GET ' + target + ' HTTP/1.1', 'Host: localhost', 'Pfme-Verified-Identity: ' + identity.login]
            if 'origin' in headers: forwarded.append('Origin: ' + headers['origin'])
            if upgrade:
                key = headers.get('sec-websocket-key', '')
                if (target != '/' or headers.get('origin') != self.config['origin'] or
                        'upgrade' not in [v.strip().lower() for v in headers.get('connection', '').split(',')] or
                        headers.get('sec-websocket-version') != '13' or
                        len(base64.b64decode(key, validate=True)) != 16 or
                        'sec-websocket-protocol' in headers):
                    raise Denied('invalid WebSocket upgrade')
                forwarded += ['Upgrade: websocket', 'Connection: Upgrade', 'Sec-WebSocket-Version: 13', 'Sec-WebSocket-Key: ' + key]
            elif target == '/' or 'upgrade' in headers:
                raise Denied('invalid upgrade')
            # Cookies, Access assertions, forwarded email/IP and all other client
            # headers are deliberately absent from the authenticated backend hop.
            back_reader, upstream = await asyncio.wait_for(asyncio.open_unix_connection(self.config['backend'], limit=32768), 3)
            upstream.transport.set_write_buffer_limits(high=32768, low=16384)
            await self.write(upstream, ('\r\n'.join(forwarded) + '\r\n\r\n').encode())
            response = await asyncio.wait_for(back_reader.readuntil(b'\r\n\r\n'), 3)
            if len(response) > 8192 or not response.startswith(b'HTTP/1.1 ' + (b'101 ' if upgrade else b'200 ')):
                raise Denied('admission denied')
            await self.write(writer, response)
            self.event('authorized_upgrade' if upgrade else 'authorized_asset', serial)
            deadline = time.monotonic() + min(900, identity.expires - time.time())
            tasks = [asyncio.create_task(self.pipe(back_reader, writer)),
                     asyncio.create_task(self.pipe(reader, upstream)),
                     asyncio.create_task(self.lease(token, deadline))]
            await asyncio.wait(tasks, return_when=asyncio.FIRST_COMPLETED)
        except Exception:
            self.event('denied_or_closed', serial)
            if upstream is None:
                with contextlib.suppress(Exception):
                    await self.write(writer, b'HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n')
        finally:
            for task in tasks: task.cancel()
            if tasks: await asyncio.gather(*tasks, return_exceptions=True)
            for stream in (writer, upstream):
                if stream:
                    stream.close()
                    with contextlib.suppress(Exception): await asyncio.wait_for(stream.wait_closed(), 1)
            if counted: self.active -= 1

    async def run(self, duration):
        if os.geteuid() == 0 or self.config['connector_uid'] == os.geteuid():
            raise Denied('separate unprivileged identities required')
        path = Path(self.config['frontend']);parent = path.parent;s = parent.lstat()
        if (parent.resolve() != parent or not stat.S_ISDIR(s.st_mode) or s.st_uid != os.geteuid() or
                s.st_gid != self.config['frontend_gid'] or stat.S_IMODE(s.st_mode) != 0o710 or os.path.lexists(path)):
            raise Denied('unsafe socket path')
        os.umask(0o077)
        server = await asyncio.start_unix_server(self.handle, str(path), limit=32768, backlog=8)
        os.chown(path, -1, self.config['frontend_gid']);os.chmod(path, 0o660)
        owned = path.lstat();done = asyncio.Event();loop = asyncio.get_running_loop()
        for sig in (signal.SIGINT, signal.SIGTERM): loop.add_signal_handler(sig, done.set)
        loop.call_later(duration, done.set)
        self.event('listening_unix', 0)
        try:
            async with server: await done.wait()
        finally:
            server.close();await server.wait_closed()
            if path.exists():
                current = path.lstat()
                if stat.S_ISSOCK(current.st_mode) and (current.st_dev, current.st_ino) == (owned.st_dev, owned.st_ino): path.unlink()


if __name__ == '__main__':
    parser = argparse.ArgumentParser();parser.add_argument('--config', required=True);parser.add_argument('--run-for-seconds', type=int, required=True)
    args = parser.parse_args()
    try:
        if not 1 <= args.run_for_seconds <= 3600: raise Denied('duration')
        asyncio.run(Gateway(root_document(args.config)).run(args.run_for_seconds))
    except Exception:
        print('{"event":"startup_failed"}', flush=True)
        raise SystemExit(1) from None
