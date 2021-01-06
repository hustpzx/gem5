#ifndef __HUM_HYBRID_UMCONTROLLER_HH__
#define __HUM_HYBRID_UMCONTROLLER_HH__

#include <cmath>
#include <unordered_map>

#include "base/bitfield.hh"
#include "mem/physical.hh"
#include "mem/port.hh"
#include "params/UMController.hh"
#include "sim/clocked_object.hh"
#include "sim/system.hh"

/**
 * A simple controller for hybrid universal memories(HUM)
 * Manage hybrid memories consists of SRAM and STT-RAM for SoC IoT devices
 * Offering uniform addressing, using a hardware-supported remapping table to
 * manage page swapping and migration.
 */
class UMController : public ClockedObject
{
  private:

    /**
     * Port on CPU side that receives requests.
     */
    class CPUSidePort : public SlavePort
    {
      private:
        /// Since there are two port in cpu side, need to set port id.
        /// inst_port id=0, data_port id=1
        int cpu_port_id;

        UMController *owner;

        bool needRetry;

        PacketPtr blockedPacket;

      public:
        /**
         * Constructor. Just calls the superclass constructor.
         */
        CPUSidePort(const std::string& name, int id, UMController *owner):
          SlavePort(name, owner), cpu_port_id(id),
          owner(owner), needRetry(false),
          blockedPacket(nullptr)
        { }

        /**
         * Send a packet across this port, called by owner.
         *
         * @param packet to send
         */
        void sendPacket(PacketPtr pkt);

        /**
         * Get a list of the non-overlapping address ranges the owner is
         * responsible for, must be overrided.
         *
         * @return a list of ranges responded to
         */
        AddrRangeList getAddrRanges() const override;

        void trySendRetry();

      protected:
        /**
         * Receive an atomic request packet from the master port
         * No need to implement in this umcontroller
         */
        Tick recvAtomic(PacketPtr pkt) override
        { panic("recvAtomic unimpl."); }

        /**
         * Receive a functional request packet from the master port
         */
        void recvFunctional(PacketPtr pkt) override;

        /**
         * Receive a timing request from the master port
         *
         * @param the packet that the requestor sent
         * @return whether this object can consume to packet. If false, we
         *         will call sendRetry() when we can try to receive this
         *         request again.
         */
        bool recvTimingReq(PacketPtr pkt) override;

        /**
         * Called by the master port if sendTimingResp was called on this
         * slave port and was unsuccesful.
         */
        void recvRespRetry() override;
    };

    /**
     * Port on memory-side that receives responses.
     */
    class MemSidePort : public MasterPort
    {
      private:
        /// Since there may be multiple memories, need to set id.
        int mem_port_id;

        UMController *owner;

        PacketPtr blockedPacket;

      public:
        MemSidePort(const std::string& name,int id, UMController *owner) :
          MasterPort(name, owner), mem_port_id(id), owner(owner),
          blockedPacket(nullptr)
        { }

        void sendPacket(PacketPtr pkt);

        int getPortID();

      protected:
        bool recvTimingResp(PacketPtr pkt) override;

        void recvReqRetry() override;

        void recvRangeChange() override;
    };


    /**
     * Handle the requests from the CPU side. Called from the CPU port on a
     * timing request.
     * In this function, we mainly handle the multi-pages case. We split the
     * large pkt to small page-alligned pkts and record page numbers.
     *
     * @param port_id id of the port to send the response
     */
    bool handleRequest(PacketPtr pkt, int port_id);

    /**
     * Handle the single page request from handleRequest()
     */
    void handlePageRequest(PacketPtr pkt);

    /**
     * Handle the response from the memory side. Called from the memory port
     * on a timing response.
     * Here, we need to combine the splited-page-pkts to original pkt.
     *
     */
    bool handleResponse(PacketPtr pkt);

    /**
     * Send response to the CPU side.
     * This function assumes the pkt is already a response packet and forwards
     * it to the correct port. This function alse unblocks this object and
     * clean up the whole request.
     *
     */
    void sendResponse(PacketPtr pkt);

    /**
     * Handle a packet functionally.
     * Mainly split multi-page pkt to small single-page pkts.
     * Called from CPU port on a send functional.
     */
    void handleFunctional(PacketPtr pkt);

    /**
     * Handle a single-page pkt functionally which means we just forward the
     * pkt to correct position(by lookup remapping table) but don't do any
     * page swapping or migration.
     */
    void handlePageFunctional(PacketPtr pkt);

    /**
     * Return the address ranges this memobj is responsible for. Just use the
     * same as the next upper level of the hierarchy.
     */
    AddrRangeList getAddrRanges() const;

    /**
     * Tell the CPU side to ask for our memory ranges.
     */
    void sendRangeChange() const;

    /**
     * Caculating the utilization level of SRAM.
     * Execute adaptive migration policy according to the utilizaiton.
     *
     * lv1: ut<0.6, migrate all writes to SRAM
     * lv2: 0.6<= ut <0.8, migrate write-burst blocks only
     * lv3: ut>0.8, replace dead-blocks, prevent migration.
     *
     * @return current utilization level.
     */
    //int utilizationLevel();

    /**
     * Redirect the request/packet to relative memory port according to the
     * vaddr(since we have two memories).
     *
     * @return the mem_port_id this request should be sent
     */
    int redirectReq(PacketPtr pkt);

    /**
     * self-increment/decrement for the 4bits-satuaration hotCounter
     * @Param &entry: after each counter update, we write it to the rmp_entry
     */
    void hotCounterInc(uint64_t &entry, int idx);

    void hotCounterDec(uint64_t &entry, int idx);

    /**
     * When UMC just completes a page swap, we reset all non-hotpos counter
     * and we set the hotpos counter to 7, since we don't want to the page
     * swapping too frequently.
     */
    void hotCounterReset(uint64_t &entry, int idx);

    /**
     * Extract hotpos, tag and counter info from remapping table entry
     */
    void parseRmpEntry(uint64_t entry);


    /// Pointer to System object
    System *system;

    /// Instantiation of the CPU-side port
    std::vector<CPUSidePort> cpuPorts;

    /// instantiation of the memory-side port
    std::vector<MemSidePort> memPorts;

    /// True if this is currently blocked waiting for a response.
    bool blocked;

    /// The port to send the response when we recieve it back
    int waitingPortId;

    /// Packet that we are currently handling . Used for large pkt that spans
    // multiple pages
    PacketPtr originalPacket;

    /// Number of singe page pkts, we need this to combine all small pkts to
    /// originalpkt when handle response.
    int pagePktNum;

    /// UMC statistics
    Stats::Scalar migrationNum;
    Stats::Scalar fmReadNum;
    Stats::Scalar fmWriteNum;
    Stats::Scalar nmReadNum;
    Stats::Scalar nmWriteNum;
    Stats::Formula extraFMAccess;
    Stats::Formula extraNMAccess;
    Stats::Formula extraTotalAccess;


  public:

    /// The capacity of near and far memories
    AddrRange nearMem;

    AddrRange farMem;

    /// The capacity ratio between FM and NM, it should be a integer
    int ratio;

    /// The migration granularity in byte, default set 1kB
    unsigned int BLK_SIZE;

    /// The remapping table to record migration mappings
    std::vector<uint64_t> remappingTable;

    /// The pseudo register for extract the current hotspot position
    uint64_t hotpos;

    /// The tag bit used to identify whether NM block logically used by CPU
    bool tag;

    /// The hotness competition counter array, the array size is equal
    /// to capaticy ratio between STTRAM and SRAM
    std::vector<uint64_t> hotCounter;

    /** constructor
      */
    UMController(UMControllerParams *params);

    /**
     * Get a port with a given name and index. This is used at
     * binding time and returns a reference to protocol-agnostic port.
     */
    Port &getPort(const std::string &if_name,
                    PortID idx=InvalidPortID) override;

    /**
     * Register the stats
     */
    void regStats() override;
};

#endif // __HUM_UMC_UMCONTROLLER_HH__
