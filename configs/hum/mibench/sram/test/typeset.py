"""
pure memory controller test file.
"""

# import the m5(gem5) library created when gem5 is built
import m5
# import all of the SimObjects
from m5.objects import *

system = System()

# Set the clock frequency of the system
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "1GHz"
system.clk_domain.voltage_domain = VoltageDomain()

# Set up the system
system.mem_mode = 'timing'
# create two ranges to represent different memories
system.mem_ranges = [AddrRange('20MB')]

# Create a simple CPU
system.cpu = TimingSimpleCPU()

# Create a ctrl
system.ctrl = MemController()

# Connect the I and D cache ports of th CPU to the ctrl.
# Since cpu_side is a vector port, each time one of these is connected, it
# will create a new instance of the CPUSidePort class.
system.cpu.icache_port = system.ctrl.cpu_side
system.cpu.dcache_port = system.ctrl.cpu_side

# Craete interrupt controller for the CPU. Sicnce we use ARM ISA, no need to
# connect PIO and interrupt ports(only X86 need)
system.cpu.createInterruptController()

# Create SimpleMemDelay object for simulating different delay
system.mem_delay = SimpleMemDelay(read_req = '2.202ns', write_req = '1.313ns')

# Connect SimpleMemDelay to mem_port of ctrl
system.ctrl.mem_side = system.mem_delay.slave

# Connect the system to the ctrl
system.system_port = system.ctrl.cpu_side

# Create two SimpleMemory objects to simulate SRAM and STT-RAM
system.memories = [SimpleMemory(latency='0ns')]

# Connect SimpleMemory to SimpleMemDelay
system.memories[0].range = system.mem_ranges[0]
system.memories[0].port = system.mem_delay.master

process = Process()
process.cmd = ['tests/mibench/typeset/lout-3.24/lout'] + \
    ['-I'] + ['tests/mibench/typeset/lout-3.24/include'] + \
    ['-D'] + ['tests/mibench/typeset/lout-3.24/data'] + \
    ['-F'] + ['tests/mibench/typeset/lout-3.24/font'] + \
    ['-C'] + ['tests/mibench/typeset/lout-3.24/maps'] + \
    ['-H'] + ['tests/mibench/typeset/lout-3.24/hyph'] + \
    ['tests/mibench/typeset/small.lout']
process.output = 'output_small.ps'
system.cpu.workload = process
system.cpu.createThreads()

root = Root(full_system = False, system = system)
m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()
print("Exiting @ tick {} because {}".format(
    m5.curTick(), exit_event.getCause()))






