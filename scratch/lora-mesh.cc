#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h" 
#include <set>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LoRaMeshEvaluation");

uint32_t g_txPackets = 0;
uint32_t g_rxPackets = 0;
Time g_totalDelay = Seconds (0.0);
double g_nodeDistance = 10.0;

NetDeviceContainer g_lrWpanDevices;
std::set<uint32_t> g_gatewaySeenPackets;
std::set<std::pair<uint32_t, uint32_t>> g_relaySeenPackets; // pair<nodeId, seqNum>

// Custom timestamp & sequence tracking tag
class EvaluationTag : public Tag
{
public:
  static TypeId GetTypeId (void) {
    static TypeId tid = TypeId ("ns3::EvaluationTag")
      .SetParent<Tag> ()
      .AddConstructor<EvaluationTag> ();
    return tid;
  }
  virtual TypeId GetInstanceTypeId (void) const { return GetTypeId (); }
  virtual uint32_t GetSerializedSize (void) const { return sizeof (uint64_t) + sizeof (uint32_t); }
  
  virtual void Serialize (TagBuffer i) const { 
    i.WriteU64 (m_timestamp); 
    i.WriteU32 (m_seqNum);
  }
  virtual void Deserialize (TagBuffer i) { 
    m_timestamp = i.ReadU64 (); 
    m_seqNum = i.ReadU32 ();
  }
  virtual void Print (std::ostream &os) const { 
    os << "Time=" << m_timestamp << " Seq=" << m_seqNum; 
  }

  void SetTimestamp (Time t) { m_timestamp = t.GetMicroSeconds (); }
  Time GetTimestamp (void) const { return MicroSeconds (m_timestamp); }

  void SetSeqNum (uint32_t seq) { m_seqNum = seq; }
  uint32_t GetSeqNum (void) const { return m_seqNum; }

private:
  uint64_t m_timestamp;
  uint32_t m_seqNum;
};

// Periodic threat telemetry generator deployed on the Leaf Node
void GenerateSensorAlert (uint32_t leafNodeIndex, uint32_t packetsRemaining, Time interval, uint32_t currentSeq)
{
  if (packetsRemaining == 0) return;

  Ptr<Packet> packet = Create<Packet> (64); // 64-byte compact telemetry payload
  EvaluationTag tag;
  tag.SetTimestamp (Simulator::Now ());
  tag.SetSeqNum (currentSeq);
  packet->AddPacketTag (tag);

  g_txPackets++;
  Ptr<NetDevice> dev = g_lrWpanDevices.Get (leafNodeIndex);
  if (dev)
    {
      dev->Send (packet, dev->GetBroadcast (), 0x88BB);
    }

  Simulator::Schedule (interval, &GenerateSensorAlert, leafNodeIndex, packetsRemaining - 1, interval, currentSeq + 1);
}

// Delayed forwarding helper to prevent wireless collisions
void ForwardPacket (uint32_t relayNodeId, Ptr<Packet> packet)
{
  Ptr<NetDevice> dev = g_lrWpanDevices.Get (relayNodeId);
  if (dev)
    {
      dev->Send (packet, dev->GetBroadcast (), 0x88BB);
    }
}

// Native Layer-2 NetDevice Receive Callback Engine
bool Layer2ReceivePacketSink (Ptr<NetDevice> dev, Ptr<const Packet> packet, uint16_t protocol, const Address &sender)
{
  uint32_t currentNodeId = dev->GetNode ()->GetId ();
  EvaluationTag tag;

  if (!packet->PeekPacketTag (tag))
    {
      return true;
    }

  uint32_t seq = tag.GetSeqNum ();

  // Gateway (Node 0) reception - accept each unique packet sequence once
  if (currentNodeId == 0)
    {
      if (g_gatewaySeenPackets.find (seq) == g_gatewaySeenPackets.end ())
        {
          g_gatewaySeenPackets.insert (seq);
          g_rxPackets++;
          g_totalDelay += (Simulator::Now () - tag.GetTimestamp ());
        }
    }
  // Intermediate Relays (forward towards Node 0)
  else if (currentNodeId > 0 && currentNodeId < g_lrWpanDevices.GetN () - 1)
    {
      auto relayKey = std::make_pair (currentNodeId, seq);
      if (g_relaySeenPackets.find (relayKey) == g_relaySeenPackets.end ())
        {
          g_relaySeenPackets.insert (relayKey);
          Ptr<Packet> forwardPacket = packet->Copy ();
          // Forward down towards gateway with 20ms jitter
          Simulator::Schedule (MilliSeconds (20), &ForwardPacket, currentNodeId, forwardPacket);
        }
    }
  return true;
}

int main (int argc, char *argv[])
{
  uint32_t numNodes = 5;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("numNodes", "Number of nodes in the mesh chain", numNodes);
  cmd.AddValue ("nodeDistance", "Distance between hops in meters", g_nodeDistance);
  cmd.Parse (argc, argv);

  // 1. Create Nodes
  NodeContainer meshNodes;
  meshNodes.Create (numNodes);

  // 2. Configure Node Positions (Linear Array Topology)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      positionAlloc->Add (Vector (i * g_nodeDistance, 0.0, 0.0)); 
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (meshNodes);

  // 3. Install LR-WPAN Physical and MAC Layers
  LrWpanHelper lrWpanHelper;
  g_lrWpanDevices = lrWpanHelper.Install (meshNodes);

  // 4. Hook native SetReceiveCallback on all nodes
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      Ptr<NetDevice> dev = g_lrWpanDevices.Get (i);
      if (dev)
        {
          dev->SetReceiveCallback (MakeCallback (&Layer2ReceivePacketSink));
        }
    }

  // 5. Schedule 5 telemetry alerts (every 2.0s starting at t = 2.0s) from Leaf Node
  Simulator::Schedule (Seconds (2.0), &GenerateSensorAlert, numNodes - 1, 5, Seconds (2.0), 1);

  std::cout << "\n==========================================================" << std::endl;
  std::cout << "Starting Clean Low-Power LoRa-Mesh Simulation (Non-IP): " << numNodes 
            << " nodes spaced " << g_nodeDistance << "m apart." << std::endl;
  std::cout << "==========================================================\n" << std::endl;

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();

  // =======================================================================
  // TECHNICAL PERFORMANCE METRICS EVALUATION
  // =======================================================================
  std::cout << "\n==========================================================" << std::endl;
  std::cout << "               BIODIVERSITY GRID EVALUATION RESULTS         " << std::endl;
  std::cout << "==========================================================" << std::endl;
  std::cout << "  Total Telemetry Packets Transmitted: " << g_txPackets << std::endl;
  std::cout << "  Total Telemetry Packets Received:    " << g_rxPackets << std::endl;

  double pdr = (g_txPackets > 0) ? ((double)g_rxPackets / g_txPackets) * 100.0 : 0.0;
  std::cout << "  Packet Delivery Ratio (PDR):         " << pdr << " %" << std::endl;

  if (g_rxPackets > 0)
    {
      std::cout << "  Average End-to-End Delay:            " << (g_totalDelay.GetSeconds () / g_rxPackets) << " s" << std::endl;
    }
  else
    {
      std::cout << "  Average End-to-End Delay:            N/A (Foliage Drop Out / No Path Found)" << std::endl;
    }
  std::cout << "==========================================================\n" << std::endl;

  Simulator::Destroy ();
  return 0;
}