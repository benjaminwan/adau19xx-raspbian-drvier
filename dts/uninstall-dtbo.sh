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

if [ -f /boot/overlays/adau19xx-2ch.dtbo ] ; then
  echo "remove adau19xx-2ch.dtbo in /boot/overlays"
  rm /boot/overlays/adau19xx-2ch.dtbo
fi

echo "------------------------------------------------------"
echo "Please reboot your raspberry pi to apply all settings"
echo "------------------------------------------------------"