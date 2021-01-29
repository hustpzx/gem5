#include "hum/hybrid/umcontroller.hh"

#include <cmath>

#include "debug/UMController.hh"

//#include "debug/TEST.hh"


#define UMC_REQ_EXTRADATA 0xaaaa

UMController::UMController(UMControllerParams *params) :
    ClockedObject(params),
    system(params->system),
    blocked(false), waitingPortId(-1),
    originalPacket(nullptr), pagePktNum(0),
    nearMem(params->nearmem), farMem(params->farmem),
    BLK_SIZE(1024), hotpos(0), tag(false)
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

    // calc the ratio of FM/NM, we use this to initialize the hotCounter.size
    ratio = farMem.size() / nearMem.size() ;

    int entryNum = nearMem.size() / BLK_SIZE;

    // TODO: where does the remappingTable actually locate in?
    // Solved: We assume the remapping table locates in a small extra SRAM
    // cache in design, but in simulation, we needn't worry about it. The host
    // will allocate temporary memory area for it(maybe?)
    remappingTable.resize(entryNum,0);

    hotCounter.resize((ratio+1),0);

}

Port&
UMController::getPort(const std::string &if_name, PortID idx)
{
    /// This is the name from the Python SimObject declaration in
    /// UMController.py
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
UMController::CPUSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    //DPRINTF(UMController, "Sending %s to CPU\n\n", pkt->print());
    if (!sendTimingResp(pkt)) {
        DPRINTF(UMController, "failed!\n");
        blockedPacket = pkt;
    }
}

AddrRangeList
UMController::CPUSidePort::getAddrRanges() const
{
    return owner->getAddrRanges();
}

void
UMController::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        //Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(UMController, "Sending retry req.\n");
        sendRetryReq();
    }
}

void
UMController::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    return owner->handleFunctional(pkt);
}

bool
UMController::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
        //DPRINTF(TEST, "%s\n", pkt->print());
        DPRINTF(UMController, "Got request %s\n", pkt->print());

    if (blockedPacket || needRetry) {
        DPRINTF(UMController, "Request blocked\n");
        needRetry = true;
        return false;
    }

    if (!owner->handleRequest(pkt, cpu_port_id)) {
        DPRINTF(UMController, "Request failed\n");
        //stalling
        needRetry = true;
        return false;
    } else {
        //DPRINTF(UMController, "Request succeeded\n");
        return true;
    }
}

void
UMController::CPUSidePort::recvRespRetry()
{
    // should have a blocked packet if this function is called
    assert(blockedPacket != nullptr);

    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    DPRINTF(UMController, " Retrying response pkt %s\n", pkt->print());
    // Try to resend it. It's possible that it fails again
    sendPacket(pkt);

    // We may now be able to accept new packets
    trySendRetry();
}

void
UMController::MemSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr,
            "Should never try to send if blocked\n");

    // If we can't send it , store it for later
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

int
UMController::MemSidePort::getPortID()
{
    return mem_port_id;
}

bool
UMController::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    return owner->handleResponse(pkt);
}

void
UMController::MemSidePort::recvReqRetry()
{
    // We should have a blocked packet if this function is called
    assert(blockedPacket != nullptr);

    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again
    sendPacket(pkt);
}

void
UMController::MemSidePort::recvRangeChange()
{
    return owner->sendRangeChange();
}

bool
UMController::handleRequest(PacketPtr pkt, int port_id)
{
    if (blocked) {
        // There is currently an outstanding request so we can't respond. Stall
        return false;
    }
    /*
    // try to get the physical addrress in host memory of NM and FM
    PhysicalMemory &physmem = system->getPhysMem();
    std::vector<BackingStoreEntry> bks = physmem.getBackingStore();
    for (const auto entry : bks){
        if (entry.pmem != nullptr){
            DPRINTF(UMController,"Memory physical address: %p\n",
            (void *)entry.pmem);
        }
    }
    DPRINTF(UMController,"FM address: %#x-%#x\n",farMem.start(),farMem.end());
    DPRINTF(UMController,"NM address:%#x-%#x\n",nearMem.start(),nearMem.end());
    */
    // We should block this object waiting for the response
    blocked = true;

    // Store the port for when we get the response
    assert(waitingPortId == -1);
    waitingPortId = port_id;

    Addr laddr = pkt->getAddr();
    Addr block_addr = pkt->getBlockAddr(BLK_SIZE);
    Addr offset = pkt->getOffset(BLK_SIZE);
    unsigned size = pkt->getSize();

    // if this req spans multi-pages.
    unsigned length = size + offset;
    if (length > BLK_SIZE) {
        // Handle the span multi-page case
        // Split the pkt to multiple singe page pkt
        int page_num = length / BLK_SIZE;
        if (length % BLK_SIZE)
            page_num++;

        // since we splite the original pkt to multiple pkts, we need to keep
        // this original pkt to handle the response for recognizing the pkts
        // which belongs to same originalpkt.
        if (pkt->needsResponse()){
            originalPacket = pkt;
            pagePktNum = page_num;
        }
        DPRINTF(UMController, "Got a %d-page request from %#x to %#x\n",
                page_num, laddr, laddr+size);
        for (int i = 0; i < page_num; i++) {
            // Create new pkt for each page
            PacketPtr new_pkt = new Packet(pkt->req, pkt->cmd);
            // Set addr and size for each page
            if (i == 0){
                // first page, maybe not left aligned
                new_pkt->setAddr(laddr);
                new_pkt->setSize(BLK_SIZE - offset);
                laddr = block_addr + BLK_SIZE;
                size = size - (BLK_SIZE - offset);
            } else if (i == page_num - 1) {
                // last page, maybe not right aligned
                new_pkt->setAddr(block_addr + BLK_SIZE * i);
                new_pkt->setSize(size);
            } else {
                new_pkt->setAddr(laddr);
                new_pkt->setSize(BLK_SIZE);
                laddr += BLK_SIZE;
                size -= BLK_SIZE;
            }
            new_pkt->allocate();
            if (new_pkt->isWrite()){
                // for wirte req, we should copy the payload to new pkt
                uint8_t *data = pkt->getPtr<uint8_t>() + (new_pkt->getAddr()-
                            pkt->getAddr());
                new_pkt->setData(data);
            }
            // single page handler
            DPRINTF(UMController, "The %dth-page: %#x - %#x\n",
             i, new_pkt->getAddr(), new_pkt->getAddr() + new_pkt->getSize());
            handlePageRequest(new_pkt);
        }
    } else {
        // Handle the single page req
       // DPRINTF(UMController, "Got a single page request from %#x to %#x\n",
        //        laddr, laddr+size-1);
        handlePageRequest(pkt);
    }

    return true;
}

void
UMController::handlePageRequest(PacketPtr pkt)
{
    Addr addr = pkt->getAddr();
    Addr block_addr = pkt->getBlockAddr(BLK_SIZE);
    Addr NM_addr, NM_block_addr;
    int page_num;
    uint64_t curpos;
    int index;
    uint64_t rmp_entry;

    // judge this addr belongs to NM or FM
    if (farMem.contains(addr)) {
        // calc the page number and lookup remappingTable
        page_num = addr / BLK_SIZE;
        index = page_num % (nearMem.size() / BLK_SIZE);
        curpos = page_num / (nearMem.size() / BLK_SIZE) + 1;

        rmp_entry = remappingTable[index];
        // TODO: should the NM_addr plus FM addr range? we should test
        // Solved: We confirmed that the NM addr is in its address range which
        // means it need to plus the nearMem.start()
        NM_block_addr = index * BLK_SIZE + nearMem.start();
        NM_addr = NM_block_addr + pkt->getOffset(BLK_SIZE);

        // Extract hotpos, tag, and counter info
        // TODO: we should use ratio to calc the length of hotpos and
        // hotCounter, now we just assume ratio = 4
        /**
         * build a parse function for the extraction process
        hotpos = bits(rmp_entry, 31, 29);
        tag = bits(rmp_entry, 28);
        hotCounter[4] = bits(rmp_entry, 19, 16);
        hotCounter[3] = bits(rmp_entry, 15, 12);
        hotCounter[2] = bits(rmp_emtry, 11, 8);
        hotCounter[1] = bits(rmp_entry, 7, 4);
        hotCounter[0] = bits(rmp_entry, 3, 0);

        if (pkt->cmd == MemCmd::WriteReq){
            DPRINTF(UMController, "Writing data: %s\n",
                    pkt->getPtr<uint8_t>());
        } */

        parseRmpEntry(rmp_entry);
        /*
        if (pkt->isWrite()){
            DPRINTF(TEST, "FM: index=%d, curpos=%d, hotpos=%d,"\
                " tag=%d,hotCntr=%d\n", index, curpos, hotpos, tag,\
                hotCounter[curpos-1]);
        }
        */

        if (curpos == hotpos){
            // this page has already built a remapping
            // we just need to forward the req to NM port
    /*        DPRINTF(UMController, "case1:remapping exists FM(%#x-%#x) "\
            ", forward FM request to NM\n",block_addr,block_addr+BLK_SIZE);
            */
            //DPRINTF(TEST, "case1\n");
            pkt->setAddr(NM_addr);

            // we assume port 1 connect to NM, port0 connects to FM
            memPorts[1].sendPacket(pkt);
            // update the hotCounter, if remapping exists, both read and write
            // will increase the hotCntr.
            hotCounterInc(rmp_entry, curpos-1);
    //        DPRINTF(UMController, "hotCntr[%d]=%d, page(%#x-%#x)\n",curpos-1,
    //            hotCounter[curpos-1], block_addr, block_addr+BLK_SIZE);
            remappingTable[index] = rmp_entry;
        } else {
            // no remapping relation between two pages
            if (hotpos==0){
                // hotpos=0, means the NM page hasn't beed used
                // if curpos hotCntr > 4 which means the curpage was
                // accessed at least 5 times, so curpos could migrate to
                // hotpage
                hotCounterInc(rmp_entry, curpos-1);
                if (hotCounter[curpos-1] < 7){
                    //DPRINTF(TEST, "case3\n");
                /*    DPRINTF(UMController, "case3: hotCntr[%d]=%d <5,"\
                    " don't remap this req:%s\n", curpos-1, \
                    hotCounter[curpos-1], pkt->print()); */
                    memPorts[0].sendPacket(pkt);
                }else{
                    // hotCntr>4, now we think this page can occupy the
                    // hotpage.
                    //DPRINTF(TEST, "case4\n");
                    DPRINTF(UMController, "case4: hotCntr[%d]=%d >=4, now"\
                    " we need a page-swapping.\n",curpos-1,
                        hotCounter[curpos-1]);

                    // 1. make a read request to read out curpage
                    RequestPtr rd_req = std::make_shared<Request>(
                        block_addr, BLK_SIZE, 0, 0);
                    // we use _extraData to handle response(should we?)
                    // rd_req->setExtraData(UMC_REQ_EXTRADATA);
                    PacketPtr rd_pkt = new Packet(rd_req, MemCmd::ReadReq,
                        BLK_SIZE);
                    rd_pkt->allocate();
                    //DPRINTF(UMController, "Read out curpage to migrate "
                    //    "all page with write-pkt: %s\n",rd_pkt->print());
                    // Scheme: we stats all UMC-issued requests independently
                    // and use functional access to complete all the request
                    // to avoid complicated events scheduling.
                    memPorts[0].sendFunctional(rd_pkt);

                    // 2. ensure we get the read result and update stats
                    assert(rd_pkt->isResponse());
                    fmReadNum++;

                    // 3. write the curpage to hotpos
                    RequestPtr wt_req = std::make_shared<Request>(
                        NM_block_addr, BLK_SIZE, 0, 0);
                    PacketPtr wt_pkt = new Packet(wt_req, MemCmd::WriteReq,
                        BLK_SIZE);
                    wt_pkt->allocate();
                    wt_pkt->setData(rd_pkt->getPtr<uint8_t>());
                    memPorts[1].sendFunctional(wt_pkt);

                    assert(wt_pkt->isResponse());
                    nmWriteNum++;

                    delete rd_pkt;
                    delete wt_pkt;

                    pkt->setAddr(NM_addr);
                    memPorts[1].sendPacket(pkt);
                    // update the hotpos to curpos
                    replaceBits(rmp_entry, 63, 64-(log(ratio)/log(2)+1),
                            curpos);
                    // update the hotCounter
                    // since hotpage is occupied first time, reset all counter
                    // to avoid page-swapping shake
                    hotCounterReset(rmp_entry, curpos-1);
                    migrationNum++;
                }

                remappingTable[index] = rmp_entry;

            } else {
                // hotpos!=0, means the NM pages has beed used
                // update and compare the hotCounter
                hotCounterInc(rmp_entry, curpos-1);
                hotCounterDec(rmp_entry, hotpos-1);
                if (hotCounter[curpos-1] <= hotCounter[hotpos-1]) {
                    // failed to get the hot page
                    // just forward the req to FM port
                    //DPRINTF(TEST, "case5\n");
                    memPorts[0].sendPacket(pkt);

                    remappingTable[index] = rmp_entry;
                } else {
                    // now curpos page is hotter, we should do the page-
                    // swapping process
                    //DPRINTF(TEST, "case6 \n");
                    // 1. create a new read request to get the curpage
                    RequestPtr rd_cur_req = std::make_shared<Request>(
                        block_addr, BLK_SIZE, 0, 0);
                    //rd_cur_req->setExtraData(UMC_REQ_EXTRADATA);
                    PacketPtr rd_cur_pkt = new Packet(rd_cur_req,
                    MemCmd::ReadReq, BLK_SIZE);
                    rd_cur_pkt->allocate();
                    memPorts[0].sendFunctional(rd_cur_pkt);

                    assert(rd_cur_pkt->isResponse());
                    fmReadNum++;

                    Addr hotposFMBlockAddr = (hotpos-1) * nearMem.size()
                                + index * BLK_SIZE + farMem.start();
                    if (tag == 0){
                        //DPRINTF(TEST, "case7\n");
                        // need to swap hotpage and curpage
                        // 1. create a new req-pkt pair to read out hotpage
                        RequestPtr rd_hot_req = std::make_shared<Request>(
                            NM_block_addr, BLK_SIZE, 0, 0);
                        //rd_hot_req->setExtraData(UMC_REQ_EXTRADATA);
                        PacketPtr rd_hot_pkt = new Packet(rd_hot_req,
                            MemCmd::ReadReq, BLK_SIZE);
                        rd_hot_pkt->allocate();
                        memPorts[1].sendFunctional(rd_hot_pkt);

                        assert(rd_hot_pkt->isResponse());
                        nmReadNum++;

                        // 2. writeback the hotpage to original position
                        RequestPtr wb_req = std::make_shared<Request>(
                                hotposFMBlockAddr, BLK_SIZE, 0 ,0);
                        //wb_req->setExtraData(UMC_REQ_EXTRADATA);
                        PacketPtr wb_pkt = new Packet(wb_req,
                                MemCmd::WriteReq, BLK_SIZE);
                        wb_pkt->allocate();
                        wb_pkt->setData(rd_hot_pkt->getPtr<uint8_t>());
                        memPorts[0].sendFunctional(wb_pkt);

                        assert(wb_pkt->isResponse());
                        fmWriteNum++;
                        delete rd_hot_pkt;
                        delete wb_pkt;

                        // 2. migrate the curpos page to NM
                        // this part is same for bellow cases, we leave it
                        // to outer if-else later
                    } else{
                        // tag=1, means NM page had a rmp and used in its
                        // logic addres, the swapping process may involve
                        // 3 pages
                        if (hotpos == ratio+1){
                            //DPRINTF(TEST, "case8\n");
                            // hotpage is the NM page in original position
                            // now we just need to swap curpage and hotpage
                            // 1. read out curpage(done this before)
                            // 2. read out hotpage from NM
                            RequestPtr rd_hot_req =
                                std::make_shared<Request>(
                                    NM_block_addr, BLK_SIZE, 0, 0);
                            //rd_hot_req->setExtraData(UMC_REQ_EXTRADATA);
                            PacketPtr rd_hot_pkt = new Packet(rd_hot_req,
                                    MemCmd::ReadReq, BLK_SIZE);
                            rd_hot_pkt->allocate();
                            memPorts[1].sendFunctional(rd_hot_pkt);

                            assert(rd_hot_pkt->isResponse());
                            nmReadNum++;

                            // 3. write the hotpage to curpage
                            RequestPtr wb_hot_req =
                                std::make_shared<Request>(
                                    block_addr, BLK_SIZE, 0, 0);
                            //wb_hot_req->setExtraData(UMC_REQ_EXTRADATA);
                            PacketPtr wb_hot_pkt = new Packet(wb_hot_req,
                                    MemCmd::WriteReq, BLK_SIZE);
                            wb_hot_pkt->allocate();
                            wb_hot_pkt->setData(
                                    rd_hot_pkt->getPtr<uint8_t>());
                            memPorts[0].sendFunctional(wb_hot_pkt);

                            assert(wb_hot_pkt->isResponse());
                            fmWriteNum++;
                            delete rd_hot_pkt;
                            delete wb_hot_pkt;

                            // 4. migrate the curpage to NM
                        } else{
                            // hotpos!=5, tag =1, the swapping process
                            // will involve 3 pages in this case
                            // 1. read out curpage(done this before)
                            // 2. migrate the hotpos-indicated FM page to
                            // curpos FM
                            // 2.1 read out hotpos-indicated FM page(tagpage)
                            //DPRINTF(TEST, "case9\n");
                            RequestPtr rd_tag_req = std::make_shared<
                                Request>(hotposFMBlockAddr, BLK_SIZE, 0, 0);
                            PacketPtr rd_tag_pkt = new Packet(rd_tag_req,
                                MemCmd::ReadReq, BLK_SIZE);
                            rd_tag_pkt->allocate();
                            memPorts[0].sendFunctional(rd_tag_pkt);

                            assert(rd_tag_pkt->isResponse());
                            fmReadNum++;
                            // 2.2 write tagpage to curpos
                            RequestPtr wt_tag_req = std::make_shared<
                                Request>(block_addr, BLK_SIZE, 0, 0);
                            PacketPtr wt_tag_pkt = new Packet(wt_tag_req,
                                MemCmd::WriteReq, BLK_SIZE);
                            wt_tag_pkt->allocate();
                            wt_tag_pkt->setData(
                                    rd_tag_pkt->getPtr<uint8_t>());
                            memPorts[0].sendFunctional(wt_tag_pkt);

                            assert(wt_tag_pkt->isResponse());
                            fmWriteNum++;

                            delete rd_tag_pkt;
                            delete wt_tag_pkt;

                            // 3. migrate the hotpage to original position
                            // 3.1 read hotpage out from NM
                            RequestPtr rd_hot_req = std::make_shared<
                                Request>(NM_block_addr, BLK_SIZE, 0, 0);
                            //rd_hot_req->setExtraData(UMC_REQ_EXTRADATA);
                            PacketPtr rd_hot_pkt = new Packet(rd_hot_req,
                                    MemCmd::ReadReq, BLK_SIZE);
                            rd_hot_pkt->allocate();
                            memPorts[1].sendFunctional(rd_hot_pkt);

                            assert(rd_hot_pkt->isResponse());
                            nmReadNum++;
                            // 3.2 write hotpage to original positon
                            RequestPtr wb_hot_req = std::make_shared<
                                Request>(hotposFMBlockAddr, BLK_SIZE, 0, 0);
                            PacketPtr wb_hot_pkt = new Packet(wb_hot_req,
                                MemCmd::WriteReq, BLK_SIZE);
                            wb_hot_pkt->allocate();
                            wb_hot_pkt->setData(
                                    rd_hot_pkt->getPtr<uint8_t>());
                            memPorts[0].sendFunctional(wb_hot_pkt);

                            assert(wb_hot_pkt->isResponse());
                            fmWriteNum++;

                            delete rd_hot_pkt;
                            delete wb_hot_pkt;
                            // 4. migrate the curpage to NM
                        } // end if hotpos==5
                    } // end if tag==0
                    // 4. now we should impl the curpage migration
                    // Try: we first finish the page-swapping, then write
                    // the pkt to new positon
                    // 4.1 write curpage to hotpos
                    RequestPtr mg_req = std::make_shared<Request>(
                        NM_block_addr, BLK_SIZE, 0, 0);
                    PacketPtr mg_pkt = new Packet(mg_req, MemCmd::WriteReq,
                        BLK_SIZE);
                    mg_pkt->allocate();
                    mg_pkt->setData(rd_cur_pkt->getPtr<uint8_t>());
                    memPorts[1].sendFunctional(mg_pkt);

                    assert(mg_pkt->isResponse());
                    nmWriteNum++;
                    delete rd_cur_pkt;
                    delete mg_pkt;

                    pkt->setAddr(NM_addr);
                    memPorts[1].sendPacket(pkt);

                    migrationNum++;
                    //update hotpos
                    replaceBits(rmp_entry, 63, 64-(log(ratio)/log(2)+1),
                        curpos);
                    hotCounterReset(rmp_entry, curpos-1);
                    remappingTable[index] = rmp_entry;
                } // end if counter compare
            } // end if hospos==0
        } // end if curpos==hotpos
        /* DPRINTF(UMController, "Req done: hotpos=%d, tag=%d, hotCntr("\
            "%d,%d,%d,%d,%d)\n",hotpos, tag, hotCounter[0],hotCounter[1],
            hotCounter[2],hotCounter[3],hotCounter[4]); */
    } else {
        // this pkt should be in NM address range
        assert(nearMem.contains(addr));

        // calc index and lookup remapping table
        index = (addr - nearMem.start()) / BLK_SIZE;
        curpos = ratio + 1;
        rmp_entry = remappingTable[index];

        NM_block_addr = pkt->getBlockAddr(BLK_SIZE);
        NM_addr = addr;

        // extract info
        parseRmpEntry(rmp_entry);
        /*
        DPRINTF(TEST, "NM: index=%d, curpos=%d, hotpos=%d,"\
                " tag=%d,hotCntr=%d\n", index, curpos, hotpos, tag,
                hotCounter[hotpos-1]);
        */

        if (hotpos==0){
            // the NM page hasn't been used, we can use it
            //DPRINTF(TEST, "case10\n");
            memPorts[1].sendPacket(pkt);
            // update hotpos, tag and counter
            replaceBits(rmp_entry, 63, 64-(log(ratio)/log(2)+1), curpos);
            replaceBits(rmp_entry, 63-(log(ratio)/log(2)+1), 1);
            // first time occupied by a req
            hotCounterReset(rmp_entry, curpos-1);

            remappingTable[index] = rmp_entry;
        } else {
            // hotpos!=0, this NM page has beed used
            if (hotpos==5){
                // this NM page used itself
                // pkt->setAddr(NM_addr);
                //DPRINTF(TEST, "case11\n");
                memPorts[1].sendPacket(pkt);
                //update counter
                hotCounterInc(rmp_entry, curpos-1);

                remappingTable[index] = rmp_entry;
            } else{
                // hotpos!=5 or 0, means this page was used by FM page for rmp
                Addr FM_addr = (hotpos-1) * nearMem.size() +
                    (NM_addr-nearMem.start());
                Addr FM_block_addr = (hotpos-1)*nearMem.size() +
                        index * BLK_SIZE;
                if (tag==0){
                    // NM logical addr hasn't been used

                    if (pkt->isRead()){
                        // ERROR: try to read nonsense data
                        panic("Error: try to read nonsense data");
                    } else if (pkt->isWrite()){
                        pkt->setAddr(FM_addr);
                        //DPRINTF(TEST, "case12\n");
                        memPorts[0].sendPacket(pkt);
                        // update tag=1, and counter
                        replaceBits(rmp_entry, 63-(log(ratio)/log(2)+1), 1);
                        hotCounterInc(rmp_entry, curpos-1);
                        hotCounterDec(rmp_entry, hotpos-1);

                        remappingTable[index] = rmp_entry;
                    } else{
                        panic("unknown pkt type in NM addr");
                    } // end if pkt->isRead()
                } else {
                    // tag=1, means logic addr was used, may need to swap
                    // update and compare counter
                    hotCounterInc(rmp_entry, curpos-1);
                    hotCounterDec(rmp_entry, hotpos-1);
                    if (hotCounter[curpos-1] <= hotCounter[hotpos-1]){
                        // can't get NM hotpage, forward req to FM page
                        //DPRINTF(TEST, "case13\n");
                        pkt->setAddr(FM_addr);
                        memPorts[0].sendPacket(pkt);
                        //update rmp counter
                        remappingTable[index] = rmp_entry;
                    } else{
                        //DPRINTF(TEST, "case14\n");
                        // now we need to swap pages
                        // 1. read out hotpos-indicated FM page(tagpage)
                        RequestPtr rd_tag_req = std::make_shared<Request>(
                            FM_block_addr, BLK_SIZE, 0, 0);
                        PacketPtr rd_tag_pkt = new Packet(rd_tag_req,
                            MemCmd::ReadReq, BLK_SIZE);
                        rd_tag_pkt->allocate();
                        memPorts[0].sendFunctional(rd_tag_pkt);

                        assert(rd_tag_pkt->isResponse());
                        fmReadNum++;
                        // 2. read hotpage out from NM
                        RequestPtr rd_hot_req = std::make_shared<Request>(
                            NM_block_addr, BLK_SIZE, 0, 0);
                        PacketPtr rd_hot_pkt = new Packet(rd_hot_req,
                            MemCmd::ReadReq, BLK_SIZE);
                        rd_hot_pkt->allocate();
                        memPorts[1].sendFunctional(rd_hot_pkt);

                        assert(rd_hot_pkt->isResponse());
                        nmReadNum++;
                        // 3. writeback the hotpage to FM addr
                        RequestPtr wb_hot_req = std::make_shared<Request>(
                                FM_block_addr, BLK_SIZE, 0, 0);
                        PacketPtr wb_hot_pkt = new Packet(wb_hot_req,
                                MemCmd::WriteReq, BLK_SIZE);
                        wb_hot_pkt->allocate();
                        wb_hot_pkt->setData(rd_hot_pkt->getPtr<uint8_t>());
                        memPorts[0].sendFunctional(wb_hot_pkt);

                        assert(wb_hot_pkt->isResponse());
                        fmWriteNum++;
                        delete rd_hot_pkt;
                        delete wb_hot_pkt;

                        // 4. migrate tag page to original positon
                        RequestPtr mg_tag_req = std::make_shared<Request>(
                            NM_block_addr, BLK_SIZE, 0, 0);
                        PacketPtr mg_tag_pkt = new Packet(mg_tag_req,
                            MemCmd::WriteReq, BLK_SIZE);
                        mg_tag_pkt->allocate();
                        mg_tag_pkt->setData(rd_tag_pkt->getPtr<uint8_t>());
                        memPorts[1].sendFunctional(mg_tag_pkt);

                        assert(mg_tag_pkt->isResponse());
                        nmWriteNum++;
                        delete rd_tag_pkt;
                        delete mg_tag_pkt;

                        // 5. write pkt to new positon
                        memPorts[1].sendPacket(pkt);

                        migrationNum++;
                        //update hotpos
                        replaceBits(rmp_entry, 63, 64-(log(ratio)/log(2)+1),
                             curpos);
                        hotCounterReset(rmp_entry, curpos-1);
                        remappingTable[index] = rmp_entry;
                    } // end if cnter compare
                } // end if tag=0
            } // end if hotpos = 5
        } // end if hotpos=0

    } // end if farMem contains addr
}

bool
UMController::handleResponse(PacketPtr pkt)
{
    assert(blocked);
    //DPRINTF(UMController, "Got response for addr %#x\n", pkt->getAddr());
    /*
    if (pkt->req->extraDataValid()){
        //assert(pkt->isRead() || pkt->isWrite());
        if (pkt->req->getExtraData() == UMC_REQ_EXTRADATA){
            DPRINTF(UMController, "This response binds to a UMC-req\n");
            if (pkt != nullptr){
                delete pkt;
            } else {
                DPRINTF(UMController, "Functional accessing has a response\n");
            }

            return true;
        }
    }
    */

    if (originalPacket == nullptr){
        // this pkt maybe a singe page req-pkt, we just need to send it to
        // upper for responsing.
        assert(originalPacket == nullptr && pagePktNum == 0);
        sendResponse(pkt);
    } else {
        // originalPacket!=nullptr
        // if pagePktNum>1, means this pkt is a splited singe page pkt from a
        // large span-multi-pages pkt, we should combine these small pkts to
        // originalpkt
        assert(pagePktNum > 0 && pkt->req == originalPacket->req);
        if (pagePktNum > 0){
            if (originalPacket->isRead()){
                // for read req, we should copy the payload to originalpkt
                DPRINTF(UMController,
                        "Copying data from pagePkt to original\n");
                uint8_t *data = originalPacket->getPtr<uint8_t>() +
                    (pkt->getAddr() - originalPacket->getAddr());
                std::memcpy(data, pkt->getPtr<uint8_t>(), pkt->getSize());
            }
            pagePktNum--;
            delete pkt;
        }
        if (pagePktNum == 0){
            // we have handled all pages, now we can send response
            originalPacket->makeResponse();
            pkt = originalPacket;
            originalPacket = nullptr;
            sendResponse(pkt);
        }
    }
    return true;
}

void
UMController::sendResponse(PacketPtr pkt)
{
    assert(blocked);
    //DPRINTF(UMController, "Sending response for addr %#x\n", pkt->getAddr());
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
}

void
UMController::handleFunctional(PacketPtr pkt)
{
    // when recieve functional access, we don't do any page swap.
    // Just forward the pkt to correct place
    //DPRINTF(UMController, "Got a functional access for addr %#x\n",
    //            pkt->getAddr());

    Addr addr = pkt->getAddr();
    Addr block_addr = pkt->getBlockAddr(BLK_SIZE);
    Addr offset = pkt->getOffset(BLK_SIZE);
    unsigned size = pkt->getSize();

    unsigned length = size + offset;
    if (length > BLK_SIZE){
        if (pkt->needsResponse()){
            originalPacket = pkt;
            pagePktNum = (length / BLK_SIZE) + ((length % BLK_SIZE) ? 1 : 0);
        }
        DPRINTF(UMController,"Functional access spans %d pages\n",pagePktNum);

        while (size > 0){
            PacketPtr new_pkt = new Packet(pkt->req, pkt->cmd);
            new_pkt->setAddr(addr);
            unsigned pkt_size;
            if (addr + size > block_addr + BLK_SIZE){
                pkt_size = block_addr + BLK_SIZE - addr;
            }else{
                pkt_size = size;
            }
            new_pkt->setSize(pkt_size);
            new_pkt->allocate();
            if (new_pkt->isWrite()){
                uint8_t *data = pkt->getPtr<uint8_t>() + (new_pkt->getAddr() -
                        pkt->getAddr());
                new_pkt->setData(data);
            }

            handlePageFunctional(new_pkt);

            addr += pkt_size;
            size -= pkt_size;
            block_addr += BLK_SIZE;

        }
    }else{
        handlePageFunctional(pkt);
    }
}

void
UMController::handlePageFunctional(PacketPtr pkt)
{
    Addr addr = pkt->getAddr();
    Addr NM_addr, NM_block_addr;
    int page_num, index;
    uint64_t curpos, rmp_entry;

    if (farMem.contains(addr)){
        page_num = addr / BLK_SIZE;
        index = page_num % (nearMem.size() / BLK_SIZE);
        curpos = page_num / (nearMem.size() / BLK_SIZE) + 1;

        rmp_entry = remappingTable[index];
        NM_block_addr = index * BLK_SIZE + nearMem.start();
        NM_addr = NM_block_addr + pkt->getOffset(BLK_SIZE);

        parseRmpEntry(rmp_entry);

        if (curpos == hotpos){
            pkt->setAddr(NM_addr);
            memPorts[1].sendFunctional(pkt);
        } else {
            //DPRINTF(UMController, "Forward funcReq to FM addr %#x \n",
            //        pkt->getAddr());
            memPorts[0].sendFunctional(pkt);
        }
    }else{
        assert(nearMem.contains(addr));

        index = (addr - nearMem.start()) / BLK_SIZE;
        curpos = 5;
        rmp_entry = remappingTable[index];

        Addr FM_addr = (hotpos - 1) * nearMem.size() +
                    (addr - nearMem.start());

        if (hotpos != curpos && hotpos != 0){
            pkt->setAddr(FM_addr);
            memPorts[0].sendFunctional(pkt);
        }else{
            //DPRINTF(UMController, "Forward funcReq to NM addr %#x \n",
            //        pkt->getAddr());
            memPorts[1].sendFunctional(pkt);
        }
    }
}

int
UMController::redirectReq(PacketPtr pkt)
{
    // Grab the range of packet
    AddrRange pktRange = pkt->getAddrRange();
    AddrRangeList rangeList;
    AddrRange range;
    bool flag = true;
    int port_id = -1;

    // Find which memory this packet belongs to
    for (auto& port : memPorts) {
        rangeList = port.getAddrRanges();
        for (const auto& range : rangeList) {
            if (pktRange.isSubset(range)) {
                /**
                DPRINTF(UMController, "Redirect packet to memory %d\n",
                        port.mem_port_id);
                flag = true;
                port.sendPacket(pkt);
                */
                flag = false;
                port_id = port.getPortID();
            }
        }
    }
    panic_if(flag, "ERROR:The packet address doesn't belong to any memory\n");
    return port_id;
}

void
UMController::hotCounterInc(uint64_t &entry, int idx)
{
    assert(idx>=0 && idx<ratio+1);
    if (hotCounter[idx] < 15){
        // a 3 bits-satuaration counter can't bigger than 7
        hotCounter[idx]++;
        replaceBits(entry, idx*4+3, idx*4, hotCounter[idx]);
    }
}

void
UMController::hotCounterDec(uint64_t &entry, int idx)
{
    assert(idx>=0 && idx < ratio+1);
    if (hotCounter[idx] > 0){
        hotCounter[idx]--;
        replaceBits(entry, idx*4+3, idx*4, hotCounter[idx]);
    }
}

void
UMController::hotCounterReset(uint64_t &entry, int idx)
{
    assert(idx>=0 && idx< ratio+1);
    for (int i=0; i<ratio+1;i++){
        if (i != idx){
            hotCounter[i] = 0;
        }else{
            hotCounter[i] = 8;
        }
        replaceBits(entry, i*4+3, i*4, hotCounter[i]);
    }
}

void
UMController::parseRmpEntry(uint64_t entry)
{
    // log2(Ratio) + 1
    int hotpos_bits = log(ratio) / log(2) + 1;
    hotpos = bits(entry, 63, 64-hotpos_bits);
    int tag_bits = 64 - hotpos_bits -1;
    tag = bits(entry, tag_bits);
    // 3-bits saturation counter
    for (int i=0; i<ratio+1; i++){
        hotCounter[i] = bits(entry, (i*4)+3, i*4);
    }
}

AddrRangeList
UMController::getAddrRanges() const
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
UMController::sendRangeChange() const
{
    for (auto& port : cpuPorts) {
        port.sendRangeChange();
    }
}

void
UMController::regStats()
{
    ClockedObject::regStats();

    migrationNum.name(name() + ".migrations")
                .desc("Number of migrations");

    fmReadNum.name(name() + ".extra_fm_reads")
              .desc("Number of extra FM read requests issued by UMC");
    fmWriteNum.name(name() + ".extra_fm_writes")
              .desc("Number of extra FM write requests issued by UMC");
    nmReadNum.name(name() + ".extra_nm_reads")
             .desc("Number of extra NM read requests issued by UMC");
    nmWriteNum.name(name() + ".extra_nm_writes")
              .desc("Number of extra NM write requests issued by UMC");
    extraTimeConsumption.name(name() + ".extraTimeConsumption")
            .desc("extra time consumption by umc controller");

    // bandwidth in ticks/byte
    // bw(tick/byte) = frequency(tick/s) / bandwidth(bytes/s)
    int fm_bandwidth = 30;
    int nm_bandwidth = 13;

    // latency in tick
    int fm_readLatency = 3511;
    int fm_writeLatency = 13026;
    int nm_readLatency = 2202;
    int nm_writeLatency = 1313;

    // extra time consumption in tick, spend by umc controller for swapping
    // pages
    extraTimeConsumption =
        fmReadNum * (BLK_SIZE * fm_bandwidth + fm_readLatency) +
        fmWriteNum * (BLK_SIZE * fm_bandwidth + fm_writeLatency) +
        nmReadNum * (BLK_SIZE * nm_bandwidth + nm_readLatency) +
        nmWriteNum * (BLK_SIZE * nm_bandwidth + nm_writeLatency);

}


UMController*
UMControllerParams::create()
{
    return new UMController(this);
}

