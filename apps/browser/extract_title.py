"""Compile the handheld's actual drawing rules; do not maintain another layout.

Only this audited, self-contained slice is extracted. Nothing runs on ESP32.
The original display.cpp remains the source of truth, including animation.
"""
import hashlib
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1])
output = pathlib.Path(sys.argv[2])
text = source.read_text(encoding="utf-8")

def function(signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]

functions = [
    function("void drawSpriteTransparent(\n"),
    function("void drawSpriteTransparent64(\n"),
    function("void drawStartScreen()"),
    function("void drawStartAnimation()"),
]
# Fail closed if a future slice adds dependencies not handled by the host.
for name in ("drawStartScreen", "drawStartAnimation"):
    if not re.search(r"void " + name + r"\(\)", "\n".join(functions)):
        raise RuntimeError("Missing title entry point")
output.write_text("// Generated from graphics/display.cpp SHA256 " +
                  hashlib.sha256(source.read_bytes()).hexdigest() + "\n" +
                  "\n\n".join(functions) + "\n", encoding="utf-8")
