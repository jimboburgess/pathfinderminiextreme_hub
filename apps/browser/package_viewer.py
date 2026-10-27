"""Package an offline viewer. No server, credentials, or gameplay snapshots."""
import base64
import hashlib
import json
import pathlib
import shutil
import stat
import subprocess
import sys

executable, destination, web = sys.argv[1:]
destination = pathlib.Path(destination)
destination.mkdir(parents=True, exist_ok=True)
def copy_asset(source, target):
    # Source checkouts can be read-only; generated outputs must remain rebuildable.
    if source.resolve() == target.resolve():
        raise RuntimeError("Capture output must be separate from its source assets")
    if target.exists():
        target.chmod(target.stat().st_mode | stat.S_IWUSR)
    shutil.copyfile(source, target)

paths = [destination / name for name in
         ("reference.rgb565", "title.rgb565", "animated.rgb565")]
subprocess.run([executable, *map(str, paths)], check=True)
frames = [path.read_bytes() for path in paths]
if any(len(frame) != 240 * 240 * 2 for frame in frames):
    raise RuntimeError("Invalid logical frame size")
if frames[0] != frames[1] or frames[1] == frames[2]:
    raise RuntimeError("Reference mismatch or missing animation")
# Animating the fighter must not change title, message, or other characters.
changed = 0
for i in range(240 * 240):
    if frames[1][2*i:2*i+2] != frames[2][2*i:2*i+2]:
        x, y = i % 240, i // 240
        if not (64 <= x < 128 and 95 <= y < 159):
            raise RuntimeError("Animation modified pixels outside original fighter bounds")
        changed += 1
for file in pathlib.Path(web).iterdir():
    if file.is_file():
        copy_asset(file, destination / file.name)
root = pathlib.Path(__file__).resolve().parent
copy_asset(root / "third_party/adafruit_gfx/license.txt", destination / "ADAFRUIT-LICENSE.txt")
copy_asset(root / "README.md", destination / "RENDERING-NOTES.md")
payload = [base64.b64encode(frame).decode("ascii") for frame in frames[1:]]
(destination / "frames.js").write_text(
    '"use strict";\nwindow.PFME_FRAMES = Object.freeze(' + json.dumps(payload) + ');\n',
    encoding="utf-8")
manifest = {"logical_size": [240, 240], "format": "RGB565 little endian",
            "reference": "original title calls + Adafruit GFX generic drawPixel backend",
            "differing_title_pixels": 0, "animated_pixels": changed,
            "sha256": {p.name: hashlib.sha256(b).hexdigest() for p, b in zip(paths, frames)}}
(destination / "capture.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
print(json.dumps(manifest))
