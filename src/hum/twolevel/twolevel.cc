#include "hum/twolevel/twolevel.hh"

#include "debug/TwoLevel.hh"

TwoLevel::TwoLevel(TwoLevelParams *params) :
    ClockedObject(params),
    readLatency(2202), writeLatency(1313), insertLatency(3000),
    blockSize(params->system->cacheLineSize()),
    capacity(params->size / blockSize),
    memPort(params->name + ".mem_side", this),
    blocked(false), originalPacket(nullptr), waitingPortId(-1)
{
    // Since the CPU side ports are a vector of ports, create an instance of
    // the CPUSidePort for each connection. This member of params is
    // automatically created depending on the name of the vector port and
    // holds the number of connections to this port name
    for (int i = 0; i < params->port_cpu_side_connection_count; ++i) {
        cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
    }

    Tag *iter = new Tag;
    iter->dirty = false;
    iter->position = -1;
    for (auto i=0;i<capacity; i++){
        tagList.push_back(*iter);
    }
    cacheStore.resize(capacity, nullptr);
}

Port &
TwoLevel::getPort(const std::string &if_name, PortID idx)
{
    // This is the name from the Python SimObject declaration in TwoLevel.py
    if (if_name == "mem_side") {
        panic_if(idx != InvalidPortID,
                 "Mem side of simple cache not a vector port");
        return memPort;
    } else if (if_name == "cpu_side" && idx < cpuPorts.size()) {
        // We should have already created all of the ports in the constructor
        return cpuPorts[idx];
    } else {
        // pass it along to our super class
        return ClockedObject::getPort(if_name, idx);
    }
}

void
TwoLevel::CPUSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the cache is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    //DPRINTF(TwoLevel, "Sending %s to CPU\n", pkt->print());
    if (!sendTimingResp(pkt)) {
        DPRINTF(TwoLevel, "failed!\n");
        blockedPacket = pkt;
    }
}

AddrRangeList
TwoLevel::CPUSidePort::getAddrRanges() const
{
    return owner->getAddrRanges();
}

void
TwoLevel::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        // Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(TwoLevel, "Sending retry req.\n");
        sendRetryReq();
    }
}

void
TwoLevel::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    // Just forward to the cache.
    return owner->handleFunctional(pkt);
}

bool
TwoLevel::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(TwoLevel, "Got request %s\n", pkt->print());

    if (blockedPacket || needRetry) {
        // The cache may not be able to send a reply if this is blocked
        DPRINTF(TwoLevel, "Request blocked\n");
        needRetry = true;
        return false;
    }
    // Just forward to the cache.
    if (!owner->handleRequest(pkt, id)) {
        DPRINTF(TwoLevel, "Request failed\n");
        // stalling
        needRetry = true;
        return false;
    } else {
        //DPRINTF(TwoLevel, "Request succeeded\n");
        return true;
    }
}

void
TwoLevel::CPUSidePort::recvRespRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    DPRINTF(TwoLevel, "Retrying response pkt %s\n", pkt->print());
    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);

    // We may now be able to accept new packets
    trySendRetry();
}

void
TwoLevel::MemSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the cache is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

bool
TwoLevel::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    // Just forward to the cache.
    return owner->handleResponse(pkt);
}

void
TwoLevel::MemSidePort::recvReqRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);
}

void
TwoLevel::MemSidePort::recvRangeChange()
{
    owner->sendRangeChange();
}

bool
TwoLevel::handleRequest(PacketPtr pkt, int port_id)
{
    if (blocked) {
        // There is currently an outstanding request so we can't respond. Stall
        return false;
    }

    //DPRINTF(TwoLevel, "Got request for addr %#x\n", pkt->getAddr());

    // This cache is now blocked waiting for the response to this packet.
    blocked = true;

    // Store the port for when we get the response
    assert(waitingPortId == -1);
    waitingPortId = port_id;

    Tick when_to_send = curTick();
    if (pkt->isWrite()){
        when_to_send += writeLatency;
        schedule(new EventFunctionWrapper([this, pkt]{ accessTiming(pkt);},
                        name() + ".accessEvent", true),
                when_to_send);
    }else if (pkt->isRead()){
        when_to_send += readLatency;
        schedule(new EventFunctionWrapper([this, pkt]{ accessTiming(pkt);},
                        name() + ".accessEvent", true),
                when_to_send);
    }else{
        panic("Unknown packet type!");
    }

    return true;
}

bool
TwoLevel::handleResponse(PacketPtr pkt)
{
    assert(blocked);
    //DPRINTF(TwoLevel, "Got response %s\n", pkt->print());
    Tick whentoinsert = curTick() + insertLatency;
    schedule(new EventFunctionWrapper([this, pkt]{ insert(pkt); },
                            name() + ".accessEvent", true),
        whentoinsert);


/*
    missLatency.sample(curTick() - missTime);

    // If we had to upgrade the request packet to a full cache line, now we
    // can use that packet to construct the response.
    if (originalPacket != nullptr) {
        DPRINTF(TwoLevel, "Copying data from new packet to old\n");
        // We had to upgrade a previous packet. We can functionally deal with
        // the cache access now. It better be a hit.
        bool hit M5_VAR_USED = accessFunctional(originalPacket);
        panic_if(!hit, "Should always hit after inserting");
        originalPacket->makeResponse();
        delete pkt; // We may need to delay this, I'm not sure.
        pkt = originalPacket;
        originalPacket = nullptr;
    } // else, pkt contains the data it needs

    sendResponse(pkt);
*/
    return true;
}

void TwoLevel::sendResponse(PacketPtr pkt)
{
    assert(blocked);
    //DPRINTF(TwoLevel, "Sending resp for addr %#x\n", pkt->getAddr());

    int port = waitingPortId;

    // The packet is now done. We're about to put it in the port, no need for
    // this object to continue to stall.
    // We need to free the resource before sending the packet in case the CPU
    // tries to send another request immediately (e.g., in the same callchain).
    blocked = false;
    waitingPortId = -1;

    // Simply forward to the memory port
    cpuPorts[port].sendPacket(pkt);

    // For each of the cpu ports, if it needs to send a retry, it should do it
    // now since this memory object may be unblocked now.
    for (auto& port : cpuPorts) {
        port.trySendRetry();
    }
}

void
TwoLevel::handleFunctional(PacketPtr pkt)
{
    if (accessFunctional(pkt)) {
        if (pkt->needsResponse())
            pkt->makeResponse();
    } else {
        memPort.sendFunctional(pkt);
    }
}

void
TwoLevel::accessTiming(PacketPtr pkt)
{
    bool hit = accessFunctional(pkt);

    //DPRINTF(TwoLevel, "%s for packet: %s\n", hit ? "Hit" : "Miss",
    //        pkt->print());

    if (hit) {
        // Respond to the CPU side
        hits++; // update stats
        //DDUMP(TwoLevel, pkt->getConstPtr<uint8_t>(), pkt->getSize());
        if (pkt->needsResponse()){
            pkt->makeResponse();
        }
        sendResponse(pkt);
    } else {
        misses++; // update stats
        missTime = curTick();
        // Forward to the memory side.
        // We can't directly forward the packet unless it is exactly the size
        // of the cache line, and aligned. Check for that here.
        Addr addr = pkt->getAddr();
        Addr block_addr = pkt->getBlockAddr(blockSize);
        unsigned size = pkt->getSize();
        if (addr == block_addr && size == blockSize) {
            // Aligned and block size. We can just forward.
            //DPRINTF(TwoLevel, "forwarding packet\n");
            memPort.sendPacket(pkt);
        } else {
            //DPRINTF(TwoLevel, "Upgrading packet to block size\n");
            panic_if(addr - block_addr + size > blockSize,
                     "Cannot handle accesses that span multiple cache lines");
            // Unaligned access to one cache block
            assert(pkt->needsResponse());
            MemCmd cmd;
            if (pkt->isWrite() || pkt->isRead()) {
                // Read the data from memory to write into the block.
                // We'll write the data in the cache (i.e., a writeback cache)
                cmd = MemCmd::ReadReq;
            } else {
                panic("Unknown packet type in upgrade size");
            }

            // Create a new packet that is blockSize
            PacketPtr new_pkt = new Packet(pkt->req, cmd, blockSize);
            new_pkt->allocate();

            // Should now be block aligned
            assert(new_pkt->getAddr() == new_pkt->getBlockAddr(blockSize));

            // Save the old packet
            originalPacket = pkt;

            //DPRINTF(TwoLevel, "forwarding packet\n");
            memPort.sendPacket(new_pkt);
        }
    }
}

bool
TwoLevel::accessFunctional(PacketPtr pkt)
{
    Addr addr = pkt->getAddr();
    long blknum = addr / blockSize;
    int pos = blknum / capacity;
    int index = blknum % capacity;
    Tag *tag = &tagList[index];
    uint8_t *data = cacheStore[index];

    DPRINTF(TwoLevel,"pos=%d, index=%d, %s\n",pos, index,pkt->print());

    if (data != nullptr && tag->position == pos){
        /* DPRINTF(TwoLevel, "Hit! index=%d, cache data:\n",index);
        DDUMP(TwoLevel, data, blockSize);
        Addr blkaddr = pkt->getBlockAddr(blockSize);
        RequestPtr req = std::make_shared<Request>(
            blkaddr, blockSize,0,0);
        PacketPtr new_pkt = new Packet(req, MemCmd::ReadReq, blockSize);
        new_pkt->allocate();
        memPort.sendFunctional(new_pkt);
        DDUMP(TwoLevel, new_pkt->getConstPtr<uint8_t>(), blockSize);
 */
        //memPort.sendAtomic(pkt);
         /* if (pkt->isWrite()){
            if (checkWrite(pkt)){
                pkt->writeDataToBlock(data, blockSize);
            }
            tag->dirty = true;
        } else if (pkt->isRead()){
            if (pkt->isLLSC()){
                trackLoadLocked(pkt);
            }
            assert(pkt->hasRespData());
            pkt->setDataFromBlock(data, blockSize);
        } else{
            panic("unknown packet type\n");
        } */

        if (pkt->isLLSC()){
            bool update=false;
            if (pkt->isWrite())
                update = true;
            memPort.sendAtomic(pkt);
            if (update){
                Addr blkaddr = pkt->getBlockAddr(blockSize);
                RequestPtr req = std::make_shared<Request>(
                    blkaddr, blockSize,0,0);
                PacketPtr new_pkt = new Packet(
                    req, MemCmd::ReadReq, blockSize);
                new_pkt->allocate();
                memPort.sendFunctional(new_pkt);
                new_pkt->writeDataToBlock(data, blockSize);
                tag->dirty = true;
                delete new_pkt;
            }
        }else{
            if (pkt->isWrite()){
                pkt->writeDataToBlock(data, blockSize);
                tag->dirty = true;
            } else if (pkt->isRead()){
                pkt->setDataFromBlock(data, blockSize);
            } else{
                panic("unknown pkt type\n");
            }
        }
        return true;
    }
    return false;
}

void
TwoLevel::insert(PacketPtr pkt)
{
    // The packet should be aligned.
    assert(pkt->getAddr() ==  pkt->getBlockAddr(blockSize));
    // The pkt should be a response
    assert(pkt->isResponse());

    Addr addr = pkt->getAddr();
    int blknum = addr / blockSize;
    int pos = blknum / capacity;
    int index = blknum % capacity;

    Tag *tag = &tagList[index];
    // the request data shouldn't be in the cache
    assert(!(cacheStore[index]!=nullptr && tag->position==pos));

    if (cacheStore[index] != nullptr){
        DPRINTF(TwoLevel, "writeback occured!\n");
        /*
        assert(tag->position!=pos);
        if (tag->dirty){
            // Write back the data
            Addr wb_addr= (tag->position * capacity + index) * blockSize;
            RequestPtr wb_req = std::make_shared<Request>(
                    wb_addr, blockSize, 0, 0);
            PacketPtr wb_pkt = new Packet(wb_req, MemCmd::WritebackDirty);
            wb_pkt->dataDynamic(cacheStore[index]);
            //DPRINTF(TwoLevel, "writing packet back %s\n", pkt->print());

            memPort.sendPacket(wb_pkt);
            cacheStore[index] = nullptr;
        }
        */
    }else{
        DPRINTF(TwoLevel, "Inserting %s, index=%d, pos=%d\n",
             pkt->print(), index, pos);
        //DDUMP(TwoLevel, pkt->getConstPtr<uint8_t>(), blockSize);
        uint8_t *data = new uint8_t[blockSize];
        cacheStore[index] = data;
        assert(pkt->getOffset(blockSize) == 0);
        pkt->writeDataToBlock(data, blockSize);
        tag->dirty = false;
        tag->position = pos;
        //DDUMP(TwoLevel, cacheStore[index], blockSize);
    }

    missLatency.sample(curTick() - missTime);

    if (originalPacket != nullptr){
        /* memPort.sendAtomic(originalPacket);
        delete pkt;
        pkt = originalPacket;
        originalPacket = nullptr; */

        //DPRINTF(TwoLevel, "Copying data from new packet to old!\n");
        bool hit M5_VAR_USED = accessFunctional(originalPacket);
        //panic_if(!hit, "Should always hit after inserting");
        if (!hit){
            memPort.sendAtomic(originalPacket);
        }else{
            originalPacket->makeResponse();
        }
        delete pkt;
        pkt = originalPacket;
        originalPacket = nullptr;

    }
    sendResponse(pkt);
}

AddrRangeList
TwoLevel::getAddrRanges() const
{
    //DPRINTF(TwoLevel, "Sending new ranges\n");
    // Just use the same ranges as whatever is on the memory side.
    return memPort.getAddrRanges();
}

void
TwoLevel::sendRangeChange() const
{
    for (auto& port : cpuPorts) {
        port.sendRangeChange();
    }
}

void
TwoLevel::regStats()
{
    // If you don't do this you get errors about uninitialized stats.
    ClockedObject::regStats();

    hits.name(name() + ".hits")
        .desc("Number of hits")
        ;

    misses.name(name() + ".misses")
        .desc("Number of misses")
        ;

    missLatency.name(name() + ".missLatency")
        .desc("Ticks for misses to the cache")
        .init(16) // number of buckets
        ;

    hitRatio.name(name() + ".hitRatio")
        .desc("The ratio of hits to the total accesses to the cache")
        ;

    hitRatio = hits / (hits + misses);

}


TwoLevel*
TwoLevelParams::create()
{
    return new TwoLevel(this);
}
