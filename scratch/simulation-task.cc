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
    int nodeNum = 4;
    std::string mode = "normal";
    std::string protocol = "udp";
    int distance = 5;

    CommandLine cmd;
    cmd.AddValue ("mode", "Simulation mode (debug, normal)", mode);
    cmd.AddValue ("app", "Application (udp, tcp)", mode);
    cmd.AddValue ("nodeNum", "Number of nodes", nodeNum);
    cmd.AddValue ("distance", "Distance between 2 nodes", distance);
    cmd.Parse (argc, argv);

    // Set up a 4-node wireless adhoc 802.11g network.
    NodeContainer nodes;
    nodes.Create(nodeNum);
    Ptr<Node> clientNode = nodes.Get(0);
    Ptr<Node> serverNode = nodes.Get(nodeNum-1);
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
    Ipv4Address serverAddr = interfaces.GetAddress(nodeNum-1);    

    // Set distance between nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (distance),
                                  "GridWidth", UintegerValue (nodeNum),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set up application containers
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    if (mode == "debug") {
        // Install ping application
        NS_LOG_INFO ("Debug mode. Installing ping application");
        Ipv4Address destAddr = serverAddr;
        V4PingHelper ping(destAddr);
        clientApps = ping.Install(clientNode);
        clientApps.Start (Seconds (1.0));
        clientApps.Stop (Seconds (10.0));
    }
    else {
        NS_LOG_INFO ("Normal mode");
        uint16_t port;
        ApplicationContainer sourceApps;
        ApplicationContainer sinkApps;
        std::string selectedProtocol;
        if (protocol == "udp") {
            selectedProtocol = "ns3::UdpSocketFactory";
            port = 4000;
            NS_LOG_INFO ("Installing UDP app");
        }
        else {
            selectedProtocol = "ns3::TcpSocketFactory";
            port = 9; 
            NS_LOG_INFO ("Installing TCP app");
        }
        // Install the apps
        InetSocketAddress destAddr = InetSocketAddress (serverAddr, port);
        InetSocketAddress sinkAddr = InetSocketAddress (Ipv4Address::GetAny(), port);
        OnOffHelper onOff (selectedProtocol, destAddr);
        sourceApps = onOff.Install (clientNode);
        sourceApps.Start (Seconds (1.0));
        sourceApps.Stop (Seconds (10.0));
        PacketSinkHelper sink (selectedProtocol, sinkAddr);
        sourceApps = sink.Install (serverNode);
        sourceApps.Start (Seconds (1.0));
        sourceApps.Stop (Seconds (10.0));
    }

    // enable PCAP on the channel
    phy.EnablePcapAll("debug");

    // Set up Flow Monitor module 
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    NS_LOG_INFO ("Run Simulation.");
    Simulator::Stop(Seconds(10.0));
    Simulator::Run ();
    flowMonitor->SerializeToXmlFile("throughputAnalysis.csv", true, true);
    Simulator::Destroy ();
    NS_LOG_INFO ("Done.");
}
