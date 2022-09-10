#!/bin/sh
# Reference: man7.org/linux/man-pages/man1/mknod.1.html

MODULE="ku_sa"
MAJOR=$(awk "\$2==\"$MODULE\" {print \$1}" /proc/devices)
mknod /dev/$MODULE c $MAJOR 0

