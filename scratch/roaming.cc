#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoamingExample");

int main(int argc, char *argv[]) {
    // Active logs pour affichr dans la console les messages échangés clietn/serveur UDP
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);


// Création des noeuds : une station mobile et 2 AP
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);

// Utilisation d'un canal YANS et configuration PHY wifi avec type de trace .pcap
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.SetChannel(channel.Create());

// Configuration wifi (standard 802.11g, 2.4 GHz)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

// Configuration MAC
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

// STA (client) - même ssid que les AP
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(true)); // envoie des Probe Requests pour découvrir les AP
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

// Mobilité
    MobilityHelper mobility;

// APs (serveurs) - même ssid que le client 
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNodes);



// APs fixes
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));    // AP1 à x=0
    positionAlloc->Add(Vector(60.0, 0.0, 0.0));   // AP2 à x=60
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

// STA mobile - démarre à x=0 et se déplace à 10 m/s vers la droite
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(wifiStaNode);
    Ptr<ConstantVelocityMobilityModel> mob = wifiStaNode.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    mob->SetPosition(Vector(0.0, 0.0, 0.0));
    mob->SetVelocity(Vector(10.0, 0.0, 0.0)); // Se dirige vers AP2

// Ajoute TCP/IP sur tous les noeuds
    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaNode);

// Tous les noeuds sont dans le même réseau (10.1.1.0)
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces;
    apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevice);

// UDP Echo Server sur AP2 - port 9
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(wifiApNodes.Get(1)); // AP2
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

// Client sur la STA mobile
    UdpEchoClientHelper echoClient(apInterfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

// Création d'un fichier .xml pour lire dans NetAnim - positionne les AP et affiche le mouvement de la STA
    AnimationInterface anim("roaming.xml");
    anim.SetConstantPosition(wifiApNodes.Get(0), 0, 0);
    anim.SetConstantPosition(wifiApNodes.Get(1), 60, 0);
    anim.SetMobilityPollInterval(Seconds(1.0));

//  Lancement de la simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
