"""
Test the backup FM
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
system.mem_ranges = [AddrRange('800kB'), AddrRange('800kB','900kB').
                    AddrRange('900kB', '2000kB')]

# Create a simple CPU
system.cpu = TimingSimpleCPU()

# Create a umc
system.umc = UMController()

system.umc.farmem = system.mem_ranges[0]
system.umc.nearmem = system.mem_ranges[1]
system.umc.bkpmem = system.mem_ranges[2]

# Connect the I and D cache ports of th CPU to the umc.
# Since cpu_side is a vector port, each time one of these is connected, it
# will create a new instance of the CPUSidePort class.
system.cpu.icache_port = system.umc.cpu_side
system.cpu.dcache_port = system.umc.cpu_side

# Craete interrupt controller for the CPU. Sicnce we use ARM ISA, no need to
# connect PIO and interrupt ports(only X86 need)
system.cpu.createInterruptController()

# Create SimpleMemDelay object for simulating different delay
system.sram_delay = SimpleMemDelay(read_req = '2.02ns', write_req = '1.313ns')
system.sttram_delay = SimpleMemDelay(
    read_req = '3.191ns', write_req = '11.151ns')
system.bkpmem_delay = SimpleMemDelay(
    read_req = '3.191ns', write_req = '11.151ns')

# Connect SimpleMemDelay to mem_port of umc
system.umc.mem_side = system.sttram_delay.slave
system.umc.mem_side = system.sram_delay.slave
system.umc.mem_side = system.bkpmem_delay.slave

# Connect the system to the umc
system.system_port = system.umc.cpu_side

# Create two SimpleMemory objects to simulate SRAM and STT-RAM
system.memories = [SimpleMemory(latency='0ns', bandwidth='30.668GB/s'),
                SimpleMemory(latency='0ns', bandwidth='213.689GB/s'),
                SimpleMemory(latency='0ns', bandwidth='30.668GB/s',
                in_addr_map =  False)]

# Connect SimpleMemory to SimpleMemDelay
system.memories[0].range = system.mem_ranges[0]
system.memories[0].port = system.sttram_delay.master
system.memories[1].range = system.mem_ranges[1]
system.memories[1].port = system.sram_delay.master
system.memories[2].range = system.mem_ranges[2]
system.memories[2].port = system.bkpmem_delay.master


process = Process()
#process.cmd = ['tests/test-progs/hello/bin/arm/linux/hello']
#process.cmd = ['tests/test-progs/umc/case1']
process.cmd = ['tests/embedded-workload/arm-test']
system.cpu.workload = process
system.cpu.createThreads()

root = Root(full_system = False, system = system)
m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()
print("Exiting @ tick {} because {}".format(
    m5.curTick(), exit_event.getCause()))






