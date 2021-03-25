#!/bin/bash

cd ~/pzx/gem5

GEM5=~/pzx/gem5/build/ARM/gem5.opt
SCRIPT=~/pzx/gem5/configs/hum/embedded/run_thmdtr.py
OUT_DIR=~/pzx/gem5/m5out/embedded/thermodetector

OPTIONS+=" --debug-flags=MemStatus"
OPTIONS+=" --debug-file=MemStatus.txt"

OPTIONS+=" --debug-flags=STATS"
###OPTIONS+=" --debug-file=STATS.txt"

#OPTIONS+=" --debug-flags=ChipMnt"
#OPTIONS+=" --debug-file=ChipMnt.txt"

$GEM5 -d $OUT_DIR $OPTIONS $SCRIPT