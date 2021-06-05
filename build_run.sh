#!/bin/sh

# Inupt device
DEVNODE='/dev/input/by-id/usb-Apple_Inc._Apple_Internal_Keyboard___Trackpad_D3H82120G61F-if01-event-kbd'

# Files
src_hyper="hyper.c"
out_hyper="out_hyper"
src_simul="simul.c"
out_simul="out_simul"

# Build and run
gcc $src_simul -lrt -o $out_simul && \
gcc $src_hyper -o $out_hyper && \
sudo intercept -g $DEVNODE \
    | ./"$out_simul" \
    | ./"$out_hyper" \
    | sudo nice -n -20 uinput -d $DEVNODE
