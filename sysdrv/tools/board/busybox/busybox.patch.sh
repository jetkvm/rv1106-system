#!/bin/bash

if [ ! -f "busybox_patched_done" ]; then
patch -p1 < 0002-Makefile.flags-strip-non-l-arguments-returned-by-pkg.patch
patch -p1 < 0008-busybox-support-chinese-display-in-terminal.patch
patch -p1 < 0009-halt-Support-rebooting-with-arg.patch
# patch -p1 < 0010-Remove-stime-function-calls.patch
touch busybox_patched_done
else
echo "busybox: patched done. skip"
fi
