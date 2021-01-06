from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject

class TwoLevel(ClockedObject):
    type = 'TwoLevel'
    cxx_header = "hum/twolevel/twolevel.hh"

    # Vector port example. Both the instruction and data ports connect to this
    # port which is automatically split out into two ports.
    cpu_side = VectorSlavePort("CPU side port, receives requests")
    mem_side = MasterPort("Memory side port, sends requests")

    #hit_latency = Param.Latency('2ns', "hit latency of SRAM cache")

    #wb_latency = Param.Latency('', "wirteback latency of SRAM cache")

    size = Param.MemorySize('1MB', "The size of the cache")

    system = Param.System(Parent.any, "The system this cache is part of")
