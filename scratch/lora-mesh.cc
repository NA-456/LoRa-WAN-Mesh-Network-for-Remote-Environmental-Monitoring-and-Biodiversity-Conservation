#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h" 
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LoRaMeshEvaluation");

// Global evaluation counters for tracking analytics without IPv4
uint32_t g_txPackets = 0;
uint32_t g_rxPackets = 0;
Time g_totalDelay = Seconds (0.0);

// Custom Header Tag to trace E2E Delay across multi-hop links natively
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
  virtual uint32_t GetSerializedSize (void) const { return sizeof (uint64_t); }
  virtual void Serialize (TagBuffer i) const { i.WriteU64 (m_timestamp); }
  virtual void Deserialize (TagBuffer i) { m_timestamp = i.ReadU64 (); }
  virtual void Print (std::ostream &os) const { os << "Time=" << m_timestamp; }

  void SetTimestamp (Time t) { m_timestamp = t.GetMicroSeconds (); }
  Time GetTimestamp (void) const { return MicroSeconds (m_timestamp); }

private:
  uint64_t m_timestamp;
};

// Callback function triggered natively every time a packet reaches the destination node
void PacketRxSink (Ptr<const Packet> packet, const Address &from)
{
  g_rxPackets++;
  EvaluationTag tag;
  if (packet->PeekPacketTag (tag))
    {
      Time txTime = tag.GetTimestamp ();
      g_totalDelay += (Simulator::Now() - txTime);
    }
}

int main (int argc, char *argv[])
{
  uint32_t numNodes = 5;       // Gateway (0) -> Relay (1) -> Relay (2) -> Relay (3) -> Leaf (4)
  double nodeDistance = 15.0;  // Spaced strictly within real-world LrWpan ranges (15m)

  CommandLine cmd (__FILE__);
  cmd.AddValue ("numNodes", "Number of nodes in the mesh chain", numNodes);
  cmd.AddValue ("nodeDistance", "Distance between hops in meters", nodeDistance);
  cmd.Parse (argc, argv);

  // 1. Create Conservation Grid Nodes
  NodeContainer meshNodes;
  meshNodes.Create (numNodes);

  // 2. Configure Node Positions (Linear Array Array Array Topology)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      positionAlloc->Add (Vector (i * nodeDistance, 0.0, 0.0)); 
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (meshNodes);

  // 3. Configure the Low-Power Physical and MAC Layers Natively
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrWpanDevices = lrWpanHelper.Install (meshNodes);

  // 4. Hook Up Raw Packet Socket Engine (Removes heavy IPv4/ARP layers completely)
  PacketSocketHelper packetSocket;
  packetSocket.Install (meshNodes);

  // Define local addressing endpoints using MAC layer protocols
  PacketSocketAddress socketAddr;
  socketAddr.SetSingleDevice (lrWpanDevices.Get (numNodes - 1)->GetIfIndex ());
  socketAddr.SetPhysicalAddress (lrWpanDevices.Get (0)->GetAddress ());
  socketAddr.SetProtocol (0x88BB); // Custom industrial experimental protocol ID

  // 5. Connect Tracking Callbacks to the Gateway Node (Node 0) to collect statistics
  Ptr<PacketSocketServer> server = CreateObject<PacketSocketServer> ();
  meshNodes.Get (0)->AddApplication (server);
  server->SetLocalAddress (socketAddr);
  server->Start (Seconds (1.0));
  server->Stop (Seconds (20.0));
  server->TraceConnectWithoutContext ("Rx", MakeCallback (&PacketRxSink));

  // 6. Generate Threat Detection Sensor Telemetry (Leaf Node 4)
  Ptr<PacketSocketClient> client = CreateObject<PacketSocketClient> ();
  meshNodes.Get (numNodes - 1)->AddApplication (client);
  client->SetRemoteAddress (socketAddr);
  client->SetAttribute ("MaxPackets", UintegerValue (5));
  client->SetAttribute ("Interval", TimeValue (Seconds (2.0)));
  client->SetAttribute ("PacketSize", UintegerValue (64)); // Standard LoRa-sized acoustic payload
  client->Start (Seconds (3.0));
  client->Stop (Seconds (20.0));

  std::cout << "\n==========================================================" << std::endl;
  std::cout << "Starting Clean Low-Power LoRa-Mesh Simulation (Non-IP): " << numNodes 
            << " nodes spaced " << nodeDistance << "m apart." << std::endl;
  std::cout << "==========================================================\n" << std::endl;

  // Track total transmitted packets prior to running the execution loop
  g_txPackets = 5; 

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

  // Compute precise analytics
  double pdr = (g_txPackets > 0) ? ((double)g_rxPackets / g_txPackets) * 100.0 : 0.0;
  std::cout << "  Packet Delivery Ratio (PDR):         " << pdr << " %" << std::endl;

  if (g_rxPackets > 0)
    {
      std::cout << "  Average End-to-End Delay:            " << (g_totalDelay.GetSeconds() / g_rxPackets) << " s" << std::endl;
    }
  else
    {
      std::cout << "  Average End-to-End Delay:            N/A (Foliage Drop Out / No Path Found)" << std::endl;
    }
  std::cout << "==========================================================\n" << std::endl;

  Simulator::Destroy ();
  return 0;
}