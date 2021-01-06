from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject

class SILC(ClockedObject):
    type = 'SILC'
    cxx_header = "hum/silc/silc.hh"

    cpu_side = VectorSlavePort("CPU side port, receives requests")
    mem_side = VectorMasterPort("Universal memories side ports, send requests")

    nearMem = Param.AddrRange("The size of near memory")
    farMem = Param.AddrRange("The size of far memory")

    system = Param.System(Parent.any,
            "The system this umcontroller is part of")
