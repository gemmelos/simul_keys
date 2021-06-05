#!/bin/sh
sudo nice -n -20 \
    udevmon \
    -c udevmon.yaml \
    >udevmon.log \
    2>udevmon.err &
