#!/usr/bin/bash

for var in "$@"; do

#  umount /pmem"$var"

#  mkfs.ext4 /dev/pmem"$var"
#  mount -o dax /dev/pmem"$var" /pmem"$var"

  mkfs.xfs -f -d su=2m,sw=1 -m reflink=0 -f /dev/pmem"$var"
  mount -o rw,noatime,nodiratime,dax /dev/pmem"$var" /mnt/pmem/
done
