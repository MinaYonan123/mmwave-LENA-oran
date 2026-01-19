// Copyright (c) 2023 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

// modified by: Kamil Kociszewski Orange Innovation Poland
// (kamil.kociszewski@orange.com)

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
*Customized by:
*Kamil Kociszewski <kamil.kociszewski@orange.com>
    12.2025 Add O1 get-config to autoconfigure scenario
    01.2026 Include calibrated MIMO scenario
 */

#include "../src/core/model/log-macros-enabled.h"
#include "client_o1.cc"
#include "pugixml.hpp"

#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-module.h"

#include <cmath>
#include <filesystem> // For filesystem utilities, available since C++17
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/time.h>
#include <vector>

uint64_t t_startTime_simid;
double maxXAxis = 0;
double maxYAxis = 0;

int matrix_cells_rows = 0;
const int matrix_cells_columns = 20;
double (*matrix_cells)[matrix_cells_columns];
std::string matrix_cell_names[15];

using namespace ns3;
// using namespace nr;

NS_LOG_COMPONENT_DEFINE("CttcNrMimoDemo");

void
PrintPosition(Ptr<Node> node, std::string Filename)
{
  //  uint64_t timestamp = t_startTime_simid + (uint64_t)Simulator::Now().GetMilliSeconds();

    int imsi;
    int nDevs = node->GetNDevices();

    for (int j = 0; j < nDevs; j++)
    {
        Ptr<NrUeNetDevice> nruedev = node->GetDevice(j)->GetObject<NrUeNetDevice>();
        if (nruedev)
        {
            imsi = int(nruedev->GetImsi());
            int serving_cell = nruedev->GetCellId();

            Ptr<MobilityModel> model = node->GetObject<MobilityModel>();
            Vector position = model->GetPosition();
            Vector velocity = model->GetVelocity();
            double speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y +
                                     velocity.z * velocity.z); // speed in m/s
            double speedKmh = speed * 3.6;
            speedKmh = std::round(speedKmh * 10.0) / 10.0;

            NS_LOG_UNCOND(std::fixed << std::setprecision(1) << Simulator::Now().GetSeconds()
                                     << ": Position of UE with IMSI " << imsi << " is " << position
                                     << ", Speed: " << speedKmh << " km/h"
                                     << ", UE connected to Cell: " << serving_cell);
        }
    }
    NS_LOG_UNCOND("---------------------------------------------");
}

double (*O1_get_config(int argc, char* argv[], bool update)) [matrix_cells_columns] {
    std::string url = "http://localhost:8831";
    std::string O1_filename = ""; //"Topo_Example.xml";
    std::string condition = "";   //"nci:42986586128,42986586130,43013881872";

    if (!update)
        std::cout << "Current work directory: " << std::filesystem::current_path().string()
                  << std::endl;

    try
    {
        ConfigData config = read_config();
        O1_filename = config.filename;
        condition = config.condition;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return NULL;
    }

    if (argc > 2)
    {
        O1_filename = argv[1];
        condition = argv[2];
    }
    if (!update)
        std::cout << "Config file name: " << O1_filename << std::endl;
    if (!update)
        std::cout << "condition: " << condition << std::endl;
    condition = convertToCondition(condition);
    if (!update)
        NS_LOG_UNCOND(condition);

    std::map<std::string, Entity> entities = {};

    std::string xml = FetchFilteredConfig(url, O1_filename, condition);
    // NS_LOG_UNCOND(xml);
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xml.c_str());
    if (!result)
    {
        std::cerr << "Failed to parse XML: " << result.description() << std::endl;
        return NULL;
    }

    for (pugi::xml_node entry : doc.child("config").children("entry"))
    {
        Entity entity;
        std::string key = entity.set_from_xml(entry);
        entities[key] = entity;
    }
    ///
    matrix_cells_rows = entities.size();

    if (!matrix_cells)
        matrix_cells = new double[matrix_cells_rows][matrix_cells_columns];

    int cell_count = 0;
    for (auto& entry : entities)
    {
        if (!update)
            std::cout << "\nReceived config: key=" << entry.first
                      << ", cell_name=" << entry.second.cell_name() << ", X=" << entry.second.X()
                      << ", Y=" << entry.second.Y() << ", azimut=" << entry.second.azimut()
                      << ", tilt=" << entry.second.tilt() << ", sector=" << entry.second.sector()
                      << ", gnodeb_id=" << entry.second.gnodeb_id()
                      << ", hba=" << entry.second.hba() << std::endl;
        else
            std::cout << "Update for " << entry.first << std::endl;

        matrix_cells[cell_count][9] = entry.second.gnodeb_id();
        matrix_cells[cell_count][2] = entry.second.azimut();
        matrix_cells[cell_count][3] = entry.second.tilt();
        matrix_cells[cell_count][4] = entry.second.hba();
        matrix_cells[cell_count][6] = entry.second.X();
        matrix_cells[cell_count][7] = entry.second.Y();

        std::string tmp_cell_name = entry.second.cell_name();

        double tmp_sector = 0.0;
        if (!tmp_cell_name.empty())
        {
            char last_char = tmp_cell_name.back();
            if (std::isdigit(last_char))
            {
                tmp_sector = static_cast<double>(last_char - '0');
            }
            else
            {
                std::cerr << "Warning: Last character is not a digit in cell_name = "
                          << tmp_cell_name << std::endl;
            }
        }

        matrix_cells[cell_count][8] = tmp_sector;

        if (!update)
            NS_LOG_UNCOND(
                "Using advancedDlSuMimoEnabled = " << entry.second.advancedDlSuMimoEnabled());
        if (entry.second.advancedDlSuMimoEnabled())
        {
            matrix_cells[cell_count][10] = 1;
        }
        else
        {
            matrix_cells[cell_count][10] = 0;
        }

        if (!update)
            NS_LOG_UNCOND("Using csiRsPeriodicity = " << entry.second.csiRsPeriodicity());
        matrix_cells[cell_count][11] = entry.second.csiRsPeriodicity();

        if (!update)
            NS_LOG_UNCOND("Using dlMaxMuMimoLayers = " << entry.second.dlMaxMuMimoLayers());
        matrix_cells[cell_count][12] = entry.second.dlMaxMuMimoLayers();

        if (!update)
            NS_LOG_UNCOND("Using ssbSubCarrierSpacing = " << entry.second.ssbSubCarrierSpacing());
        matrix_cells[cell_count][0] = entry.second.ssbSubCarrierSpacing();

        if (!update)
            NS_LOG_UNCOND("Using bandListManual = " << entry.second.bandListManual());
        if (entry.second.bandListManual() == 78)
        {
            matrix_cells[cell_count][1] = 3.5e9;
        }
        else if (entry.second.bandListManual() == 28)
        {
            matrix_cells[cell_count][1] = 0.7e9;
        }
        else
        {
            matrix_cells[cell_count][1] = 0;
        }

        if (!update)
            NS_LOG_UNCOND("Using srsPeriodicity = " << entry.second.srsPeriodicity());
        matrix_cells[cell_count][13] = entry.second.srsPeriodicity();

        if (!update)
            NS_LOG_UNCOND("Using pdcchLaBfGainFraction = " << entry.second.pdcchLaBfGainFraction());
        matrix_cells[cell_count][14] = entry.second.pdcchLaBfGainFraction();

        if (!update)
            NS_LOG_UNCOND("Using srsHoppingBandwidth = " << entry.second.srsHoppingBandwidth());
//not_used
        // matrix_cells[cell_count][x] = entry.second.srsHoppingBandwidth();

        if (!update)
            NS_LOG_UNCOND("Using csiReportFormat = " << entry.second.csiReportFormat());
//not_used
        // matrix_cells[cell_count][x] = entry.second.csiReportFormat();

        if (!update)
            NS_LOG_UNCOND("Using dl256QamEnabled = " << entry.second.dl256QamEnabled());
        if (entry.second.dl256QamEnabled())
        {
            matrix_cells[cell_count][15] = 1;
        }
        else
        {
            matrix_cells[cell_count][15] = 0;
        }

        if (!update)
            NS_LOG_UNCOND("Using energySavingControl = " << entry.second.energySavingControl());
        int energyControl;
        if (entry.second.energySavingControl() == "toBeEnergySaving")
        {
            energyControl = 1;
        }
        else if (entry.second.energySavingControl() == "toBeNotEnergySaving")
        {
            energyControl = 0;
        }
        else
        {
            energyControl = -1;
        }
        if (update && matrix_cells[cell_count][16] != energyControl)
            NS_LOG_UNCOND("energySavingControl = " << energyControl);
        matrix_cells[cell_count][16] = energyControl;

        if (!update)
            NS_LOG_UNCOND("Using energySavingState = " << entry.second.energySavingState());
        int energyState;
        if (entry.second.energySavingState() == "isEnergySaving")
        {
            energyState = 1;
        }
        else if (entry.second.energySavingState() == "isNotEnergySaving")
        {
            energyState = 0;
        }
        else
        {
            energyState = -1;
        }
        if (update && matrix_cells[cell_count][17] != energyState)
            NS_LOG_UNCOND("energySavingRState = " << energyState);
        matrix_cells[cell_count][17] = energyState;

        if (!update)
            NS_LOG_UNCOND("Using energySavingStateNs3 = " << entry.second.energySavingStateNs3());
        int energyStateNs3;
        if (entry.second.energySavingStateNs3() == "isEnergySaving")
        {
            energyStateNs3 = 1;
        }
        else if (entry.second.energySavingStateNs3() == "isNotEnergySaving")
        {
            energyStateNs3 = 0;
        }
        else
        {
            energyStateNs3 = -1;
        }
        if (update && matrix_cells[cell_count][18] != energyStateNs3)
            NS_LOG_UNCOND("energySavingRStatNs3 e= " << energyStateNs3);
        matrix_cells[cell_count][18] = energyStateNs3;

        // if (!update) NS_LOG_UNCOND("cell_count " << cell_count);
        matrix_cells[cell_count][19] = cell_count;
        matrix_cell_names[cell_count] = entry.second.cell_name();

        if (!update)
            NS_LOG_UNCOND("Using csiRsActivePortConfig = " << entry.second.csiRsActivePortConfig());
        // matrix_cells[cell_count][x] = entry.second.csiRsActivePortConfig();

        if (!update)
            NS_LOG_UNCOND(
                "Using cbrsCellBandwidthPreferred = " << entry.second.cbrsCellBandwidthPreferred());
        matrix_cells[cell_count][5] = entry.second.cbrsCellBandwidthPreferred();
        cell_count = cell_count + 1;
    }

    return matrix_cells;
}

void Update_O1_ES_Cells(int argc, char* argv[])
{
    static std::vector<int> prevESS; // persist between calls
    static bool firstRun = true;

    // Load latest O1 config
    O1_get_config(argc, argv, true);

    // Fill current ESS state
    std::vector<int> newESS(matrix_cells_rows);
    for (int i = 0; i < matrix_cells_rows; i++)
    {
        newESS[i] = static_cast<int>(matrix_cells[i][17]); // energySavingState as int
    }

    std::cout << "=== Energy Saving State Changes ===" << std::endl;

    if (firstRun)
    {
        // First run: just print initial state
        for (int i = 0; i < matrix_cells_rows; i++)
        {
            std::cout << "Cell: " << matrix_cell_names[(int)matrix_cells[i][19]]
                      << " | Initial ESS: " << newESS[i] << std::endl;
        }
        firstRun = false;
    }
    else
    {
        // Compare previous and current ESS states
        for (int i = 0; i < matrix_cells_rows; i++)
        {
            if (prevESS[i] != newESS[i])
            {
                std::cout << "Cell: " << matrix_cell_names[(int)matrix_cells[i][19]]
                          << " | ESS changed: " << prevESS[i] << " -> " << newESS[i] << std::endl;
            }
        }
    }

    // Update previous state
    prevESS = newESS;
}

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
static ns3::GlobalValue g_e2nrEnabled("e2nrEnabled",
                                      "If true, send NR E2 reports",
                                      ns3::BooleanValue(false),
                                      ns3::MakeBooleanChecker());

int
main(int argc, char* argv[])
{
    LogComponentEnable("E2Termination", LOG_LEVEL_ALL);
//not_used
    // double report_to_db = false;
    struct timeval time_now{};
    gettimeofday(&time_now, nullptr);
    t_startTime_simid = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);

    int udpAppStartTime_int = 400;
    Time udpAppStartTime = MilliSeconds(udpAppStartTime_int);

    // auto matrix_cells = O1_get_config(argc, argv, false);
    O1_get_config(argc, argv, false);

    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();

    // Find most southwest cell
    for (int i = 0; i < matrix_cells_rows; ++i)
    {
        int x = matrix_cells[i][6];
        int y = matrix_cells[i][7];

        if (x <= minX)
        {
            minX = x;
            minY = y;
        }
    }
    // std::cout << "\n" << std::endl;
    // std::cout << "Southwest cell original (minX, minY): " << minX << ", " << minY
    //   << std::endl;

    // Normalize: make southwest cell (2000, 2000)
    for (int i = 0; i < matrix_cells_rows; ++i)
    {
        int normalizedX = matrix_cells[i][6] - minX + 2000;
        int normalizedY = matrix_cells[i][7] - minY + 2000;

        // std::cout << "Cell " << i << " normalized position: (" << normalizedX
        //         << ", " << normalizedY << ")" << std::endl;
        matrix_cells[i][6] = normalizedX;
        matrix_cells[i][7] = normalizedY;

        if (matrix_cells[i][6] > maxXAxis)
        {
            maxXAxis = matrix_cells[i][6];
        }
        if (matrix_cells[i][7] > maxYAxis)
        {
            maxYAxis = matrix_cells[i][7];
        }
    }
    maxXAxis = maxXAxis + 2000;
    maxYAxis = maxYAxis + 2000;
    NS_LOG_UNCOND("MAX AREA: " << maxXAxis << "," << maxYAxis);
    std::cout << "\n";

    // Print the matrix
    std::cout << "Cell matrix contents:\n";
    for (int i = 0; i < matrix_cells_rows; i++)
    {
        for (int j = 0; j < matrix_cells_columns; j++)
        {
            std::cout << matrix_cells[i][j] << " ";
        }
        std::cout << "\n"; // Go to the next row after printing all columns in current row
    }
    std::cout << "\n";

   // uint16_t gNbNum = matrix_cells_rows;
    //  int UE_container = 1;

    // Create two vectors to store the rows for each matrix
    std::vector<double*> matrix_700;
    std::vector<double*> matrix_3500;

    // Populate the vectors based on the value in the second column
    for (int i = 0; i < matrix_cells_rows; ++i)
    {
        if (matrix_cells[i][1] == 0.7e9)
        {
            matrix_700.push_back(matrix_cells[i]);
        }
        else if (matrix_cells[i][1] == 3.5e9)
        {
            matrix_3500.push_back(matrix_cells[i]);
        }
    }

   // auto startExecTime = std::chrono::system_clock::now();
    // mimo

    NrHelper::MimoPmiParams mimoPmiParams;
    // mimoPmiParams.subbandSize = 16;
    //

    bool useMimoPmiParams = true;
   // bool useConfigSetDefault = false;
    //uint8_t csiFlags = 1;
    //uint32_t wbPmiUpdateIntervalMs = 10; // Wideband PMI update interval in ms
   // uint32_t sbPmiUpdateIntervalMs = 2;  // Subband PMI update interval in ms

    std::string errorModel = "ns3::NrEesmIrT2";
    // std::string scheduler = "ns3::NrMacSchedulerTdmaRR";
    std::string scheduler = "ns3::NrMacSchedulerOfdmaPF";
    // Proportional Fair (PF) balances
    // throughput and fairness among users. With
    // OFDMA (Orthogonal Frequency Division
    // Multiple Access) support, it fits well
    // with 5G NR's frame structure and
    // subcarrier spacing, especially under 100
    // MHz bandwidth.
    std::string beamformingMethod = "ns3::DirectPathBeamforming";

    // ns3::NrMacSchedulerOfdmaQos if QoS flows (e.g., URLLC vs eMBB) are central
    // to your use case. ns3::NrMacSchedulerOfdmaMR if maximum throughput is the
    // goal without fairness concerns.

    NrHelper::AntennaParams apUe;
    NrHelper::AntennaParams apGnb;

    apUe.antennaElem = "ns3::IsotropicAntennaModel";
    apUe.nAntCols = 2;
    apUe.nAntRows = 2;
    apUe.nHorizPorts = 2;
    apUe.nVertPorts = 2;
    apUe.isDualPolarized = true;

    apGnb.antennaElem = "ns3::ThreeGppAntennaModel";
    apGnb.nAntCols = 4;
    apGnb.nAntRows = 4;
    apGnb.nHorizPorts = 4;
    apGnb.nVertPorts = 4;
    apGnb.isDualPolarized = true;

    // The polarization slant angle in degrees in case of x-polarized
    double polSlantAngleGnb = 0.0;
    double polSlantAngleUe = 90.0;
    // The bearing angles in degrees
    double bearingAngleGnb = 90.0;
    double bearingAngleUe = 180.0;
    uint16_t updatePeriodMs = 100;

    /*
     * Variables that represent the parameters we will accept as input by the
     * command line. Each of them is initialized with a default value, and
     * possibly overridden below when command-line arguments are parsed.
     */

    // o-ran e2
    double indicationPeriodicity = 0.1;
    double g_e2_func_id = 2;
    double g_rc_e2_func_id = 3;

    uint16_t losCondition = 0;

    // Where the example stores the output files.
    std::string simTag = "default";
    std::string outputDir = "./";
    bool logging = false;

    // NR parameters (Reference: 3GPP TR 38.901 V17.0.0 (Release 17)
    // Table 7.8-1 for the power and BW).
    // In this example the BW has been split into two BWPs
    // We will take the input from the command line, and then we
    // will pass them inside the NR module.
    // 273PRB
    uint16_t numerologyBwp1 = 1;
   // uint16_t numerology = 1;
    //double centralFrequencyBand1 = 3.5e9;
    double centralFrequency = 3.5e9;
   // double bandwidthBand1 = 100e6;
    double bandwidth = 100e6;

    /*// 106PRB
    uint16_t numerologyBwp2 = 0;
    double centralFrequencyBand2 = 0.7e9;
    double bandwidthBand2 = 20e6;*/

    // Reduce modificator for CQI calculations
    double implementationLossDb = 20;

    // Define max loss parameter for spectrum channel, 5G NR, n78 (3.5 GHz), 100 MHz bandwidth
    double maxLossDb = 200;

    int duration = 5;

    CommandLine cmd(__FILE__);
    /**
     * The main parameters for testing MIMO
     */
    cmd.AddValue("duration", "Duration of Simulation", duration);
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

    /**
     * Other simulation parameters
     */


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
    // e2
    cmd.AddValue("indicationPeriodicity",
                 "E2 Indication Periodicity reports (value in seconds)",
                 indicationPeriodicity); // not implemented in Lena
    cmd.AddValue("KPM_E2functionID", "Function ID to subscribe)", g_e2_func_id);
    cmd.AddValue("RC_E2functionID", "Function ID to subscribe)", g_rc_e2_func_id);
   // cmd.AddValue("report_to_db", "Report to DB", report_to_db);
    //cmd.AddValue("implementationLossDb", "SINR calibration offset in dB", implementationLossDb);

    // Parse the command line
    cmd.Parse(argc, argv);

    Time simTime = Seconds(duration);

    // o-ran e2
    StringValue stringValue;
    BooleanValue booleanValue;
    DoubleValue doubleValue;

    GlobalValue::GetValueByName("e2nrEnabled", booleanValue);
    bool e2nrEnabled = booleanValue.Get();
    GlobalValue::GetValueByName("e2TermIp", stringValue);
    std::string e2TermIp = stringValue.Get();

    Config::SetDefault("ns3::NrGnbNetDevice::KPM_E2functionID", DoubleValue(g_e2_func_id));
    Config::SetDefault("ns3::NrGnbNetDevice::RC_E2functionID", DoubleValue(g_rc_e2_func_id));

    //  set defaults from CSON CMs
    Config::SetDefault("ns3::NrGnbRrc::SrsPeriodicity",
                       UintegerValue(40)); // srsPeriodicity = 40
    //Config::SetDefault("ns3::NrGnbPhy::CsiRsPeriodicity",
                     //  UintegerValue(40)); // csiRsPeriodicity = 40
   // Config::SetDefault("ns3::NrGnbNetDevice::sim_id", UintegerValue(t_startTime_simid));
    //Config::SetDefault("ns3::NrGnbNetDevice::report_to_db", BooleanValue(report_to_db));
  Config::SetDefault("ns3::NrAmc::ImplementationLossDb", DoubleValue(implementationLossDb));
    //Config::SetDefault("ns3::SpectrumChannel::MaxLossDb", DoubleValue(maxLossDb));

    NS_ABORT_IF(centralFrequency < 0.5e9 && centralFrequency > 100e9);
    NS_ABORT_UNLESS(losCondition < 3);

    if (logging)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("NrPdcp", LOG_LEVEL_INFO);
    }

    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(999999999));
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod",
                       TimeValue(MilliSeconds(updatePeriodMs)));

    // Config::SetDefault("ns3::NrGnbNetDevice::sim_id", UintegerValue(t_startTime_simid));
    // Config::SetDefault("ns3::NrGnbNetDevice::report_to_db", BooleanValue(report_to_db));

    NodeContainer gnbContainer;
    gnbContainer.Create(matrix_3500.size());

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    // Vector centerPosition = Vector(maxXAxis / 2, maxYAxis / 2, 30);
    // todo: fix here iteration
    for (uint32_t i = 0; i < matrix_3500.size(); i++)
    {
        Ptr<Node> gNB = gnbContainer.Get(i);
        // Allocate positions
        int local_cell_id = 0;
        if (matrix_3500[i][8] == 1)
        {
            positionAlloc->Add(Vector(matrix_3500[i][6] + 1, matrix_3500[i][7], matrix_3500[i][4]));
            local_cell_id = matrix_3500[i][9];
            // Checking for out-of-bound access and that local_cell_id are the same
            if (i + 1 < matrix_3500.size() && matrix_3500[i + 1][8] == 2 and
                local_cell_id == matrix_3500[i + 1][9])
            {
                positionAlloc->Add(
                    Vector(matrix_3500[i][6] - 1, matrix_3500[i][7] + 1, matrix_3500[i][4]));
                // Checking for out-of-bound access and that local_cell_id are the same
                if (i + 2 < matrix_3500.size() && matrix_3500[i + 2][8] == 3 and
                    local_cell_id == matrix_3500[i + 2][9])
                {
                    positionAlloc->Add(
                        Vector(matrix_3500[i][6] - 1, matrix_3500[i][7] - 1, matrix_3500[i][4]));
                }
            }
        }
        else
        {
            //  NS_LOG_UNCOND("This is sector, skip");
        }
    }

    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(gnbContainer);


    NodeContainer ueContainer;

    // === PARAMETERS ===
    // TODO: Set manually before simulation!

    std::vector<int> uePerSectorList = {1, 2, 1};

    double globalMinDist = 150.0;
    double globalMaxDist = 1500.0;

    // random UE speed: 10–40 km/h
    auto randSpeed = [&]() {
        double v = 10.0 + (40.0 - 10.0) * ((double)rand() / RAND_MAX);
        return v / 3.6;
    };

    int totalUEs = 0;
    for (int c : uePerSectorList)
        totalUEs += c;
    ueContainer.Create(totalUEs);

    // === UE INDICES PER SECTOR ===
    std::vector<std::vector<uint32_t>> sectorUeIndices;
    {
        uint32_t idx = 0;
        for (size_t s = 0; s < uePerSectorList.size(); s++)
        {
            std::vector<uint32_t> list;
            for (int k = 0; k < uePerSectorList[s]; k++)
                list.push_back(idx++);
            sectorUeIndices.push_back(list);
        }
    }

    // === ASSIGN MOBILITY using predefined simTime ===
    auto assignSectorMobility = [&](uint32_t ueIndex,
                                    double cellX,
                                    double cellY,
                                    double azDeg,
                                    double minDist,
                                    double maxDist,
                                    double speedValue) {
        double angle = 90.0 - azDeg;
        double azRad = angle * M_PI / 180.0;

        while (azRad <= -M_PI)
            azRad += 2 * M_PI;
        while (azRad > M_PI)
            azRad -= 2 * M_PI;

        // random starting position
        double startDist = minDist + (maxDist - minDist) * ((double)rand() / RAND_MAX);
        double endDist = (fabs(startDist - maxDist) < 1e-6 ? minDist : maxDist);

        Vector startPos(cellX + startDist * cos(azRad), cellY + startDist * sin(azRad), 1.5);

        Vector endPos(cellX + endDist * cos(azRad), cellY + endDist * sin(azRad), 1.5);

        // travel time from start to end at UE speed
        double travelTime = fabs(endDist - startDist) / speedValue;

        // UE will go start → end → start → end... until simTime is reached
        Ptr<WaypointMobilityModel> mobility = CreateObject<WaypointMobilityModel>();

        mobility->AddWaypoint(Waypoint(Seconds(0.0), startPos));

        double t = travelTime;
        bool goingOut = true;

        while (t < duration)
        {
            mobility->AddWaypoint(Waypoint(Seconds(t), goingOut ? endPos : startPos));
            t += travelTime;
            goingOut = !goingOut;
        }

        // final waypoint at simTime (clip)
        mobility->AddWaypoint(Waypoint(Seconds(duration), goingOut ? startPos : endPos));

        ueContainer.Get(ueIndex)->AggregateObject(mobility);

        NS_LOG_UNCOND("UE " << ueIndex << " speed=" << speedValue << " m/s az=" << azDeg
                            << " start=" << startDist << " end=" << endDist);
    };

    // === ASSIGN MOBILITY TO ALL UEs ===
    uint32_t ueIndex = 0;
    for (size_t i = 0; i < uePerSectorList.size(); i++)
    {
        double sectorAz = matrix_cells[i][2];
        double cellX = matrix_cells[i][6];
        double cellY = matrix_cells[i][7];

        for (int j = 0; j < uePerSectorList[i]; j++)
        {
            double jitter = -3.0 + 6.0 * ((double)rand() / RAND_MAX);
            double ueAz = sectorAz + jitter;
            double speed = randSpeed();

            assignSectorMobility(ueIndex, cellX, cellY, ueAz, globalMinDist, globalMaxDist, speed);
            ueIndex++;
        }
    }

    NS_LOG_UNCOND("=== Using predefined simulation time: " << duration << " s ===");

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
        BandwidthPartInfo::Scenario(BandwidthPartInfo::RMa + losCondition);

    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1;
    CcBwpCreator::SimpleOperationBandConf bandConf(centralFrequency,
                                                   bandwidth,
                                                   numCcPerBand,
                                                   scenario);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

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
        params.pmSearchMethod = "ns3::NrPmSearchFull";
        params.fullSearchCb = "ns3::NrCbTypeOneSp";
        params.subbandSize = 32;
        params.rankLimit = 4;
       // params.rankThreshold = 1;
      //  params.rankTechnique = "SVD";
       // params.downsamplingTechnique= "AveragePRB";

        nrHelper->SetupMimoPmi(params);
    }

    /**
     * Configure gNb antenna
     */
    nrHelper->SetupGnbAntennas(apGnb);
    /**
     * Configure UE antenna
     */
    nrHelper->SetupUeAntennas(apUe);

    nrHelper->SetGnbPhyAttribute("Numerology", UintegerValue(numerologyBwp1));
    nrHelper->SetGnbPhyAttribute("TxPower", DoubleValue(45));
    //nrHelper->SetUePhyAttribute("TxPower", DoubleValue(txPowerUe));

    uint32_t bwpId = 0;
    // GBR_CONV_VIDEO
    nrHelper->SetGnbBwpManagerAlgorithmAttribute("NGBR_VIDEO_TCP_DEFAULT", UintegerValue(bwpId));
    // UE routing between bearer type and bandwidth part
    nrHelper->SetUeBwpManagerAlgorithmAttribute("NGBR_VIDEO_TCP_DEFAULT", UintegerValue(bwpId));

    nrHelper->InitializeOperationBand(&band);
    BandwidthPartInfoPtrVector allBwps;
    allBwps = CcBwpCreator::GetAllBwps({band});


    /**
     * Finally, create the gNB and the UE device.
     */
    NetDeviceContainer gnbNetDev = nrHelper->InstallGnbDevice(gnbContainer, allBwps);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueContainer, allBwps);

    if (matrix_3500.size() > 0)
    {
        for (uint32_t numCell = 0; numCell < gnbContainer.GetN(); ++numCell)
        {
            Ptr<NetDevice> gnb = gnbNetDev.Get(numCell);
            int cell_id = gnb->GetObject<NrGnbNetDevice>()->GetCellId();
//not_used
    // uint32_t numBwps = nrHelper->GetNumberBwp(gnb);
            // Access the PHY layer of the gNB
            Ptr<NrGnbPhy> phy = nrHelper->GetGnbPhy(gnb, 0);

            // Get the antenna model and configure its parameters
            Ptr<UniformPlanarArray> antenna =
                DynamicCast<UniformPlanarArray>(phy->GetSpectrumPhy()->GetAntenna());

            double bearingAngleDeg = matrix_3500[numCell][2];
            // Calculate angle directly (no normalization to 0-360)
            double angle = 90.0 - bearingAngleDeg;
            // Convert to radians
            double bearingAngleRad = angle * (M_PI / 180.0);
            // Normalize radians to [-pi, pi]
            while (bearingAngleRad <= -M_PI)
                bearingAngleRad += 2 * M_PI;
            while (bearingAngleRad > M_PI)
                bearingAngleRad -= 2 * M_PI;

            // 5. Set the angles for the entire array
            antenna->SetAttribute("BearingAngle", DoubleValue(bearingAngleRad));

            double downtiltDeg = matrix_3500[numCell][3];     // Column 3 is downtilt in degrees
            double downtiltRad = -downtiltDeg * M_PI / 180.0; // Note the negation for downtilt
            antenna->SetAttribute("DowntiltAngle", DoubleValue(downtiltRad));

            nrHelper->GetGnbPhy(gnbNetDev.Get(numCell), 0)
                ->SetAttribute("TxPower", DoubleValue(30));
            // Set numerology (e.g., set according to RAN helper)
            nrHelper->GetGnbPhy(gnb, 0)->SetAttribute("Numerology", UintegerValue(numerologyBwp1));

            NS_LOG_UNCOND("gNB 3500 id: "
                          << cell_id << " initialized at azimuth: " << matrix_3500[numCell][2]
                          << " degrees with frequency: " << matrix_3500[numCell][1] << " MHz.");
        }
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
    // TODO: Check if this is necessary to call when we do not reconfigure anything after devices
    // have been created
    for (auto it = gnbNetDev.Begin(); it != gnbNetDev.End(); ++it)
    {
        DynamicCast<NrGnbNetDevice>(*it)->UpdateConfig();
    }

    for (auto it = ueNetDev.Begin(); it != ueNetDev.End(); ++it)
    {
        DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();
    }

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

    // nrHelper->AttachToClosestGnb(ueNetDev, gnbNetDev);

    // === ATTACH UEs TO THEIR SECTOR gNB ===
    uint32_t ueAttIndex = 0;

    for (size_t s = 0; s < uePerSectorList.size(); s++)
    {
        int count = uePerSectorList[s];

        for (int k = 0; k < count; k++)
        {
            nrHelper->AttachToGnb(ueNetDev.Get(ueAttIndex), gnbNetDev.Get(s));

            NS_LOG_UNCOND("Attaching UE " << ueAttIndex << " to gNB sector " << s);
            ueAttIndex++;
        }
    }

    /**
     * Install DL traffic part.
     */
    uint16_t dlPort = 1234; // base port

    // ---- Bearer: use NON-GBR for TCP/FTP
    NrEpsBearer epsBearer(NrEpsBearer::NGBR_VIDEO_TCP_DEFAULT);

    // === PARAMETERS ===
    // TODO: Set manually before simulation!
    // traffic requirements per sector
    std::vector<int> dlPerSector = {1, 1, 1};

    std::vector<uint32_t> trafficUeList; // final selected UE list

    for (int s = 0; s < (int)dlPerSector.size(); s++)
    {
        int need = dlPerSector[s];
        auto& candidates = sectorUeIndices[s];

        // Random shuffle so we get random UEs
        std::random_shuffle(candidates.begin(), candidates.end());

        // Select first N UEs (N = need)
        for (int n = 0; n < need; n++)
        {
            trafficUeList.push_back(candidates[n]);
        }
    }

    std::string trafficType = "cbr";

    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    if (trafficType == "cbr")
    {
        for (uint32_t ti = 0; ti < trafficUeList.size(); ++ti)
        {
            uint32_t i = trafficUeList[ti]; // UE index selected for traffic
            uint16_t port = dlPort + i;     // unique port per UE

            // --- TFT / Packet Filter ---
            Ptr<NrEpcTft> dlTft = Create<NrEpcTft>();
            NrEpcTft::PacketFilter dlPktFilter;
            dlPktFilter.localPortStart = port;
            dlPktFilter.localPortEnd = port;
            dlTft->Add(dlPktFilter);

            // --- Server installed on UE ---
            UdpServerHelper dlServer(port);
            serverApps.Add(dlServer.Install(ueContainer.Get(i)));

            // --- Client in remoteHost -> UE ---
            UdpClientHelper dlClient;
            dlClient.SetAttribute("RemoteAddress", AddressValue(ueIpIface.GetAddress(i)));
            dlClient.SetAttribute("RemotePort", UintegerValue(port));
            dlClient.SetAttribute("PacketSize", UintegerValue(1400));
            dlClient.SetAttribute("Interval", TimeValue(MicroSeconds(66)));
            dlClient.SetAttribute("MaxPackets", UintegerValue(0)); // unlimited

            clientApps.Add(dlClient.Install(remoteHost));

            // --- Dedicated EPS bearer only for selected UE ---
            nrHelper->ActivateDedicatedEpsBearer(ueNetDev.Get(i), epsBearer, dlTft);
        }

        // --- Start/Stop ---
        for (uint32_t k = 0; k < serverApps.GetN(); ++k)
        {
            serverApps.Get(k)->SetStartTime(udpAppStartTime);
            clientApps.Get(k)->SetStartTime(udpAppStartTime);
        }

        serverApps.Stop(simTime);
        clientApps.Stop(simTime);
    }

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost);
    endpointNodes.Add(ueContainer);

    std::string ue_poss_out = "ue_position.txt";
    std::string gnbs_out = "gnbs.txt";

    NS_LOG_UNCOND("----------");
    NS_LOG_UNCOND("SIM ID: " << t_startTime_simid);
    NS_LOG_UNCOND("----------");

    double simTime_dbl = double(simTime.GetSeconds());
    int numPrints = simTime_dbl / 0.1;
    for (int i = 1; i < numPrints; i++)
    {
        for (uint32_t j = 0; j < ueContainer.GetN(); j++)
        {
            Simulator::Schedule(Seconds(i * simTime.GetSeconds() / numPrints),
                                &PrintPosition,
                                ueContainer.Get(j),
                                ue_poss_out);
        }
        Simulator::Schedule(Seconds(i * simTime.GetSeconds() / numPrints),
                            &Update_O1_ES_Cells,
                            argc,
                            argv);
    }

    Simulator::Stop(simTime);
    Simulator::Run();

    Ptr<ns3::FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));

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

    outFile << "\n\n  Mean flow throughput: " << averageFlowThroughput / stats.size() << "\n";
    outFile << "  Mean flow delay: " << averageFlowDelay / stats.size() << "\n";
    outFile.close();
    std::ifstream f(filename.c_str());
    if (f.is_open())
    {
        std::cout << f.rdbuf();
    }

    Simulator::Destroy();
    return 0;
}
