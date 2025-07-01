// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

/**
 * \ingroup examples
 * \file cttc-nr-demo.cc
 * \brief A cozy, simple, NR demo (in a tutorial style)
 *
 * Notice: this entire program uses technical terms defined by the 3GPP TS 38.300 [1].
 *
 * This example describes how to setup a simulation using the 3GPP channel model from TR 38.901 [2].

 *
 * With the default configuration, the example will create two flows that will
 * go through two different subband numerologies (or bandwidth parts). For that,
 * specifically, two bands are created, each with a single CC, and each CC containing
 * one bandwidth part.
 *
 * The example will print on-screen the end-to-end result of one (or two) flows,
 * as well as writing them on a file.
 *
 * \code{.unparsed}
$ ./ns3 run "cttc-nr-demo --PrintHelp"
    \endcode
 *
 */

// NOLINTBEGIN
// clang-format off

/**
 * Useful references that will be used for this tutorial:
 * [1] <a href="https://portal.3gpp.org/desktopmodules/Specifications/SpecificationDetails.aspx?specificationId=3191">3GPP TS 38.300</a>
 * [2] <a href="https://portal.3gpp.org/desktopmodules/Specifications/SpecificationDetails.aspx?specificationId=3173">3GPP channel model from TR 38.901</a>
 * [3] <a href="https://www.nsnam.org/docs/release/3.38/tutorial/html/tweaking.html#using-the-logging-module">ns-3 documentation</a>
 */

/**
 * Modified by:
 * Kamil Kociszewski <kamil.kociszewski@orange.com> // Modified to work with RIC TaaP GUI
 */

// clang-format on
// NOLINTEND

/*
 * Include part. Often, you will have to include the headers for an entire module;
 * do that by including the name of the module you need with the suffix "-module.h".
 */

#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/buildings-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-module.h"
#include <string>
/*
 * Use, always, the namespace ns3. All the NR classes are inside such namespace.
 */
using namespace ns3;
using namespace nr;
/*
 * With this line, we will be able to see the logs of the file by enabling the
 * component "CttcNrDemo".
 * Further information on how logging works can be found in the ns-3 documentation [3].
 */
NS_LOG_COMPONENT_DEFINE("rfchannel");
static ns3::GlobalValue g_e2TermIp ("e2TermIp", "The IP address of the RIC E2 termination",
                                    ns3::StringValue ("127.0.0.1"), ns3::MakeStringChecker ());
static ns3::GlobalValue g_e2_func_id("KPM_E2functionID", "Function ID to subscribe",
                                     ns3::DoubleValue(2),
                                     ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_rc_e2_func_id("RC_E2functionID", "Function ID to subscribe",
                                        ns3::DoubleValue(3),
                                        ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_e2nrEnabled("e2nrEnabled", "If true, send NR E2 reports",
                                      ns3::BooleanValue(true), ns3::MakeBooleanChecker());
static ns3::GlobalValue
        g_enableE2FileLogging("enableE2FileLogging",
                              "If true, generate offline file logging instead of connecting to RIC",
                              ns3::BooleanValue(true), ns3::MakeBooleanChecker());
uint64_t t_startTime_simid;
double maxXAxis;
double maxYAxis;
int cell_it = 0;

void
PrintGnuplottableEnbListToFile() {
    uint64_t timestamp = t_startTime_simid + (uint64_t) Simulator::Now().GetMilliSeconds();
    std::string filename1 = "gnbs.txt";
    Ptr <NrHelper> nrHelper = CreateObject<NrHelper>();
    //int cell_it = 0;
    for (NodeList::Iterator it = NodeList::Begin(); it != NodeList::End(); ++it) {
        Ptr <Node> node = *it;
        int nDevs = node->GetNDevices();

        for (int j = 0; j < nDevs; j++) {
            Ptr <NrGnbNetDevice> nrdev = node->GetDevice(j)->GetObject<NrGnbNetDevice>();
            if (!nrdev) {
                //NS_LOG_UNCOND("Not a NR gNB device" << node->GetId() << " " << j);
                continue;
            }

            Vector pos = node->GetObject<MobilityModel>()->GetPosition();
            std::ofstream outFile1;

            // Open the output file with the full path
            outFile1.open(filename1, std::ios_base::out | std::ios_base::app);

            if (!outFile1.is_open()) {
                NS_LOG_ERROR("Can't open file " << filename1);
                return;
            }
            outFile1 << timestamp << "," << nrdev->GetCellId() - cell_it << "," << pos.x << "," << pos.y << ","
                     << t_startTime_simid << "," << 0 << ","
                     << 0 << "," << 0
                     << "," << 0 << std::endl;

            outFile1.close();
            cell_it++;
        }
    }
}

void
ClearFile(std::string Filename, uint64_t m_startTime) {
    std::string filename = Filename;
    std::ofstream outFile;
    outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::trunc);
    if (!outFile.is_open()) {
        NS_LOG_ERROR("Can't open file " << filename);
        return;
    }
    outFile.close();
    //  struct timeval time_now{};
    //  gettimeofday (&time_now, nullptr);
    //uint64_t m_startTime = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
    uint64_t timestamp = m_startTime + (uint64_t) Simulator::Now().GetMilliSeconds();
    std::ofstream outFile1;
    outFile1.open(filename.c_str(), std::ios_base::out | std::ios_base::app);

    if (Filename == "ue_position.txt") {
        outFile1 << "timestamp,id,x,y,type,cell,simid" << std::endl;
    } else {
        outFile1 << "timestamp,id,x,y,simid,ESstate,currEC,maxEC,totalcurrEC" << std::endl;
        outFile1 << timestamp << "," << "0" << "," << maxXAxis << "," << maxYAxis << std::endl;
    }
    outFile1.close();
}

void
PrintPosition(Ptr <Node> node, std::string Filename) {
    uint64_t timestamp = t_startTime_simid + (uint64_t) Simulator::Now().GetMilliSeconds();

    int imsi;
    int nDevs = node->GetNDevices();

    std::string filename = Filename;
    std::ofstream outFile;

    for (int j = 0; j < nDevs; j++) {
        Ptr <NrUeNetDevice> nruedev = node->GetDevice(j)->GetObject<NrUeNetDevice>();
        if (nruedev) {
            imsi = int(nruedev->GetImsi());
            int serving_cell = nruedev->GetCellId();

            Ptr <MobilityModel> model = node->GetObject<MobilityModel>();
            Vector position = model->GetPosition();
            Vector velocity = model->GetVelocity();
            double speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y +
                                     velocity.z * velocity.z); // speed in m/s
            double speedKmh = speed * 3.6;
            speedKmh = std::round(speedKmh * 10.0) / 10.0;
            int gui_ue_id = imsi - cell_it + 1;

            // Log position and SINR
            NS_LOG_UNCOND(std::noshowpos << std::fixed << std::setprecision(2)
                                         << "Position of UE with IMSI " << std::dec << static_cast<uint32_t>(imsi) << " (UE_ID in GUI: " <<
                              static_cast<uint32_t>(gui_ue_id) << ") is " << position.x << ":" << position.y << ":" << position.z
                                         << ", Speed: " << speedKmh << " km/h"
                                         << " at time " << Simulator::Now().GetSeconds()
                                         << ", UE connected to Cell: " << std::dec << static_cast<uint32_t>(serving_cell));


            outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::app);
            if (!outFile.is_open()) {
                NS_LOG_ERROR("Can't open file " << filename);
                return;
            }

            outFile << timestamp << "," << static_cast<uint32_t>(gui_ue_id) << "," << position.x << "," << position.y
                    << ",nr,"
                    << static_cast<uint32_t>((serving_cell - 1) / 2 + 1) << "," << t_startTime_simid << std::endl;

            outFile.close();
        }
    }
}

int
main(int argc, char* argv[])
{
       LogComponentEnableAll (LOG_PREFIX_ALL);
       LogComponentEnable ("E2Termination", LOG_LEVEL_ALL);
       LogComponentEnable ("rfchannel", LOG_LEVEL_ALL);
        //LogComponentEnable("NrGnbNetDevice", LOG_LEVEL_ALL);
    /*
     * Variables that represent the parameters we will accept as input by the
     * command line. Each of them is initialized with a default value, and
     * possibly overridden below when command-line arguments are parsed.
     */
    // Scenario parameters (that we will use inside this script):
    uint16_t gNbNum = 1;
    uint16_t N_Ues = 3;
    bool logging = true;
    bool doubleOperationalBand = false;

    // Traffic parameters (that we will use inside this script):
    uint32_t udpPacketSizeULL = 1500;
    uint32_t udpPacketSizeBe = 1252;
    uint32_t lambdaULL = 10000;
    uint32_t lambdaBe = 10000;

    maxXAxis = 4000;
    // The maximum Y coordinate of the scenario
    maxYAxis = 4000;

    // Simulation parameters. Please don't use double to indicate seconds; use
    // ns-3 Time values which use integers to avoid portability issues.
    Time simTime = MilliSeconds(10000);
    Time udpAppStartTime = MilliSeconds(400);

    // NR parameters (Reference: 3GPP TR 38.901 V17.0.0 (Release 17)
    // Table 7.8-1 for the power and BW).
    // In this example the BW has been split into two BWPs
    // We will take the input from the command line, and then we
    // will pass them inside the NR module.
    uint16_t numerologyBwp1 = 4;
    double centralFrequencyBand1 = 28e9;
    double bandwidthBand1 = 50e6;
    uint16_t numerologyBwp2 = 2;
    double centralFrequencyBand2 = 28.2e9;
    double bandwidthBand2 = 50e6;
    double totalTxPower = 35;

    //GUI related flags
    bool enableE2FileLogging;
    double hoSinrDifference = 1000;
    double indicationPeriodicity = 0.1;
    double g_e2_func_id = 2;
    double g_rc_e2_func_id = 3;
    double isd_ue = 500;
    double isd_cell = 500;

    // Where we will store the output files.
    std::string simTag = "default";
    std::string outputDir = "./";

    /*
     * From here, we instruct the ns3::CommandLine class of all the input parameters
     * that we may accept as input, as well as their description, and the storage
     * variable.
     */
    CommandLine cmd(__FILE__);
    cmd.AddValue("enableMimoFeedback", "ns3::NrHelper::EnableMimoFeedback");
    cmd.AddValue("pmSearchMethod", "ns3::NrHelper::PmSearchMethod");
    cmd.AddValue("fullSearchCb", "ns3::NrPmSearchFull::CodebookType");
    cmd.AddValue("rankLimit", "ns3::NrPmSearch::RankLimit");
    cmd.AddValue("subbandSize", "ns3::NrPmSearch::SubbandSize");
    cmd.AddValue("downsamplingTechnique", "ns3::NrPmSearch::DownsamplingTechnique");
    //cmd.AddValue("gNbNum", "The number of gNbs in multiple-ue topology", gNbNum);
    cmd.AddValue("N_MmWaveEnbNodes", "The number of gNbs in multiple-ue topology", gNbNum);
    // cmd.AddValue("ueNumPergNb", "The number of UE per gNb in multiple-ue topology", ueNumPergNb);
    cmd.AddValue("N_Ues", "The number of UE per gNb in multiple-ue topology", N_Ues);
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.AddValue("doubleOperationalBand",
                 "If true, simulate two operational bands with one CC for each band,"
                 "and each CC will have 1 BWP that spans the entire CC.",
                 doubleOperationalBand);
    cmd.AddValue("packetSizeUll",
                 "packet size in bytes to be used by ultra low latency traffic",
                 udpPacketSizeULL);
    cmd.AddValue("packetSizeBe",
                 "packet size in bytes to be used by best effort traffic",
                 udpPacketSizeBe);
    cmd.AddValue("lambdaUll",
                 "Number of UDP packets in one second for ultra low latency traffic",
                 lambdaULL);
    cmd.AddValue("lambdaBe",
                 "Number of UDP packets in one second for best effort traffic",
                 lambdaBe);
    cmd.AddValue("simTime", "Simulation time", simTime);
    cmd.AddValue("numerologyBwp1", "The numerology to be used in bandwidth part 1", numerologyBwp1);
    cmd.AddValue("CenterFrequency",
                 "The system frequency to be used in band 1",
                 centralFrequencyBand1);
    // cmd.AddValue("bandwidthBand1", "The system bandwidth to be used in band 1", bandwidthBand1);
    cmd.AddValue("Bandwidth", "The system bandwidth to be used in band 1", bandwidthBand1);
    cmd.AddValue("numerologyBwp2", "The numerology to be used in bandwidth part 2", numerologyBwp2);
    cmd.AddValue("centralFrequencyBand2",
                 "The system frequency to be used in band 2",
                 centralFrequencyBand2);
    cmd.AddValue("bandwidthBand2", "The system bandwidth to be used in band 2", bandwidthBand2);
    cmd.AddValue("totalTxPower",
                 "total tx power that will be proportionally assigned to"
                 " bands, CCs and bandwidth parts depending on each BWP bandwidth ",
                 totalTxPower);
    cmd.AddValue("simTag",
                 "tag to be appended to output filenames to distinguish simulation campaigns",
                 simTag);
    cmd.AddValue("outputDir", "directory where to store simulation results", outputDir);
    // GUI related flags
    cmd.AddValue("enableE2FileLogging", "Do not use E2 interface, instead, produce file trace for KPIs",
                 enableE2FileLogging);
    cmd.AddValue("hoSinrDifference", "hoSinrDifference", hoSinrDifference); // not implemented in Lena
    cmd.AddValue("indicationPeriodicity", "E2 Indication Periodicity reports (value in seconds)",
                 indicationPeriodicity); // not implemented in Lena
    cmd.AddValue("KPM_E2functionID", "Function ID to subscribe)", g_e2_func_id);
    cmd.AddValue("RC_E2functionID", "Function ID to subscribe)", g_rc_e2_func_id);
    cmd.AddValue("IntersideDistanceUEs", "Interside Distance Value",
                 isd_ue); //not supported due to GRID allocation
    cmd.AddValue("IntersideDistanceCells", "Interside Distance Value",
                 isd_cell); //not supported due to GRID allocation
    // Parse the command line
    cmd.Parse(argc, argv);


    StringValue stringValue;
    BooleanValue booleanValue;
    DoubleValue doubleValue;


    GlobalValue::GetValueByName("e2nrEnabled", booleanValue);
    bool e2nrEnabled = booleanValue.Get();
    GlobalValue::GetValueByName("e2TermIp", stringValue);
    std::string e2TermIp = stringValue.Get();


    e2nrEnabled = enableE2FileLogging;

    Config::SetDefault("ns3::NrGnbNetDevice::KPM_E2functionID",
                       DoubleValue(g_e2_func_id));
    Config::SetDefault("ns3::NrGnbNetDevice::RC_E2functionID",
                       DoubleValue(g_rc_e2_func_id));

    Config::SetDefault("ns3::NrHelper::E2TermIp", StringValue(e2TermIp));
    Config::SetDefault("ns3::NrHelper::E2ModeNr", BooleanValue(e2nrEnabled));

    /*
     * Check if the frequency is in the allowed range.
     * If you need to add other checks, here is the best position to put them.
     */
    NS_ABORT_IF(centralFrequencyBand1 < 0.5e9 && centralFrequencyBand1 > 100e9);
    NS_ABORT_IF(centralFrequencyBand2 < 0.5e9 && centralFrequencyBand2 > 100e9);

    /*
     * If the logging variable is set to true, enable the log of some components
     * through the code. The same effect can be obtained through the use
     * of the NS_LOG environment variable:
     *
     * export NS_LOG="UdpClient=level_info|prefix_time|prefix_func|prefix_node:UdpServer=..."
     *
     * Usually, the environment variable way is preferred, as it is more customizable,
     * and more expressive.
     */
    if (logging)
    {
       //LogComponentEnable("UdpClient", LOG_LEVEL_ALL);
    //   LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    //   LogComponentEnable("NrPdcp", LOG_LEVEL_INFO);
      LogComponentEnable ("E2Termination", LOG_LEVEL_ALL);
    //  LogComponentEnable("NrGnbNetDevice", LOG_LEVEL_ALL);
    }

    /*
     * In general, attributes for the NR module are typically configured in NrHelper.  However, some
     * attributes need to be configured globally through the Config::SetDefault() method. Below is
     * an example: if you want to make the RLC buffer very large, you can pass a very large integer
     * here.
     */
    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(999999999));

    int64_t randomStream = 2;

    NodeContainer gnbContainer;
    gnbContainer.Create(gNbNum);
    NodeContainer ueContainer;
    ueContainer.Create(N_Ues);
    // Position
    Vector centerPosition = Vector(maxXAxis / 2, maxYAxis / 2, 3);

    // Install Mobility Model
    Ptr <ListPositionAllocator> gnbPositionAlloc = CreateObject<ListPositionAllocator>();

    // We want a center with one LTE enb and one mmWave co-located in the same place
    gnbPositionAlloc->Add(centerPosition);
    if (gNbNum != 1) {
        double nConstellation = gNbNum - 1;
        for (int8_t i = 0; i < nConstellation; ++i) {
            double x_pos, y_pos;
            x_pos = isd_cell * cos((2 * M_PI * i) / (nConstellation));
            y_pos = isd_cell * sin((2 * M_PI * i) / (nConstellation));
            gnbPositionAlloc->Add(Vector(centerPosition.x + x_pos, centerPosition.y + y_pos, 3));
        }
    }
    // This guarantee that each of the rest BSs is placed at the same distance from the two co-located in the center


    MobilityHelper gnbmobility;
    gnbmobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbmobility.SetPositionAllocator(gnbPositionAlloc);
    gnbmobility.Install(gnbContainer);


    MobilityHelper uemobility;
    Ptr <UniformDiscPositionAllocator> uePositionAlloc = CreateObject<UniformDiscPositionAllocator>();
    uePositionAlloc->SetX(centerPosition.x);
    uePositionAlloc->SetY(centerPosition.y);
    uePositionAlloc->SetRho(isd_ue);
    // Configure mobility model: leave it as random walk model
    Ptr <UniformRandomVariable> speed = CreateObject<UniformRandomVariable>();
    speed->SetAttribute("Min", DoubleValue(2.0));
    speed->SetAttribute("Max", DoubleValue(4.0));
    uemobility.SetMobilityModel("ns3::RandomWalk2dOutdoorMobilityModel", "Speed",
                                PointerValue(speed), "Bounds",
                                RectangleValue(Rectangle(0, maxXAxis, 0, maxYAxis)));
    // Set initial positions using the position allocator
    uemobility.SetPositionAllocator(uePositionAlloc);
    // Install the mobility model on UEs
    uemobility.Install(ueContainer);


    /*
     * Create two different NodeContainer for the different traffic type.
     * In ueLowLat we will put the UEs that will receive low-latency traffic,
     * while in ueVoice we will put the UEs that will receive the voice traffic.
     */
    NodeContainer ueLowLatContainer;
    NodeContainer ueVoiceContainer;


    for (uint32_t j = 0; j < ueContainer.GetN(); ++j) {
        Ptr <Node> ueNode = ueContainer.Get(j);
        if (j % 2 == 0) {
            ueLowLatContainer.Add(ueNode);
        } else {
            ueVoiceContainer.Add(ueNode);
        }
    }

/*
 * TODO: Add a print, or a plot, that shows the scenario.
 */

    /*
     * Setup the NR module. We create the various helpers needed for the
     * NR simulation:
     * - nrEpcHelper, which will setup the core network
     * - IdealBeamformingHelper, which takes care of the beamforming part
     * - NrHelper, which takes care of creating and connecting the various
     * part of the NR stack
     */
    Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // Put the pointers inside nrHelper
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);

    /*
     * Spectrum division. We create two operational bands, each of them containing
     * one component carrier, and each CC containing a single bandwidth part
     * centered at the frequency specified by the input parameters.
     * Each spectrum part length is, as well, specified by the input parameters.
     * Both operational bands will use the StreetCanyon channel modeling.
     */
    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1; // in this example, both bands have a single CC

    // Create the configuration for the CcBwpHelper. SimpleOperationBandConf creates
    // a single BWP per CC
    CcBwpCreator::SimpleOperationBandConf bandConf1(centralFrequencyBand1,
                                                    bandwidthBand1,
                                                    numCcPerBand,
                                                    BandwidthPartInfo::UMi_StreetCanyon);
    CcBwpCreator::SimpleOperationBandConf bandConf2(centralFrequencyBand2,
                                                    bandwidthBand2,
                                                    numCcPerBand,
                                                    BandwidthPartInfo::UMi_StreetCanyon);

    // By using the configuration created, it is time to make the operation bands
    OperationBandInfo band1 = ccBwpCreator.CreateOperationBandContiguousCc(bandConf1);
    OperationBandInfo band2 = ccBwpCreator.CreateOperationBandContiguousCc(bandConf2);

    /*
     * The configured spectrum division is:
     * ------------Band1--------------|--------------Band2-----------------
     * ------------CC1----------------|--------------CC2-------------------
     * ------------BWP1---------------|--------------BWP2------------------
     */

    /*
     * Attributes of ThreeGppChannelModel still cannot be set in our way.
     * TODO: Coordinate with Tommaso
     */
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(0)));
    nrHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    nrHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));

    /*
     * Initialize channel and pathloss, plus other things inside band1. If needed,
     * the band configuration can be done manually, but we leave it for more
     * sophisticated examples. For the moment, this method will take care
     * of all the spectrum initialization needs.
     */
    nrHelper->InitializeOperationBand(&band1);

    /*
     * Start to account for the bandwidth used by the example, as well as
     * the total power that has to be divided among the BWPs.
     */
    double x = pow(10, totalTxPower / 10);
    double totalBandwidth = bandwidthBand1;

    /*
     * if not single band simulation, initialize and setup power in the second band
     */
    if (doubleOperationalBand)
    {
        // Initialize channel and pathloss, plus other things inside band2
        nrHelper->InitializeOperationBand(&band2);
        totalBandwidth += bandwidthBand2;
        allBwps = CcBwpCreator::GetAllBwps({band1, band2});
    }
    else
    {
        allBwps = CcBwpCreator::GetAllBwps({band1});
    }

    /*
     * allBwps contains all the spectrum configuration needed for the nrHelper.
     *
     * Now, we can setup the attributes. We can have three kind of attributes:
     * (i) parameters that are valid for all the bandwidth parts and applies to
     * all nodes, (ii) parameters that are valid for all the bandwidth parts
     * and applies to some node only, and (iii) parameters that are different for
     * every bandwidth parts. The approach is:
     *
     * - for (i): Configure the attribute through the helper, and then install;
     * - for (ii): Configure the attribute through the helper, and then install
     * for the first set of nodes. Then, change the attribute through the helper,
     * and install again;
     * - for (iii): Install, and then configure the attributes by retrieving
     * the pointer needed, and calling "SetAttribute" on top of such pointer.
     *
     */

    Packet::EnableChecking();
    Packet::EnablePrinting();

    /*
     *  Case (i): Attributes valid for all the nodes
     */
    // Beamforming method
    idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                         TypeIdValue(DirectPathBeamforming::GetTypeId()));

    // Core latency
    nrEpcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));

//==============================================================================

    // Antennas for all the UEs
    // Antennas for all the gNbs
    //Config::SetDefault("ns3::NrHelper::EnableMimoFeedback", BooleanValue(true));
   // Config::SetDefault("ns3::NrPmSearch::SubbandSize", UintegerValue(16));
   // bool useMimoPmiParams = false ;


   Ptr<ThreeGppAntennaModel> Ue_antennaElem = CreateObject<ThreeGppAntennaModel>();
   uint16_t Ue_nAntCols = 2;
   uint16_t Ue_nAntRows = 2;
   uint16_t Ue_nHorizPorts = 2;
  uint16_t  Ue_nVertPorts = 1;
  bool  Ue_isDualPolarized = false;
  Ptr<ThreeGppAntennaModel> Gnb_antennaElem = CreateObject<ThreeGppAntennaModel>();
  uint16_t  Gnb_nAntCols = 4;
  uint16_t   Gnb_nAntRows = 2;
  uint16_t   Gnb_nHorizPorts = 2;
  uint16_t    Gnb_nVertPorts = 1;
  bool    Gnb_isDualPolarized = false;
    // The polarization slant angle in degrees in case of x-polarized
     double polSlantAngleGnb = 0.0;
     double polSlantAngleUe = 90.0;
    // The bearing angles in degrees
    double bearingAngleGnb = 0.0;
    double bearingAngleUe = 180.0;

    nrHelper->SetUeAntennaAttribute("AntennaElement", PointerValue(Ue_antennaElem));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(Ue_nAntCols));
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(Ue_nAntRows));
    nrHelper->SetUeAntennaAttribute("IsDualPolarized", BooleanValue(Ue_isDualPolarized));
    nrHelper->SetUeAntennaAttribute("NumHorizontalPorts", UintegerValue(Ue_nHorizPorts));
    nrHelper->SetUeAntennaAttribute("NumVerticalPorts", UintegerValue(Ue_nVertPorts));
    nrHelper->SetUeAntennaAttribute("BearingAngle", DoubleValue(bearingAngleUe* (M_PI / 180)));
    nrHelper->SetUeAntennaAttribute("PolSlantAngle", DoubleValue(polSlantAngleUe* (M_PI / 180)));


    nrHelper->SetGnbAntennaAttribute("AntennaElement", PointerValue(Gnb_antennaElem));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(Gnb_nAntCols));
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(Gnb_nAntRows));
    nrHelper->SetGnbAntennaAttribute("IsDualPolarized", BooleanValue(Gnb_isDualPolarized));
    nrHelper->SetGnbAntennaAttribute("NumHorizontalPorts", UintegerValue(Gnb_nHorizPorts));
    nrHelper->SetGnbAntennaAttribute("NumVerticalPorts", UintegerValue(Gnb_nVertPorts));
    nrHelper->SetGnbAntennaAttribute("BearingAngle", DoubleValue(bearingAngleGnb* (M_PI / 180)));
    nrHelper->SetGnbAntennaAttribute("PolSlantAngle", DoubleValue(polSlantAngleGnb* (M_PI / 180)));
//==============================================================================


/*

    // Antennas for all the UEs
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
   nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));






    // Antennas for all the gNbs
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));
*/
    uint32_t bwpIdForLowLat = 0;
    uint32_t bwpIdForVoice = 0;

    if (doubleOperationalBand)
    {
        bwpIdForVoice = 1;
        bwpIdForLowLat = 0;
    }

    // gNb routing between Bearer and bandwidh part
    nrHelper->SetGnbBwpManagerAlgorithmAttribute("NGBR_VIDEO_TCP_DEFAULT",
                                                 UintegerValue(bwpIdForLowLat));
    nrHelper->SetGnbBwpManagerAlgorithmAttribute("GBR_CONV_VOICE", UintegerValue(bwpIdForVoice));

    // Ue routing between Bearer and bandwidth part
    nrHelper->SetUeBwpManagerAlgorithmAttribute("NGBR_VIDEO_TCP_DEFAULT", UintegerValue(bwpIdForLowLat));
    nrHelper->SetUeBwpManagerAlgorithmAttribute("GBR_CONV_VOICE", UintegerValue(bwpIdForVoice));

    /*
     * We miss many other parameters. By default, not configuring them is equivalent
     * to use the default values. Please, have a look at the documentation to see
     * what are the default values for all the attributes you are not seeing here.
     */

    /*
     * Case (ii): Attributes valid for a subset of the nodes
     */

    // NOT PRESENT IN THIS SIMPLE EXAMPLE

    /*
     * We have configured the attributes we needed. Now, install and get the pointers
     * to the NetDevices, which contains all the NR stack:
     */

    NetDeviceContainer gnbNetDev =
            nrHelper->InstallGnbDevice(gnbContainer, allBwps);
    NetDeviceContainer ueLowLatNetDev = nrHelper->InstallUeDevice(ueLowLatContainer, allBwps);
    NetDeviceContainer ueVoiceNetDev = nrHelper->InstallUeDevice(ueVoiceContainer, allBwps);

    randomStream += nrHelper->AssignStreams(gnbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueLowLatNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueVoiceNetDev, randomStream);
    /*
     * Case (iii): Go node for node and change the attributes we have to setup
     * per-node.
     */

    // Get the first netdevice (gnbNetDev.Get (0)) and the first bandwidth part (0)
    // and set the attribute.
    nrHelper->GetGnbPhy(gnbNetDev.Get(0), 0)
        ->SetAttribute("Numerology", UintegerValue(numerologyBwp1));
    nrHelper->GetGnbPhy(gnbNetDev.Get(0), 0)
        ->SetAttribute("TxPower", DoubleValue(10 * log10((bandwidthBand1 / totalBandwidth) * x)));

    if (doubleOperationalBand)
    {
        // Get the first netdevice (gnbNetDev.Get (0)) and the second bandwidth part (1)
        // and set the attribute.
        nrHelper->GetGnbPhy(gnbNetDev.Get(0), 1)
            ->SetAttribute("Numerology", UintegerValue(numerologyBwp2));
        nrHelper->GetGnbPhy(gnbNetDev.Get(0), 1)
            ->SetTxPower(10 * log10((bandwidthBand2 / totalBandwidth) * x));
    }

    // When all the configuration is done, explicitly call UpdateConfig ()
    // Instead of calling individually for each netDevice, we can call
    // NrHelper::UpdateDeviceConfigs() to update a NetDeviceContainer with a single call. This was
    // introduced with the v.3.2 Release.
    nrHelper->UpdateDeviceConfigs(gnbNetDev);
    nrHelper->UpdateDeviceConfigs(ueLowLatNetDev);
    nrHelper->UpdateDeviceConfigs(ueVoiceNetDev);

    // From here, it is standard NS3. In the future, we will create helpers
    // for this part as well.

    // create the internet and install the IP stack on the UEs
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
    Ptr <Ipv4StaticRouting> remoteHostStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->
            AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"),
                              1);
    internet.Install(ueContainer);

    Ipv4InterfaceContainer ueLowLatIpIface =
        nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLowLatNetDev));
    Ipv4InterfaceContainer ueVoiceIpIface =
        nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueVoiceNetDev));

// Set the default gateway for the UEs
    for (uint32_t u = 0; u < ueContainer.GetN(); ++u) {
        Ptr <Node> ueNode = ueContainer.Get(u);
        // Set the default gateway for the UE
        Ptr <Ipv4StaticRouting> ueStaticRouting =
                ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(nrEpcHelper->GetUeDefaultGatewayAddress(), 1);
    }

// attach UEs to the closest gNB
    nrHelper->
            AttachToClosestGnb(ueLowLatNetDev, gnbNetDev
    );
    nrHelper->AttachToClosestGnb(ueVoiceNetDev, gnbNetDev);

    /*
     * Traffic part. Install two kind of traffic: low-latency and voice, each
     * identified by a particular source port.
     */
    uint16_t dlPortLowLat = 1234;
    uint16_t dlPortVoice = 1235;

    ApplicationContainer serverApps;

    // The sink will always listen to the specified ports
    UdpServerHelper dlPacketSinkLowLat(dlPortLowLat);
    UdpServerHelper dlPacketSinkVoice(dlPortVoice);

    // The server, that is the application which is listening, is installed in the UE
    serverApps.Add(dlPacketSinkLowLat.Install(ueLowLatContainer));
   serverApps.Add(dlPacketSinkVoice.Install(ueVoiceContainer));

    /*
     * Configure attributes for the different generators, using user-provided
     * parameters for generating a CBR traffic
     *
     * Low-Latency configuration and object creation:
     */
    UdpClientHelper dlClientLowLat;
    dlClientLowLat.SetAttribute("RemotePort", UintegerValue(dlPortLowLat));
    dlClientLowLat.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    dlClientLowLat.SetAttribute("PacketSize", UintegerValue(udpPacketSizeULL));
    dlClientLowLat.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambdaULL)));

    // The bearer that will carry low latency traffic
    NrEpsBearer lowLatBearer(NrEpsBearer::NGBR_LOW_LAT_EMBB);

    // The filter for the low-latency traffic
    Ptr<NrEpcTft> lowLatTft = Create<NrEpcTft>();
    NrEpcTft::PacketFilter dlpfLowLat;
    dlpfLowLat.localPortStart = dlPortLowLat;
    dlpfLowLat.localPortEnd = dlPortLowLat;
    lowLatTft->Add(dlpfLowLat);

    // Voice configuration and object creation:
    UdpClientHelper dlClientVoice;
    dlClientVoice.SetAttribute("RemotePort", UintegerValue(dlPortVoice));
    dlClientVoice.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    dlClientVoice.SetAttribute("PacketSize", UintegerValue(udpPacketSizeBe));
    dlClientVoice.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambdaBe)));

    // The bearer that will carry voice traffic
    NrEpsBearer voiceBearer(NrEpsBearer::GBR_CONV_VOICE);

    // The filter for the voice traffic
    Ptr<NrEpcTft> voiceTft = Create<NrEpcTft>();
    NrEpcTft::PacketFilter dlpfVoice;
    dlpfVoice.localPortStart = dlPortVoice;
    dlpfVoice.localPortEnd = dlPortVoice;
    voiceTft->Add(dlpfVoice);

    /*
     * Let's install the applications!
     */
    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < ueLowLatContainer.GetN(); ++i)
    {
        Ptr<Node> ue = ueLowLatContainer.Get(i);
        Ptr<NetDevice> ueDevice = ueLowLatNetDev.Get(i);
        Address ueAddress = ueLowLatIpIface.GetAddress(i);

        // The client, who is transmitting, is installed in the remote host,
        // with destination address set to the address of the UE
        dlClientLowLat.SetAttribute("RemoteAddress", AddressValue(ueAddress));
        clientApps.Add(dlClientLowLat.Install(remoteHost));

        // Activate a dedicated bearer for the traffic type
        nrHelper->ActivateDedicatedEpsBearer(ueDevice, lowLatBearer, lowLatTft);
    }

    for (uint32_t i = 0; i < ueVoiceContainer.GetN(); ++i)
    {
        Ptr<Node> ue = ueVoiceContainer.Get(i);
        Ptr<NetDevice> ueDevice = ueVoiceNetDev.Get(i);
        Address ueAddress = ueVoiceIpIface.GetAddress(i);

        // The client, who is transmitting, is installed in the remote host,
        // with destination address set to the address of the UE
        dlClientVoice.SetAttribute("RemoteAddress", AddressValue(ueAddress));
        clientApps.Add(dlClientVoice.Install(remoteHost));

        // Activate a dedicated bearer for the traffic type
        nrHelper->ActivateDedicatedEpsBearer(ueDevice, voiceBearer, voiceTft);
    }

    // start UDP server and client apps
    serverApps.Start(udpAppStartTime);
    clientApps.Start(udpAppStartTime);
    serverApps.Stop(simTime);
    clientApps.Stop(simTime);

    // enable the traces provided by the nr module
    // nrHelper->EnableTraces();

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost);
    endpointNodes.Add(ueContainer);

    struct timeval time_now{};
    gettimeofday(&time_now, nullptr
    );
    std::string ue_poss_out = "ue_position.txt";
    std::string gnbs_out = "gnbs.txt";



    t_startTime_simid = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
    ClearFile(gnbs_out, t_startTime_simid);
    ClearFile(ue_poss_out, t_startTime_simid);

    NS_LOG_UNCOND("----------");
    NS_LOG_UNCOND("SIM ID: " << t_startTime_simid);
    NS_LOG_UNCOND("----------");

    double simTime_dbl = double(simTime.GetSeconds());
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

    Ptr<ns3::FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));
//=======================================================================================
  /*
      // configure REM parameters
    uint16_t sector = 0;
    double theta = 60;
    double xMin = -1000.0;
    double xMax = 1000.0;
    uint16_t xRes = 100;
    double yMin = -1000.0;
    double yMax = 1000.0;
    uint16_t yRes = 100;



    Ptr<NrRadioEnvironmentMapHelper> remHelper = CreateObject<NrRadioEnvironmentMapHelper>();
    remHelper->SetMinX(xMin);
    remHelper->SetMaxX(xMax);
    remHelper->SetResX(xRes);
    remHelper->SetMinY(yMin);
    remHelper->SetMaxY(yMax);
    remHelper->SetResY(yRes);
    remHelper->SetSimTag(simTag);
    remHelper->SetRemMode(NrRadioEnvironmentMapHelper::BEAM_SHAPE);

    // configure beam that will be shown in REM map
    DynamicCast<NrGnbNetDevice>(gnbNetDev.Get(0))
        ->GetPhy(0)
        ->GetSpectrumPhy()
        ->GetBeamManager()
        ->SetSector(sector, theta);
    DynamicCast<NrUeNetDevice>(ueLowLatNetDev.Get(0))
        ->GetPhy(0)
        ->GetSpectrumPhy()
        ->GetBeamManager()
        ->ChangeToQuasiOmniBeamformingVector();
    remHelper->CreateRem(gnbNetDev, ueLowLatNetDev.Get(0), 0);

//==============================================================================
    */
    Simulator::Stop(simTime);
    Simulator::Run();

    // Print per-flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
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

    double meanFlowThroughput = averageFlowThroughput / stats.size();
    double meanFlowDelay = averageFlowDelay / stats.size();

    outFile << "\n\n  Mean flow throughput: " << meanFlowThroughput << "\n";
    outFile << "  Mean flow delay: " << meanFlowDelay << "\n";

    outFile.close();

    std::ifstream f(filename.c_str());

    if (f.is_open())
    {
        std::cout << f.rdbuf();
    }

    Simulator::Destroy();

    if (argc == 0)
    {
        double toleranceMeanFlowThroughput = 0.0001 * 56.258560;
        double toleranceMeanFlowDelay = 0.0001 * 0.553292;

        if (meanFlowThroughput >= 56.258560 - toleranceMeanFlowThroughput &&
            meanFlowThroughput <= 56.258560 + toleranceMeanFlowThroughput &&
            meanFlowDelay >= 0.553292 - toleranceMeanFlowDelay &&
            meanFlowDelay <= 0.553292 + toleranceMeanFlowDelay)
        {
            return EXIT_SUCCESS;
        }
        else
        {
            return EXIT_FAILURE;
        }
    } else if (argc == 1 and N_Ues == 9) // called from examples-to-run.py with these parameters
    {
        double toleranceMeanFlowThroughput = 0.0001 * 47.858536;
        double toleranceMeanFlowDelay = 0.0001 * 10.504189;

        if (meanFlowThroughput >= 47.858536 - toleranceMeanFlowThroughput &&
            meanFlowThroughput <= 47.858536 + toleranceMeanFlowThroughput &&
            meanFlowDelay >= 10.504189 - toleranceMeanFlowDelay &&
            meanFlowDelay <= 10.504189 + toleranceMeanFlowDelay)
        {
            return EXIT_SUCCESS;
        }
        else
        {
            return EXIT_FAILURE;
        }
    }
    else
    {
        return EXIT_SUCCESS; // we dont check other parameters configurations at the moment
    }
}
