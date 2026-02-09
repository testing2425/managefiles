#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetAnimVisualizationScript");

int main(int argc, char *argv[])
{
    // 1. DEFAULT PARAMETERS
    std::string dataRate = "10Mbps";
    std::string delay = "10ms";
    uint32_t packetSize = 1024;
    uint32_t numPackets = 1;
    double simulationStopTime = 5.0;

    // 2. COMMAND-LINE INPUT
    CommandLine cmd;
    cmd.AddValue("dataRate", "Data rate of the link", dataRate);
    cmd.AddValue("delay", "Propagation delay of the link", delay);
    cmd.AddValue("packetSize", "Size of packets", packetSize);
    cmd.AddValue("numPackets", "Number of packets", numPackets);
    cmd.Parse(argc, argv);

    // 3. CREATE TWO NODES
    NodeContainer nodes;
    nodes.Create(2);

    // 4. POINT-TO-POINT LINK
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // 5. INTERNET STACK
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 6. UDP SERVER ON NODE 1
    uint16_t port = 7;
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationStopTime));

    // 7. UDP CLIENT ON NODE 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationStopTime));

    // 8. NetAnim Visualization
    AnimationInterface anim("scratch/netanim-exercise.xml");

    // Set node positions in the visualization window
    anim.SetConstantPosition(nodes.Get(0), 5.0, 5.0);
    anim.SetConstantPosition(nodes.Get(1), 20.0, 5.0);

    // Add descriptions (labels)
    anim.UpdateNodeDescription(nodes.Get(0), "Client");
    anim.UpdateNodeDescription(nodes.Get(1), "Server");

    // Add colors (RGB)
    anim.UpdateNodeColor(nodes.Get(0), 0, 255, 0);   // Green
    anim.UpdateNodeColor(nodes.Get(1), 0, 0, 255);   // Blue

    // 9. RUN SIMULATION
    Simulator::Stop(Seconds(simulationStopTime));
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation complete. Open netanim-exercise.xml in NetAnim.");
    return 0;
}
