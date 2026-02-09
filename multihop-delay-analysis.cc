#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiHopDelayAnalysis");

int main(int argc, char *argv[])
{
    std::string rate01 = "10Mbps";  
    std::string delay01 = "5ms";

    std::string rate12 = "5Mbps";  
    std::string delay12 = "10ms";

    std::string rate23 = "2Mbps";  
    std::string delay23 = "15ms";

    uint32_t packetSize = 1024;
    double simTime = 8.0;

    CommandLine cmd;
    cmd.AddValue("packetSize", "Size of UDP packet", packetSize);
    cmd.Parse(argc, argv);

    // CREATE 4 NODES
    NodeContainer nodes;
    nodes.Create(4); 

    // INSTALL LINKS

    // Link: 0 → 1
    PointToPointHelper p2p01;
    p2p01.SetDeviceAttribute("DataRate", StringValue(rate01));
    p2p01.SetChannelAttribute("Delay", StringValue(delay01));
    NetDeviceContainer d01 = p2p01.Install(nodes.Get(0), nodes.Get(1));

    // Link: 1 → 2
    PointToPointHelper p2p12;
    p2p12.SetDeviceAttribute("DataRate", StringValue(rate12));
    p2p12.SetChannelAttribute("Delay", StringValue(delay12));
    NetDeviceContainer d12 = p2p12.Install(nodes.Get(1), nodes.Get(2));

    // Link: 2 → 3
    PointToPointHelper p2p23;
    p2p23.SetDeviceAttribute("DataRate", StringValue(rate23));
    p2p23.SetChannelAttribute("Delay", StringValue(delay23));
    NetDeviceContainer d23 = p2p23.Install(nodes.Get(2), nodes.Get(3));

    // STACK + ROUTING
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper addr;

    // 0–1 subnet
    addr.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i01 = addr.Assign(d01);

    // 1–2 subnet
    addr.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i12 = addr.Assign(d12);

    // 2–3 subnet
    addr.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i23 = addr.Assign(d23);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // APPLICATIONS
    uint16_t port = 9;

    // Server on Node 3
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(3));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // Client on Node 0 sending to Node 3
    UdpEchoClientHelper client(i23.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    // TRACING (.tr file)
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("scratch/multihop.tr");
    p2p01.EnableAsciiAll(stream);
    p2p12.EnableAsciiAll(stream);
    p2p23.EnableAsciiAll(stream);

    // NETANIM (.xml file)
    AnimationInterface anim("scratch/multihop.xml");

    anim.SetConstantPosition(nodes.Get(0), 5, 10);   // Sender
    anim.SetConstantPosition(nodes.Get(1), 20, 10);  // Hop 1
    anim.SetConstantPosition(nodes.Get(2), 35, 10);  // Hop 2
    anim.SetConstantPosition(nodes.Get(3), 50, 10);  // Receiver

    anim.UpdateNodeDescription(nodes.Get(0), "Node 0 (Client)");
    anim.UpdateNodeDescription(nodes.Get(1), "Node 1 (Router 1)");
    anim.UpdateNodeDescription(nodes.Get(2), "Node 2 (Router 2)");
    anim.UpdateNodeDescription(nodes.Get(3), "Node 3 (Server)");

    anim.UpdateNodeColor(nodes.Get(0), 0, 255, 0);
    anim.UpdateNodeColor(nodes.Get(1), 255, 255, 0);
    anim.UpdateNodeColor(nodes.Get(2), 255, 128, 0);
    anim.UpdateNodeColor(nodes.Get(3), 0, 0, 255);

    //SIMULATION RUN
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
