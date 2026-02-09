#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

//Logging
using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE("PointToPointDelay");

int main(int argc,char *argv[])
{
    //can be changed in compile time
    string dataRate ="10Mbps";
    string delay="10ms";
    uint32_t packetSize=1024;
    uint32_t numPackets=1;

    CommandLine cmd;
    cmd.AddValue("dataRate", "Data rate of the link", dataRate);    
    cmd.AddValue("delay", "Propagation delay", delay);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("numPackets", "Number of packets", numPackets);
    cmd.Parse(argc, argv);


    //Node creation
    NS_LOG_INFO("Creating two nodes");
    NodeContainer nodes;
    nodes.Create(2);//two nodews are created

    //p2p
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));


    //install nic
    NetDeviceContainer devices=pointToPoint.Install(nodes);

    //internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    //ip addr
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0","255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    //Udpserver at node 1
    uint16_t port=7;
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApp=echoServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(5.0));

    //NODE 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1),port);
    echoClient.SetAttribute("MaxPackets",UintegerValue(numPackets));
    echoClient.SetAttribute("Interval",TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize",UintegerValue(packetSize));

    ApplicationContainer clientApps=echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(5.0));

    //pcap
    NS_LOG_INFO("enabling pcap");
    pointToPoint.EnablePcap("scratch/point-to-point",devices.Get(0),true);

    //simulator
    NS_LOG_INFO("Running simulation");
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("SIMULATOR ENDED");
    return 0;

}
