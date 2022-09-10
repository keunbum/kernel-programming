#!/bin/sh
# Reference: man7.org/linux/man-pages/man1/mknod.1.html

MODULE="ch3_dev"
MAJOR=$(awk "\$2==\"$MODULE\" {print \$1}" /proc/devices)

mknod /dev/ch3_dev2 c $MAJOR 1
