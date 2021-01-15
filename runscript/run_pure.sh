#!/bin/bash
cd ../
Gem5=~/gem5/build/ARM/gem5.opt
Script=~/gem5/configs/hum/mibench/pure/
Output=~/gem5/m5out/mibench/pure/

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

for Bench in ${Benches[*]}
do
    echo "bench:"$Bench
    $Gem5 -d $Output"$Bench"/ \
    --stats-file=small_stats.txt \
    --dump-config=small_config.ini \
    $Script"$Bench".py
done;

echo "Run pure Mibench finished"
exit 0
