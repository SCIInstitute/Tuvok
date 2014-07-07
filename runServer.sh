#!/bin/sh

cd shadowBuild/TuvokServer || exit 1
cat /tmp/authkey | xauth merge -
export DEBUG="netcreate=+all;netsrc=+all;net=+all;params=+all;bricks=+all;sync=+all;netload=+all;dataset=+all;renderer=+all;file=+all;log=+all"
DISPLAY=:0 gdb ./TuvokServer