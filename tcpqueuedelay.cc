#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;
using namespace std;

/* ---------- GLOBAL FLAGS ---------- */
bool firstDropPrinted = false;
double firstDropTime = 0.0;

/* ---------- QUEUEDISC DROP TRACE ---------- */
void
QueueDiscDropTrace(Ptr<const QueueDiscItem> item)
{
    if (!firstDropPrinted)
    {
        firstDropTime = Simulator::Now().GetSeconds();
        cout << "\n[FIRST TCP PACKET LOSS]\n"
             << "Time = " << firstDropTime << " s\n"
             << "Packet Size = "
             << item->GetPacket()->GetSize()
             << " bytes\n"
             << endl;

        firstDropPrinted = true;
    }
}

int
main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    /* ---------- NODES ---------- */
    NodeContainer clients, router, server;
    clients.Create(2);
    router.Create(1);
    server.Create(1);

    /* ---------- MOBILITY ---------- */
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.InstallAll();

    /* ---------- LINKS ---------- */
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    accessLink.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer d0r = accessLink.Install(clients.Get(0), router.Get(0));
    NetDeviceContainer d1r = accessLink.Install(clients.Get(1), router.Get(0));
    NetDeviceContainer drs = bottleneck.Install(router.Get(0), server.Get(0));

    /* ---------- INTERNET STACK ---------- */
    InternetStackHelper stack;
    stack.InstallAll();

    /* ---------- IP ADDRESSING ---------- */
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    address.Assign(d0r);

    address.SetBase("10.1.2.0", "255.255.255.0");
    address.Assign(d1r);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer serverIf = address.Assign(drs);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    /* ---------- TRAFFIC CONTROL (CRITICAL FIX) ---------- */
    TrafficControlHelper tch;

    // Remove default FqCoDel
    tch.Uninstall(drs.Get(0));

    // Install small FIFO queue to force drops
    tch.SetRootQueueDisc(
        "ns3::PfifoFastQueueDisc",
        "MaxSize", QueueSizeValue(QueueSize("5p"))
    );

    QueueDiscContainer qdiscs = tch.Install(drs.Get(0));

    // Connect drop trace
    qdiscs.Get(0)->TraceConnectWithoutContext(
        "Drop", MakeCallback(&QueueDiscDropTrace));

    /* ---------- TCP SERVER ---------- */
    uint16_t port = 50000;
    PacketSinkHelper sink(
        "ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), port)
    );

    ApplicationContainer serverApp = sink.Install(server.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(3.0));

    /* ---------- TCP CLIENTS ---------- */
    OnOffHelper client(
        "ns3::TcpSocketFactory",
        Address(InetSocketAddress(serverIf.GetAddress(1), port))
    );

    client.SetAttribute("DataRate", StringValue("20Mbps"));
    client.SetAttribute("PacketSize", UintegerValue(1472));
    client.SetAttribute("OnTime",
        StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime",
        StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApps = client.Install(clients);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(2.0));

    /* ---------- FLOW MONITOR ---------- */
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    /* ---------- NETANIM ---------- */
    AnimationInterface anim("scratch/tcp-bottleneck.xml");

    anim.SetConstantPosition(clients.Get(0), 5, 10);
    anim.SetConstantPosition(clients.Get(1), 5, 20);
    anim.SetConstantPosition(router.Get(0), 25, 15);
    anim.SetConstantPosition(server.Get(0), 45, 15);

    /* ---------- RUN ---------- */
    Simulator::Stop(Seconds(3.0));
    Simulator::Run();

    /* ---------- FLOW RESULTS ---------- */
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    for (auto &flow : monitor->GetFlowStats())
    {
        auto t = classifier->FindFlow(flow.first);
        cout << "Flow " << flow.first
             << " (" << t.sourceAddress
             << " -> " << t.destinationAddress << ")\n";
        cout << "  Lost Packets: "
             << flow.second.lostPackets << "\n";
    }

    if (firstDropPrinted)
    {
        cout << "\nFirst packet loss occurred at "
             << firstDropTime << " seconds\n";
    }
    else
    {
        cout << "\nNo packet loss occurred\n";
    }

    Simulator::Destroy();
    return 0;
}
