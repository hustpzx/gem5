from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject

class MemController(ClockedObject):
    type = 'MemController'
    cxx_header = "hum/pure/memcontroller.hh"

    cpu_side = VectorSlavePort("CPU side port, receives requests")
    mem_side = MasterPort("memorie side ports, send requests")
