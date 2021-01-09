#ifndef __HUM_TWOLEVEL_TWOLEVEL_HH__
#define __HUM_TWOLEVEL_TWOLEVEL_HH__

#include <vector>

#include "base/statistics.hh"
#include "mem/port.hh"
#include "params/TwoLevel.hh"
#include "sim/clocked_object.hh"
#include "sim/system.hh"

/**
 * A very simple direct-mapping cache with SRAM as medium.
 * Writeback
 */

struct Tag
{
    bool dirty;
    int position;
};

class TwoLevel : public ClockedObject
{
  private:

    /**
     * Port on the CPU-side that receives requests.
     * Mostly just forwards requests to the cache (owner)
     */
    class CPUSidePort : public SlavePort
    {
      private:
        /// Since this is a vector port, need to know what number this one is
        int id;

        /// The object that owns this object (TwoLevel)
        TwoLevel *owner;

        /// True if the port needs to send a retry req.
        bool needRetry;

        /// If we tried to send a packet and it was blocked, store it here
        PacketPtr blockedPacket;

      public:
        /**
         * Constructor. Just calls the superclass constructor.
         */
        CPUSidePort(const std::string& name, int id, TwoLevel *owner) :
            SlavePort(name, owner), id(id), owner(owner), needRetry(false),
            blockedPacket(nullptr)
        { }

        /**
         * Send a packet across this port. This is called by the owner and
         * all of the flow control is hanled in this function.
         * This is a convenience function for the TwoLevel to send pkts.
         *
         * @param packet to send.
         */
        void sendPacket(PacketPtr pkt);

        /**
         * Get a list of the non-overlapping address ranges the owner is
         * responsible for. All slave ports must override this function
         * and return a populated list with at least one item.
         *
         * @return a list of ranges responded to
         */
        AddrRangeList getAddrRanges() const override;

        /**
         * Send a retry to the peer port only if it is needed. This is called
         * from the TwoLevel whenever it is unblocked.
         */
        void trySendRetry();

      protected:
        /**
         * Receive an atomic request packet from the master port.
         * No need to implement in this simple cache.
         */
        Tick recvAtomic(PacketPtr pkt) override
        { panic("recvAtomic unimpl."); }

        /**
         * Receive a functional request packet from the master port.
         * Performs a "debug" access updating/reading the data in place.
         *
         * @param packet the requestor sent.
         */
        void recvFunctional(PacketPtr pkt) override;

        /**
         * Receive a timing request from the master port.
         *
         * @param the packet that the requestor sent
         * @return whether this object can consume to packet. If false, we
         *         will call sendRetry() when we can try to receive this
         *         request again.
         */
        bool recvTimingReq(PacketPtr pkt) override;

        /**
         * Called by the master port if sendTimingResp was called on this
         * slave port (causing recvTimingResp to be called on the master
         * port) and was unsuccesful.
         */
        void recvRespRetry() override;
    };

    /**
     * Port on the memory-side that receives responses.
     * Mostly just forwards requests to the cache (owner)
     */
    class MemSidePort : public MasterPort
    {
      private:
        /// The object that owns this object (TwoLevel)
        TwoLevel *owner;

        /// If we tried to send a packet and it was blocked, store it here
        PacketPtr blockedPacket;

      public:
        /**
         * Constructor. Just calls the superclass constructor.
         */
        MemSidePort(const std::string& name, TwoLevel *owner) :
            MasterPort(name, owner), owner(owner), blockedPacket(nullptr)
        { }

        /**
         * Send a packet across this port. This is called by the owner and
         * all of the flow control is hanled in this function.
         * This is a convenience function for the TwoLevel to send pkts.
         *
         * @param packet to send.
         */
        void sendPacket(PacketPtr pkt);

      protected:
        /**
         * Receive a timing response from the slave port.
         */
        bool recvTimingResp(PacketPtr pkt) override;

        /**
         * Called by the slave port if sendTimingReq was called on this
         * master port (causing recvTimingReq to be called on the slave
         * port) and was unsuccesful.
         */
        void recvReqRetry() override;

        /**
         * Called to receive an address range change from the peer slave
         * port. The default implementation ignores the change and does
         * nothing. Override this function in a derived class if the owner
         * needs to be aware of the address ranges, e.g. in an
         * interconnect component like a bus.
         */
        void recvRangeChange() override;
    };

    /**
     * Represents that the indicated thread context has a "lock" on the block,
     * in the LL/SC sense. (from BaseCache)
     */
    class Lock {
      public:
        ContextID contextId;
        Addr lowAddr;
        Addr highAddr;

        bool matches(const RequestPtr &req) const
        {
          Addr req_low = req->getPaddr();
          Addr req_high = req_low + req->getSize() - 1;
          return (contextId == req->contextId()) &&
                  (req_low >= lowAddr) && (req_high <= highAddr);
        }

        bool intersects(const RequestPtr &req) const
        {
          Addr req_low = req->getPaddr();
          Addr req_high = req_low + req->getSize() - 1;
          return (req_low <= highAddr) && (req_high >= lowAddr);
        }

        Lock(const RequestPtr &req)
          : contextId(req->contextId()),
            lowAddr(req->getPaddr()),
            highAddr(lowAddr + req->getSize() - 1)
            {

            }
    };

    /** List of thread contexts that have performed a load-locked(LL) on the
     * block since the last store */
    std::list<Lock> lockList;

    /**
     * Track the fact that a local locked was issued to the block.
     * Invalidate any previous LL to the same address.
     */
    void trackLoadLocked(PacketPtr pkt)
    {
      assert(pkt->isLLSC());
      auto l = lockList.begin();
      while (l != lockList.end())
      {
        if (l->intersects(pkt->req))
            l = lockList.erase(l);
        else
            ++l;
      }

      lockList.emplace_front(pkt->req);
    }

    /**
     * Clear the any load lock that intersect the request, and is from a
     * different context.
     */
    void clearLoadLocks(const RequestPtr &req)
    {
      auto l = lockList.begin();
      while (l != lockList.end())
      {
        if (l->intersects(req) && l->contextId != req->contextId()){
          l = lockList.erase(l);
        }else{
          ++l;
        }
      }

    }

    /**
     * Handle interaction of load-locked operations and stores.
     * @return true if write should proceed, fase otherwise. returns
     * false only in the case of a failed store conditional.
     */
    bool checkWrite(PacketPtr pkt)
    {
        assert(pkt->isWrite());

        if (!pkt->isLLSC() && lockList.empty())
          return true;

        const RequestPtr &req = pkt->req;

        if (pkt->isLLSC()){
          bool success = false;
          auto l = lockList.begin();
          while (!success && l != lockList.end())
          {
            if (l->matches(pkt->req)){
              success = true;
              lockList.erase(l);
            }else {
              ++l;
            }
          }
          req->setExtraData(success ? 1 : 0);
          clearLoadLocks(req);
          return success;
        }else{
          clearLoadLocks(req);
          return true;
        }

    }

    /**
     * Handle the request from the CPU side. Called from the CPU port
     * on a timing request.
     *
     * @param requesting packet
     * @param id of the port to send the response
     * @return true if we can handle the request this cycle, false if the
     *         requestor needs to retry later
     */
    bool handleRequest(PacketPtr pkt, int port_id);

    /**
     * Handle the respone from the memory side. Called from the memory port
     * on a timing response.
     *
     * @param responding packet
     * @return true if we can handle the response this cycle, false if the
     *         responder needs to retry later
     */
    bool handleResponse(PacketPtr pkt);

    /**
     * Send the packet to the CPU side.
     * This function assumes the pkt is already a response packet and forwards
     * it to the correct port. This function also unblocks this object and
     * cleans up the whole request.
     *
     * @param the packet to send to the cpu side
     */
    void sendResponse(PacketPtr pkt);

    /**
     * Handle a packet functionally. Update the data on a write and get the
     * data on a read. Called from CPU port on a recv functional.
     *
     * @param packet to functionally handle
     */
    void handleFunctional(PacketPtr pkt);

    /**
     * Access the cache for a timing access. This is called after the cache
     * access latency has already elapsed.
     */
    void accessTiming(PacketPtr pkt);

    /**
     * This is where we actually update / read from the cache. This function
     * is executed on both timing and functional accesses.
     *
     * @return true if a hit, false otherwise
     */
    bool accessFunctional(PacketPtr pkt);

    /**
     * Insert a block into the cache. If there is no room left in the cache,
     * then this function evicts a random entry t make room for the new block.
     *
     * @param packet with the data (and address) to insert into the cache
     */
    void insert(PacketPtr pkt);

    /**
     * Return the address ranges this cache is responsible for. Just use the
     * same as the next upper level of the hierarchy.
     *
     * @return the address ranges this cache is responsible for
     */
    AddrRangeList getAddrRanges() const;

    /**
     * Tell the CPU side to ask for our memory ranges.
     */
    void sendRangeChange() const;

    /// Latency to check the cache. Number of cycles for both hit and miss
    ///const Cycles latency;

    /// Latency to access 64B cache data
    const Tick readLatency;
    const Tick writeLatency;
    const Tick insertLatency;

    /// The block size for the cache
    const unsigned blockSize;

    /// Number of blocks in the cache (size of cache / block size)
    const unsigned capacity;

    /// Instantiation of the CPU-side port
    std::vector<CPUSidePort> cpuPorts;

    /// Instantiation of the memory-side port
    MemSidePort memPort;

    /// True if this cache is currently blocked waiting for a response.
    bool blocked;

    /// Packet that we are currently handling. Used for upgrading to larger
    /// cache line sizes
    PacketPtr originalPacket;

    /// The port to send the response when we recieve it back
    int waitingPortId;

    /// For tracking the miss latency
    Tick missTime;

    /// An incredibly simple cache storage. Maps block addresses to data
    std::vector<Tag> tagList;
    std::vector<uint8_t*> cacheStore;

    /// Cache statistics
    Stats::Scalar hits;
    Stats::Scalar misses;
    Stats::Histogram missLatency;
    Stats::Formula hitRatio;

  public:

    /** constructor
     */
    TwoLevel(TwoLevelParams *params);

    /**
     * Get a port with a given name and index. This is used at
     * binding time and returns a reference to a protocol-agnostic
     * port.
     *
     * @param if_name Port name
     * @param idx Index in the case of a VectorPort
     *
     * @return A reference to the given port
     */
    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;

    /**
     * Register the stats
     */
    void regStats() override;
};


#endif // __LEARNING_GEM5_SIMPLE_CACHE_SIMPLE_CACHE_HH__
