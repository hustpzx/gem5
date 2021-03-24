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
SE_SCRIPT=$GEM5ROOT/configs/hum/spec/myse.py
WARMUP_INSTS=10000000
# MAXINSTS=$((1*1000000000))
MAXINSTS=$((5*10000))

OUT_DIR=$GEM5ROOT/m5out/spec/hybrid

CPU_TYPE=TimingSimpleCPU # AtomicSimpleCPU, X86KvmCPU, TimingSimpleCPU, DerivO3CPU
NUM_CPUS=4
CPU_CLOCK=3GHz

MEM_TYPE=SimpleMemory # simple_mem, ddr3_1600_x64
MEM_SIZE=128MB # for whole physical address space

L1D_SIZE=32kB
L1D_ASSOC=2
L1I_SIZE=32kB
L1I_ASSOC=2

L2_SIZE=512kB
L2_ASSOC=8

L3_SIZE=$((2*NUM_CPUS))MB
L3_ASSOC=64

GEM5OPTIONS+=" --redirect-stdout"
OPTIONS+=" -I $MAXINSTS"
OPTIONS+=" --caches --l2cache" # --l3cache"
OPTIONS+=" --cpu-type=$CPU_TYPE"
OPTIONS+=" --num-cpus=$NUM_CPUS"
#OPTIONS+=" --cpu-clock=$CPU_CLOCK"
OPTIONS+=" --mem-type=$MEM_TYPE"
OPTIONS+=" --mem-size=$MEM_SIZE"
#OPTIONS+=" --nvmain-config=$NVMAIN_CONFIG"
OPTIONS+=" --l1d_size=$L1D_SIZE"
OPTIONS+=" --l1d_assoc=$L1D_ASSOC"
OPTIONS+=" --l1i_size=$L1I_SIZE"
OPTIONS+=" --l1i_assoc=$L1I_ASSOC"
OPTIONS+=" --l2_size=$L2_SIZE"
OPTIONS+=" --l2_assoc=$L2_ASSOC"
#OPTIONS+=" --l3_size=$L3_SIZE"
#OPTIONS+=" --l3_assoc=$L3_ASSOC"
#OPTIONS+=" --warmup-insts=$MAXINSTS"
cd $GEM5ROOT
for benchmark in ${BENCHMARKS[@]}; do
    time_begin=$(date)
    echo $time_begin
    $GEM5 -d $OUT_DIR  $SE_SCRIPT $OPTIONS -b $benchmark 
    time_end=$(date)
    echo $time_end
    #./run-thnvm-se.sh -b $benchmark $* >>$OUT_FILE.log 2>>$OUT_FILE.err
done
