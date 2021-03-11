"""
Test MiBench workload--basicmath
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
system.mem_ranges = [AddrRange('2MB')]

# Create a simple CPU
system.cpu = TimingSimpleCPU()

# Create a memory bus, a coherent crossbar in this case
system.membus = SystemXBar()

# Create the direct-mapping cache we implemented
system.cache = TwoLevel(size = '256kB')

# Connect the I and D cache ports of th CPU to the cache.
# Since cpu_side is a vector port, each time one of these is connected, it
# will create a new instance of the CPUSidePort class.
system.cpu.icache_port = system.cache.cpu_side
system.cpu.dcache_port = system.cache.cpu_side

system.cache.mem_side = system.membus.slave

# Craete interrupt controller for the CPU. Sicnce we use ARM ISA, no need to
# connect PIO and interrupt ports(only X86 need)
system.cpu.createInterruptController()

# Create SimpleMemDelay object for simulating different delay
system.mem_delay = SimpleMemDelay(read_req = '3.191ns', write_req = '11.151ns')

# Connect SimpleMemDelay to membus
system.membus.master = system.mem_delay.slave

# Connect the system to the cache
system.system_port = system.membus.slave

# Create two SimpleMemory objects to simulate SRAM and STT-RAM
system.memories = [SimpleMemory(latency='0ns',bandwidth='30.668GB/s')]

# Connect SimpleMemory to SimpleMemDelay
system.memories[0].range = system.mem_ranges[0]
system.memories[0].port = system.mem_delay.master

process = Process()
process.cmd = ['tests/mibench/blowfish/bf'] + ['e'] + ['input_small.asc'] +\
        ['output_small.enc'] + ['1234567890abcdeffedcba0987654321']
process.output = 'output_small.enc'
system.cpu.workload = process
system.cpu.createThreads()

root = Root(full_system = False, system = system)
m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()
print("Exiting @ tick {} because {}".format(
    m5.curTick(), exit_event.getCause()))






