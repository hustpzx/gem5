#!/bin/bash

cd ~/pzx/gem5

GEM5=~/pzx/gem5/build/ARM/gem5.opt
SCRIPT=~/pzx/gem5/configs/hum/embedded/run_sh.py
OUT_DIR=~/pzx/gem5/m5out/embedded/smart-home

OPTIONS+=" --debug-flags=STATS"
OPTIONS+=" --debug-file=debug_stats.txt"

$GEM5 -d $OUT_DIR $OPTIONS $SCRIPT