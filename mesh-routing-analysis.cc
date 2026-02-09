/*
 * ns-3 Exercise 2: Mesh Topology and Routing Analysis
 *
 * Topology:
 *    Node 0 (Client A) --> Node 1 (Router R) --> Node 2 (Server B)
 *
 * Link A-R: 10Mbps / 5ms
 * Link R-B: 5Mbps  / 10ms  (Slower link)
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("MeshRoutingAnalysis");

int main(int argc, char *argv[])
{
    // -------------------------------------------------------------
    // 1. CONFIGURATION
    // -------------------------------------------------------------
    std::string dataRateAR = "10Mbps";
    std::string delayAR    = "5ms";

    std::string dataRateRB = "5Mbps"; // slower link
    std::string delayRB    = "10ms";

    uint32_t packetSize = 1024;
    double simTime = 5.0;

    CommandLine cmd;
    cmd.AddValue("simTime", "Simulation duration", simTime);
    cmd.Parse(argc, argv);

    // -------------------------------------------------------------
    // 2. CREATE NODES
    // -------------------------------------------------------------
    NodeContainer nodes;
    nodes.Create(3);   // 0=A, 1=R, 2=B

    // -------------------------------------------------------------
    // 3. CREATE LINKS
    // -------------------------------------------------------------

    // Link A → R
    PointToPointHelper p2pAR;
    p2pAR.SetDeviceAttribute("DataRate", StringValue(dataRateAR));
    p2pAR.SetChannelAttribute("Delay", StringValue(delayAR));
    NetDeviceContainer dAR = p2pAR.Install(nodes.Get(0), nodes.Get(1));

    // Link R → B
    PointToPointHelper p2pRB;
    p2pRB.SetDeviceAttribute("DataRate", StringValue(dataRateRB));
    p2pRB.SetChannelAttribute("Delay", StringValue(delayRB));
    NetDeviceContainer dRB = p2pRB.Install(nodes.Get(1), nodes.Get(2));

    // -------------------------------------------------------------
    // 4. INTERNET + ROUTING
    // -------------------------------------------------------------
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper addressA;
    addressA.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iAR = addressA.Assign(dAR);

    Ipv4AddressHelper addressB;
    addressB.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iRB = addressB.Assign(dRB);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // -------------------------------------------------------------
    // 5. APPLICATIONS
    // -------------------------------------------------------------
    uint16_t port = 7;
    Ipv4Address serverAddress = iRB.GetAddress(1); // Node 2

    // Server on Node 2
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // Client on Node 0 → sends to Node 2
    UdpEchoClientHelper echoClient(serverAddress, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    // -------------------------------------------------------------
    // 6. TRACING (ASCII + NetAnim)
    // -------------------------------------------------------------

    // FIXED: Single trace stream for both links
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("scratch/mesh-routing-analysis.tr");

    p2pAR.EnableAsciiAll(stream);
    p2pRB.EnableAsciiAll(stream);

    // NetAnim XML output
    AnimationInterface anim("scratch/mesh-routing-analysis.xml");

    // Node positions
    anim.SetConstantPosition(nodes.Get(0), 5.0, 10.0);   // A
    anim.SetConstantPosition(nodes.Get(1), 20.0, 10.0);  // R
    anim.SetConstantPosition(nodes.Get(2), 35.0, 10.0);  // B

    // Node labels
    anim.UpdateNodeDescription(nodes.Get(0), "Client A");
    anim.UpdateNodeDescription(nodes.Get(1), "Router R");
    anim.UpdateNodeDescription(nodes.Get(2), "Server B");

    // Node colors
    anim.UpdateNodeColor(nodes.Get(0), 0, 255, 0);   // Green
    anim.UpdateNodeColor(nodes.Get(1), 255, 255, 0); // Yellow
    anim.UpdateNodeColor(nodes.Get(2), 0, 0, 255);   // Blue

    // -------------------------------------------------------------
    // 7. RUN SIMULATION
    // -------------------------------------------------------------
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
