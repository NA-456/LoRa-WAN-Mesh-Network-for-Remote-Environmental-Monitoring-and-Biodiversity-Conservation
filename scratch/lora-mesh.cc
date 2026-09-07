#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h"
#include <set>
#include <vector>
#include <cmath>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LoRaMeshEvaluation");

// Global metrics tracking
uint32_t g_txPackets = 0;
uint32_t g_rxPackets = 0;
Time g_totalDelay = Seconds (0.0);
double g_nodeDistance = 30.0;
std::string g_runMode = "mesh"; // "mesh" or "star"
bool g_enableFailure = false;   // Mid-run relay failure test

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

// Semtech AN1200.13 Analytical LoRa Time-on-Air (ToA) calculation
double CalculateLoRaToA (uint32_t payloadBytes, uint32_t sf, double bwHz = 125000.0)
{
  double tSym = std::pow (2.0, (double)sf) / bwHz;
  double tPreamble = (8.0 + 4.25) * tSym;
  double de = (sf >= 11) ? 1.0 : 0.0; // Low data rate optimization
  double num = 8.0 * payloadBytes - 4.0 * sf + 28.0 + 16.0;
  double den = 4.0 * (sf - 2.0 * de);
  double payloadSymbNb = 8.0 + std::max (std::ceil (num / den) * 5.0, 0.0);
  return tPreamble + (payloadSymbNb * tSym);
}

// Delayed forwarding helper
void ForwardPacket (uint32_t relayNodeId, Ptr<Packet> packet)
{
  Ptr<NetDevice> dev = g_lrWpanDevices.Get (relayNodeId);
  if (dev)
    {
      dev->Send (packet, dev->GetBroadcast (), 0x88BB);
    }
}

// Periodic threat telemetry generator deployed on Leaf Node
void GenerateSensorAlert (uint32_t leafNodeIndex, uint32_t packetsRemaining, Time interval, uint32_t currentSeq)
{
  if (packetsRemaining == 0) return;

  Ptr<Packet> packet = Create<Packet> (64); // 64-byte telemetry payload
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

// Native NetDevice Layer-2 receive sink
bool Layer2ReceivePacketSink (Ptr<NetDevice> dev, Ptr<const Packet> packet, uint16_t protocol, const Address &sender)
{
  uint32_t currentNodeId = dev->GetNode ()->GetId ();
  EvaluationTag tag;

  if (!packet->PeekPacketTag (tag))
    {
      return true;
    }

  uint32_t seq = tag.GetSeqNum ();

  // Gateway (Node 0)
  if (currentNodeId == 0)
    {
      if (g_gatewaySeenPackets.find (seq) == g_gatewaySeenPackets.end ())
        {
          g_gatewaySeenPackets.insert (seq);
          g_rxPackets++;
          g_totalDelay += (Simulator::Now () - tag.GetTimestamp ());
        }
      return true;
    }

  // In Baseline STAR Mode, intermediate nodes never relay
  if (g_runMode == "star")
    {
      return true;
    }

  // Mid-Run Failure Test: Relay 2 stops forwarding after t = 10.0 s
  if (g_enableFailure && currentNodeId == 2 && Simulator::Now ().GetSeconds () >= 10.0)
    {
      return true;
    }

  // Intermediate Relays (forward towards Node 0)
  if (currentNodeId > 0 && currentNodeId < g_lrWpanDevices.GetN () - 1)
    {
      auto relayKey = std::make_pair (currentNodeId, seq);
      if (g_relaySeenPackets.find (relayKey) == g_relaySeenPackets.end ())
        {
          g_relaySeenPackets.insert (relayKey);
          Ptr<Packet> forwardPacket = packet->Copy ();

          // Forwarding jitter (15ms - 35ms) to prevent wireless collisions
          Ptr<UniformRandomVariable> jitter = CreateObject<UniformRandomVariable> ();
          Time forwardDelay = MilliSeconds (jitter->GetValue (15.0, 35.0));
          Simulator::Schedule (forwardDelay, &ForwardPacket, currentNodeId, forwardPacket);
        }
    }
  return true;
}

int main (int argc, char *argv[])
{
  uint32_t numNodes = 5;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("numNodes", "Number of nodes in the chain (Node 0 is Gateway, Node N-1 is Leaf)", numNodes);
  cmd.AddValue ("nodeDistance", "Distance between adjacent hops in meters", g_nodeDistance);
  cmd.AddValue ("mode", "Network evaluation mode: 'mesh' or 'star'", g_runMode);
  cmd.AddValue ("enableFailure", "Simulate Node 2 relay failure after t=10s", g_enableFailure);
  cmd.Parse (argc, argv);

  // 1. Create Nodes
  NodeContainer meshNodes;
  meshNodes.Create (numNodes);

  // 2. Linear Array Positions
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      positionAlloc->Add (Vector (i * g_nodeDistance, 0.0, 0.0)); 
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (meshNodes);

  // 3. Install LR-WPAN Devices using stable native channel setup
  LrWpanHelper lrWpanHelper;
  g_lrWpanDevices = lrWpanHelper.Install (meshNodes);

  // 4. Connect Layer-2 Receive Callbacks across all devices
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      Ptr<NetDevice> dev = g_lrWpanDevices.Get (i);
      if (dev)
        {
          dev->SetReceiveCallback (MakeCallback (&Layer2ReceivePacketSink));
        }
    }

  // 5. Schedule 8 telemetry alerts (every 2.0s from t = 2.0s to 16.0s) from Leaf Node
  Simulator::Schedule (Seconds (2.0), &GenerateSensorAlert, numNodes - 1, 8, Seconds (2.0), 1);

  std::cout << "\n==========================================================" << std::endl;
  std::cout << "Starting LoRa Mesh Simulation (" << g_runMode << " mode):" << std::endl;
  std::cout << "  Nodes: " << numNodes << " | Hop Distance: " << g_nodeDistance << " m" << std::endl;
  std::cout << "  Total Span: " << (numNodes - 1) * g_nodeDistance << " m" << std::endl;
  if (g_enableFailure)
    {
      std::cout << "  Fault Injection: Relay 2 will shut down at t = 10.0 s" << std::endl;
    }
  std::cout << "==========================================================\n" << std::endl;

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();

  // =======================================================================
  // METRICS & AIRTIME EVALUATION
  // =======================================================================
  double pdr = (g_txPackets > 0) ? ((double)g_rxPackets / g_txPackets) * 100.0 : 0.0;
  uint32_t hopCount = (g_runMode == "star") ? 1 : (numNodes - 1);
  double singleHopToA_SF7  = CalculateLoRaToA (64, 7, 125000.0);
  double singleHopToA_SF12 = CalculateLoRaToA (64, 12, 125000.0);

  std::cout << "\n==========================================================" << std::endl;
  std::cout << "              BIODIVERSITY GRID EVALUATION RESULTS         " << std::endl;
  std::cout << "==========================================================" << std::endl;
  std::cout << "  Topology Mode:                    " << (g_runMode == "star" ? "STAR (Single-Hop Baseline)" : "MESH (Multi-Hop Layer-2)") << std::endl;
  std::cout << "  Total Telemetry Alerts Sent (Tx): " << g_txPackets << std::endl;
  std::cout << "  Alerts Reaching Gateway (Rx):     " << g_rxPackets << std::endl;
  std::cout << "  Packet Delivery Ratio (PDR):      " << pdr << " %" << std::endl;

  if (g_rxPackets > 0)
    {
      double emulatedDelayMs = (g_totalDelay.GetSeconds () / g_rxPackets) * 1000.0;
      std::cout << "  Emulated MAC Forwarding Delay:    " << emulatedDelayMs << " ms" << std::endl;
      std::cout << "  Mapped LoRa Airtime (SF7):        " << (hopCount * singleHopToA_SF7 * 1000.0) << " ms (" << (hopCount * singleHopToA_SF7) << " s)" << std::endl;
      std::cout << "  Mapped LoRa Airtime (SF12):       " << (hopCount * singleHopToA_SF12 * 1000.0) << " ms (" << (hopCount * singleHopToA_SF12) << " s)" << std::endl;
    }
  else
    {
      std::cout << "  Link Status:                      DROPPED (Signal below receiver sensitivity threshold)" << std::endl;
    }
  std::cout << "==========================================================\n" << std::endl;

  Simulator::Destroy ();
  return 0;
}