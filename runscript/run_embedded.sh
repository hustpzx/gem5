#!/bin/bash

cd ~/pzx/gem5

GEM5=~/pzx/gem5/build/ARM/gem5.opt

# The $1 represent workload used
if [ "$1" = "sh" ];
then
    SCRIPT=~/pzx/gem5/configs/hum/embedded/run_sh.py
    OUT_DIR=~/pzx/gem5/m5out/embedded/smart-home
elif [ "$1" = "env" ];
then
    SCRIPT=~/pzx/gem5/configs/hum/embedded/run_env.py
    OUT_DIR=~/pzx/gem5/m5out/embedded/envmnt
elif [ "$1" = "thm" ];
then
    SCRIPT=~/pzx/gem5/configs/hum/embedded/run_thmdtr.py
    OUT_DIR=~/pzx/gem5/m5out/embedded/thermodetector
else
    echo 'ERROR: workload undefine!'
    exit
fi

#OPTIONS+=" --debug-flags=MemStatus"
#OPTIONS+=" --debug-file=MemStatus.txt"

#OPTIONS+=" --debug-flags=STATS"
#OPTIONS+=" --debug-file=STATS.txt"

#OPTIONS+=" --debug-flags=ChipMnt"
#OPTIONS+=" --debug-file=ChipMnt.txt"

# the second parameter is the EC-Switch ON/OFF 1/0
if [ $2 -eq 0 ]
then
    OPTIONS+=" --stats-file=ecoff_stats.txt"
    OPTIONS+=" --dump-config=ecoff_config.ini"
elif [ $2 -eq 1 ]
then 
    OPTIONS+=" --stats-file=econ_stats.txt"
    OPTIONS+=" --dump-config=econ_config.ini"
else
    echo "ERROR: EC switch undefine"
    exit
fi



$GEM5 -d $OUT_DIR $OPTIONS $SCRIPT