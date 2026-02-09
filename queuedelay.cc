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

/* ---------- QUEUE DISC TRACES ---------- */
void EnqueueTrace(Ptr<const QueueDiscItem> item)
{
    cout << Simulator::Now().GetSeconds()
         << " s [ENQUEUE] PacketSize="
         << item->GetPacket()->GetSize()
         << " bytes" << endl;
}

void DequeueTrace(Ptr<const QueueDiscItem> item)
{
    cout << Simulator::Now().GetSeconds()
         << " s [DEQUEUE] PacketSize="
         << item->GetPacket()->GetSize()
         << " bytes" << endl;
}

void DropTrace(Ptr<const QueueDiscItem> item)
{
    double now = Simulator::Now().GetSeconds();

    cout << now
         << " s [DROP] PacketSize="
         << item->GetPacket()->GetSize()
         << " bytes" << endl;

    if (!firstDropPrinted)
    {
        firstDropPrinted = true;
        firstDropTime = now;

        cout << "\n[FIRST PACKET DROP OCCURRED]\n"
             << "Time = " << firstDropTime << " seconds\n\n";
    }
}

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    /* ---------- 1. NODES ---------- */
    NodeContainer clients, router, server;
    clients.Create(2);
    router.Create(1);
    server.Create(1);

    /* ---------- 2. MOBILITY ---------- */
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.InstallAll();

    /* ---------- 3. LINKS ---------- */
    PointToPointHelper access;
    access.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    access.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer d0r = access.Install(clients.Get(0), router.Get(0));
    NetDeviceContainer d1r = access.Install(clients.Get(1), router.Get(0));
    NetDeviceContainer drs = bottleneck.Install(router.Get(0), server.Get(0));

    /* ---------- 4. INTERNET ---------- */
    InternetStackHelper stack;
    stack.InstallAll();

    /* ---------- 5. IP ADDRESSING ---------- */
    Ipv4AddressHelper addr;

    addr.SetBase("10.1.1.0", "255.255.255.0");
    addr.Assign(d0r);

    addr.SetBase("10.1.2.0", "255.255.255.0");
    addr.Assign(d1r);

    addr.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer serverIf = addr.Assign(drs);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    /* ---------- 6. TRAFFIC CONTROL ---------- */
    TrafficControlHelper tch;

    // Remove default FqCoDel
    tch.Uninstall(drs.Get(0));

    // Install small FIFO queue
    tch.SetRootQueueDisc(
        "ns3::PfifoFastQueueDisc",
        "MaxSize", QueueSizeValue(QueueSize("5p"))
    );

    QueueDiscContainer qdiscs = tch.Install(drs.Get(0));

    // Connect traces
    qdiscs.Get(0)->TraceConnectWithoutContext(
        "Enqueue", MakeCallback(&EnqueueTrace));
    qdiscs.Get(0)->TraceConnectWithoutContext(
        "Dequeue", MakeCallback(&DequeueTrace));
    qdiscs.Get(0)->TraceConnectWithoutContext(
        "Drop", MakeCallback(&DropTrace));

    /* ---------- 7. UDP APPLICATIONS ---------- */
    uint16_t port1 = 5000;
    uint16_t port2 = 5001;

    OnOffHelper onoff1("ns3::UdpSocketFactory",
        InetSocketAddress(serverIf.GetAddress(1), port1));
    OnOffHelper onoff2("ns3::UdpSocketFactory",
        InetSocketAddress(serverIf.GetAddress(1), port2));

    onoff1.SetAttribute("DataRate", StringValue("20Mbps"));
    onoff1.SetAttribute("PacketSize", UintegerValue(1472));
    onoff1.SetAttribute("OnTime",
        StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime",
        StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    onoff2.SetAttribute("DataRate", StringValue("20Mbps"));
    onoff2.SetAttribute("PacketSize", UintegerValue(1472));
    onoff2.SetAttribute("OnTime",
        StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime",
        StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer app1 = onoff1.Install(clients.Get(0));
    ApplicationContainer app2 = onoff2.Install(clients.Get(1));

    app1.Start(Seconds(1.0));
    app2.Start(Seconds(1.0));
    app1.Stop(Seconds(2.0));
    app2.Stop(Seconds(2.0));

    /* ---------- 8. FLOW MONITOR ---------- */
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    /* ---------- 9. NETANIM ---------- */
    AnimationInterface anim("scratch/queuedelayudp.xml");

    anim.SetConstantPosition(clients.Get(0), 5, 10);
    anim.SetConstantPosition(clients.Get(1), 5, 20);
    anim.SetConstantPosition(router.Get(0), 25, 15);
    anim.SetConstantPosition(server.Get(0), 45, 15);

    /* ---------- 10. RUN ---------- */
    Simulator::Stop(Seconds(3.0));
    Simulator::Run();

    /* ---------- 11. FLOW RESULTS ---------- */
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    for (auto &flow : monitor->GetFlowStats())
    {
        auto t = classifier->FindFlow(flow.first);
        cout << "\nFlow " << flow.first
             << " (" << t.sourceAddress
             << " -> " << t.destinationAddress << ")\n";
        cout << "Lost Packets: "
             << flow.second.lostPackets << endl;
    }

    cout << "\nFIRST PACKET DROP TIME = "
         << firstDropTime << " seconds\n";

    Simulator::Destroy();
    return 0;
}
