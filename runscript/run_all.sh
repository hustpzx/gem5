#!/bin/bash
cd ~/pzx/gem5

Gem5=~/pzx/gem5/build/ARM/gem5.opt

declare -a Configs
Configs=(
    #pure
    #sram
    #twolevel
    #imo
    #silc
    hybrid
)

declare -a Benches
Benches=(
    basicmath
    #blowfish
    crc
    patricia
    rsynth
    stringsearch
    typeset
    rijndael
    sha
)

ScriptDir=~/pzx/gem5/configs/hum/mibench/
OutputDir=~/pzx/gem5/m5out/mibench/

for Config in ${Configs[*]}
do
    echo "Run "$Config" Mibench workload begin"
    Script=$ScriptDir$Config/
    Output=$OutputDir$Config/
    for Bench in ${Benches[*]}
    do
        echo "bench:"$Bench
        $Gem5 -d $Output"$Bench"/ \
        --stats-file=stats_64B.txt \
        --dump-config=config_64B.ini \
        $Script"$Bench".py
    done;
    echo "Run "$Config" - "$Bench" finished"
done;

exit 0
