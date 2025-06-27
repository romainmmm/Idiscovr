#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/log.h"


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

// MODIFS AJOUTÉES POUR LA PUISSANCE DES APS

    Ptr<NetDevice> ap1Device = apDevices.Get(0);
    Ptr<WifiNetDevice> wifiAp1Device = DynamicCast<WifiNetDevice>(ap1Device);
    Ptr<YansWifiPhy> phyAp1 = DynamicCast<YansWifiPhy>(wifiAp1Device->GetPhy());

    // Fixe TxPowerStart et TxPowerEnd à 20 dBm pour AP1
    phyAp1->SetTxPowerStart(10.0);
    phyAp1->SetTxPowerEnd(10.0);


    Ptr<NetDevice> ap2Device = apDevices.Get(1);
    Ptr<WifiNetDevice> wifiAp2Device = DynamicCast<WifiNetDevice>(ap2Device);
    Ptr<YansWifiPhy> phyAp2 = DynamicCast<YansWifiPhy>(wifiAp2Device->GetPhy());

    // Fixe TxPowerStart et TxPowerEnd à 20 dBm pour AP1
    phyAp2->SetTxPowerStart(20.0);
    phyAp2->SetTxPowerEnd(20.0);

// FIN DES MODIFS


// APs fixes
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));    // AP1 à x=0
    positionAlloc->Add(Vector(30.0, 0.0, 0.0));   // AP2 à x=60
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

// STA mobile - démarre à x=0 et se déplace à 0.5 m/s vers la droite 
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(wifiStaNode);
    Ptr<ConstantVelocityMobilityModel> mob = wifiStaNode.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    mob->SetPosition(Vector(0.0, 0.0, 0.0));
    mob->SetVelocity(Vector(1.0, 0.0, 0.0)); // Se dirige vers AP2

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

// UDP Echo Server sur AP1 - port 10
    UdpEchoServerHelper echoServerAp1(10);
    ApplicationContainer serverAppAp1 = echoServerAp1.Install(wifiApNodes.Get(0)); // AP1
    serverAppAp1.Start(Seconds(0.5));
    serverAppAp1.Stop(Seconds(60.0)); // Arrête avant que le client passe à AP2


// UDP Echo Server sur AP2 - port 9
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(wifiApNodes.Get(1)); // AP2
    serverApp.Start(Seconds(0.5));
    serverApp.Stop(Seconds(60.0));

// Client sur la STA mobile - vers AP1 au début (jusqu’à 6s)
    UdpEchoClientHelper echoClient1(apInterfaces.GetAddress(0), 10);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = echoClient1.Install(wifiStaNode.Get(0));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(60.0));
// Client sur la STA mobile - vers AP2 à partir de 6s
    UdpEchoClientHelper echoClient2(apInterfaces.GetAddress(1), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = echoClient2.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(9.0));
    clientApp.Stop(Seconds(60.0));

// Création d'un fichier .xml pour lire dans NetAnim - positionne les AP et affiche le mouvement de la STA
    AnimationInterface anim("test-fastroaming.xml");
    //anim.SetConstantPosition(wifiApNodes.Get(0), 0, 0);
    //anim.SetConstantPosition(wifiApNodes.Get(1), 60, 0);
    //anim.SetMobilityPollInterval(Seconds(1.0));
    anim.UpdateNodeColor(wifiApNodes.Get(0), 255, 0, 0);  // AP1 en rouge
    anim.UpdateNodeColor(wifiApNodes.Get(1), 0, 255, 0);  // AP2 en vert
    anim.UpdateNodeColor(wifiStaNode.Get(0), 0, 0, 255);  // STA en bleu



//  Lancement de la simulation
    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
