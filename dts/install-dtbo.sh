#!/bin/bash

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root (use sudo)" 1>&2
   exit 1
fi

is_Raspberry=$(cat /proc/device-tree/model | awk  '{print $1}')
if [ "x${is_Raspberry}" != "xRaspberry" ] ; then
  echo "Sorry, this drivers only works on raspberry pi"
  exit 1
fi

echo "build dtbo file:adau19xx-2ch.dtbo"
dtc -@ -I dts -O dtb -o adau19xx-2ch.dtbo adau19xx-2ch-overlay.dts

#mclk mode
#dtc -@ -I dts -O dtb -o adau19xx-2ch-mclk.dtbo adau19xx-2ch-overlay-mclk.dts

echo "copy dtbo file to /boot/overlays"
rm -f /boot/overlays/adau19xx-2ch.dtbo
cp adau19xx-2ch.dtbo /boot/overlays

echo "------------------------------------------------------"
echo "Please reboot your raspberry pi to apply all settings"
echo "------------------------------------------------------"