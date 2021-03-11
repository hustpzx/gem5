""" This file creates a single CPU and a two-level cache system.
This script takes a single parameter which specifies a binary to execute.
If none is provided it executes 'hello' by default (mostly used for testing)

See Part 1, Chapter 3: Adding cache to the configuration script in the
learning_gem5 book for more information about this script.
This file exports options for the L1 I/D and L2 cache sizes.

IMPORTANT: If you modify this file, it's likely that the Learning gem5 book
           also needs to be updated. For now, email Jason <power.jg@gmail.com>

"""

from __future__ import print_function
from __future__ import absolute_import

# import the m5 (gem5) library created when gem5 is built
import m5
# import all of the SimObjects
from m5.objects import *

# create the system we are going to simulate
system = System()

# Set the clock fequency of the system (and all of its children)
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '1GHz'
system.clk_domain.voltage_domain = VoltageDomain()

# Set up the system
system.mem_mode = 'timing'               # Use timing accesses
system.mem_ranges = [AddrRange('2MB')] # Create an address range

# Create a simple CPU
system.cpu = TimingSimpleCPU()

# Create an L1 instruction and data cache
system.cpu.icache = Cache(size='128kB', assoc=8, tag_latency=0,
        data_latency=2, response_latency=1, mshrs=4, tgts_per_mshr=10)
system.cpu.dcache = Cache(size='128kB', assoc=8, tag_latency=2,
        data_latency=10,response_latency=1, mshrs=10, tgts_per_mshr=10)

# Connect the instruction and data caches to the CPU
system.cpu.icache_port = system.cpu.icache.cpu_side
system.cpu.dcache_port = system.cpu.dcache.cpu_side

# Create a memory bus
system.membus = SystemXBar()

system.cpu.icache.mem_side = system.membus.slave
system.cpu.dcache.mem_side = system.membus.slave

# create the interrupt controller for the CPU
system.cpu.createInterruptController()

# Connect the system up to the membus
system.system_port = system.membus.slave

system.mem_delay = SimpleMemDelay(read_req='3.191ns', write_req='11.151ns')

system.membus.master = system.mem_delay.slave

# Create a DDR3 memory controller
system.mem_ctrl = SimpleMemory(latency='0ns', bandwidth='30.668GB/s')
system.mem_ctrl.range = system.mem_ranges[0]
system.mem_ctrl.port = system.mem_delay.master

# Create a process for a simple "Hello World" application
process = Process()
process.cmd = ['tests/mibench/patricia/patricia'] + ['large.udp']
process.output = 'output_large.txt'
# Set the cpu to use the process as its workload and create thread contexts
system.cpu.workload = process
system.cpu.createThreads()

# set up the root SimObject and start the simulation
root = Root(full_system = False, system = system)
# instantiate all of the objects we've created above
m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()
print('Exiting @ tick %i because %s' % (m5.curTick(), exit_event.getCause()))
