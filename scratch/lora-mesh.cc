#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h" 
#include "ns3/aodv-module.h"    
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LoRaMeshEvaluation");

int main (int argc, char *argv[])
{
  uint32_t numNodes = 5;       // Gateway (0) -> Relay (1) -> Relay (2) -> Relay (3) -> Leaf (4)
  double nodeDistance = 150.0; // Spaced evenly to ensure clean multi-hop link tracking

  CommandLine cmd (__FILE__);
  cmd.AddValue ("numNodes", "Number of nodes in the mesh chain", numNodes);
  cmd.AddValue ("nodeDistance", "Distance between hops in meters", nodeDistance);
  cmd.Parse (argc, argv);

  // 1. Create Nodes
  NodeContainer meshNodes;
  meshNodes.Create (numNodes);

  // 2. Configure Node Positions (Linear Array Topology)
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

  // 4. Inject AODV Ad-Hoc Routing into the Internet Stack
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv); 
  internet.Install (meshNodes);

  // 5. Assign IP Addresses across the physical network interfaces
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (lrWpanDevices);

  // 6. Setup Test Traffic (Data packets from Edge Node 4 back to Central Gateway Node 0)
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (meshNodes.Get (0)); 
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (2.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (64)); // Standard LoRa-sized payload

  ApplicationContainer clientApps = echoClient.Install (meshNodes.Get (numNodes - 1)); 
  clientApps.Start (Seconds (3.0)); // Allows AODV mesh path discovery vectors to settle
  clientApps.Stop (Seconds (20.0));

  std::cout << "\n==========================================================" << std::endl;
  std::cout << "Starting Clean LoRa-Mesh Simulation: " << numNodes 
            << " nodes spaced " << nodeDistance << "m apart." << std::endl;
  std::cout << "==========================================================\n" << std::endl;

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  
  std::cout << "\nSimulation execution finished successfully." << std::endl;
  return 0;
}