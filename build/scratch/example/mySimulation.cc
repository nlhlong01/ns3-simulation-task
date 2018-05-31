#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("Wns3Example");

int main (int argc, char *argv[]) {
    //set default attribute values
    std::string protocol = "ns3::UdpSocketFactory";
    uint16_t port = 5009;

    //parse command-line arguments
    CommandLine cmd;
    cmd.AddValue("transport", "TypeIdforsocketfactory", protocol ) ;
    cmd.Parse(argc, argv);

    //Configure the topology
    NodeContainer nodes ;
    nodes.Create(2);

    PointToPointHelper pointToPoint ;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    //Add (Internet) stack to nodes
    InternetStackHelper stack ;
    stack.Install(nodes);

    //Configure IP addressing and routing
    Ipv4AddressHelper address ;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces ;
    interfaces = address.Assign(devices);

    // Add and configure Applications
    InetSocketAddress destAddr = InetSocketAddress(Ipv4Address("10.1.1.1"), port);
    InetSocketAddress sinkAddr = InetSocketAddress(Ipv4Address::GetAny(), port);

    OnOffHelper onOff(protocol, destAddr);
    onOff.SetConstantRate(DataRate("1Mb/s"));
    PacketSinkHelper sink (protocol, sinkAddr);

    ApplicationContainer serverApps = onOff.Install(nodes.Get(1));
    ApplicationContainer clientApps = sink.Install(nodes.Get(0));

    serverApps.Start(Seconds(1.0));
    serverApps.Start(Seconds(11.0));

    clientApps.Start(Seconds(1.0));
    clientApps.Start(Seconds(12.0));

    //Run simulator
    Simulator::Stop(Seconds(13.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
