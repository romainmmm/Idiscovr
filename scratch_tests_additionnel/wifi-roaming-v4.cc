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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RoamingExample");

// Variables globales pour le suivi du handover
static std::ofstream handoverFile;
static std::ofstream rssiFile;  // Nouveau fichier pour le RSSI
static uint32_t handoverCount = 0;
static std::map<uint32_t, Mac48Address> currentAp; // Stocke le dernier AP auquel chaque STA était connectée

// Fonction pour calculer le RSSI entre deux noeuds
double CalculateRssi(Ptr<MobilityModel> txMobility, Ptr<MobilityModel> rxMobility)
{
    // Obtenir les positions
    Vector txPosition = txMobility->GetPosition();
    Vector rxPosition = rxMobility->GetPosition();
    
    // Calculer la distance en mètres
    double distance = CalculateDistance(txPosition, rxPosition);
    
    // Paramètres du modèle de propagation LogDistance
    double txPowerDbm = 16.0;          // 16dBm - puissance de transmission
    double exponent = 3.0;             // Exposant de perte
    double referenceDistance = 1.0;    // Distance de référence en mètres
    double referenceLoss = 46.6777;    // Perte à la distance de référence (dB)
    
    // Calcul de la perte de chemin
    double pathLossDb;
    if (distance <= referenceDistance) {
        pathLossDb = referenceLoss;
    } else {
        pathLossDb = referenceLoss + 10 * exponent * std::log10(distance / referenceDistance);
    }
    
    // RSSI = Puissance de transmission - Perte de chemin
    double rssi = txPowerDbm - pathLossDb;
    
    return rssi;
}

// Fonction pour calculer la distance entre deux points
double CalculateDistance(Vector a, Vector b)
{
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double dz = b.z - a.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

// Callback pour mesurer le RSSI périodiquement
void RssiMonitorCallback(NodeContainer staNodes, NodeContainer apNodes)
{
    double currentTime = Simulator::Now().GetSeconds();
    
    // Pour chaque station
    for (uint32_t i = 0; i < staNodes.GetN(); i++) {
        Ptr<MobilityModel> staMobility = staNodes.Get(i)->GetObject<MobilityModel>();
        Vector staPosition = staMobility->GetPosition();
        
        // Mesurer le RSSI vers chaque AP
        for (uint32_t j = 0; j < apNodes.GetN(); j++) {
            Ptr<MobilityModel> apMobility = apNodes.Get(j)->GetObject<MobilityModel>();
            double rssi = CalculateRssi(apMobility, staMobility);
            
            // Écrire dans le fichier RSSI
            rssiFile << currentTime << "," << i << "," << j << "," 
                    << staPosition.x << "," << staPosition.y << "," 
                    << rssi << std::endl;
            
            // Afficher les RSSI dans la console à des moments clés (toutes les 5 secondes)
            if (std::fmod(currentTime, 5.0) < 0.1) {
                std::cout << "T=" << currentTime << "s - STA" << i 
                         << " Position: (" << staPosition.x << "," << staPosition.y 
                         << ") - RSSI vers AP" << j << ": " << rssi << " dBm" << std::endl;
            }
        }
    }
    
    // Planifier le prochain monitoring RSSI (toutes les 100ms)
    Simulator::Schedule(MilliSeconds(100), &RssiMonitorCallback, staNodes, apNodes);
}

// Association callback - appelé quand la STA s'associe à un AP
void AssociationCallback(std::string context, Mac48Address apAddr)
{
    // Extraire l'ID du noeud à partir du contexte
    std::string::size_type start = context.find("/NodeList/");
    std::string::size_type end = context.find("/Device", start);
    std::string nodeIdStr = context.substr(start + 10, end - (start + 10));
    uint32_t nodeId = std::stoi(nodeIdStr);
    
    // Enregistrer l'événement d'association
    double currentTime = Simulator::Now().GetSeconds();
    std::cout << "ASSOCIATION à t=" << currentTime 
              << "s: STA" << nodeId << " s'est associée avec AP " << apAddr << std::endl;
    
    // Vérifier si c'est un handover (si la STA était déjà associée à un autre AP)
    if (currentAp.find(nodeId) != currentAp.end() && currentAp[nodeId] != apAddr) {
        // C'est un handover - incrémenter le compteur
        handoverCount++;
        std::cout << "  --> HANDOVER #" << handoverCount << ": STA" << nodeId 
                 << " de " << currentAp[nodeId] << " vers " << apAddr << std::endl;
        
        // Écrire dans le fichier de log
        handoverFile << currentTime << ",HANDOVER," << nodeId << "," 
                    << currentAp[nodeId] << "," << apAddr << std::endl;
    }
    
    // Mettre à jour l'AP actuel de cette station
    currentAp[nodeId] = apAddr;
    
    // Écriture dans le fichier de log
    handoverFile << currentTime << ",ASSOC," << nodeId << "," << apAddr << std::endl;
}

// Désassociation callback - appelé quand la STA se désassocie d'un AP
void DisassociationCallback(std::string context, Mac48Address apAddr)
{
    // Extraire l'ID du noeud à partir du contexte
    std::string::size_type start = context.find("/NodeList/");
    std::string::size_type end = context.find("/Device", start);
    std::string nodeIdStr = context.substr(start + 10, end - (start + 10));
    uint32_t nodeId = std::stoi(nodeIdStr);
    
    // Enregistrer l'événement de désassociation
    double currentTime = Simulator::Now().GetSeconds();
    std::cout << "DÉSASSOCIATION à t=" << currentTime 
              << "s: STA" << nodeId << " s'est désassociée de AP " << apAddr << std::endl;
    
    // Écriture dans le fichier de log
    handoverFile << currentTime << ",DEASSOC," << nodeId << "," << apAddr << std::endl;
}

int main(int argc, char *argv[]) {
    // Active logs pour afficher dans la console les messages échangés client/serveur UDP
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("RoamingExample", LOG_LEVEL_INFO);

    // Paramètres configurables
    uint32_t nStas = 5;         // Nombre de stations
    double apDistance = 60.0;   // Distance entre AP1 et AP2 en mètres
    double speed = 2.0;         // Vitesse des STAs en m/s
    double txPower = 16.0;      // Puissance de transmission des APs en dBm
    double simTime = 60.0;      // Temps de simulation en secondes
    
    // Configuration via ligne de commande
    CommandLine cmd;
    cmd.AddValue("nStas", "Nombre de stations", nStas);
    cmd.AddValue("apDistance", "Distance entre AP1 et AP2 (m)", apDistance);
    cmd.AddValue("speed", "Vitesse des stations (m/s)", speed);
    cmd.AddValue("txPower", "Puissance de transmission (dBm)", txPower);
    cmd.AddValue("simTime", "Temps de simulation (s)", simTime);
    cmd.Parse(argc, argv);

    // Création des noeuds : stations mobiles et 2 AP
    NodeContainer wifiApNodes;
    wifiApNodes.Create(2);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStas);

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
    staDevice = wifi.Install(phy, mac, wifiStaNodes);

    // Mobilité
    MobilityHelper mobility;

    // APs (serveurs) - même ssid que le client 
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNodes);

    // Configuration de la puissance des APs pour mieux contrôler les zones de couverture
    for (uint32_t i = 0; i < apDevices.GetN(); i++) {
        Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(apDevices.Get(i));
        Ptr<WifiPhy> wifiPhy = device->GetPhy();
        wifiPhy->SetTxPowerStart(txPower);
        wifiPhy->SetTxPowerEnd(txPower);
    }

    // Paramétrage du roaming pour des handovers plus réactifs
    for (uint32_t i = 0; i < staDevice.GetN(); i++) {
        Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(staDevice.Get(i));
        Ptr<StaWifiMac> staMac = DynamicCast<StaWifiMac>(device->GetMac());
        staMac->SetAttribute("ProbeRequestTimeout", TimeValue(MilliSeconds(50)));
    }

    // APs fixes
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));              // AP1 à x=0
    positionAlloc->Add(Vector(apDistance, 0.0, 0.0));       // AP2 à x=60 par défaut
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

    // STA mobile - démarre à x=0 et se déplace à 2 m/s vers la droite
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(wifiStaNodes);
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        Ptr<ConstantVelocityMobilityModel> mob = wifiStaNodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mob->SetPosition(Vector(0.0, 1.50 + i*0.10, 0.0)); // Chaque STA décalée sur Y
        mob->SetVelocity(Vector(speed, 0.0, 0.0)); // Toutes se déplacent vers la droite
    }

    // Planifie le demi-tour à la moitié du temps de simulation
    Simulator::Schedule(Seconds(simTime/2), [&wifiStaNodes, speed]() {
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
            Ptr<ConstantVelocityMobilityModel> mob = wifiStaNodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
            mob->SetVelocity(Vector(-speed, 0.0, 0.0)); // Inverse la direction (retour vers AP1)
            
            std::cout << "À t=" << Simulator::Now().GetSeconds() << "s: STA" << i 
                     << " fait demi-tour et revient vers AP1" << std::endl;
        }
    });

    // Ajoute TCP/IP sur tous les noeuds
    InternetStackHelper stack;
    stack.Install(wifiApNodes);
    stack.Install(wifiStaNodes);

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
    serverAppAp1.Start(Seconds(1.0));
    serverAppAp1.Stop(Seconds(simTime)); 

    // UDP Echo Server sur AP2 - port 9
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(wifiApNodes.Get(1)); // AP2
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // Clients pour toutes les STA    
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        // Vers AP1 (port 10)
        UdpEchoClientHelper echoClientToAp1(apInterfaces.GetAddress(0), 10);
        echoClientToAp1.SetAttribute("MaxPackets", UintegerValue(1000));
        echoClientToAp1.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        echoClientToAp1.SetAttribute("PacketSize", UintegerValue(4096));
        ApplicationContainer clientApp1 = echoClientToAp1.Install(wifiStaNodes.Get(i));
        clientApp1.Start(Seconds(1.0));
        clientApp1.Stop(Seconds(simTime));

        // Vers AP2 (port 9)
        UdpEchoClientHelper echoClientToAp2(apInterfaces.GetAddress(1), 9);
        echoClientToAp2.SetAttribute("MaxPackets", UintegerValue(1000));
        echoClientToAp2.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        echoClientToAp2.SetAttribute("PacketSize", UintegerValue(4096));
        ApplicationContainer clientApp2 = echoClientToAp2.Install(wifiStaNodes.Get(i));
        clientApp2.Start(Seconds(simTime * 0.4)); // Commence à 40% du temps de simulation
        clientApp2.Stop(Seconds(simTime));
    }

    // Configuration de FlowMonitor pour surveiller les performances
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Création d'un fichier .xml pour lire dans NetAnim
    AnimationInterface anim("wifi-roaming-complete.xml");
    // Configuration NetAnim
    anim.SetStopTime(Seconds(simTime));        // Durée explicite de l'animation
    anim.SetMobilityPollInterval(Seconds(0.5)); // Sondage fréquent des positions
    anim.EnablePacketMetadata(true);           // Visualisation des paquets
    
    anim.UpdateNodeColor(wifiApNodes.Get(0), 255, 0, 0);  // AP1 en rouge
    anim.UpdateNodeColor(wifiApNodes.Get(1), 0, 255, 0);  // AP2 en vert
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        // Couleurs dégradées pour les STAs
        anim.UpdateNodeColor(wifiStaNodes.Get(i), 0, 50 * i, 255 - 50 * i);
        anim.UpdateNodeDescription(wifiStaNodes.Get(i), "STA" + std::to_string(i));
    }
    anim.UpdateNodeDescription(wifiApNodes.Get(0), "AP1");
    anim.UpdateNodeDescription(wifiApNodes.Get(1), "AP2");
    
    // Ajouter un événement planifié pour garantir la continuité de l'animation
    Simulator::Schedule(Seconds(0.75 * simTime), [&anim, &wifiStaNodes]() {
        // Force une mise à jour de toutes les positions à 75% du temps
        for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
            anim.UpdateNodeDescription(wifiStaNodes.Get(i), 
                "STA" + std::to_string(i) + " (retour)");
        }
    });

    // Préparation des fichiers pour tracer les handovers et RSSI
    handoverFile.open("handover_events.csv");
    handoverFile << "Time,EventType,StationID,AccessPoint1,AccessPoint2" << std::endl;
    
    // Fichier RSSI
    rssiFile.open("rssi_measurements.csv");
    rssiFile << "Time,StationID,APID,PosX,PosY,RSSI" << std::endl;

    // Connecter les callbacks pour suivre les associations et désassociations
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc", 
                  MakeCallback(&AssociationCallback));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc", 
                  MakeCallback(&DisassociationCallback));
                  
    // Démarrer la surveillance RSSI
    Simulator::Schedule(Seconds(0.0), &RssiMonitorCallback, wifiStaNodes, wifiApNodes);

    // Lancement de la simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Analyse des données FlowMonitor
    monitor->CheckForLostPackets();
    
    // Écrire les résultats FlowMonitor dans un fichier XML
    monitor->SerializeToXmlFile("wifi-roaming-flowmon.xml", true, true);
    
    // Créer un fichier CSV pour une analyse plus facile
    std::ofstream flowCsv;
    flowCsv.open("flow_stats.csv");
    flowCsv << "FlowID,Source,Destination,TxPackets,RxPackets,LostPackets,DelaySum,JitterSum,LastDelay,TxBytes,RxBytes,Duration,Throughput(Kbps)" << std::endl;
    
    // Récupérer les statistiques de chaque flux
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        
        // Calculer le débit en Kbps
        double duration = (i->second.timeLastRxPacket - i->second.timeFirstTxPacket).GetSeconds();
        double throughput = 0;
        if (duration > 0) {
            throughput = i->second.rxBytes * 8.0 / duration / 1000; // Kbps
        }
        
        // Écrire dans le CSV
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
                
        // Afficher un résumé des statistiques dans la console
        std::cout << "Flux " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << throughput << " kbps\n";
        std::cout << "  Mean Delay: " << (i->second.rxPackets > 0 ? i->second.delaySum.GetSeconds() / i->second.rxPackets : 0) << " s\n";
    }
    
    flowCsv.close();

    // Fermeture des fichiers
    handoverFile.close();
    rssiFile.close();

    // Affichage des statistiques
    std::cout << "\n=== Statistiques de simulation ===" << std::endl;
    std::cout << "Nombre total de handovers: " << handoverCount << std::endl;
    std::cout << "Temps de simulation: " << simTime << " secondes" << std::endl;
    std::cout << "Distance entre APs: " << apDistance << " mètres" << std::endl;
    std::cout << "Vitesse des STAs: " << speed << " m/s" << std::endl;
    std::cout << "Nombre de stations: " << nStas << std::endl;
    std::cout << "Puissance de transmission des APs: " << txPower << " dBm" << std::endl;
    std::cout << "Fichiers générés:" << std::endl;
    std::cout << "  - handover_events.csv: événements de handover" << std::endl;
    std::cout << "  - rssi_measurements.csv: mesures RSSI" << std::endl;
    std::cout << "  - flow_stats.csv: statistiques de flux" << std::endl;
    std::cout << "  - wifi-roaming-flowmon.xml: données FlowMonitor détaillées" << std::endl;
    std::cout << "  - wifi-roaming-complete.xml: animation NetAnim" << std::endl;

    Simulator::Destroy();
    return 0;
}
