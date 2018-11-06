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

uname_r=$(uname -r)

if [ -f /lib/modules/${uname_r}/kernel/sound/soc/codecs/snd-soc-adau19xx.ko ] ; then
  echo "remove snd-soc-adau19xx.ko"
  rm  /lib/modules/${uname_r}/kernel/sound/soc/codecs/snd-soc-adau19xx.ko
fi

if [ -d /var/lib/dkms/adau19xx ] ; then
  echo "remove adau19xx dkms"
  rm  -rf  /var/lib/dkms/adau19xx
fi

echo "------------------------------------------------------"
echo "Please reboot your raspberry pi to apply all settings"
echo "------------------------------------------------------"
