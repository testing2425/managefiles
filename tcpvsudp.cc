#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpVsUdpBottleneck");

// -------- TCP CWND TRACE --------
void CwndTracer(uint32_t oldCwnd, uint32_t newCwnd)
{
    std::cout << Simulator::Now().GetSeconds()
              << " s : CWND " << oldCwnd
              << " -> " << newCwnd << " bytes" << std::endl;
}

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // ---------- NODES ----------
    NodeContainer clients, router, server;
    clients.Create(2);
    router.Create(1);
    server.Create(1);

    // ---------- MOBILITY (for NetAnim) ----------
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(clients);
    mobility.Install(router);
    mobility.Install(server);

    // ---------- LINKS ----------
    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    accessLink.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));
    bottleneck.SetQueue("ns3::DropTailQueue<Packet>",
                        "MaxSize", QueueSizeValue(QueueSize("5p")));

    NetDeviceContainer d02 = accessLink.Install(clients.Get(0), router.Get(0));
    NetDeviceContainer d12 = accessLink.Install(clients.Get(1), router.Get(0));
    NetDeviceContainer d23 = bottleneck.Install(router.Get(0), server.Get(0));

    // ---------- INTERNET ----------
    InternetStackHelper stack;
    stack.Install(clients);
    stack.Install(router);
    stack.Install(server);

    // ---------- IP ADDRESSING ----------
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    address.Assign(d02);

    address.SetBase("10.1.2.0", "255.255.255.0");
    address.Assign(d12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer serverIf = address.Assign(d23);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ---------- TCP APPLICATION ----------
    uint16_t tcpPort = 9000;

    BulkSendHelper tcpClient("ns3::TcpSocketFactory",
        InetSocketAddress(serverIf.GetAddress(1), tcpPort));
    tcpClient.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer tcpApps = tcpClient.Install(clients.Get(0));
    tcpApps.Start(Seconds(1.0));
    tcpApps.Stop(Seconds(10.0));

    PacketSinkHelper tcpSink("ns3::TcpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer tcpSinkApp = tcpSink.Install(server.Get(0));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(10.0));

    // Trace TCP CWND
    Ptr<Socket> tcpSocket =
        Socket::CreateSocket(clients.Get(0), TcpSocketFactory::GetTypeId());
    tcpSocket->TraceConnectWithoutContext(
        "CongestionWindow", MakeCallback(&CwndTracer));

    // ---------- UDP APPLICATION ----------
    uint16_t udpPort = 8000;

    OnOffHelper udpClient("ns3::UdpSocketFactory",
        InetSocketAddress(serverIf.GetAddress(1), udpPort));
    udpClient.SetAttribute("DataRate", StringValue("20Mbps"));
    udpClient.SetAttribute("PacketSize", UintegerValue(1472));
    udpClient.SetAttribute("OnTime",
        StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpClient.SetAttribute("OffTime",
        StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer udpApps = udpClient.Install(clients.Get(1));
    udpApps.Start(Seconds(1.0));
    udpApps.Stop(Seconds(10.0));

    PacketSinkHelper udpSink("ns3::UdpSocketFactory",
        InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer udpSinkApp = udpSink.Install(server.Get(0));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(10.0));

    // ---------- FLOW MONITOR ----------
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // ---------- NETANIM ----------
    AnimationInterface anim("scratch/tcp-vs-udp.xml");

    anim.SetConstantPosition(clients.Get(0), 5, 10);
    anim.SetConstantPosition(clients.Get(1), 5, 20);
    anim.SetConstantPosition(router.Get(0), 25, 15);
    anim.SetConstantPosition(server.Get(0), 45, 15);

    anim.UpdateNodeDescription(clients.Get(0), "TCP Client");
    anim.UpdateNodeDescription(clients.Get(1), "UDP Client");
    anim.UpdateNodeDescription(router.Get(0), "Bottleneck Router");
    anim.UpdateNodeDescription(server.Get(0), "Server");

    anim.UpdateNodeColor(clients.Get(0), 0, 255, 0);
    anim.UpdateNodeColor(clients.Get(1), 255, 255, 0);
    anim.UpdateNodeColor(router.Get(0), 255, 0, 0);
    anim.UpdateNodeColor(server.Get(0), 0, 0, 255);

    // ---------- RUN ----------
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // ---------- FLOW RESULTS ----------
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    for (auto &flow : monitor->GetFlowStats())
    {
        auto t = classifier->FindFlow(flow.first);
        double throughput =
            flow.second.rxBytes * 8.0 /
            (flow.second.timeLastRxPacket.GetSeconds()
            - flow.second.timeFirstTxPacket.GetSeconds()) / 1e6;

        std::cout << "Flow " << flow.first
                  << " (" << t.sourceAddress
                  << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Throughput: " << throughput
                  << " Mbps, Lost Packets: "
                  << flow.second.lostPackets << "\n";
    }

    Simulator::Destroy();
    return 0;
}
