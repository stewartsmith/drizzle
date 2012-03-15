#!/bin/sh
file=$1
sed -e 's/[0-9]/0/g' -e 's/=00*/=0/g' -i.bak $file
