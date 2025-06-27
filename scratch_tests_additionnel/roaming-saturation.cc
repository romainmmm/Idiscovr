#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <map>
#include <cmath>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoamingSaturationExample");

// Variables globales pour le suivi du handover
static std::ofstream handoverFile;
static std::ofstream rssiFile;
static uint32_t handoverCount = 0;
static std::map<uint32_t, Mac48Address> currentAp;

// Fonction utilitaire pour convertir une Mac48Address en string
std::string MacToString(const Mac48Address& addr) {
    std::ostringstream oss;
    oss << addr;
    return oss.str();
}

// Fonction pour calculer le RSSI entre deux noeuds
double CalculateRssi(Ptr<MobilityModel> txMobility, Ptr<MobilityModel> rxMobility)
{
    Vector txPosition = txMobility->GetPosition();
    Vector rxPosition = rxMobility->GetPosition();
    double distance = std::sqrt(std::pow(txPosition.x - rxPosition.x, 2)
                              + std::pow(txPosition.y - rxPosition.y, 2)
                              + std::pow(txPosition.z - rxPosition.z, 2));
    double txPowerDbm = 16.0;
    double exponent = 3.0;
    double referenceDistance = 1.0;
    double referenceLoss = 46.6777;
    double pathLossDb;
    if (distance <= referenceDistance) {
        pathLossDb = referenceLoss;
    } else {
        pathLossDb = referenceLoss + 10 * exponent * std::log10(distance / referenceDistance);
    }
    double rssi = txPowerDbm - pathLossDb;
    return rssi;
}

void RssiMonitorCallback(NodeContainer staNodes, NodeContainer apNodes)
{
    double currentTime = Simulator::Now().GetSeconds();
    for (uint32_t i = 0; i < staNodes.GetN(); i++) {
        Ptr<MobilityModel> staMobility = staNodes.Get(i)->GetObject<MobilityModel>();
        Vector staPosition = staMobility->GetPosition();
        for (uint32_t j = 0; j < apNodes.GetN(); j++) {
            Ptr<MobilityModel> apMobility = apNodes.Get(j)->GetObject<MobilityModel>();
            double rssi = CalculateRssi(apMobility, staMobility);
            rssiFile << currentTime << "," << i << "," << j << ","
                     << staPosition.x << "," << staPosition.y << ","
                     << rssi << std::endl;
            if (std::fmod(currentTime, 5.0) < 0.1) {
                std::cout << "T=" << currentTime << "s - STA" << i
                          << " Position: (" << staPosition.x << "," << staPosition.y
                          << ") - RSSI vers AP" << j << ": " << rssi << " dBm" << std::endl;
            }
        }
    }
    Simulator::Schedule(MilliSeconds(100), &RssiMonitorCallback, staNodes, apNodes);
}

// Association callback - appelé quand la STA s'associe à un AP
void AssociationCallback(std::string context, Mac48Address apAddr)
{
    std::string::size_type start = context.find("/NodeList/");
    std::string::size_type end = context.find("/Device", start);
    std::string nodeIdStr = context.substr(start + 10, end - (start + 10));
    uint32_t nodeId = std::stoi(nodeIdStr);

    double currentTime = Simulator::Now().GetSeconds();
    std::cout << "ASSOCIATION à t=" << currentTime
              << "s: STA" << nodeId << " s'est associée avec AP " << MacToString(apAddr) << std::endl;

    if (currentAp.find(nodeId) != currentAp.end() && currentAp[nodeId] != apAddr) {
        handoverCount++;
        std::cout << "  --> HANDOVER #" << handoverCount << ": STA" << nodeId
                  << " de " << MacToString(currentAp[nodeId]) << " vers " << MacToString(apAddr) << std::endl;
        handoverFile << currentTime << ",HANDOVER," << nodeId << ","
                     << MacToString(currentAp[nodeId]) << "," << MacToString(apAddr) << std::endl;
    }
    // Réassociation au même AP ou première association
    else if (currentAp.find(nodeId) != currentAp.end() && currentAp[nodeId] == apAddr) {
        std::cout << "  Réassociation au même AP: " << MacToString(apAddr) << std::endl;
    } else {
        std::cout << "  Première association: " << MacToString(apAddr) << std::endl;
    }
    currentAp[nodeId] = apAddr;
    handoverFile << currentTime << ",ASSOC," << nodeId << "," << MacToString(apAddr) << std::endl;
}

// Désassociation callback - appelé quand la STA se désassocie d'un AP
void DisassociationCallback(std::string context, Mac48Address apAddr)
{
    std::string::size_type start = context.find("/NodeList/");
    std::string::size_type end = context.find("/Device", start);
    std::string nodeIdStr = context.substr(start + 10, end - (start + 10));
    uint32_t nodeId = std::stoi(nodeIdStr);

    double currentTime = Simulator::Now().GetSeconds();
    std::cout << "DÉSASSOCIATION à t=" << currentTime
              << "s: STA" << nodeId << " s'est désassociée de AP " << MacToString(apAddr) << std::endl;
    handoverFile << currentTime << ",DEASSOC," << nodeId << "," << MacToString(apAddr) << std::endl;
}

int main(int argc, char *argv[]) {
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("RoamingSaturationExample", LOG_LEVEL_INFO);

    uint32_t nStaFixed = 5;      // STA fixes pour saturer AP1
    double apDistance = 60.0;    // Distance entre APs
    double speed = 2.0;          // Vitesse de la STA mobile
    double txPower = 16.0;       // Puissance des APs
    double simTime = 60.0;       // Durée simulation
    std::string dataRate = "10Mbps"; // Débit pour OnOff

    CommandLine cmd;
    cmd.AddValue("nStaFixed", "Nombre de stations fixes", nStaFixed);
    cmd.AddValue("apDistance", "Distance entre AP1 et AP2 (m)", apDistance);
    cmd.AddValue("speed", "Vitesse de la station mobile (m/s)", speed);
    cmd.AddValue("txPower", "Puissance de transmission (dBm)", txPower);
    cmd.AddValue("simTime", "Temps de simulation (s)", simTime);
    cmd.AddValue("dataRate", "Débit généré par chaque STA fixe (ex: 10Mbps)", dataRate);
    cmd.Parse(argc, argv);

    // Création des noeuds
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);
    NodeContainer wifiStaFixedNodes;
    wifiStaFixedNodes.Create(nStaFixed);
    NodeContainer wifiStaMobileNode;
    wifiStaMobileNode.Create(1);

    // Canal et PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    // STA FIXES
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(true));
    NetDeviceContainer staFixedDevices = wifi.Install(phy, mac, wifiStaFixedNodes);

    // STA MOBILE
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(true));
    NetDeviceContainer staMobileDevice = wifi.Install(phy, mac, wifiStaMobileNode);

    // APs
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNodes);

    // Puissance des APs
    for (uint32_t i = 0; i < apDevices.GetN(); i++) {
        Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(apDevices.Get(i));
        Ptr<WifiPhy> wifiPhy = device->GetPhy();
        wifiPhy->SetTxPowerStart(txPower);
        wifiPhy->SetTxPowerEnd(txPower);
    }

    // --- MOBILITE ---
    MobilityHelper mobility;

    // AP fixes
    Ptr<ListPositionAllocator> apPosAlloc = CreateObject<ListPositionAllocator>();
    apPosAlloc->Add(Vector(0.0, 0.0, 0.0));              // AP1 à x=0
    apPosAlloc->Add(Vector(apDistance, 0.0, 0.0));       // AP2 à x=apDistance
    mobility.SetPositionAllocator(apPosAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

    // STA fixes : toutes proches de AP1, elles ne doivent jamais changer d'AP
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> staFixedPosAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nStaFixed; i++) {
        staFixedPosAlloc->Add(Vector(2.0 + 0.5*i, 1.0 + i, 0.0)); // Proche AP1
    }
    mobility.SetPositionAllocator(staFixedPosAlloc);
    mobility.Install(wifiStaFixedNodes);

    // STA mobile : seule à bouger et à faire du roaming
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    Ptr<ListPositionAllocator> staMobilePosAlloc = CreateObject<ListPositionAllocator>();
    staMobilePosAlloc->Add(Vector(0.0, 2.0, 0.0)); // départ à côté d'AP1
    mobility.SetPositionAllocator(staMobilePosAlloc);
    mobility.Install(wifiStaMobileNode);

    Ptr<ConstantVelocityMobilityModel> mob = wifiStaMobileNode.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    mob->SetVelocity(Vector(speed, 0.0, 0.0));

    // Demi-tour à mi-simulation
    Simulator::Schedule(Seconds(simTime/2), [mob, speed]() {
        mob->SetVelocity(Vector(-speed, 0.0, 0.0));
        std::cout << "À t=" << Simulator::Now().GetSeconds() << "s: STA mobile fait demi-tour" << std::endl;
    });

    // Stack IP
    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaFixedNodes);
    stack.Install(wifiStaMobileNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staFixedInterfaces = address.Assign(staFixedDevices);
    Ipv4InterfaceContainer staMobileInterface = address.Assign(staMobileDevice);

    // Applications : STA fixes saturent AP1 avec OnOff à haut débit
    ApplicationContainer fixedApps;
    for (uint32_t i = 0; i < nStaFixed; ++i) {
        InetSocketAddress sinkAddr(apInterfaces.GetAddress(0), 5000 + i); // Port différent pour chaque STA
        OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddr);
        onoff.SetAttribute("DataRate", StringValue(dataRate));
        onoff.SetAttribute("PacketSize", UintegerValue(1472));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
        ApplicationContainer app = onoff.Install(wifiStaFixedNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simTime));
        fixedApps.Add(app);

        // Récepteur sur AP1
        PacketSinkHelper sink("ns3::UdpSocketFactory", sinkAddr);
        ApplicationContainer sinkApp = sink.Install(wifiApNodes.Get(0));
        sinkApp.Start(Seconds(0.5));
        sinkApp.Stop(Seconds(simTime));
    }

    // Application de test (ping) pour la STA mobile - vers AP1 et AP2
    UdpEchoServerHelper echoServerAp1(6000);
    ApplicationContainer serverAppAp1 = echoServerAp1.Install(wifiApNodes.Get(0));
    serverAppAp1.Start(Seconds(1.0));
    serverAppAp1.Stop(Seconds(simTime));
    UdpEchoServerHelper echoServerAp2(6001);
    ApplicationContainer serverAppAp2 = echoServerAp2.Install(wifiApNodes.Get(1));
    serverAppAp2.Start(Seconds(1.0));
    serverAppAp2.Stop(Seconds(simTime));

    // Client - la STA mobile envoie vers AP1 puis AP2 (toute la sim)
    UdpEchoClientHelper echoClientToAp1(apInterfaces.GetAddress(0), 6000);
    echoClientToAp1.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    echoClientToAp1.SetAttribute("Interval", TimeValue(Seconds(0.25)));
    echoClientToAp1.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer mobileClientApp1 = echoClientToAp1.Install(wifiStaMobileNode.Get(0));
    mobileClientApp1.Start(Seconds(1.0));
    mobileClientApp1.Stop(Seconds(simTime));
    UdpEchoClientHelper echoClientToAp2(apInterfaces.GetAddress(1), 6001);
    echoClientToAp2.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    echoClientToAp2.SetAttribute("Interval", TimeValue(Seconds(0.25)));
    echoClientToAp2.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer mobileClientApp2 = echoClientToAp2.Install(wifiStaMobileNode.Get(0));
    mobileClientApp2.Start(Seconds(1.0));
    mobileClientApp2.Stop(Seconds(simTime));

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // NetAnim
    AnimationInterface anim("wifi-roaming-saturation.xml");
    anim.SetStopTime(Seconds(simTime));
    anim.SetMobilityPollInterval(Seconds(0.5));
    anim.EnablePacketMetadata(true);
    anim.UpdateNodeColor(wifiApNodes.Get(0), 255, 0, 0);
    anim.UpdateNodeColor(wifiApNodes.Get(1), 0, 255, 0);
    anim.UpdateNodeDescription(wifiApNodes.Get(0), "AP1");
    anim.UpdateNodeDescription(wifiApNodes.Get(1), "AP2");
    for (uint32_t i = 0; i < wifiStaFixedNodes.GetN(); ++i) {
        anim.UpdateNodeColor(wifiStaFixedNodes.Get(i), 200, 200, 255);
        anim.UpdateNodeDescription(wifiStaFixedNodes.Get(i), "STAfix" + std::to_string(i));
    }
    anim.UpdateNodeColor(wifiStaMobileNode.Get(0), 0, 0, 255);
    anim.UpdateNodeDescription(wifiStaMobileNode.Get(0), "STAroaming");

    // Fichiers CSV pour traces
    handoverFile.open("handover_events.csv");
    handoverFile << "Time,EventType,StationID,AccessPoint1,AccessPoint2" << std::endl;
    rssiFile.open("rssi_measurements.csv");
    rssiFile << "Time,StationID,APID,PosX,PosY,RSSI" << std::endl;

    // Callbacks
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc",
                  MakeCallback(&AssociationCallback));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc",
                  MakeCallback(&DisassociationCallback));

    // Créer un container général pour RSSI
    NodeContainer allStaNodes;
    for (uint32_t i = 0; i < wifiStaFixedNodes.GetN(); ++i)
        allStaNodes.Add(wifiStaFixedNodes.Get(i));
    allStaNodes.Add(wifiStaMobileNode.Get(0));

    Simulator::Schedule(Seconds(0.0), &RssiMonitorCallback, allStaNodes, wifiApNodes);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Analyse FlowMonitor
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("wifi-roaming-saturation-flowmon.xml", true, true);

    std::ofstream flowCsv;
    flowCsv.open("flow_stats.csv");
    flowCsv << "FlowID,Source,Destination,TxPackets,RxPackets,LostPackets,DelaySum,JitterSum,LastDelay,TxBytes,RxBytes,Duration,Throughput(Kbps)" << std::endl;
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        double duration = (i->second.timeLastRxPacket - i->second.timeFirstTxPacket).GetSeconds();
        double throughput = (duration > 0) ? i->second.rxBytes * 8.0 / duration / 1000 : 0;
        flowCsv << i->first << ","
                << t.sourceAddress << ","
                << t.destinationAddress << ","
                << i->second.txPackets << ","
                << i->second.rxPackets << ","
                << i->second.lostPackets << ","
                << i->second.delaySum.GetSeconds() << ","
                << i->second.jitterSum.GetSeconds() << ","
                << i->second.lastDelay.GetSeconds() << ","
                << i->second.txBytes << ","
                << i->second.rxBytes << ","
                << duration << ","
                << throughput << std::endl;
        std::cout << "Flux " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << throughput << " kbps\n";
        std::cout << "  Mean Delay: " << (i->second.rxPackets > 0 ? i->second.delaySum.GetSeconds() / i->second.rxPackets : 0) << " s\n";
    }
    flowCsv.close();

    handoverFile.close();
    rssiFile.close();

    std::cout << "\n=== Statistiques de simulation ===" << std::endl;
    std::cout << "Nombre total de handovers: " << handoverCount << std::endl;
    std::cout << "Temps de simulation: " << simTime << " secondes" << std::endl;
    std::cout << "Distance entre APs: " << apDistance << " mètres" << std::endl;
    std::cout << "Vitesse de la STA mobile: " << speed << " m/s" << std::endl;
    std::cout << "Nombre de STA fixes: " << nStaFixed << std::endl;
    std::cout << "Fichiers générés:" << std::endl;
    std::cout << "  - handover_events.csv: événements de handover" << std::endl;
    std::cout << "  - rssi_measurements.csv: mesures RSSI" << std::endl;
    std::cout << "  - flow_stats.csv: statistiques de flux" << std::endl;
    std::cout << "  - wifi-roaming-saturation-flowmon.xml: données FlowMonitor" << std::endl;
    std::cout << "  - wifi-roaming-saturation.xml: animation NetAnim" << std::endl;

    Simulator::Destroy();
    return 0;
}
