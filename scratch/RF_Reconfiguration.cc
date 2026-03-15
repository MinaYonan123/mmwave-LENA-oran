// Copyright (c) 2023 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

/**
 * \ingroup examples
 * \file cttc-nr-mimo-demo.cc
 * \brief An example that shows how to setup and use MIMO
 *
 * This example describes how to setup a simulation using MIMO. The scenario
 * consists of a simple topology, in which there
 * is only one gNB and one UE. An additional pair of gNB and UE can be enabled
 * to simulate the interference (see enableInterfNode).
 * Example creates one DL flow that goes through only BWP.
 * please make sure that you run this scenario inside the scratch folder and build it there ( very important !!!!!!!!!!!!!!)
 *
 * The example prints on-screen and into the file the end-to-end result of the flow.
 * To see all the input parameters run:
 *
 * \code{.unparsed}
$ ./ns3 run cttc-nr-mimo-demo -- --PrintHelp
    \endcode
 *
 *
 * MIMO is enabled by default. To disable it run:
 *  * \code{.unparsed}
$  ./ns3 run cttc-nr-mimo-demo -- --enableMimoFeedback=0
    \endcode
 *
 *
 */

 #include "ns3/antenna-module.h"
 #include "ns3/applications-module.h"
 #include "ns3/config-store-module.h"
 #include "ns3/core-module.h"
 #include "ns3/enum.h"
 #include "ns3/flow-monitor-module.h"
 #include "ns3/internet-apps-module.h"
 #include "ns3/internet-module.h"
 #include "ns3/mobility-module.h"
 #include "ns3/nr-module.h"
 #include "ns3/point-to-point-module.h"
 #include "ns3/string.h"
 #include <iomanip> // For std::setprecision
 #include <fstream>
 #include <sstream>
 #include <sys/time.h>
 
 using namespace ns3;
 NS_LOG_COMPONENT_DEFINE("CttcNrMimoDemo");

// E2 Interface Global Configuration
static ns3::GlobalValue g_e2TermIp("e2TermIp", 
                                    "The IP address of the RIC E2 termination",
                                    ns3::StringValue("127.0.0.1"), 
                                    ns3::MakeStringChecker());
static ns3::GlobalValue g_e2_func_id("KPM_E2functionID", 
                                     "Function ID to subscribe",
                                     ns3::DoubleValue(2),
                                     ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_rc_e2_func_id("RC_E2functionID", 
                                        "Function ID to subscribe",
                                        ns3::DoubleValue(3),
                                        ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_ccc_e2_func_id("CCC_E2functionID",
                                         "Function ID to subscribe",
                                         ns3::DoubleValue(4),
                                         ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_e2nrEnabled("e2nrEnabled", 
                                      "If true, send NR E2 reports",
                                      ns3::BooleanValue(true), 
                                      ns3::MakeBooleanChecker());
static ns3::GlobalValue g_enableE2FileLogging("enableE2FileLogging",
                                              "If true, generate offline file logging instead of connecting to RIC",
                                              ns3::BooleanValue(false), 
                                              ns3::MakeBooleanChecker());

uint64_t t_startTime_simid;
double maxXAxis;
double maxYAxis;
int cell_it = 0;

void PrintGnuplottableEnbListToFile() 
{
    uint64_t timestamp = t_startTime_simid + (uint64_t) Simulator::Now().GetMilliSeconds();
    std::string filename1 = "gnbs.txt";
    
    for (NodeList::Iterator it = NodeList::Begin(); it != NodeList::End(); ++it) {
        Ptr<Node> node = *it;
        int nDevs = node->GetNDevices();
        
        for (int j = 0; j < nDevs; j++) {
            Ptr<NrGnbNetDevice> nrdev = node->GetDevice(j)->GetObject<NrGnbNetDevice>();
            if (!nrdev) {
                continue;
            }
            
            Ptr<MobilityModel> mobilityModel = node->GetObject<MobilityModel>();
            if (!mobilityModel) {
                NS_LOG_ERROR("No mobility model found for gNB node!");
                continue;
            }
            Vector pos = mobilityModel->GetPosition();
            
            std::ofstream outFile1;
            outFile1.open(filename1, std::ios_base::out | std::ios_base::app);
            if (!outFile1.is_open()) {
                NS_LOG_ERROR("Can't open file " << filename1);
                return;
            }
            
            outFile1 << timestamp << "," << nrdev->GetCellId() << "," << pos.x << "," << pos.y << ","
                     << t_startTime_simid << "," << 0 << "," << 0 << "," << 0 << "," << 0 << std::endl;
            outFile1.flush();
            outFile1.close();
        }
    }
}

void PrintPosition(Ptr<Node> node, std::string Filename) 
{
    uint64_t timestamp = t_startTime_simid + (uint64_t) Simulator::Now().GetMilliSeconds();
    
    int imsi;
    int nDevs = node->GetNDevices();
    std::string filename = Filename;
    std::ofstream outFile;
    
    for (int j = 0; j < nDevs; j++) {
        Ptr<NrUeNetDevice> nruedev = node->GetDevice(j)->GetObject<NrUeNetDevice>();
        if (nruedev) {
            imsi = int(nruedev->GetImsi());
            int serving_cell = nruedev->GetCellId();
            
            Ptr<MobilityModel> model = node->GetObject<MobilityModel>();
            Vector position = model->GetPosition();
            Vector velocity = model->GetVelocity();
            double speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
            double speedKmh = std::round(speed * 3.6 * 10.0) / 10.0;
            int gui_ue_id = imsi;  // Simplified for single-UE scenario
            
            NS_LOG_UNCOND(std::fixed << std::setprecision(2)
                         << "Position of UE with IMSI " << imsi << " (UE_ID in GUI: " << gui_ue_id 
                         << ") is " << position.x << ":" << position.y << ":" << position.z
                         << ", Speed: " << speedKmh << " km/h"
                         << " at time " << Simulator::Now().GetSeconds()
                         << ", UE connected to Cell: " << serving_cell);
            
            outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::app);
            if (!outFile.is_open()) {
                NS_LOG_ERROR("Can't open file " << filename);
                return;
            }
            
            outFile << timestamp << "," << gui_ue_id << "," << position.x << "," << position.y
                    << ",nr," << serving_cell << "," << t_startTime_simid << std::endl;
            outFile.flush();
            outFile.close();
        }
    }
}

void ClearFile(std::string Filename, uint64_t m_startTime) 
{
    std::string filename = Filename;
    std::ofstream outFile;
    outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::trunc);
    if (!outFile.is_open()) {
        NS_LOG_ERROR("Can't open file " << filename);
        return;
    }
    outFile.close();
    
    uint64_t timestamp = m_startTime + (uint64_t) Simulator::Now().GetMilliSeconds();
    std::ofstream outFile1;
    outFile1.open(filename.c_str(), std::ios_base::out | std::ios_base::app);
    
    if (Filename == "ue_position.txt") {
        outFile1 << "timestamp,id,x,y,type,cell,simid" << std::endl;
    } else {
        outFile1 << "timestamp,id,x,y,simid,ESstate,currEC,maxEC,totalcurrEC" << std::endl;
        outFile1 << timestamp << "," << "0" << "," << maxXAxis << "," << maxYAxis << std::endl;
    }
    outFile1.flush();
    outFile1.close();
}

void SetFlowMonitorOnAllGnbDevices(Ptr<FlowMonitor> monitor,
                                   Ptr<Ipv4FlowClassifier> classifier) 
{
    for (NodeList::Iterator it = NodeList::Begin(); it != NodeList::End(); ++it) {
        Ptr<Node> node = *it;
        int nDevs = node->GetNDevices();
        for (int j = 0; j < nDevs; ++j) {
            Ptr<NetDevice> dev = node->GetDevice(j);
            Ptr<NrGnbNetDevice> nrdev = dev->GetObject<NrGnbNetDevice>();
            if (!nrdev)
                continue;
            nrdev->SetFlowMonitor(monitor);
            nrdev->SetIpv4FlowClassifier(classifier);
            NS_LOG_INFO("Attached FlowMonitor to gNB device on node " << node->GetId());
        }
    }
}

 int main(int argc, char* argv[])
 {
     Config::SetDefault("ns3::NrHelper::EnableMimoFeedback", BooleanValue(true));
     Config::SetDefault("ns3::NrPmSearch::SubbandSize", UintegerValue(16));
     bool useMimoPmiParams = true;
 
     // E2 Interface Configuration
     bool enableE2FileLogging = false;
     double indicationPeriodicity = 0.1;
     double g_e2_func_id_val = 2;
     double g_rc_e2_func_id_val = 3;
     double g_ccc_e2_func_id_val = 4;
     bool report_to_db = true;  // Enable InfluxDB reporting for GUI
     uint64_t sim_id_val = 0;
     
     // Enable logging for codebook and power allocation
     LogComponentEnable("NrCbTypeOneSp", LOG_LEVEL_INFO);
     LogComponentEnable("E2Termination", LOG_LEVEL_INFO);
     
     NrHelper::AntennaParams apUe;
     NrHelper::AntennaParams apGnb;
     apUe.antennaElem = "ns3::ThreeGppAntennaModel";
     apUe.nAntCols = 2;
     apUe.nAntRows = 2;
     apUe.nHorizPorts = 2;
     apUe.nVertPorts = 1;
     apUe.isDualPolarized = true;
     apGnb.antennaElem = "ns3::ThreeGppAntennaModel";
     apGnb.nAntCols = 4;
     apGnb.nAntRows = 2;
     apGnb.nHorizPorts = 2;
     apGnb.nVertPorts = 1;
     apGnb.isDualPolarized = true;   
     apGnb.port_power = {1,1,1,1}; // port power for each antenna element

     // The polarization slant angle in degrees in case of x-polarized
     double polSlantAngleGnb = 0.0;
     double polSlantAngleUe = 90.0;
     // The bearing angles in degrees
     double bearingAngleGnb = 0.0;
     double bearingAngleUe = 180.0;

     // Traffic parameters
     uint32_t udpPacketSize = 1000;
     // For 2x2 MIMO and NR MCS table 2, packet interval is 40000 ns to
     // reach 200 mb/s
    
     Time highInterval = NanoSeconds(25000);    // ~320 Mbps (original)
     Time mediumInterval = NanoSeconds(50000);  // ~160 Mbps (half)
     Time lowInterval = NanoSeconds(100000);    // ~80 Mbps (quarter) //  Time packetInterval = NanoSeconds(25000);
     Time udpAppStartTime = MilliSeconds(1000); // Increased from 400ms to 1000ms for better initialization
     Time packetInterval = highInterval;
     // Interference
     bool enableInterfNode = false; // if true an additional pair of gNB and UE will be created
                                    // to create an interference towards the original pair
     double interfDistance = 100.0; // the distance in meters between the original node pair, and the
                                    // interfering node pair
     double interfPolSlantDelta = 0; // the difference between the pol. slant angle between the
                                     // original node and the interfering one
 
     // Other simulation scenario parameters
     Time simTime = MilliSeconds(600000); // 600 seconds (10 minutes) for comprehensive demonstration
     uint16_t gnbUeDistance = 20; // meters
     uint16_t numerology = 0;
     
     // Scenario boundaries for GUI
     maxXAxis = 100.0;
     maxYAxis = 100.0;
     double centralFrequency = 3.5e9;
     double bandwidth = 20e6;
     double txPowerGnb = 50; // dBm
     double txPowerUe = 23;  // dBm
     uint16_t updatePeriodMs = 100;
     std::string errorModel = "ns3::NrEesmIrT2";
     std::string scheduler = "ns3::NrMacSchedulerOfdmaRR"; // Changed from TDMA to OFDMA for better MIMO support
     std::string beamformingMethod = "ns3::DirectPathBeamforming";
     /**
      *   UMi_StreetCanyon,      //!< UMi_StreetCanyon
      *   UMi_StreetCanyon_LoS,  //!< UMi_StreetCanyon where all the nodes will be in Line-of-Sight
      *   UMi_StreetCanyon_nLoS, //!< UMi_StreetCanyon where all the nodes will not be in
      *
      */
 
     uint16_t losCondition = 0;
 
     // Where the example stores the output files.
     std::string simTag = "default";
     std::string outputDir = "./";
     bool logging = false;
 
     CommandLine cmd(__FILE__);
     /**
      * The main parameters for testing MIMO
      */
     cmd.AddValue("enableMimoFeedback", "ns3::NrHelper::EnableMimoFeedback");
     cmd.AddValue("pmSearchMethod", "ns3::NrHelper::PmSearchMethod");
     cmd.AddValue("fullSearchCb", "ns3::NrPmSearchFull::CodebookType");
     cmd.AddValue("rankLimit", "ns3::NrPmSearch::RankLimit");
     cmd.AddValue("subbandSize", "ns3::NrPmSearch::SubbandSize");
     cmd.AddValue("downsamplingTechnique", "ns3::NrPmSearch::DownsamplingTechnique");
     cmd.AddValue("numRowsGnb", "Number of antenna rows at the gNB", apGnb.nAntRows);
     cmd.AddValue("numRowsUe", "Number of antenna rows at the UE", apUe.nAntRows);
     cmd.AddValue("numColumnsGnb", "Number of antenna columns at the gNB", apGnb.nAntCols);
     cmd.AddValue("numColumnsUe", "Number of antenna columns at the UE", apUe.nAntCols);
     cmd.AddValue("numVPortsGnb",
                  "Number of vertical ports of the antenna at the gNB",
                  apGnb.nVertPorts);
     cmd.AddValue("numVPortsUe",
                  "Number of vertical ports of the antenna at the UE",
                  apUe.nVertPorts);
     cmd.AddValue("numHPortsGnb",
                  "Number of horizontal ports of the antenna the gNB",
                  apGnb.nHorizPorts);
     cmd.AddValue("numHPortsUe",
                  "Number of horizontal ports of the antenna at the UE",
                  apUe.nHorizPorts);
     cmd.AddValue("xPolGnb",
                  "Whether the gNB antenna array has the cross polarized antenna "
                  "elements.",
                  apGnb.isDualPolarized);
     cmd.AddValue("xPolUe",
                  "Whether the UE antenna array has the cross polarized antenna "
                  "elements.",
                  apUe.isDualPolarized);
     cmd.AddValue("polSlantAngleGnb",
                  "Polarization slant angle of gNB in degrees",
                  polSlantAngleGnb);
     cmd.AddValue("polSlantAngleUe", "Polarization slant angle of UE in degrees", polSlantAngleUe);
     cmd.AddValue("bearingAngleGnb", "Bearing angle of gNB in degrees", bearingAngleGnb);
     cmd.AddValue("bearingAngleUe", "Bearing angle of UE in degrees", bearingAngleUe);
     cmd.AddValue("enableInterfNode", "Whether to enable an interfering node", enableInterfNode);
     cmd.AddValue(
         "interfDistance",
         "The distance between the pairs of gNB and UE (the original and the interfering one)",
         interfDistance);
     cmd.AddValue("interfPolSlantDelta",
                  "The difference between the pol. slant angles of the original pairs of gNB and UE "
                  "and the interfering one",
                  interfPolSlantDelta);
 
 
 
     /**
      * Other simulation parameters
      */
     cmd.AddValue("packetSize",
                  "packet size in bytes to be used by best effort traffic",
                  udpPacketSize);
     cmd.AddValue("packetInterval", "Inter-packet interval for CBR traffic", packetInterval);
     cmd.AddValue("simTime", "Simulation time", simTime);
     cmd.AddValue("numerology", "The numerology to be used", numerology);
     cmd.AddValue("centralFrequency", "The system frequency to be used in band 1", centralFrequency);
     cmd.AddValue("bandwidth", "The system bandwidth to be used", bandwidth);
     cmd.AddValue("txPowerGnb", "gNB TX power", txPowerGnb);
     cmd.AddValue("txPowerUe", "UE TX power", txPowerUe);
     cmd.AddValue("gnbUeDistance",
                  "The distance between the gNB and the UE in the scenario",
                  gnbUeDistance);
     cmd.AddValue(
         "updatePeriodMs",
         "Channel update period in ms. If set to 0 then the channel update will be disabled",
         updatePeriodMs);
     cmd.AddValue("errorModel",
                  "Error model: ns3::NrEesmCcT1, ns3::NrEesmCcT2, "
                  "ns3::NrEesmIrT1, ns3::NrEesmIrT2, ns3::NrLteMiErrorModel",
                  errorModel);
     cmd.AddValue("scheduler",
                  "The scheduler: ns3::NrMacSchedulerTdmaRR, "
                  "ns3::NrMacSchedulerTdmaPF, ns3::NrMacSchedulerTdmaMR,"
                  "ns3::NrMacSchedulerTdmaQos, ns3::NrMacSchedulerOfdmaRR, "
                  "ns3::NrMacSchedulerOfdmaPF, ns3::NrMacSchedulerOfdmaMR,"
                  "ns3::NrMacSchedulerOfdmaQos",
                  scheduler);
     cmd.AddValue("beamformingMethod",
                  "The beamforming method: ns3::CellScanBeamforming,"
                  "ns3::CellScanBeamformingAzimuthZenith,"
                  "ns3::CellScanQuasiOmniBeamforming,"
                  "ns3::DirectPathBeamforming,"
                  "ns3::QuasiOmniDirectPathBeamforming,"
                  "ns3::DirectPathQuasiOmniBeamforming",
                  beamformingMethod);
     cmd.AddValue("losCondition",
                  "0 - for 3GPP channel condition model,"
                  "1 - for always LOS channel condition model,"
                  "2 - for always NLOS channel condition model",
                  losCondition);
     cmd.AddValue("simTag",
                  "tag to be appended to output filenames to distinguish simulation campaigns",
                  simTag);
    cmd.AddValue("outputDir", "directory where to store simulation results", outputDir);
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.AddValue("useMimoPmiParams", "Configure via the MimoPmiParams structure", useMimoPmiParams);
    cmd.AddValue("enableE2FileLogging", "Do not use E2 interface, instead, produce file trace for KPIs",
                 enableE2FileLogging);
    cmd.AddValue("indicationPeriodicity", "E2 Indication Periodicity reports (value in seconds)",
                 indicationPeriodicity);
    cmd.AddValue("KPM_E2functionID", "Function ID to subscribe", g_e2_func_id_val);
    cmd.AddValue("RC_E2functionID", "Function ID to subscribe", g_rc_e2_func_id_val);
    cmd.AddValue("CCC_E2functionID", "Function ID to subscribe", g_ccc_e2_func_id_val);
    cmd.AddValue("report_to_db", "Enable reporting to InfluxDB for GUI", report_to_db);
    cmd.AddValue("sim_id", "Simulation ID", sim_id_val);
    
    // Parse the command line
    cmd.Parse(argc, argv);
    
    // Apply E2 configuration
    StringValue stringValue;
    BooleanValue booleanValue;
    
    GlobalValue::GetValueByName("e2nrEnabled", booleanValue);
    bool e2nrEnabled = booleanValue.Get();
    GlobalValue::GetValueByName("e2TermIp", stringValue);
    std::string e2TermIp = stringValue.Get();
    
    e2nrEnabled = !enableE2FileLogging;
    
    Config::SetDefault("ns3::NrGnbNetDevice::KPM_E2functionID", DoubleValue(g_e2_func_id_val));
    Config::SetDefault("ns3::NrGnbNetDevice::RC_E2functionID", DoubleValue(g_rc_e2_func_id_val));
    Config::SetDefault("ns3::NrGnbNetDevice::CCC_E2functionID", DoubleValue(g_ccc_e2_func_id_val));
    Config::SetDefault("ns3::NrGnbNetDevice::EnableE2FileLogging", BooleanValue(enableE2FileLogging));
    Config::SetDefault("ns3::NrGnbNetDevice::E2Periodicity", DoubleValue(indicationPeriodicity));
    Config::SetDefault("ns3::NrGnbNetDevice::report_to_db", BooleanValue(report_to_db));
    Config::SetDefault("ns3::NrGnbNetDevice::sim_id", UintegerValue(sim_id_val));
    Config::SetDefault("ns3::NrHelper::E2TermIp", StringValue(e2TermIp));
    Config::SetDefault("ns3::NrHelper::E2ModeNr", BooleanValue(e2nrEnabled));
 
     // convert angle values into radians
     apUe.bearingAngle = bearingAngleUe * (M_PI / 180);
     apUe.polSlantAngle = polSlantAngleUe * (M_PI / 180);
     apGnb.bearingAngle = bearingAngleGnb * (M_PI / 180);
     apGnb.polSlantAngle = polSlantAngleGnb * (M_PI / 180);
 
     NS_ABORT_IF(centralFrequency < 0.5e9 && centralFrequency > 100e9);
     NS_ABORT_UNLESS(losCondition < 3);
 
     if (logging)
     {
        //  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        //  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        //  LogComponentEnable("NrPdcp", LOG_LEVEL_INFO);
         LogComponentEnable("NrCbTypeOneSp", LOG_LEVEL_INFO);
     }
 
     Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(999999999));
     Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod",
                        TimeValue(MilliSeconds(updatePeriodMs)));
 
     uint16_t pairsToCreate = 1;
     if (enableInterfNode)
     {
         pairsToCreate = 2;
     }
 
     NodeContainer gnbContainer;
     gnbContainer.Create(pairsToCreate);
     NodeContainer ueContainer;
     ueContainer.Create(pairsToCreate);
 
     /**
      * We configure the mobility model to ConstantPositionMobilityModel.
      * The default topology is the following:
      *
      *         gNB .........(20 m) .........UE
      *    (0.0, h, 10.0)              (d, h, 1.5)
      *
      *
      *         gNB..........(20 m)..........UE
      *   (0.0, 0.0, 10.0)               (d, 0.0, 1.5)
      */
     // Center the topology for better GUI visualization
     double centerX = maxXAxis / 2.0;
     double centerY = maxYAxis / 2.0;
     
     MobilityHelper mobility;
     mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
     Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
     positionAlloc->Add(Vector(centerX, centerY, 10.0));
     positionAlloc->Add(Vector(centerX + gnbUeDistance, centerY, 1.5));
     // the positions for the second interfering pair of gNB and UE
     if (enableInterfNode)
     {
         positionAlloc->Add(Vector(centerX, centerY + interfDistance, 10.0));
         positionAlloc->Add(Vector(centerX + gnbUeDistance, centerY + interfDistance, 1.5));
     }
     mobility.SetPositionAllocator(positionAlloc);
     mobility.Install(gnbContainer.Get(0));
     mobility.Install(ueContainer.Get(0));
     // install mobility of the second pair of gNB and UE
     if (enableInterfNode)
     {
         mobility.Install(gnbContainer.Get(1));
         mobility.Install(ueContainer.Get(1));
     }
 
     /**
      * Create the NR helpers that will be used to create and setup NR devices, spectrum, ...
      */
     Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
     Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
     Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
     nrHelper->SetBeamformingHelper(idealBeamformingHelper);
     nrHelper->SetEpcHelper(nrEpcHelper);
     /**
      * Prepare spectrum. Prepare one operational band, containing
      * one component carrier, and a single bandwidth part
      * centered at the frequency specified by the input parameters.
      *
      *
      * The configured spectrum division is:
      * ------------Band--------------
      * ------------CC1----------------
      * ------------BWP1---------------
      */
 
     BandwidthPartInfo::Scenario scenario =
         BandwidthPartInfo::Scenario(BandwidthPartInfo::V2V_Highway); ; 
 
     CcBwpCreator ccBwpCreator;
     const uint8_t numCcPerBand = 1;  // Single CC per band
     CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency,
                                                    bandwidth,
                                                    numCcPerBand,
                                                    scenario);
     OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);
     
     NS_LOG_INFO("Spectrum configuration: 1 Band, " << numCcPerBand << " CC per band, 1 BWP per CC");
 
     /**
      * Configure NrHelper, prepare most of the parameters that will be used in the simulation.
      */
     nrHelper->SetChannelConditionModelAttribute("UpdatePeriod",
                                                 TimeValue(MilliSeconds(updatePeriodMs)));
     nrHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
     nrHelper->SetDlErrorModel(errorModel);
     nrHelper->SetUlErrorModel(errorModel);
     nrHelper->SetGnbDlAmcAttribute("AmcModel", EnumValue(NrAmc::ErrorModel));
     nrHelper->SetGnbUlAmcAttribute("AmcModel", EnumValue(NrAmc::ErrorModel));
     nrHelper->SetSchedulerTypeId(TypeId::LookupByName(scheduler));
     idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                          TypeIdValue(TypeId::LookupByName(beamformingMethod)));
     // Core latency
     nrEpcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));
 
     // We can configure not only via Configure::SetDefault, but also via the MimoPmiParams structure
     if (useMimoPmiParams)
     {
         ns3::NrHelper::MimoPmiParams params;
         params.subbandSize = 16; // Increased from 8 for better stability
         params.fullSearchCb = "ns3::NrCbTypeOneSp";
         params.pmSearchMethod = "ns3::NrPmSearchFull";
         nrHelper->SetupMimoPmi(params);
         
         // Set port power directly on codebook
         if (!apGnb.port_power.empty())
         {
             std::stringstream ss;
             NS_LOG_INFO("Port power vector from antenna params:");
             for (size_t i = 0; i < apGnb.port_power.size(); ++i)
             {
                 NS_LOG_INFO("  Port " << i << ": " << apGnb.port_power[i]);
                 if (i > 0) ss << ",";
                 ss << apGnb.port_power[i];
             }
             NS_LOG_INFO("Setting port power string: " << ss.str());
             Config::SetDefault("ns3::NrCbTypeOneSp::PortPower", StringValue(ss.str()));
         }
     }
 
     /**
      * Configure gNb antenna
      */
     nrHelper->SetupGnbAntennas(apGnb);
     /**
      * Configure UE antenna
      */
     nrHelper->SetupUeAntennas(apUe);
 
     nrHelper->SetGnbPhyAttribute("Numerology", UintegerValue(numerology));
     nrHelper->SetGnbPhyAttribute("TxPower", DoubleValue(txPowerGnb));
     nrHelper->SetUePhyAttribute("TxPower", DoubleValue(txPowerUe));
 
     uint32_t bwpId = 0;
     // gNb routing between bearer type and bandwidth part
     nrHelper->SetGnbBwpManagerAlgorithmAttribute("NGBR_LOW_LAT_EMBB", UintegerValue(bwpId));
     // UE routing between bearer type and bandwidth part
     nrHelper->SetUeBwpManagerAlgorithmAttribute("NGBR_LOW_LAT_EMBB", UintegerValue(bwpId));
     /**
      * Initialize channel and pathloss, plus other things inside band.
      */
     nrHelper->InitializeOperationBand(&band);
     BandwidthPartInfoPtrVector allBwps;
     allBwps = CcBwpCreator::GetAllBwps({band});
 
     /**
      * Finally, create the gNB and the UE device.
      */
     NetDeviceContainer gnbNetDev = nrHelper->InstallGnbDevice(gnbContainer, allBwps);
     NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueContainer, allBwps);
     
     // Log device creation
     NS_LOG_UNCOND("Created " << gnbNetDev.GetN() << " gNB devices");
     NS_LOG_UNCOND("Created " << ueNetDev.GetN() << " UE devices");
     for (uint32_t i = 0; i < gnbNetDev.GetN(); ++i) {
         Ptr<NrGnbNetDevice> gnbDev = DynamicCast<NrGnbNetDevice>(gnbNetDev.Get(i));
         if (gnbDev) {
             NS_LOG_UNCOND("  gNB " << i << " has " << gnbDev->GetCcMapSize() << " BWPs/CCs");
         }
     }
     if (!apGnb.port_power.empty())
     {
         for (uint32_t i = 0; i < gnbNetDev.GetN(); ++i)
         {
             Ptr<NrGnbNetDevice> gnbDev = DynamicCast<NrGnbNetDevice>(gnbNetDev.Get(i));
             if (gnbDev)
             {
                 gnbDev->SetPortPower(apGnb.port_power);
                 NS_LOG_UNCOND("Initialized port power on gNB " << i 
                              << " (Cell ID: " << gnbDev->GetCellId() 
                              << ") with " << apGnb.port_power.size() << " ports");
             }
         }
     }
 
     if (enableInterfNode && interfPolSlantDelta != 0)
     {
         // reconfigure the polarization slant angle of the interferer
         nrHelper->GetGnbPhy(gnbNetDev.Get(1), 0)
             ->GetSpectrumPhy()
             ->GetAntenna()
             ->SetAttribute("PolSlantAngle",
                            DoubleValue((polSlantAngleGnb + interfPolSlantDelta) * (M_PI / 180)));
         nrHelper->GetUePhy(ueNetDev.Get(1), 0)
             ->GetSpectrumPhy()
             ->GetAntenna()
             ->SetAttribute("PolSlantAngle",
                            DoubleValue((polSlantAngleUe + interfPolSlantDelta) * (M_PI / 180)));
     }
    
     /**
      * Fix the random stream throughout the nr, propagation, and spectrum
      * modules classes. This configuration is extremely important for the
      * reproducibility of the results.
      */
     int64_t randomStream = 1;
     randomStream += nrHelper->AssignStreams(gnbNetDev, randomStream);
     randomStream += nrHelper->AssignStreams(ueNetDev, randomStream);
 
     // When all the configuration is done, explicitly call UpdateConfig ()
     // NOTE: This will trigger E2 registration for each gNB device
     // To avoid multiple E2 setup requests, we ensure each physical gNB only registers once
     NS_LOG_UNCOND("Calling UpdateConfig for " << gnbNetDev.GetN() << " gNB devices...");
     
     for (auto it = gnbNetDev.Begin(); it != gnbNetDev.End(); ++it)
     {
         Ptr<NrGnbNetDevice> gnbDev = DynamicCast<NrGnbNetDevice>(*it);
         NS_LOG_UNCOND("Updating config for gNB with Cell ID: " << gnbDev->GetCellId() 
                       << " (BWPs: " << gnbDev->GetCcMapSize() << ")");
         gnbDev->UpdateConfig();
     }

     for (auto it = ueNetDev.Begin(); it != ueNetDev.End(); ++it)
     {
         DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();
     }
     
     NS_LOG_UNCOND("UpdateConfig completed. E2 registration should be complete.");
 
     // create the Internet and install the IP stack on the UEs
     // get SGW/PGW and create a single RemoteHost
     Ptr<Node> pgw = nrEpcHelper->GetPgwNode();
     NodeContainer remoteHostContainer;
     remoteHostContainer.Create(1);
     Ptr<Node> remoteHost = remoteHostContainer.Get(0);
     InternetStackHelper internet;
     internet.Install(remoteHostContainer);
 
     // connect a remoteHost to pgw. Setup routing too
     PointToPointHelper p2ph;
     p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
     p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
     p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.000)));
     NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
     Ipv4AddressHelper ipv4h;
     Ipv4StaticRoutingHelper ipv4RoutingHelper;
     ipv4h.SetBase("1.0.0.0", "255.0.0.0");
     Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
     Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
         ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
     remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
     internet.Install(ueContainer);
     Ipv4InterfaceContainer ueIpIface =
         nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));
     // Set the default gateway for the UE
     Ptr<Ipv4StaticRouting> ueStaticRouting =
         ipv4RoutingHelper.GetStaticRouting(ueContainer.Get(0)->GetObject<Ipv4>());
     ueStaticRouting->SetDefaultRoute(nrEpcHelper->GetUeDefaultGatewayAddress(), 1);
 
    // attach each UE to its gNB according to desired scenario
    nrHelper->AttachToGnb(ueNetDev.Get(0), gnbNetDev.Get(0));
    if (enableInterfNode)
    {
        nrHelper->AttachToGnb(ueNetDev.Get(1), gnbNetDev.Get(1));

    }


    
    /**
     * Install DL traffic part.
     */
     uint16_t dlPort = 1234;
     ApplicationContainer serverApps;
     // The sink will always listen to the specified ports
     UdpServerHelper dlPacketSink(dlPort);
     // The server, that is the application which is listening, is installed in the UE
     serverApps.Add(dlPacketSink.Install(ueContainer));
     /**
      * Configure attributes for the CBR traffic generator, using user-provided
      * parameters
      */
     UdpClientHelper dlClient;
     dlClient.SetAttribute("RemotePort", UintegerValue(dlPort));
     dlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
     dlClient.SetAttribute("PacketSize", UintegerValue(udpPacketSize));
     dlClient.SetAttribute("Interval", TimeValue(packetInterval));
 
     // The bearer that will carry the traffic
     NrEpsBearer epsBearer(NrEpsBearer::NGBR_LOW_LAT_EMBB);
 
     // The filter for the traffic
     Ptr<NrEpcTft> dlTft = Create<NrEpcTft>();
     NrEpcTft::PacketFilter dlPktFilter;
     dlPktFilter.localPortStart = dlPort;
     dlPktFilter.localPortEnd = dlPort;
     dlTft->Add(dlPktFilter);
 
     /**
      * Let's install the applications!
      */
     ApplicationContainer clientApps;
 
     for (uint32_t i = 0; i < ueContainer.GetN(); ++i)
     {
         Ptr<Node> ue = ueContainer.Get(i);
         Ptr<NetDevice> ueDevice = ueNetDev.Get(i);
         Address ueAddress = ueIpIface.GetAddress(i);
 
         // The client, who is transmitting, is installed in the remote host,
         // with destination address set to the address of the UE
         dlClient.SetAttribute("RemoteAddress", AddressValue(ueAddress));
         clientApps.Add(dlClient.Install(remoteHost));
 
         // Activate a dedicated bearer for the traffic
         nrHelper->ActivateDedicatedEpsBearer(ueDevice, epsBearer, dlTft);
     }
 
     // start UDP server and client apps
     serverApps.Start(udpAppStartTime);
     clientApps.Start(udpAppStartTime);
     serverApps.Stop(simTime);
     clientApps.Stop(simTime);
         // ===================================================================
    // Dynamic Throughput Pattern: Changes every 20 seconds
    // Pattern: High -> Medium -> Low -> Medium -> High (repeats every 100s)
    // ===================================================================
    
    // Define throughput levels
    
    
     auto ChangeToMedium = [clientApps, mediumInterval]() {
        std::cout << "\n[Time " << Simulator::Now().GetSeconds() << "s] Changing throughput to MEDIUM (~160 Mbps)" << std::endl;
        for (uint32_t i = 0; i < clientApps.GetN(); ++i)
        {
            Ptr<Application> app = clientApps.Get(i);
            Ptr<UdpClient> udpClient = DynamicCast<UdpClient>(app);
            if (udpClient)
            {
                // Change interval - this will affect the next scheduled packet
                udpClient->SetAttribute("Interval", TimeValue(mediumInterval));
                std::cout << "  Changed interval for client " << i << " to " << mediumInterval.GetNanoSeconds() << " ns" << std::endl;
            }
        }
    };
    
    auto ChangeToLow = [clientApps, lowInterval]() {
        std::cout << "\n[Time " << Simulator::Now().GetSeconds() << "s] Changing throughput to LOW (~80 Mbps)" << std::endl;
        for (uint32_t i = 0; i < clientApps.GetN(); ++i)
        {
            Ptr<Application> app = clientApps.Get(i);
            Ptr<UdpClient> udpClient = DynamicCast<UdpClient>(app);
            if (udpClient)
            {
                udpClient->SetAttribute("Interval", TimeValue(lowInterval));
                std::cout << "  Changed interval for client " << i << " to " << lowInterval.GetNanoSeconds() << " ns" << std::endl;
            }
        }
    };
    
    auto ChangeToHigh = [clientApps, highInterval]() {
        std::cout << "\n[Time " << Simulator::Now().GetSeconds() << "s] Changing throughput to HIGH (~320 Mbps)" << std::endl;
        for (uint32_t i = 0; i < clientApps.GetN(); ++i)
        {
            Ptr<Application> app = clientApps.Get(i);
            Ptr<UdpClient> udpClient = DynamicCast<UdpClient>(app);
            if (udpClient)
            {
                udpClient->SetAttribute("Interval", TimeValue(highInterval));
                std::cout << "  Changed interval for client " << i << " to " << highInterval.GetNanoSeconds() << " ns" << std::endl;
            }
        }
    };

    // Schedule throughput changes (starting from udpAppStartTime)
    Time baseTime = udpAppStartTime;
    
    // Time gap between throughput level changes (5 seconds - good for testing)
    double timeGap = 5.0; // seconds
    double cycleDuration = timeGap * 5; // 25 seconds per cycle (High->Medium->Low->Medium->High)
    
    // Calculate how many cycles fit in the simulation
    double totalSeconds = (simTime - baseTime).GetSeconds();
    uint32_t numCycles = static_cast<uint32_t>(totalSeconds / cycleDuration) + 1;
    
    for (uint32_t cycle = 0; cycle < numCycles; ++cycle)
    {
        Time cycleStart = baseTime + Seconds(cycle * cycleDuration);
        
        // 0-5s: High throughput (initial, no change needed)
        // 5-10s: Medium throughput
        if (cycleStart + Seconds(timeGap) <= simTime)
        {
            Simulator::Schedule(cycleStart + Seconds(timeGap), ChangeToMedium);
        }
        
        // 10-15s: Low throughput
        if (cycleStart + Seconds(timeGap * 2) <= simTime)
        {
            Simulator::Schedule(cycleStart + Seconds(timeGap * 2), ChangeToLow);
        }
        
        // 15-20s: Medium throughput
        if (cycleStart + Seconds(timeGap * 3) <= simTime)
        {
            Simulator::Schedule(cycleStart + Seconds(timeGap * 3), ChangeToMedium);
        }
        
        // 20-25s: High throughput (back to max)
        if (cycleStart + Seconds(timeGap * 4) <= simTime)
        {
            Simulator::Schedule(cycleStart + Seconds(timeGap * 4), ChangeToHigh);
        }
    }
    
    // Log initial throughput level
    std::cout << "\n[Traffic Pattern] Starting with HIGH throughput (~320 Mbps)" << std::endl;
    std::cout << "[Traffic Pattern] Pattern repeats every " << cycleDuration << " seconds:" << std::endl;
    std::cout << "  - 0-" << timeGap << "s:    HIGH   (~320 Mbps)" << std::endl;
    std::cout << "  - " << timeGap << "-" << (timeGap*2) << "s:   MEDIUM (~160 Mbps)" << std::endl;
    std::cout << "  - " << (timeGap*2) << "-" << (timeGap*3) << "s:  LOW    (~80 Mbps)" << std::endl;
    std::cout << "  - " << (timeGap*3) << "-" << (timeGap*4) << "s:  MEDIUM (~160 Mbps)" << std::endl;
    std::cout << "  - " << (timeGap*4) << "-" << cycleDuration << "s: HIGH   (~320 Mbps)\n" << std::endl;
     // enable the traces provided by the nr module
     nrHelper->EnableTraces();
     // enable the traces provided by the nr module
nrHelper->EnableTraces();

    // ===================================================================
    // CRITICAL: Get PDCP stats calculator and set it on all gNB devices
    // This enables E2 PDCP statistics collection
    // ===================================================================
    Ptr<NrBearerStatsCalculator> pdcpStats = nrHelper->GetPdcpStatsCalculator();
    if (pdcpStats)
    {
        // Set start time to 0 so stats are recorded from the beginning
        pdcpStats->SetStartTime(Seconds(0.0));
        
        // Set the PDCP calculator on all gNB devices
        for (uint32_t i = 0; i < gnbNetDev.GetN(); ++i)
        {
            Ptr<NrGnbNetDevice> gnbDev = DynamicCast<NrGnbNetDevice>(gnbNetDev.Get(i));
            if (gnbDev)
            {
                gnbDev->SetAttribute("E2PdcpCalculator", PointerValue(pdcpStats));
                NS_LOG_UNCOND("Set PDCP stats calculator on gNB " << i << " (Cell ID: " << gnbDev->GetCellId() << ")");
            }
        }
    }
    else
    {
        NS_LOG_WARN("PDCP Stats Calculator is NULL - E2 PDCP reporting will not work!");
    }

    nrHelper->StartEnergyMonitoring();

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost);
    endpointNodes.Add(ueContainer);

    struct timeval time_now{};
    gettimeofday(&time_now, nullptr);
    t_startTime_simid = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);

    // Install FlowMonitor and get classifier
    Ptr<FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));
    
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    Simulator::Schedule(MilliSeconds(0), &SetFlowMonitorOnAllGnbDevices, monitor, classifier);

    // Initialize tracking files
    std::string ue_poss_out = "ue_position.txt";
    std::string gnbs_out = "gnbs.txt";
    ClearFile(gnbs_out, t_startTime_simid);
    ClearFile(ue_poss_out, t_startTime_simid);


    // Schedule position tracking for GUI
    double simTime_dbl = simTime.GetSeconds();
    int numPrints = simTime_dbl / 0.1;
    
    Simulator::Schedule(Seconds(0.1), &PrintGnuplottableEnbListToFile);
    for (int i = 1; i < numPrints; i++) {
        for (uint32_t j = 0; j < ueContainer.GetN(); j++) {
            Simulator::Schedule(Seconds(i * simTime.GetSeconds() / numPrints),
                               &PrintPosition,
                               ueContainer.Get(j),
                               ue_poss_out);
        }
    }

   Simulator::Stop(simTime);
   Simulator::Run();
 
     // Print per-flow statistics
     monitor->CheckForLostPackets();
     FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
 
     double averageFlowThroughput = 0.0;
     double averageFlowDelay = 0.0;
 
     std::ofstream outFile;
     std::string filename = outputDir + "/" + simTag;
     outFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
     if (!outFile.is_open())
     {
         std::cerr << "Can't open file " << filename << std::endl;
         return 1;
     }
 
     outFile.setf(std::ios_base::fixed);
 
     double flowDuration = (simTime - udpAppStartTime).GetSeconds();
     for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
          i != stats.end();
          ++i)
     {
         Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
         std::stringstream protoStream;
         protoStream << (uint16_t)t.protocol;
         if (t.protocol == 6)
         {
             protoStream.str("TCP");
         }
         if (t.protocol == 17)
         {
             protoStream.str("UDP");
         }
         outFile << "Flow " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
                 << t.destinationAddress << ":" << t.destinationPort << ") proto "
                 << protoStream.str() << "\n";
         outFile << "  Tx Packets: " << i->second.txPackets << "\n";
         outFile << "  Tx Bytes:   " << i->second.txBytes << "\n";
         outFile << "  TxOffered:  " << i->second.txBytes * 8.0 / flowDuration / 1000.0 / 1000.0
                 << " Mbps\n";
         outFile << "  Rx Bytes:   " << i->second.rxBytes << "\n";
         if (i->second.rxPackets > 0)
         {
             // Measure the duration of the flow from receiver's perspective
             averageFlowThroughput += i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000;
             averageFlowDelay += 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets;
 
             outFile << "  Throughput: " << i->second.rxBytes * 8.0 / flowDuration / 1000 / 1000
                     << " Mbps\n";
             outFile << "  Mean delay:  "
                     << 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets << " ms\n";
             // outFile << "  Mean upt:  " << i->second.uptSum / i->second.rxPackets / 1000/1000 << "
             // Mbps \n";
             outFile << "  Mean jitter:  "
                     << 1000 * i->second.jitterSum.GetSeconds() / i->second.rxPackets << " ms\n";
         }
         else
         {
             outFile << "  Throughput:  0 Mbps\n";
             outFile << "  Mean delay:  0 ms\n";
             outFile << "  Mean jitter: 0 ms\n";
         }
         outFile << "  Rx Packets: " << i->second.rxPackets << "\n";
     }
 
    outFile << "\n\n  Mean flow throughput: " << averageFlowThroughput / stats.size() << "\n";
    outFile << "  Mean flow delay: " << averageFlowDelay / stats.size() << "\n";
    outFile.close();
    
    // ================== SIMULATION SUMMARY ==================
    std::cout << "\n=== NR MIMO SIMULATION COMPLETE ===" << std::endl;
    std::cout << "Performance Summary:" << std::endl;
    std::cout << "   Average Throughput: " << std::setprecision(2) << averageFlowThroughput / stats.size() << " Mbps" << std::endl;
    std::cout << "   Average Delay: " << std::setprecision(2) << averageFlowDelay / stats.size() << " ms" << std::endl;
    std::cout << "   Simulation Time: " << simTime.GetSeconds() << " seconds" << std::endl;
    std::cout << "   Output File: " << filename << std::endl;
    
    std::ifstream f(filename.c_str());
    if (f.is_open())
    {
        std::cout << "\n" << f.rdbuf();
    }
 
     Simulator::Destroy();
     return 0;
 }
