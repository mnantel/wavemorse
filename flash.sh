#!/bin/sh
# Compile and flash MorseKey to the Waveshare ESP32-S3-LCD-1.47.
# Usage: ./flash.sh [port]   (port autodetected if omitted)
set -e

FQBN="esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=cdc,UploadMode=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=default"
PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"

# USB CDC-on-boot starts the USB stack before setup(), so the device name
# must be baked in at compile time.
NAMEFLAGS='-DUSB_PRODUCT="MorseKey" -DUSB_MANUFACTURER="MorseKey"'

arduino-cli compile --fqbn "$FQBN" \
  --build-property "compiler.cpp.extra_flags=$NAMEFLAGS" morsekey

if [ -z "$PORT" ]; then
  echo "No port found. Plug the board in; if it never shows up, hold BOOT,"
  echo "tap RESET, release BOOT to force the bootloader, then re-run."
  exit 1
fi

arduino-cli upload --fqbn "$FQBN" -p "$PORT" morsekey
