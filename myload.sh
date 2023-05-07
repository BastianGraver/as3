#!/bin/bash

make
# create directory
mkdir /home/tomik21/510/assignement3/filesystem/ 

# set directory permissions
chmod 755 /home/tomik21/510/assignement3/filesystem/ 

# mount LFS to directory
./lfs -d -f /home/tomik21/510/assignement3/filesystem/ data
