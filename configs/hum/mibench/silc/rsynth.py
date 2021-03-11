"""
SILC: Mibench basicmath workload testing.
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
system.mem_ranges = [AddrRange('256kB'), AddrRange('256kB','2304kB')]

# Create a simple CPU
system.cpu = TimingSimpleCPU()

# Create a silc
system.silc = SILC()

system.silc.nearMem = system.mem_ranges[0]
system.silc.farMem = system.mem_ranges[1]

# Connect the I and D cache ports of th CPU to the silc.
# Since cpu_side is a vector port, each time one of these is connected, it
# will create a new instance of the CPUSidePort class.
system.cpu.icache_port = system.silc.cpu_side
system.cpu.dcache_port = system.silc.cpu_side

# Craete interrupt controller for the CPU. Sicnce we use ARM ISA, no need to
# connect PIO and interrupt ports(only X86 need)
system.cpu.createInterruptController()

# Create SimpleMemDelay object for simulating different delay
system.sram_delay = SimpleMemDelay(read_req = '2.02ns', write_req = '1.313ns')
system.sttram_delay = SimpleMemDelay(
        read_req = '3.511ns', write_req = '13.026ns')

# Connect SimpleMemDelay to mem_port of silc
system.silc.mem_side = system.sram_delay.slave
system.silc.mem_side = system.sttram_delay.slave

# Connect the system to the silc
system.system_port = system.silc.cpu_side

# Create two SimpleMemory objects to simulate SRAM and STT-RAM
system.memories = [SimpleMemory(latency='0ns', bandwidth='74.143GB/s'),
                SimpleMemory(latency='0ns', bandwidth='30.759GB/s')]

# Connect SimpleMemory to SimpleMemDelay
system.memories[0].range = system.mem_ranges[0]
system.memories[0].port = system.sram_delay.master
system.memories[1].range = system.mem_ranges[1]
system.memories[1].port = system.sttram_delay.master


process = Process()
process.cmd = ['tests/mibench/rsynth/say'] +['-a'] +['-q'] + ['-o'] + \
    ['small_output.au'] + ['<'] + ['smallinput.txt']
process.output = 'output_small.au'
system.cpu.workload = process
system.cpu.createThreads()

root = Root(full_system = False, system = system)
m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()
print("Exiting @ tick {} because {}".format(
    m5.curTick(), exit_event.getCause()))






