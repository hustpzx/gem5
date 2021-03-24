from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject

class UMController(ClockedObject):
    type = 'UMController'
    cxx_header = "hum/hybrid/umcontroller.hh"

    cpu_side = VectorSlavePort("CPU side port, receives requests")
    mem_side = VectorMasterPort("Universal memories side ports, send requests")

    nearmem = Param.AddrRange("The size of near/fast memory ")
    farmem = Param.AddrRange("The size of far/slow memory")
    bkpmem = Param.AddrRange("The size of backup memory")
    system = Param.System(Parent.any,
            "The system this umcontroller is part of")
