"""Character-only archive integrity/path tests; no production mounts."""
from pathlib import Path
import hashlib
import importlib.util
import io
import os
import tarfile
import tempfile
import unittest
spec=importlib.util.spec_from_file_location('character_backup',Path(__file__).resolve().parents[2]/'apps/accounts/character_backup.py')
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
class BackupTests(unittest.TestCase):
 def setUp(self):
  self.mask=os.umask(0o077);self.work=tempfile.TemporaryDirectory();self.root=Path(self.work.name)
  self.source=self.root/'saves';self.destination=self.root/'backups';self.source.mkdir(mode=0o700);self.destination.mkdir(mode=0o700)
  self.account=self.source/'account-6161';self.account.mkdir(mode=0o700);self.file=self.account/'save_slot_1.dat';self.file.write_bytes(b'PFME character fixture');self.file.chmod(0o600)
 def tearDown(self):self.work.cleanup();os.umask(self.mask)
 def archive(self):
  b.backup(self.source,self.destination);return next(self.destination.glob('characters-*.tar.gz'))
 def test_round_trip_and_private_permissions(self):
  archive=self.archive();self.assertEqual(b.verify(archive),{'account-6161/save_slot_1.dat':self.file.read_bytes()})
  self.assertEqual(archive.stat().st_mode&0o777,0o600)
 def test_checksum_tampering(self):
  archive=self.archive();archive.write_bytes(archive.read_bytes()+b'tampered')
  with self.assertRaises(ValueError):b.verify(archive)
 def test_unexpected_files_fail_closed(self):
  (self.source/'secret').write_text('must never be backed up')
  with self.assertRaises(ValueError):b.backup(self.source,self.destination)
 def test_symlink_rejected(self):
  self.file.unlink();self.file.symlink_to('/etc/passwd')
  with self.assertRaises((ValueError,OSError)):b.backup(self.source,self.destination)
 def test_path_traversal_archive_rejected(self):
  archive=self.destination/'malicious.tar.gz'
  with tarfile.open(archive,'w:gz') as tar:
   entry=tarfile.TarInfo('../../escape');entry.size=1;tar.addfile(entry,io.BytesIO(b'x'))
  archive.with_suffix('.gz.sha256').write_text(hashlib.sha256(archive.read_bytes()).hexdigest())
  with self.assertRaises(ValueError):b.verify(archive)
 def test_restore_refuses_nonempty_destination(self):
  target=self.root/'restore';target.mkdir();(target/'existing').write_text('preserve')
  with self.assertRaises(ValueError):b.restore(self.archive(),target)
  self.assertEqual((target/'existing').read_text(),'preserve')
if __name__=='__main__':unittest.main()
