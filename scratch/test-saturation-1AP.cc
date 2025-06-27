#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoamingSingleAp1Mbps");


int main(int argc, char *argv[]) {
    // Logs
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Création des nœuds : 1 AP + 5 STA
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(5);

    // Configuration PHY + canal YANS
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Configuration standard WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    // Configuration des STA (clients mobiles)
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(true));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Configuration de l'AP (fixe)
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobilité de l'AP (fixe à 20m)
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> apPosition = CreateObject<ListPositionAllocator>();
    apPosition->Add(Vector(20.0, 0.0, 0.0));
    mobilityAp.SetPositionAllocator(apPosition);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNode);

    // Mobilité des STA (de 0 à 40m à 2 m/s)
    // MobilityHelper mobilitySta;
    // mobilitySta.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    // Ptr<ListPositionAllocator> staPositions = CreateObject<ListPositionAllocator>();
    // for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
    //     staPositions->Add(Vector(0.0, 1.5 + i * 0.5, 0.0));
    // }
    // mobilitySta.SetPositionAllocator(staPositions);
    // mobilitySta.Install(wifiStaNodes);
    // for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
    //     Ptr<ConstantVelocityMobilityModel> mob = wifiStaNodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
    //     mob->SetVelocity(Vector(2.0, 0.0, 0.0));
    // }


    // Mobilité des STA (fixes à x = 10m, réparties en y)
    MobilityHelper mobilitySta;
    Ptr<ListPositionAllocator> staPositions = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        staPositions->Add(Vector(10.0, 1.5 + i * 0.5, 0.0)); // x = 10m
    }
    mobilitySta.SetPositionAllocator(staPositions);
    mobilitySta.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilitySta.Install(wifiStaNodes);


    // Pile protocolaire
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // Attribution d'adresses IP
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Création du récepteur PacketSink sur l’AP (port 8000)
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 8000));
    ApplicationContainer sinkApp = sink.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(20.0));

    std::vector<Ptr<Application>> onOffApps; // pour stocker les OnOff des STA

    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        OnOffHelper onOff("ns3::UdpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), 8000));
        onOff.SetAttribute("DataRate", StringValue("5.1Mbps"));
        onOff.SetAttribute("PacketSize", UintegerValue(1024));
        onOff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onOff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        onOff.SetAttribute("StopTime", TimeValue(Seconds(20.0)));

        ApplicationContainer app = onOff.Install(wifiStaNodes.Get(i));
        onOffApps.push_back(app.Get(0));
    }


    // Animation NetAnim
    AnimationInterface anim("saturation-1AP.xml");
    anim.UpdateNodeColor(wifiApNode.Get(0), 255, 0, 0);  // AP en rouge
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        anim.UpdateNodeColor(wifiStaNodes.Get(i), 0, 50 * i, 255 - 50 * i);
    }


    // Exécution
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
