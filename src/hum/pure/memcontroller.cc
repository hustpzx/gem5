/**
 * This is a simple memory controller which can catch and forward all
 * request with any additional operations.
 */

#include "hum/pure/memcontroller.hh"

#include "debug/MemController.hh"

MemController::MemController(MemControllerParams *params) :
    ClockedObject(params),
    memPort(params->name + ".mem_side", this),
    blocked(false),waitingPortId(-1)

{
    /// Since the CPU and memory side ports are a vector of ports, create an
    /// instance of the CPUSidePort for each connection. This member
    /// of params is automatically created depending on the name of the
    /// vector port and  holds the number of the connections to this port name
    for (int i = 0; i < params->port_cpu_side_connection_count; ++i) {
        cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
    }
}

Port&
MemController::getPort(const std::string &if_name, PortID idx)
{
    /// This is the name from the Python SimObject declaration in
    /// MemController.py
    if (if_name == "mem_side") {
        /// should have already created all of the ports in the constructor
        return memPort;
    } else if (if_name == "cpu_side" && idx < cpuPorts.size()) {
        return cpuPorts[idx];
    } else {
        /// pass it along to our super class
        return ClockedObject::getPort(if_name, idx);
    }
}

void
MemController::CPUSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    DPRINTF(MemController, "Sending %s to CPU\n", pkt->print());
    if (!sendTimingResp(pkt)) {
        DPRINTF(MemController, "failed!\n");
        blockedPacket = pkt;
    }
}

AddrRangeList
MemController::CPUSidePort::getAddrRanges() const
{
    return owner->getAddrRanges();
}

void
MemController::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        //Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(MemController, "Sending retry req.\n");
        sendRetryReq();
    }
}

void
MemController::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    return owner->handleFunctional(pkt);
}

bool
MemController::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(MemController, "Got request %s\n", pkt->print());

    if (blockedPacket || needRetry) {
        DPRINTF(MemController, "Request blocked\n");
        needRetry = true;
        return false;
    }

    if (!owner->handleRequest(pkt, cpu_port_id)) {
        DPRINTF(MemController, "Request failed\n");
        //stalling
        needRetry = true;
        return false;
    } else {
        DPRINTF(MemController, "Request succeeded\n");
        return true;
    }
}

void
MemController::CPUSidePort::recvRespRetry()
{
    // should have a blocked packet if this function is called
    assert(blockedPacket != nullptr);

    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    DPRINTF(MemController, " Retrying response pkt %s\n", pkt->print());
    // Try to resend it. It's possible that it fails again
    sendPacket(pkt);

    // We may now be able to accept new packets
    trySendRetry();
}

void
MemController::MemSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr,
            "Should never try to send if blocked\n");

    // If we can't send it , store it for later
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

bool
MemController::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    return owner->handleResponse(pkt);
}

void
MemController::MemSidePort::recvReqRetry()
{
    // We should have a blocked packet if this function is called
    assert(blockedPacket != nullptr);

    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again
    sendPacket(pkt);
}

void
MemController::MemSidePort::recvRangeChange()
{
    return owner->sendRangeChange();
}

bool
MemController::handleRequest(PacketPtr pkt, int port_id)
{
    if (blocked) {
        // There is currently an outstanding request so we can't respond. Stall
        return false;
    }

    DPRINTF(MemController, "Got request for addr %#x\n", pkt->getAddr());

    // We should block this object waiting for the response
    blocked = true;

    // Store the port for when we get the response
    assert(waitingPortId == -1);
    waitingPortId = port_id;

    memPort.sendPacket(pkt);



    return true;
}

bool
MemController::handleResponse(PacketPtr pkt)
{
    assert(blocked);
    DPRINTF(MemController, "Got response for addr %#x\n", pkt->getAddr());
    int port = waitingPortId;

    // The packet is now done. We are about to put it in the port, no need for
    // this object to continue to stall.
    // We need to free the resource before sending the packet in case the CPU
    // tries to send another request immediately
    blocked = false;
    waitingPortId = -1;

    // Forward to the cpu port
    cpuPorts[port].sendPacket(pkt);

    // For each of the cpu ports, if it needs to send a retry, it should do it
    // now since this memory may be unblocked now.
    for (auto &port : cpuPorts) {
        port.trySendRetry();
    }
    return true;
}

void
MemController::handleFunctional(PacketPtr pkt)
{
    memPort.sendFunctional(pkt);
}

/*
int
MemController::redirectReq(PacketPtr pkt)
{
    // Grab the range of packet
    Addr addr = pkt->getAddr();
    Addr size = pkt->getSize();
    int port_id = -1;

    // Find which memory this packet belongs to
    if (farMem.contains(addr)){
        //DPRINTF(MemController, "Get a FM pkt->addr= %#x\n", addr);
        if (addr + size < farMem.size()){
            port_id = 0;
        }else {
            panic("We can't handle the pkt span across two memory");
        }
    }else if (nearMem.contains(addr)){
        if (addr + size < nearMem.end()){
            port_id = 1;
        } else {
            panic("We can't handle a overflowed NM pkt");
        }
    }

    return port_id;
}
*/

AddrRangeList
MemController::getAddrRanges() const
{
    // Just use the same ranges as whatever on the memory side.
    return memPort.getAddrRanges();
}

void
MemController::sendRangeChange() const
{
    for (auto& port : cpuPorts) {
        port.sendRangeChange();
    }
}


MemController*
MemControllerParams::create()
{
    return new MemController(this);
}

