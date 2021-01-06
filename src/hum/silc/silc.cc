/**
 * SILC implementation (v0)
 */

#include "debug/SILC.hh"
#include "hum/silc//silc.hh"

SILC::SILC(SILCParams *params) :
    ClockedObject(params),
    system(nullptr), blocked(false), waitingPortId(-1), agingCntr(0),
    nearMem(params->nearMem),
    farMem(params->farMem)
{
    /// Since the CPU and memory side ports are a vector of ports, create an
    /// instance of the CPUSidePort for each connection. This member
    /// of params is automatically created depending on the name of the
    /// vector port and  holds the number of the connections to this port name
    for (int i = 0; i < params->port_cpu_side_connection_count; ++i) {
        cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
    }

    for (int i=0; i < params->port_mem_side_connection_count; ++i) {
        memPorts.emplace_back(name() + csprintf(".mem_side[%d]", i), i, this);
    }

    long entryNum = nearMem.size() / PAGE_SIZE;

    // Initializing rmpTable
    rmpEntry *iter = new rmpEntry;
    iter->lock = false;
    iter->bvt_index = 0;
    iter->remap = 0;
    iter->lruinfo = 0;
    iter->NM_counter = iter->FM_counter = 0;
    iter->bitvector = 0;
    for (auto i=0; i<entryNum; i++){
        rmpTable.push_back(*iter);
    }
}

Port&
SILC::getPort(const std::string &if_name, PortID idx)
{
    /// This is the name from the Python SimObject declaration in
    /// SILC.py
    if (if_name == "mem_side" && idx < memPorts.size()) {
        /// should have already created all of the ports in the constructor
        return memPorts[idx];
    } else if (if_name == "cpu_side" && idx < cpuPorts.size()) {
        return cpuPorts[idx];
    } else {
        /// pass it along to our super class
        return ClockedObject::getPort(if_name, idx);
    }
}

void
SILC::CPUSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    //DPRINTF(SILC, "Sending %s to CPU\n", pkt->print());
    if (!sendTimingResp(pkt)) {
        DPRINTF(SILC, "failed!\n");
        blockedPacket = pkt;
    }
}

AddrRangeList
SILC::CPUSidePort::getAddrRanges() const
{
    return owner->getAddrRanges();
}

void
SILC::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        //Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(SILC, "Sending retry req.\n");
        sendRetryReq();
    }
}

void
SILC::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    //DPRINTF(SILC, "Got functional request %s\n", pkt->print());
    return owner->handleFunctional(pkt);
}

bool
SILC::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(SILC, "Got request %s\n", pkt->print());

    if (blockedPacket || needRetry) {
        DPRINTF(SILC, "Request blocked\n");
        needRetry = true;
        return false;
    }

    if (!owner->handleRequest(pkt, cpu_port_id)) {
        DPRINTF(SILC, "Request failed\n");
        //stalling
        needRetry = true;
        return false;
    } else {
        //DPRINTF(SILC, "Request succeeded\n");
        return true;
    }
}

void
SILC::CPUSidePort::recvRespRetry()
{
    // should have a blocked packet if this function is called
    assert(blockedPacket != nullptr);

    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    DPRINTF(SILC, " Retrying response pkt %s\n", pkt->print());
    // Try to resend it. It's possible that it fails again
    sendPacket(pkt);

    // We may now be able to accept new packets
    trySendRetry();
}

void
SILC::MemSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr,
            "Should never try to send if blocked\n");

    // If we can't send it , store it for later
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

int
SILC::MemSidePort::getPortID()
{
    return mem_port_id;
}

bool
SILC::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    return owner->handleResponse(pkt);
}

void
SILC::MemSidePort::recvReqRetry()
{
    // We should have a blocked packet if this function is called
    assert(blockedPacket != nullptr);

    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again
    sendPacket(pkt);
}

void
SILC::MemSidePort::recvRangeChange()
{
    return owner->sendRangeChange();
}

bool
SILC::handleRequest(PacketPtr pkt, int port_id)
{
    if (blocked) {
        // There is currently an outstanding request so we can't respond. Stall
        return false;
    }

    //DPRINTF(SILC, "Got request for addr %#x\n", pkt->getAddr());

    // We should block this object waiting for the response
    blocked = true;

    // Store the port for when we get the response
    assert(waitingPortId == -1);
    waitingPortId = port_id;

    // aging counter handler
    if (agingCntr < 1000000){
        agingCntr++;
    }else{
        DPRINTF(SILC, "Aging counter reset!\n");
        agingCntr = 0;
        // TODO: need to stats travaling cost
        for (auto iter=rmpTable.begin(); iter!=rmpTable.end(); iter++){
            iter->NM_counter = iter->NM_counter >> 1;
            iter->FM_counter = iter->FM_counter >> 1;
            // if cntr lower than threshold, unlock page
            if (iter->lock){
                iter->lock = false;
            }
        }
        agingResetNum++;
    }

    Addr addr = pkt->getAddr();
    Addr page_addr = pkt->getBlockAddr(PAGE_SIZE);

    int subblock_num = pkt->getOffset(PAGE_SIZE) / 64 ;
    int page_num = addr / PAGE_SIZE;
    int page_index = page_num % (nearMem.size() / PAGE_SIZE);
    rmpEntry *Entry = nullptr;

    if (nearMem.contains(addr)){
        Entry = &rmpTable[page_index];
        if (Entry->remap == 0){
            DPRINTF(SILC, "CASE1: %d-page\n",page_index);
            memPorts[0].sendPacket(pkt);
            bool over = counterInc(Entry->NM_counter);
            if (over && !Entry->lock){
                DPRINTF(SILC, "CASE1-lock: %d-page\n",page_num);
                Entry->lock = true;
            }
        } else if (farMem.contains(Entry->remap)){
            if (Entry->lock){
                DPRINTF(SILC, "CASE2, %d-page -map- %d-page\n",
                                page_num, Entry->remap/PAGE_SIZE);
                // locking, can't swap, forward req to FM
                Addr FMaddr = Entry->remap + pkt->getOffset(PAGE_SIZE);
                DPRINTF(SILC, "FMaddr: %#x\n", FMaddr);
                pkt->setAddr(FMaddr);
                memPorts[1].sendPacket(pkt);
                // should we increase NM-cntr here? not sure
                counterInc(Entry->NM_counter);
            } else {
                DPRINTF(SILC, "CASE3, %d-page -map- %d-page\n",
                                page_num, Entry->remap/PAGE_SIZE);
                bool valid = bits(Entry->bitvector, subblock_num);
                if (valid){
                    DPRINTF(SILC, "CASE4 %d-page -map- %d-page\n",
                                page_num, Entry->remap/PAGE_SIZE);
                    // need to swap FM subblock back to NM
                    Addr NM_subblk = page_addr + subblock_num * 64;
                    Addr FM_subblk = Entry->remap + subblock_num * 64;
                    DPRINTF(SILC, "NM-block:%#x - FM-block:%#x\n",
                                NM_subblk,FM_subblk);
                    // TODO: shoudl stats extra req
                    swapSubblk(NM_subblk, FM_subblk);
                    swapNum++;
                    replaceBits(Entry->bitvector, subblock_num, 0);
                }
                memPorts[0].sendPacket(pkt);

                bool over = counterInc(Entry->NM_counter);
                if (over && !Entry->lock){
                    DPRINTF(SILC, "CASE3-4-LOCK: %d-page\n",page_num);
                    // NM counter larger than threshold
                    int num = lockPage(Entry, page_addr, 0);
                    swapNum += num;
                    Entry->lock = true;
                    Entry->remap = 0;
                }

            }
        } else {
            panic("Error: unknown remap field!\n");
        }
        Entry->lruinfo = curTick();
    }else{
        assert(farMem.contains(addr));
        int way_index_offset = page_index % 4;
        int index = page_index - way_index_offset;
        bool match = false;
        Tick lru = MAX_TICK; // MaxTick
        int lru_index = -1;
/*
        Entry = &rmpTable[page_index];
        if (page_addr == Entry->remap){
            match = true;
            Addr NM_page = page_index * PAGE_SIZE;
            Addr NM_addr = NM_page + pkt->getOffset(PAGE_SIZE);
            bool valid = bits(Entry->bitvector, subblock_num);
            if (!valid){
                Addr NM_subblk = NM_page + subblock_num * 64;
                Addr FM_subblk = page_addr + subblock_num * 64;
                swapSubblk(NM_subblk, FM_subblk);
                replaceBits(Entry->bitvector, subblock_num, 1);
            }
            pkt->setAddr(NM_addr);
            memPorts[0].sendPacket(pkt);
        }
*/

        // 4-way associativity
        // TODO: should stats querying cost here
        for (int way=0; way<4; way++, index++){
            Entry = &rmpTable[index];
            if (page_addr == Entry->remap){
                match = true;
                Addr NM_page = index * PAGE_SIZE + nearMem.start();
                Addr NM_addr = NM_page + pkt->getOffset(PAGE_SIZE);
                DPRINTF(SILC, "NM_page:%#x, NM_addr:%#x\n", NM_page, NM_addr);
                if (Entry->lock){
                    DPRINTF(SILC, "CASE5: %d-page -map- %d-page\n",
                            page_num, index);
                    pkt->setAddr(NM_addr);
                    memPorts[0].sendPacket(pkt);
                    counterInc(Entry->FM_counter);
                } else {
                    DPRINTF(SILC, "CASE6:%d-page -map- %d-page\n",
                            page_num, index);
                    bool valid = bits(Entry->bitvector, subblock_num);
                    if (!valid){
                        DPRINTF(SILC, "CASE7: block-%d\n", subblock_num);
                        // need to swap FM subblk to NM
                        Addr NM_subblk = NM_page + subblock_num * 64;
                        Addr FM_subblk = page_addr + subblock_num * 64;
                        DPRINTF(SILC, "NM-block:%#x - FM-block:%#x\n",
                                        NM_subblk,FM_subblk);
                        swapSubblk(NM_subblk, FM_subblk);
                        swapNum++;
                        replaceBits(Entry->bitvector, subblock_num, 1);
                    }
                    pkt->setAddr(NM_addr);
                    // TODO: NM use port 0?
                    memPorts[0].sendPacket(pkt);

                    bool over = counterInc(Entry->FM_counter);
                    if (over && !Entry->lock){
                        DPRINTF(SILC, "CASE6-7-LOCK: %d-page - %d-page\n",
                                page_num, index);
                        int num = lockPage(Entry, NM_page, 1);
                        swapNum += num;
                        Entry->lock = true;
                    }

                }
                Entry->lruinfo = curTick();
            }
            if (lru > Entry->lruinfo && Entry->lock != true){
                lru = Entry->lruinfo;
                lru_index = index;
            }
            queryNum++;
        }


        if (!match){
/*
            Addr NM_page = page_index * PAGE_SIZE + nearMem.start();
            Addr NM_addr = NM_page + pkt->getOffset(PAGE_SIZE);
            if (farMem.contains(Entry->remap)){
                DPRINTF(SILC, "restorePage\n");
                restorePage(Entry, NM_page);
            }
            Entry->remap = page_addr;
            Entry->bitvector = 0;
            Addr NM_subblk = NM_page + subblock_num * 64;
            Addr FM_subblk = page_addr + subblock_num * 64;
            swapSubblk(NM_subblk, FM_subblk);
            replaceBits(Entry->bitvector, subblock_num,1);
            pkt->setAddr(NM_addr);
            memPorts[0].sendPacket(pkt);
*/

            // no remap for this page
            if (lru == MAX_TICK){
                DPRINTF(SILC, "CASE8, %d-page\n", page_num);
                // all 4-way NM page are locked by other pages
                memPorts[1].sendPacket(pkt);
            }else{
                assert(lru_index != -1);
                DPRINTF(SILC, "CASE9, %d-page -map- %d-page\n",
                            page_num, lru_index);
                // restore the lru page in NM
                Entry = &rmpTable[lru_index];
                Addr NM_page = lru_index * PAGE_SIZE + nearMem.start();
                Addr NM_addr = NM_page + pkt->getOffset(PAGE_SIZE);
                DPRINTF(SILC, "NM_page:%#x, NM_addr:%#x\n", NM_page, NM_addr);
                if (farMem.contains(Entry->remap)){
                    DPRINTF(SILC, "CASE10\n");
                // its possible that remap=0, means its an original page of NM
                    int num = restorePage(Entry, NM_page);
                    swapNum += num;

                    // record PC+Addr & bitvector to bvTable
                    if (Entry->bvt_index != 0){
                        std::pair<Addr, uint32_t> bvt_entry{Entry->bvt_index,
                            Entry->bitvector};
                        // Limitation of bvtable: 10k entries.
                        if (bitVectorTable.size() < 10240){
                            bitVectorTable.insert(bvt_entry);
                        }

                    }

                }
                // rebuild metadata
                Addr pc = pkt->req->getPC();
                Entry->bvt_index = pc ^ page_addr;
                Entry->lock = false;
                Entry->remap = page_addr;
                Entry->lruinfo = curTick();
                Entry->NM_counter = 0;
                Entry->FM_counter = 0;
                Entry->bitvector = 0;
                // build new remap and lookup bvtable

                std::unordered_map<Addr, uint32_t>::const_iterator iter;
                iter = bitVectorTable.find(Entry->bvt_index);
                if (iter != bitVectorTable.end()){
                    DPRINTF(SILC, "CASE11\n");
                    Entry->bitvector = iter->second;
                    int num = restorePage(Entry, NM_page);
                    swapNum += num;
                }

                bool valid = bits(Entry->bitvector, subblock_num);
                if (!valid){
                    DPRINTF(SILC, "CASE12, block-%d\n", subblock_num);
                    Addr NM_subblk = NM_page + subblock_num * 64;
                    Addr FM_subblk = page_addr + subblock_num * 64;
                    DPRINTF(SILC, "NM-block:%#x - FM-block:%#x\n",
                            NM_subblk,FM_subblk);
                    swapSubblk(NM_subblk, FM_subblk);
                    swapNum++;
                    replaceBits(Entry->bitvector, subblock_num,1);
                }
                pkt->setAddr(NM_addr);
                memPorts[0].sendPacket(pkt);
                counterInc(Entry->FM_counter);


            }

        }
    }
    return true;
}

bool
SILC::handleResponse(PacketPtr pkt)
{
    assert(blocked);
    //DPRINTF(SILC, "Got response for addr %#x\n", pkt->getAddr());
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
SILC::handleFunctional(PacketPtr pkt)
{
    // for functional access, we just forward it to correct place without any
    // other feature handling.
    Addr addr = pkt->getAddr();
    Addr page_addr = pkt->getBlockAddr(PAGE_SIZE);

    int subblk_num = pkt->getOffset(PAGE_SIZE) / 64;
    int page_num = addr / PAGE_SIZE;
    int page_index = page_num % (nearMem.size() / PAGE_SIZE);
    rmpEntry *Entry = nullptr;

    if (nearMem.contains(addr)){
        Entry = &rmpTable[page_index];
        if (Entry->remap == 0){
            memPorts[0].sendFunctional(pkt);
        }else if (farMem.contains(Entry->remap)){
            Addr FMaddr = Entry->remap + pkt->getOffset(PAGE_SIZE);
            if (Entry->lock){
                pkt->setAddr(FMaddr);
                memPorts[1].sendFunctional(pkt);
            }else{
                bool valid = bits(Entry->bitvector, subblk_num);
                if (valid){
                    pkt->setAddr(FMaddr);
                    memPorts[1].sendFunctional(pkt);
                }else{
                    memPorts[0].sendFunctional(pkt);
                }
            }
        }else{
            panic("Error: unknown remap field!\n");
        }
    }else{
        assert(farMem.contains(addr));
        int index = page_index - (page_index % 4);
        bool match = false;
        for (int way=0; way<4; way++, index++){
            Entry = &rmpTable[index];
            if (page_addr == Entry->remap){
                match = true;
                Addr NM_page = index * PAGE_SIZE + nearMem.start();
                Addr NM_addr = NM_page + pkt->getOffset(PAGE_SIZE);
                if (Entry->lock){
                    pkt->setAddr(NM_addr);
                    memPorts[0].sendFunctional(pkt);
                }else{
                    bool valid = bits(Entry->bitvector, subblk_num);
                    if (valid){
                        pkt->setAddr(NM_addr);
                        memPorts[0].sendFunctional(pkt);
                    }else{
                        memPorts[1].sendFunctional(pkt);
                    }
                }
            }
        }

        if (!match){
            memPorts[1].sendFunctional(pkt);
        }
    }
}

void
SILC::swapSubblk(Addr nmaddr, Addr fmaddr)
{
    RequestPtr rd_nm_req = std::make_shared<Request>(
        nmaddr, 64, 0, 0);
    PacketPtr rd_nm_pkt = new Packet(rd_nm_req, MemCmd::ReadReq);
    rd_nm_pkt->allocate();
    memPorts[0].sendFunctional(rd_nm_pkt);
    assert(rd_nm_pkt->isResponse());
    // TODO: should stats extra request


    RequestPtr rd_fm_req = std::make_shared<Request>(
        fmaddr, 64, 0, 0);
    PacketPtr rd_fm_pkt = new Packet(rd_fm_req, MemCmd::ReadReq);
    rd_fm_pkt->allocate();
    memPorts[1].sendFunctional(rd_fm_pkt);
    assert(rd_fm_pkt->isResponse());

    RequestPtr wt_fm_req = std::make_shared<Request>(
        nmaddr, 64, 0, 0);
    PacketPtr wt_fm_pkt = new Packet(wt_fm_req, MemCmd::WriteReq);
    wt_fm_pkt->allocate();
    wt_fm_pkt->setData(rd_fm_pkt->getPtr<uint8_t>());
    memPorts[0].sendFunctional(wt_fm_pkt);
    assert(wt_fm_pkt->isResponse());

    RequestPtr wt_nm_req = std::make_shared<Request>(
        fmaddr, 64, 0,0);
    PacketPtr wt_nm_pkt = new Packet(wt_nm_req, MemCmd::WriteReq);
    wt_nm_pkt->allocate();
    wt_nm_pkt->setData(rd_nm_pkt->getPtr<uint8_t>());
    memPorts[1].sendFunctional(wt_nm_pkt);
    assert(wt_nm_pkt->isResponse());

}

int
SILC::lockPage(rmpEntry *entry, Addr nmaddr, bool flag)
{
    int ret=0;
    uint32_t bitvector = entry->bitvector;
    for (int i=0; i<32; i++){
        bool valid = bits(bitvector, i);
        Addr nm_subblk = nmaddr + i * 64;
        Addr fm_subblk = entry->remap + i * 64;
        if (flag){
            // lock FM page to NM
            if (valid == 0){
                swapSubblk(nm_subblk, fm_subblk);
                ret++;
                replaceBits(entry->bitvector, i, 1);
            }
        }else {
            // lock NM page
            if (valid){
                swapSubblk(nm_subblk, fm_subblk);
                ret++;
                replaceBits(entry->bitvector, i, 0);
            }
        }
    }
    return ret;
}

int
SILC::restorePage(rmpEntry *entry, Addr nmaddr)
{
    int ret=0;
    uint32_t bitvector = entry->bitvector;
    for (int i=0; i<32; i++){
        bool valid = bits(bitvector, i);
        Addr nm_subblk = nmaddr + i * 64;
        Addr fm_subblk = entry->remap + i * 64;
        if (valid){
            swapSubblk(nm_subblk, fm_subblk);
            ret++;
        }
    }
    return ret;
}

bool
SILC::counterInc(uint8_t &cntr)
{
    if (cntr < 63){
        cntr++;
    }
    // set threshold = 60
    if (cntr >= 60){
        return true;
    } else {
        return false;
    }
}

int
SILC::redirectReq(PacketPtr pkt)
{
    // Grab the range of packet
    Addr addr = pkt->getAddr();
    Addr size = pkt->getSize();
    int port_id = -1;

    // Find which memory this packet belongs to
    if (farMem.contains(addr)){
        //DPRINTF(SILC, "Get a FM pkt->addr= %#x\n", addr);
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

AddrRangeList
SILC::getAddrRanges() const
{
    // Just use the same ranges as whatever on the memory side.
    AddrRangeList rangeList;
    AddrRangeList tmp;
    for (auto &port : memPorts) {
        tmp = port.getAddrRanges();
        rangeList.splice(rangeList.end(), tmp);
    }
    return rangeList;
}

void
SILC::sendRangeChange() const
{
    for (auto& port : cpuPorts) {
        port.sendRangeChange();
    }
}

void
SILC::regStats()
{
    ClockedObject::regStats();

    agingResetNum.name(name() + ".agingResetNum")
        .desc("Reseting number of aging countert");
    swapNum.name(name() + ".swapNum")
        .desc("Subblock swapping number");
    queryNum.name(name() + ".queryNum")
        .desc("Number of associaty quering");

}

SILC*
SILCParams::create()
{
    return new SILC(this);
}

