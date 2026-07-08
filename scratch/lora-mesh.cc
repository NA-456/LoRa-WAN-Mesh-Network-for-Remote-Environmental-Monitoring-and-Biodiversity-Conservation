#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LoRaMeshEvaluation");

int main (int argc, char *argv[])
{
    uint32_t numNodes = 5;
    double nodeDistance = 30.0; // Spacing optimized to ensure stable multi-hop wireless link budgets

    CommandLine cmd (__FILE__);
    cmd.AddValue ("numNodes", "Number of nodes in the mesh chain", numNodes);
    cmd.AddValue ("nodeDistance", "Distance between hops in meters", nodeDistance);
    cmd.Parse (argc, argv);

    // 1. Create Network Nodes
    NodeContainer meshNodes;
    meshNodes.Create (numNodes);

    // 2. Configure Node Positions (Linear Array Topology matching your remote deployment framework)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        positionAlloc->Add (Vector (i * nodeDistance, 0.0, 0.0));
    }
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (meshNodes);

    // 3. Install the Low-Power Wireless Physical and MAC Layers
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrWpanDevices = lrWpanHelper.Install (meshNodes);

    // 4. Bind the 6LoWPAN Adaptation Layer (Allows low-power devices to speak standard IP)
    SixLowPanHelper sixLowPanHelper;
    NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install (lrWpanDevices);

    // 5. Build the Routing Infrastructure (Inject AODV over the network layer nodes)
    AodvHelper aodv;
    InternetStackHelper internetStack;
    internetStack.SetRoutingHelper (aodv);
    internetStack.Install (meshNodes);

    // 6. Partition the Mesh Network Subnet Addresses
    Ipv4AddressHelper ipv4Addressing;
    ipv4Addressing.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer virtualInterfaces = ipv4Addressing.Assign (sixLowPanDevices);

    // 7. Deploy Applications 
    uint16_t trafficPort = 9;

    // Install UDP Receiver Server Application on the Central Gateway (Node 0)
    Address serverAddress (Address (virtualInterfaces.GetAddress (0)));
    PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), trafficPort));
    ApplicationContainer serverApps = packetSinkHelper.Install (meshNodes.Get (0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (20.0));

    // Generate sensor alert payloads from the furthest edge leaf node (Node 4) using UDP over IP
    OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (virtualInterfaces.GetAddress (0), trafficPort));
    onoff.SetAttribute ("DataRate", StringValue ("1kbps"));  // Authentic low data-rate sensor stream
    onoff.SetAttribute ("PacketSize", UintegerValue (64));   // Standard 64-byte compact telemetry payload
    
    ApplicationContainer clientApps = onoff.Install (meshNodes.Get (numNodes - 1));
    clientApps.Start (Seconds (4.0)); // Delay start to let AODV complete initial neighbor routing lookups
    clientApps.Stop (Seconds (20.0));

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "Starting Clean 6LoWPAN LoRa-Mesh Simulation: " << numNodes 
              << " nodes spaced " << nodeDistance << "m apart." << std::endl;
    std::cout << "==========================================================\n" << std::endl;

    // 8. Hook FlowMonitor right before execution to profile PDR, Latency, and Drops
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> networkMonitor = flowMonitorHelper.InstallAll();

    // Execute the network engine
    Simulator::Stop (Seconds (20.0));
    Simulator::Run ();

    // 9. Metric Extraction and Data Analytics System
    networkMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> flowClassifier = DynamicCast<Ipv4FlowClassifier> (flowMonitorHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> flowStatistics = networkMonitor->GetFlowStats();

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "                      EVALUATION RESULTS                  " << std::endl;
    std::cout << "==========================================================" << std::endl;

    for (auto const& item : flowStatistics)
    {
        Ipv4FlowClassifier::FiveTuple flowTuple = flowClassifier->FindFlow (item.first);
        std::cout << "Flow ID " << item.first << " (" << flowTuple.sourceAddress << " -> " << flowTuple.destinationAddress << ")" << std::endl;
        std::cout << "  Tx Packets: " << item.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << item.second.rxPackets << std::endl;

        double calculatedPDR = (item.second.txPackets > 0) ? ((double)item.second.rxPackets / item.second.txPackets) * 100.0 : 0.0;
        std::cout << "  Packet Delivery Ratio: " << calculatedPDR << " %" << std::endl;

        if (item.second.rxPackets > 0)
        {
            std::cout << "  Average E2E Delay:    " << (item.second.delaySum.GetSeconds() / item.second.rxPackets) << " s" << std::endl;
        }
        else
        {
            std::cout << "  Average E2E Delay:    N/A (No packets reached the gateway)" << std::endl;
        }
    }
    std::cout << "==========================================================\n" << std::endl;

    // Export metrics file for external data logging scripts
    networkMonitor->SerializeToXmlFile("lora-mesh-results.xml", true, true);

    Simulator::Destroy ();
    return 0;
}