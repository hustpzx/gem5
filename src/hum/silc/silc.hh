#ifndef __HUM_SILC_SILC_HH__
#define __HUM_SILC_SILC_HH__

#include <cmath>
#include <unordered_map>

#include "base/bitfield.hh"
#include "mem/physical.hh"
#include "mem/port.hh"
#include "params/SILC.hh"
#include "sim/clocked_object.hh"
#include "sim/system.hh"

/**
 * SILC-FM from HPCA'17, implemented for testing.
 */

#define PAGE_SIZE 2048
#define MAX_TICK ULL(0xffffffffffffffff)

struct  rmpEntry
{
  bool lock;
  Addr bvt_index; // PC xor Addr, index for bit vector table
  Addr remap;     // addr of remapping FM page(2K)
  Tick lruinfo;     // finding candidate NM page for swapping
  uint8_t NM_counter;
  uint8_t FM_counter;
  uint32_t bitvector;
};

class SILC : public ClockedObject
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

        SILC *owner;

        bool needRetry;

        PacketPtr blockedPacket;

      public:
        /**
         * Constructor. Just calls the superclass constructor.
         */
        CPUSidePort(const std::string& name, int id, SILC *owner):
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

        SILC *owner;

        PacketPtr blockedPacket;

      public:
        MemSidePort(const std::string& name,int id, SILC *owner) :
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
     *
     * @param port_id id of the port to send the response
     */
    bool handleRequest(PacketPtr pkt, int port_id);

    /**
     * Handle the response from the memory side. Called from the memory port
     * on a timing response.
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
    //void sendResponse(PacketPtr pkt);

    /**
     * Handle a packet functionally.
     * Redirect pkt path according to current operation and the utilization
     * of SRAM. Called from CPU port on a send functional.
     */
    void handleFunctional(PacketPtr pkt);

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
     * swap subblock between FM and NM
     */
    void swapSubblk(Addr nmaddr, Addr fmaddr);

    /**
     * locking feature: Lock page funciton
     * @param Entry: neccessary metadata such as lock, bitvector, remap
     * @param nmaddr
     * @param flag: lock NM or FM page, 0->NM, 1->FM
     * @return: return the number of swapped subblocks
     */
    int lockPage(rmpEntry *entry, Addr nmaddr, bool flag);

    /**
     * restore remapped NM page to original state
     */
    int restorePage(rmpEntry *entry, Addr nmaddr);

    /**
     * 6-bits saturation counter self-increasing function.
     * can't bigger than 63
     * @return if the counter bigger than threshorld(60)
     */
    bool counterInc(uint8_t &cntr);
    /**
     * Redirect the request/packet to relative memory port according to the
     * vaddr(since we have two memories).
     *
     * @return the mem_port_id this request should be sent
     */
    int redirectReq(PacketPtr pkt);

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

    long agingCntr;

    // statistics
    Stats::Scalar agingResetNum;
    Stats::Scalar swapNum;
    Stats::Scalar queryNum;

    Stats::Formula extraTimeConsumption;

  public:
    AddrRange nearMem;

    AddrRange farMem;

    std::vector<rmpEntry> rmpTable;

    std::unordered_map<Addr, uint32_t> bitVectorTable;

    /** constructor
      */
    SILC(SILCParams *params);

    /**
     * Get a port with a given name and index. This is used at
     * binding time and returns a reference to protocol-agnostic port.
     */
    Port &getPort(const std::string &if_name,
                    PortID idx=InvalidPortID) override;

    void regStats() override;
};

#endif // __HUM_SILC_SILC_HH__
