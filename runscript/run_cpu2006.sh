#!/bin/bash

declare -a BENCHMARKS
BENCHMARKS=(
    bzip2
)

# BENCHMARKS=(
# 410.bwaves
# 433.milc
# 434.zeusmp
# 436.cactusADM
# 437.leslie3d
# 444.namd
# 462.libquantum
# 481.wrf
# )

# BENCHMARKS=(
# 410.bwaves
# 429.mcf
# 434.zeusmp
# 436.cactusADM
# 450.soplex
# 456.hmmer
# 458.sjeng
# 462.libquantum
# 470.lbm
# 471.omnetpp
# 473.astar
# )


GEM5ROOT=~/pzx/gem5
GEM5=$GEM5ROOT/build/ARM/gem5.opt
SE_SCRIPT=$GEM5ROOT/configs/hum/spec/imo.py
WARMUP_INSTS=10000000
# MAXINSTS=$((1*1000000000))
MAXINSTS=$((5*10000))

OUT_DIR=$GEM5ROOT/m5out/spec/hybrid

GEM5OPTIONS+=" --redirect-stdout"
OPTIONS+=" -I $MAXINSTS"

cd $GEM5ROOT
for benchmark in ${BENCHMARKS[@]}; do
    time_begin=$(date)
    echo $time_begin
    $GEM5 -d $OUT_DIR  $SE_SCRIPT $OPTIONS -b $benchmark 
    time_end=$(date)
    echo $time_end
    #./run-thnvm-se.sh -b $benchmark $* >>$OUT_FILE.log 2>>$OUT_FILE.err
done
