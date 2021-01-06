#ifndef __HUM_PURE_MEMCONTROLLER_HH__
#define __HUM_PURE_MEMCONTROLLER_HH__

#include <unordered_map>

#include "mem/port.hh"
#include "params/MemController.hh"
#include "sim/clocked_object.hh"
#include "sim/system.hh"

/**
 * A simple memory controller without any features, just forward request
 * and record some statistics.
 */
class MemController : public ClockedObject
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

        MemController *owner;

        bool needRetry;

        PacketPtr blockedPacket;

      public:
        /**
         * Constructor. Just calls the superclass constructor.
         */
        CPUSidePort(const std::string& name, int id, MemController *owner):
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

        MemController *owner;

        PacketPtr blockedPacket;

      public:
        MemSidePort(const std::string& name, MemController *owner) :
          MasterPort(name, owner), owner(owner),
          blockedPacket(nullptr)
        { }

        void sendPacket(PacketPtr pkt);

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

    int redirectReq(PacketPtr pkt);
    */
    /// Instantiation of the CPU-side port
    std::vector<CPUSidePort> cpuPorts;

    /// instantiation of the memory-side port
    MemSidePort memPort;

    /// True if this is currently blocked waiting for a response.
    bool blocked;

    /// The port to send the response when we recieve it back
    int waitingPortId;

  public:

    /** constructor
      */
    MemController(MemControllerParams *params);

    /**
     * Get a port with a given name and index. This is used at
     * binding time and returns a reference to protocol-agnostic port.
     */
    Port &getPort(const std::string &if_name,
                    PortID idx=InvalidPortID) override;
};

#endif // __HUM_UMC_MEMCONTROLLER_HH__
