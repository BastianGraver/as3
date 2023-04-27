#!/bin/bash
#
./unload.sh

make
# create directory
mkdir /tmp/lfs-mountpoint

# set directory permissions
chmod 755 /tmp/lfs-mountpoint/

# mount LFS to directory
./lfs /tmp/lfs-mountpoint/ -d -f

# change to mount directory
cd /tmp/lfs-mountpoint/

