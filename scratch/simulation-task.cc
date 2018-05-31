#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/config.h"
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
    std::string app = "tcp";

    CommandLine cmd;
    cmd.AddValue ("mode", "Simulation mode (debug, normal)", mode);
    cmd.AddValue ("app", "Application (udp, tcp)", mode);
    cmd.AddValue ("nodeNum", "Number of nodes", nodeNum);
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
    addresses.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = addresses.Assign(devices);
    // Ipv4Address clientAddr = interfaces.GetAddress(0); 
    Ipv4Address serverAddr = interfaces.GetAddress(nodeNum-1);    

    // Set distance between nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (5.0),
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
        clientApps.Stop (Seconds (5.0));
    }

    else {
        NS_LOG_INFO ("Normal mode");
        if (app == "udp") {
            // Install UDP apps
            NS_LOG_INFO ("Installing UDP app");
            uint16_t port = 4000;
            UdpServerHelper udpServer(port);
            serverApps = udpServer.Install(serverNode);
            serverApps.Start (Seconds (1.0));
            serverApps.Stop (Seconds (5.0));
            // uint32_t MaxPacketSize = 1472;  
            UdpTraceClientHelper udpClient(serverAddr, port, "");
            // udpClient.SetAttribute ("MaxPacketSize", UintegerValue (MaxPacketSize));
            clientApps = udpClient.Install (clientNode);
            clientApps.Start (Seconds (1.0));
            clientApps.Stop (Seconds (5.0));
        }
        else {
            // Install TCP apps
            NS_LOG_INFO ("Installing TCP app");
            uint16_t port = 9;  
            BulkSendHelper source ("ns3::TcpSocketFactory", 
                                    InetSocketAddress (interfaces.GetAddress(1), port));
            source.SetAttribute ("MaxBytes", UintegerValue (0));
            clientApps = source.Install (clientNode);
            clientApps.Start (Seconds (1.0));
            clientApps.Stop (Seconds (5.0));
            PacketSinkHelper sink ("ns3::TcpSocketFactory",
                                    InetSocketAddress (Ipv4Address::GetAny (), port));
            serverApps = sink.Install (serverNode);
            serverApps.Start (Seconds (1.0));
            serverApps.Stop (Seconds (5.0));
        }
    }

    // // Set up Flow Monitor module 
    // Ptr<FlowMonitor> flowMonitor;
    // FlowMonitorHelper flowHelper;
    // flowMonitor = flowHelper.Install(nodes);
    // flowMonitor->SerializeToXmlFile("throughputAnalysis.csv", true, true);

    phy.EnablePcapAll("debug");

    NS_LOG_INFO ("Run Simulation.");
    Simulator::Stop(Seconds(10.0));
    Simulator::Run ();
    Simulator::Destroy ();
    NS_LOG_INFO ("Done.");
}
