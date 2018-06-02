
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/v4ping.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-routing-protocol.h"
#include "ns3/v4ping-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-helper.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HoangViet-Simulation");


int main (int argc, char *argv[])
{
  double distance = 5;  // distance of nodes , in "m"
  //uint32_t numNodes = 4; // number of nodes
  double udpStart = 1.0; // Start time of UDP App
  double udpStop = 5.0; // Stop time of UDP App
  double tcpStart = 6.0; // Start time of TCP App
  double tcpStop = 10.0;  // Stop time of TCP App
  double pingStart = 11.0;  // Start time of Ping App
  double pingStop = 15.0;  // Stop time of Ping App
  double simStop = 30.0;  // Stop time of Simulator
  bool verbose = false;
  bool enPing = false;
  bool enTcp = false;
  bool enUdp = false;



  CommandLine cmd;
  cmd.AddValue ("distance", "distance (m)", distance);
  //cmd.AddValue ("numNodes", "number of nodes", numNodes);
  cmd.AddValue ("enPing", "enable Ping Application", enPing);
  cmd.AddValue ("enTcp", "enable TCP Application", enTcp);
  cmd.AddValue ("enUdp", "enable UDP Application", enUdp);
  cmd.AddValue ("udpStart", "Start time of UDP", udpStart);
  cmd.AddValue ("udpStop", "Stop time of UDP", udpStop);
  cmd.AddValue ("tcpStart", "Start time of TCP", tcpStart);
  cmd.AddValue ("tcpStop", "Stop time of TCP", tcpStop);
  cmd.AddValue ("pingStart", "Start time of Ping", pingStart);
  cmd.AddValue ("pingStop", "Stop time of Ping", udpStop);
  cmd.AddValue ("simStop", "Stop time of Simulator", simStop);  

  cmd.Parse (argc, argv);



  //Time::SetResolution (Time::NS);
 // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  //LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);


  // Create 4 Nodes 
  NodeContainer c;
  c.Create (4);

  
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
  wifi.SetRemoteStationManager ("ns3::MinstrelWifiManager");

  if (verbose)
    {
      wifi.EnableLogComponents ();  // Turn on all Wifi logging
    }
  
  // Configure Physical Layer Properties
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue(1.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue(1.0));
  wifiPhy.Set ("RxGain", DoubleValue (1.0));
  wifiPhy.Set ("TxGain", DoubleValue (1.0));
  wifiPhy.Set ("ChannelWidth", UintegerValue(20));

  // Setting up Wifi Channel
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());


  // Add an upper mac and disable rate control
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);



  // Place 4 nodes in a row, the distance between each node is the value of parameter "distance" ( can be change in ./waf --run)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject <ListPositionAllocator>();
  positionAlloc ->Add(Vector(0, 0, 0)); // place node 0 at origin
  positionAlloc ->Add(Vector(distance, 0, 0)); // place node 1 at "1*distance" (m) from node 0
  positionAlloc ->Add(Vector(2*distance, 0, 0)); // place node 2 at "2*distance" (m) from node 0
  positionAlloc ->Add(Vector(3*distance, 0, 0)); // place node 3 at "3*distance" (m) from node 0
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(c);



  //Enable OLSR
  OlsrHelper olsr;
  
  Ipv4ListRoutingHelper list;
  list.Add (olsr, 10);


  // Install OLSR 
  InternetStackHelper internet;
  internet.SetRoutingHelper (list); 
  internet.Install (c);



  // Assgign Adresses to all Devices
  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);



  // Install Ping Application sent from node 0 to node 3
  if (enPing)
  {
    V4PingHelper icmp(i.GetAddress (3));  // destination address of icmp packets is node 3
    ApplicationContainer pingApp = icmp.Install(c.Get (0));  // Source of icmp packets
    pingApp.Start (Seconds (pingStart));  // Start time of Ping App
    pingApp.Stop (Seconds (pingStop));   // Stop time of Ping App
  }



  
  // Install TCP Application sent from node 0 to node 3
  // Create a BulkSendApplication and install it on node 0
  
  if (enTcp)
  {
    uint16_t tcpport = 9;  // well-known echo port number

    BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (i.GetAddress (3), tcpport)); // socket address of destination, node 3 in this case
    source.SetAttribute ("MaxBytes", UintegerValue (0));  // Set the amount of data to send in bytes.  Zero is unlimited.
    ApplicationContainer tcpSourceApp = source.Install (c.Get (0)); // Install the Source App Container on node 0
    tcpSourceApp.Start (Seconds (tcpStart)); // Start time of tcp source
    tcpSourceApp.Stop (Seconds (tcpStop));  // Stop time of tcp source

    // Create a PacketSinkApplication and install it on node 3

    PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), tcpport));  // socket address of senders
    ApplicationContainer tcpSinkApp = sink.Install (c.Get (3));  // Install the Sink App Container on node 3
    tcpSinkApp.Start (Seconds (tcpStart));	// Start time of tcp sink
    tcpSinkApp.Stop (Seconds (tcpStop));     // Stop time of tcp sink
  }


  if (enUdp)
  {

    uint16_t port = 4000;
    UdpServerHelper server (port);
    ApplicationContainer udpApp = server.Install (c.Get (3));
    udpApp.Start (Seconds (udpStart));
    udpApp.Stop (Seconds (udpStop));


    uint32_t MaxPacketSize = 1472;
    uint32_t MaxPackets = 1000000;
    Time interPacketInterval = Seconds (0.00066);
    UdpClientHelper udpClient (i.GetAddress(3), port);
    udpClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
    udpClient.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));
    udpClient.SetAttribute ("MaxPackets", UintegerValue (MaxPackets));
    udpApp = udpClient.Install (c.Get (0));
    udpApp.Start (Seconds (udpStart));
    udpApp.Stop (Seconds (udpStop));

  } 
 



  // Configure Flow Monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();






  // Enable Pcap file for Wireshark
  wifiPhy.EnablePcapAll ("adhoc");
    

  Simulator::Stop(Seconds(simStop));
  Simulator::Run ();
  Simulator::Destroy ();
  

  // Capture Flow Monitor to Csv File
  flowMonitor->CheckForLostPackets ();
  flowMonitor->SerializeToXmlFile("NameOfFile.csv", true, true);

  return 0;
}


