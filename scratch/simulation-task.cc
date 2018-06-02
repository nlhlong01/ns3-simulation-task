#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-helper.h"
#include "ns3/v4ping-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/olsr-routing-protocol.h"
#include "ns3/olsr-helper.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("SimulationTask");


int main (int argc, char *argv[]) {
    int nodeCount = 4;
    bool debug = false;
    bool tcp = true;
    int distance = 1;
    DataRate dataRate = DataRate("54Mbps");
    double simTime = 5.0;

    CommandLine cmd;
    cmd.AddValue ("debug", "Simulation mode", debug);
    cmd.AddValue ("tcp", "Protocol", tcp);
    cmd.AddValue ("nodeCount", "Number of nodes", nodeCount);
    cmd.AddValue ("distance", "Distance between 2 nodes", distance);
    cmd.AddValue ("simTime", "Simulation time", simTime);
    cmd.Parse (argc, argv);

    // Set up a 4-node wireless adhoc 802.11g network.
    NodeContainer nodes;
    nodes.Create(nodeCount);
    Ptr<Node> clientNode = nodes.Get(0);
    Ptr<Node> serverNode = nodes.Get(nodeCount-1);
    NetDeviceContainer devices;

    // Set up wifi channel
    YansWifiChannelHelper wifiChannelHelper = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> channel = wifiChannelHelper.Create();

    // Configure physical layer
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.Set("ChannelWidth", UintegerValue(20));
    phy.Set("TxGain", DoubleValue(1.0));
    phy.Set("RxGain", DoubleValue(1.0));
    phy.Set("TxPowerStart", DoubleValue(1.0));
    phy.Set("TxPowerEnd", DoubleValue(1.0));
    phy.SetChannel(channel);

    // Set up MAC layer
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Configure wifi net device
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");
    devices = wifi.Install(phy, mac, nodes);

    // Set OLSR as the routing protocol of the network
    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper list;
    list.Add (staticRouting, 0);
    list.Add (olsr, 10);

    // Install internet stack
    InternetStackHelper stack;
    stack.SetRoutingHelper (list);
    stack.Install(nodes);
    Ipv4AddressHelper addresses ;
    addresses.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = addresses.Assign(devices);
    // Ipv4Address clientAddr = interfaces.GetAddress(0); 
    Ipv4Address serverAddr = interfaces.GetAddress(nodeCount-1);    

    // Set up application containers
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    ApplicationContainer sourceApps;
    ApplicationContainer sinkApps;

    if (debug) {
        // Install ping application
        NS_LOG_INFO ("Debug mode. Installing ping application");
        Ipv4Address destAddr = serverAddr;
        V4PingHelper ping(destAddr);
        clientApps = ping.Install(clientNode);
        clientApps.Start (Seconds (1.0));
        clientApps.Stop (Seconds (simTime));
    }
    else {
        NS_LOG_INFO ("Normal mode");
        uint16_t port;
        std::string selectedProtocol;
        if (tcp) {
            selectedProtocol = "ns3::TcpSocketFactory";
            port = 9; 
            NS_LOG_INFO ("Installing TCP app");
        }
        else {
            selectedProtocol = "ns3::UdpSocketFactory";
            port = 4000;
            NS_LOG_INFO ("Installing UDP app");
        }
        // Install the apps
        InetSocketAddress destAddr = InetSocketAddress (serverAddr, port);
        InetSocketAddress sinkAddr = InetSocketAddress (Ipv4Address::GetAny(), port);
        OnOffHelper onOff (selectedProtocol, destAddr);
        onOff.SetConstantRate(DataRate(dataRate));
        sourceApps = onOff.Install (clientNode);
        sourceApps.Start (Seconds (1.0));
        sourceApps.Stop (Seconds (simTime));

        PacketSinkHelper sink (selectedProtocol, sinkAddr);
        sinkApps = sink.Install (serverNode);
        sinkApps.Start (Seconds (1.0));
        sinkApps.Stop (Seconds (simTime));
    }

    // enable PCAP on the channel
    phy.EnablePcapAll("result");

    // Set distance between nodes
    Ptr<ListPositionAllocator>pstAlloc = CreateObject <ListPositionAllocator>();
    for (int i = 0; i < nodeCount; i++) {
        pstAlloc->Add(Vector(i*distance, 0, 0));
    }
    MobilityHelper mobility;
    mobility.SetPositionAllocator(pstAlloc);
    // mobility.SetPositionAllocator("ns3::GridPositionAllocator",
    //                               "MinX", DoubleValue (0.0),
    //                               "MinY", DoubleValue (0.0),
    //                               "DeltaX", DoubleValue (distance),
    //                               "GridWidth", UintegerValue (nodeCount),
    //                               "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor= flowHelper.InstallAll();

    // Run simulation
    NS_LOG_INFO ("Run Simulation.");
    Simulator::Stop(Seconds(simTime + 5));
    Simulator::Run ();

    // Calculate statistical numbers
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    double offeredLoad = stats[1].txBytes * 8.0 / (stats[1].timeLastTxPacket.GetSeconds () - stats[1].timeFirstTxPacket.GetSeconds ()) / 1000000;
    double throughput = stats[1].rxBytes * 8.0 / (stats[1].timeLastRxPacket.GetSeconds () - stats[1].timeFirstRxPacket.GetSeconds ()) / 1000000;
    uint32_t totalPacketsThr = DynamicCast<PacketSink> (sinkApps.Get(0))->GetTotalRx ();
    double avgGoodput = totalPacketsThr * 8 / (simTime * 1000000.0); //Mbit/s

    // Display the statistics
    std::cout << std::endl << "*** Flow monitor statistics (distance = " << distance
                << ")***" << std::endl;
    std::cout << "  Tx Packets/Bytes:   " << stats[1].txPackets
                << " / " << stats[1].txBytes << std::endl;
    std::cout << "  Offered Load: " << offeredLoad << " Mbps" << std::endl;
    std::cout << "  Rx Packets/Bytes:   " << stats[1].rxPackets
                << " / " << stats[1].rxBytes << std::endl;
    std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
    std::cout << "  Average Goodput: " << avgGoodput << " Mbps" << std::endl;

    monitor->SerializeToXmlFile("throughputAnalysis.csv", true, true);

    // Stop simulation
    Simulator::Destroy ();
    NS_LOG_INFO ("Done.");
}
