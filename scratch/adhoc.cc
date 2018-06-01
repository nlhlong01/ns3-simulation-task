
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/v4ping.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-routing-protocol.h"
#include "ns3/v4ping-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimpleAdhocGrid");



void ReceivePacket (Ptr<Socket> socket)
{
  while (socket->Recv ())
    {
      NS_LOG_UNCOND ("Received one packet!");
    }
}

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize,
                             uint32_t pktCount, Time pktInterval )
{
  if (pktCount > 0)
    {
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule (pktInterval, &GenerateTraffic,
                           socket, pktSize,pktCount - 1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}




int main (int argc, char *argv[])
{
  //std::string phyMode ("ErpOfdmRate54Mbps");
  double distance = 50;  // m
  uint32_t numNodes = 4;
  //uint32_t packetSize = 1024;
  //uint32_t numPackets = 10;
  //double interval = 1.0;
  // bool tracing = false;
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;
  //cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("numNodes", "number of nodes", numNodes);
  cmd.Parse (argc, argv);



  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);



  NodeContainer c;
  c.Create (numNodes);
  WifiHelper wifi;
  if (verbose)
    {
      wifi.EnableLogComponents ();  // Turn on all Wifi logging
    }

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (1) );
  //wifiPhy.Set ("txGain", DoubleValue (1) );
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);



  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());


  // Add an upper mac and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
  wifi.SetRemoteStationManager ("ns3::MinstrelWifiManager");//, "DataMode",StringValue (phyMode), "ControlMode",StringValue (phyMode));


  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);



  // distance to be set later
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (distance),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (c);


  //Enable OLSR
  OlsrHelper olsr;
  //Ipv4StaticRoutingHelper staticRouting;
  
  Ipv4ListRoutingHelper list;
  //list.Add (staticRouting, 0);
  list.Add (olsr, 10);



  InternetStackHelper internet;
  internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (c);




  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);



  ////// PING ///////////
  V4PingHelper icmp(i.GetAddress (3));  // destination address of icmp packets is node 3
  ApplicationContainer pingapp = icmp.Install(c.Get (0));  // Source of icmp packets
  pingapp.Start (Seconds (11.0));
  pingapp.Stop (Seconds (15.0));






// Create a BulkSendApplication and install it on node 0////
//
  uint16_t tcpport = 9;  // well-known echo port number


  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (i.GetAddress (3), tcpport));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (10000));
  ApplicationContainer sourceApps = source.Install (c.Get (0));
  sourceApps.Start (Seconds (6.0));
  sourceApps.Stop (Seconds (10.0));

//
// Create a PacketSinkApplication and install it on node 3
//
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), tcpport));
  ApplicationContainer sinkApps = sink.Install (c.Get (3));
  sinkApps.Start (Seconds (6.0));
  sinkApps.Stop (Seconds (11.0));



//////////////////////////////////////////


 // Create one udpServer applications on node one.
//
  uint16_t port = 4000;
  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (c.Get (3));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (6.0));

//
// Create one UdpClient application to send UDP datagrams from node zero to
// node one.
//
  uint32_t MaxPacketSize = 1472;
  Time interPacketInterval = Seconds (0.05);
  //uint32_t maxPacketCount = 320;
  UdpTraceClientHelper client (i.GetAddress(3), port,"");
  //client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  //client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("MaxPacketSize", UintegerValue (MaxPacketSize));
  apps = client.Install (c.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (6.0));




 // UdpEchoServerHelper echoServer (9);

 // ApplicationContainer serverApps = echoServer.Install (c.Get (3));
 // serverApps.Start (Seconds (1.0));
 // serverApps.Stop (Seconds (10.0));

 // UdpEchoClientHelper echoClient (i.GetAddress (3), 9);
  //echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
 // echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
 // echoClient.SetAttribute ("PacketSize", UintegerValue (1518));
//
 // ApplicationContainer clientApps = echoClient.Install (c.Get (0));
 // clientApps.Start (Seconds (2.0));
 // clientApps.Stop (Seconds (10.0));



  if (tracing == true)
    {
      wifiPhy.EnablePcapAll ("adhoc");
    }


  Simulator::Stop(Seconds(16.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}


