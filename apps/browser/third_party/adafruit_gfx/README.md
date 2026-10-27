# Adafruit GFX 1.12.6 (native capture only)

The five upstream files in this directory were copied byte-for-byte from the
existing PlatformIO dependency `Adafruit GFX Library` version 1.12.6 used by this
repository's ESP32 build. Upstream: https://github.com/adafruit/Adafruit-GFX-Library
at tag 1.12.6. `license.txt` retains its BSD license. No upstream file was edited.
`SHA256SUMS` records the exact local source bytes (including line endings).

The native compatibility headers are separate in `../../host`. Firmware continues
using its own PlatformIO dependency; this vendored subset is never in its include
or source paths. It includes only GFX, its classic font and type declarations;
no ST7789, SPI, I2C or device-driver implementation is bundled or emulated.
