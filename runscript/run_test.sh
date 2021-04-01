#!/bin/bash
cd ~/pzx/gem5

Gem5=~/pzx/gem5/build/ARM/gem5.opt

declare -a Configs
Configs=(
    pure
    #sram
    hybrid
)

declare -a Benches
Benches=(
    basicmath
    blowfish
    crc
    patricia
    rsynth
    stringsearch
    typeset
)

ScriptDir=~/pzx/gem5/configs/hum/mibench/
OutputDir=~/pzx/gem5/m5out/mibench/

time_begin=$(date)
echo $time_begin

for Config in ${Configs[*]}
do
    echo "Run "$Config" Mibench workload begin"
    Script=$ScriptDir$Config/test/
    Output=$OutputDir$Config/
    for Bench in ${Benches[*]}
    do
        echo "bench:"$Bench
        $Gem5 -d $Output"$Bench"/ \
        --stats-file=test1_stats.txt \
        --dump-config=test1_config.ini \
        $Script"$Bench".py
    done;
    echo "Run "$Config" Mibench finished"
done;

time_end=$(date)
echo $time_end

exit 0
