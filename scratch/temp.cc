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
    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Set up a 4-node wireless adhoc 802.11g network.
    NodeContainer nodes;
    nodes.Create(4);
    Ptr<Node> clientNode = nodes.Get(0);
    Ptr<Node> serverNode = nodes.Get(3);
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
    phy.EnablePcapAll("debug");

    // Set up MAC layer
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Configure wifi net device
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");
    devices = wifi.Install(phy, mac, nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);
    Ipv4AddressHelper addresses ;
    addresses.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    // Ipv4Address clientAddr = interfaces.GetAddress(0); 
    // Ipv4Address serverAddr = interfaces.GetAddress(3);  
    
    // Set OLSR as the routing protocol of the network
    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper list;
    list.Add (staticRouting, 0);
    list.Add (olsr, 10);
    stack.SetRoutingHelper (list);

    // Set distance between nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator");
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set up application containers
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    // Install ping application
    Ipv4Address destAddr = interfaces.GetAddress(0);
    V4PingHelper ping(destAddr);
    clientApps = ping.Install(nodes.Get(0));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (5.0));

    NS_LOG_INFO ("Run Simulation.");
    Simulator::Run ();
    Simulator::Destroy ();
    NS_LOG_INFO ("Done.");
}
