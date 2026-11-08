"""Scoped character-only backups. No account/password database is mounted here."""
import argparse
import datetime
import hashlib
import io
import json
import os
from pathlib import Path
import re
import stat
import tarfile
import uuid

OWNER = re.compile(r"account-(?:[0-9a-f]{2}){1,64}\Z")
SLOT = re.compile(r"save_slot_[123]\.dat\Z")
ARCHIVE = re.compile(r"characters-\d{8}T\d{6}Z-[0-9a-f]{8}\.tar\.gz\Z")
MAX_FILES = 768
MAX_SIZE = 65536


def members(source):
    result = {}
    for account in sorted(source.iterdir()):
        if not OWNER.fullmatch(account.name) or account.is_symlink() or not account.is_dir():
            raise ValueError("Unexpected save directory entry")
        if stat.S_IMODE(account.stat().st_mode) != 0o700:
            raise ValueError("Insecure account directory permissions")
        for item in sorted(account.iterdir()):
            # A concurrent atomic save may leave a brief temporary file. It is
            # never included, opened or deleted by the backup process.
            if re.fullmatch(r"save_slot_[123]\.dat\.tmp\.[0-9.]+", item.name):
                continue
            if not SLOT.fullmatch(item.name):
                raise ValueError("Unexpected character file")
            fd = os.open(item, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
            with os.fdopen(fd, "rb") as stream:
                info = os.fstat(stream.fileno())
                if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or stat.S_IMODE(info.st_mode) != 0o600 or not 0 < info.st_size <= MAX_SIZE:
                    raise ValueError("Invalid character file")
                data = stream.read(MAX_SIZE + 1)
            if not 0 < len(data) <= MAX_SIZE:
                raise ValueError("Invalid character length")
            result[account.name + "/" + item.name] = data
            if len(result) > MAX_FILES:
                raise ValueError("Backup file limit exceeded")
    return result


def verify(archive):
    checksum = archive.with_suffix(archive.suffix + ".sha256").read_text().strip()
    if hashlib.sha256(archive.read_bytes()).hexdigest() != checksum:
        raise ValueError("Backup checksum mismatch")
    data = {}
    with tarfile.open(archive, "r:gz") as tar:
        entries = tar.getmembers()
        if len(entries) > MAX_FILES + 1:
            raise ValueError("Archive file limit exceeded")
        for entry in entries:
            if not entry.isfile() or entry.size > MAX_SIZE * 4 or entry.name in data:
                raise ValueError("Invalid archive entry")
            if entry.name != "manifest.json":
                parts = entry.name.split("/")
                if len(parts) != 2 or not OWNER.fullmatch(parts[0]) or not SLOT.fullmatch(parts[1]) or not 0 < entry.size <= MAX_SIZE:
                    raise ValueError("Invalid archive path")
            with tar.extractfile(entry) as stream:
                data[entry.name] = stream.read()
    manifest = json.loads(data.pop("manifest.json"))
    if manifest != {name: hashlib.sha256(content).hexdigest() for name, content in data.items()}:
        raise ValueError("Character hash mismatch")
    return data


def backup(source, destination):
    if source.is_symlink() or destination.is_symlink() or not destination.is_dir():
        raise ValueError("Invalid backup roots")
    if stat.S_IMODE(destination.stat().st_mode) != 0o700:
        raise ValueError("Backup directory must be private")
    data = members(source)
    manifest = json.dumps({name: hashlib.sha256(content).hexdigest() for name, content in data.items()}, sort_keys=True).encode()
    name = "characters-" + datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ-") + uuid.uuid4().hex[:8] + ".tar.gz"
    target = destination / name
    temporary = destination / (name + ".tmp")
    with temporary.open("xb") as output:
        with tarfile.open(fileobj=output, mode="w:gz") as tar:
            for path, content in [("manifest.json", manifest), *data.items()]:
                info = tarfile.TarInfo(path)
                info.size = len(content)
                info.mode = 0o600
                tar.addfile(info, io.BytesIO(content))
        output.flush()
        os.fsync(output.fileno())
    temporary.rename(target)
    checksum = hashlib.sha256(target.read_bytes()).hexdigest()
    with target.with_suffix(target.suffix + ".sha256").open("x") as output:
        output.write(checksum + "\n"); output.flush(); os.fsync(output.fileno())
    verify(target)
    # Prune only this helper's completed archives; retain the latest 336
    # (14 days at the documented hourly interval). Never touch unrelated files.
    archives = sorted(p for p in destination.iterdir() if ARCHIVE.fullmatch(p.name) and not p.is_symlink())
    for old in archives[:-336]:
        old.unlink()
        old.with_suffix(old.suffix + ".sha256").unlink(missing_ok=True)
    fd = os.open(destination, os.O_RDONLY | os.O_DIRECTORY)
    try: os.fsync(fd)
    finally: os.close(fd)
    print(json.dumps({"backup": name, "characters": len(data), "bytes": target.stat().st_size, "sha256": checksum, "verified": True}))


def restore(archive, destination):
    # Administrative, stopped-game recovery only; never overwrite live saves.
    if destination.is_symlink() or not destination.is_dir() or any(destination.iterdir()):
        raise ValueError("Restore requires an empty dedicated directory")
    data = verify(archive)
    for name, content in data.items():
        account, filename = name.split("/")
        folder = destination / account
        folder.mkdir(mode=0o700, exist_ok=True)
        os.chown(folder, 999, 980)
        fd = os.open(folder / filename, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        with os.fdopen(fd, "wb") as stream:
            stream.write(content);stream.flush();os.fsync(stream.fileno())
        os.chown(folder / filename, 999, 980)
    os.chmod(destination, 0o700);os.chown(destination, 999, 980)
    print(json.dumps({"restored_characters": len(data), "verified": True}))


def main():
    os.umask(0o077)
    p = argparse.ArgumentParser()
    p.add_argument("operation", choices=["backup", "verify", "restore"])
    p.add_argument("source", type=Path)
    p.add_argument("destination", nargs="?", type=Path)
    a = p.parse_args()
    if a.operation == "backup": backup(a.source, a.destination)
    elif a.operation == "restore": restore(a.source, a.destination)
    else: print(json.dumps({"verified_characters": len(verify(a.source))}))

if __name__ == "__main__": main()
