/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/* *
 * Copyright (c) 2025 Orange Innovation Poland
 * Copyright (c) 2024 Orange Innovation Egypt
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Andrea Lacava <thecave003@gmail.com>
 *          Michele Polese <michele.polese@gmail.com>
 *          Argha Sen <arghasen10@gmail.com>
 *          Kamil Kociszewski <kamil.kociszewski@orange.com>
 *          Mostafa Ashraf <mostafa.ashraf.ext@orange.com>
 */

#include "../src/mmwave/model/node-container-manager.h"
#include "client_o1.cc"

#include "ns3/applications-module.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/core-module.h"
#include "ns3/epc-helper.h"
#include "ns3/internet-module.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/lte-helper.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/mmwave-radio-energy-model-enb-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-helper.h"
#include <ns3/lte-ue-net-device.h>

#include <chrono>
#include <cmath>
#include <filesystem> // For filesystem utilities, available since C++17
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <sys/types.h>
#include <vector>

using namespace ns3;
using namespace mmwave;

//values store area of simulation
double maxXAxis = 0.0;
double maxYAxis = 0.0;

std::unordered_map<uint64_t, uint16_t> ueAssocByImsi; // IMSI -> serving cellId
std::unordered_map<uint16_t, bool> esON_list; // cellId -> ES state
std::unordered_map<uint16_t, double> totalnewEnergyConsumption_storage; // cellId -> J
std::unordered_map<uint16_t, double> totaloldEnergyConsumption_storage; // cellId -> J
std::unordered_map<uint16_t, double> current_energy_consumption;        // cellId -> J (delta)

double curr_total_energy_consumption = 0;
double max_energy_consumption = 0;
double sum_curr_total_energy_consumption = 0;
int num_of_mmdev = 0;

// O1 related variables
int matrix_cells_rows = 0;
const int matrix_cells_columns = 20;
double (*matrix_cells)[matrix_cells_columns];
std::string matrix_cell_names[20];


NS_LOG_COMPONENT_DEFINE("Energy_saving_with_cell_utilization_scenario_O1");

//This function prints all UEs that takes part in simulation to an external file.
void
PrintGnuplottableUeListToFile(std::string filename)
{
    std::ofstream outFile;
    outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::trunc);
    if (!outFile.is_open())
    {
        NS_LOG_ERROR("Can't open file " << filename);
        return;
    }
    for (NodeList::Iterator it = NodeList::Begin(); it != NodeList::End(); ++it)
    {
        Ptr<Node> node = *it;
        int nDevs = node->GetNDevices();
        for (int j = 0; j < nDevs; j++)
        {
            Ptr<LteUeNetDevice> uedev = node->GetDevice(j)->GetObject<LteUeNetDevice>();
            Ptr<MmWaveUeNetDevice> mmuedev = node->GetDevice(j)->GetObject<MmWaveUeNetDevice>();
            Ptr<McUeNetDevice> mcuedev = node->GetDevice(j)->GetObject<McUeNetDevice>();
            if (uedev)
            {
                Vector pos = node->GetObject<MobilityModel>()->GetPosition();
                outFile << "set label \"" << uedev->GetImsi() << "\" at " << pos.x << "," << pos.y
                        << " left font \"Helvetica,8\" textcolor rgb \"black\" front point pt 1 ps "
                           "0.3 lc rgb \"black\" offset 0,0"
                        << std::endl;
            }
            else if (mmuedev)
            {
                Vector pos = node->GetObject<MobilityModel>()->GetPosition();
                outFile << "set label \"" << mmuedev->GetImsi() << "\" at " << pos.x << "," << pos.y
                        << " left font \"Helvetica,8\" textcolor rgb \"black\" front point pt 1 ps "
                           "0.3 lc rgb \"black\" offset 0,0"
                        << std::endl;
            }
            else if (mcuedev)
            {
                Vector pos = node->GetObject<MobilityModel>()->GetPosition();
                outFile << "set label \"" << mcuedev->GetImsi() << "\" at " << pos.x << "," << pos.y
                        << " left font \"Helvetica,8\" textcolor rgb \"black\" front point pt 1 ps "
                           "0.3 lc rgb \"black\" offset 0,0"
                        << std::endl;
            }
        }
    }
}

//This function prints all Cells that takes part in simulation to an external file.
void
PrintGnuplottableEnbListToFile(uint64_t m_startTime)
{
    // uint64_t m_startTime = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
    uint64_t timestamp = m_startTime + (uint64_t)Simulator::Now().GetMilliSeconds();
    //
    std::string filename1 = "enbs.txt";
    std::string filename2 = "gnbs.txt";
    //
    int mmnode_iterator = 0;
    curr_total_energy_consumption = 0;
    for (NodeList::Iterator it = NodeList::Begin(); it != NodeList::End(); ++it)
    {
        Ptr<Node> node = *it;
        int nDevs = node->GetNDevices();
        for (int j = 0; j < nDevs; j++)
        {
            Ptr<LteEnbNetDevice> enbdev = node->GetDevice(j)->GetObject<LteEnbNetDevice>();
            Ptr<MmWaveEnbNetDevice> mmdev = node->GetDevice(j)->GetObject<MmWaveEnbNetDevice>();
            if (enbdev)
            {
                Vector pos = node->GetObject<MobilityModel>()->GetPosition();
                std::ofstream outFile1;
                outFile1.open(filename1.c_str(), std::ios_base::out | std::ios_base::app);
                if (!outFile1.is_open())
                {
                    NS_LOG_ERROR("Can't open file " << filename1);
                    return;
                }
                // outFile1 << timestamp << "," << enbdev->GetCellId() << "," << pos.x << "," <<
                // pos.y << pos.z << std::endl;
                outFile1 << timestamp << "," << enbdev->GetCellId() << "," << pos.x << "," << pos.y
                         << "," << m_startTime << "," << "0" << "," << "30" << std::endl;
                outFile1.close();
            }
            else if (mmdev)
            {
                Vector pos = node->GetObject<MobilityModel>()->GetPosition();
                std::ofstream outFile2;
                outFile2.open(filename2.c_str(), std::ios_base::out | std::ios_base::app);
                if (!outFile2.is_open())
                {
                    NS_LOG_ERROR("Can't open file " << filename2);
                    return;
                }
                auto ueMap = mmdev->GetUeMap();
                Ptr<MmWaveEnbPhy> enbPhy =
                    node->GetDevice(j)->GetObject<MmWaveEnbNetDevice>()->GetPhy();
                for (const auto& ue : ueMap)
                {
                    uint64_t imsi_assoc = ue.second->GetImsi();
                    // NS_LOG_UNCOND ("IMSI: " << imsi_assoc << " associated with cell: "  <<
                    // mmdev->GetCellId ());
                    // Old: ue_assoc_list[imsi_assoc - 1] = mmdev->GetCellId();
                    ueAssocByImsi[imsi_assoc] = mmdev->GetCellId();
                }
                uint16_t cell_id = mmdev->GetCellId();
                double es_power = enbPhy->GetTxPower();
                if (es_power == 0)
                {
                    esON_list[cell_id] = true;
                }
                else
                {
                    esON_list[cell_id] = false;
                }
                curr_total_energy_consumption =
                    curr_total_energy_consumption + current_energy_consumption[cell_id];
                // outFile2 << timestamp << "," << enbdev->GetCellId() << "," << pos.x << "," <<
                // pos.y << pos.z << std::endl;
                outFile2 << timestamp << "," << cell_id << "," << pos.x << "," << pos.y << ","
                         << m_startTime << "," << esON_list[cell_id] << ","
                         << current_energy_consumption[cell_id] << "," << max_energy_consumption
                         << "," << sum_curr_total_energy_consumption << std::endl;
                outFile2.close();
            }
        }
    }
    if (mmnode_iterator == num_of_mmdev)
    {
        sum_curr_total_energy_consumption = curr_total_energy_consumption;
    }
    if (mmnode_iterator == num_of_mmdev && max_energy_consumption < curr_total_energy_consumption)
    {
        max_energy_consumption = curr_total_energy_consumption;
    }
}

//This function clears external files and it's mainly used for RIC TaaP Studio GUI
void
ClearFile(std::string Filename, uint64_t m_startTime)
{
    std::string filename = Filename;
    std::ofstream outFile;
    outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::trunc);
    if (!outFile.is_open())
    {
        NS_LOG_ERROR("Can't open file " << filename);
        return;
    }
    outFile.close();
    //  struct timeval time_now{};
    //  gettimeofday (&time_now, nullptr);
    // uint64_t m_startTime = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
    uint64_t timestamp = m_startTime + (uint64_t)Simulator::Now().GetMilliSeconds();
    std::ofstream outFile1;
    outFile1.open(filename.c_str(), std::ios_base::out | std::ios_base::app);

    if (Filename == "ue_position.txt")
    {
        outFile1 << "timestamp,id,x,y,type,cell,simid" << std::endl;
    }
    else
    {
        outFile1 << "timestamp,id,x,y,simid,ESstate,currEC,maxEC,totalcurrEC" << std::endl;
        outFile1 << timestamp << "," << "0" << "," << maxXAxis << "," << maxYAxis << std::endl;
    }
    outFile1.close();
}

//This function print current status of UE every 100ms
void
PrintPosition(Ptr<Node> node, int iterator, std::string Filename, uint64_t m_startTime)
{
    // uint64_t m_startTime = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
    uint64_t timestamp = m_startTime + (uint64_t)Simulator::Now().GetMilliSeconds();

    int imsi;
    Ptr<Node> node1 = NodeList::GetNode(iterator);
    int nDevs = node->GetNDevices();
    std::string filename = Filename;
    std::ofstream outFile;
    for (int j = 0; j < nDevs; j++)
    {
        Ptr<McUeNetDevice> mcuedev = node1->GetDevice(j)->GetObject<McUeNetDevice>();
        Ptr<LteUeNetDevice> uedev = node->GetDevice(j)->GetObject<LteUeNetDevice>();
        Ptr<MmWaveUeNetDevice> mmuedev = node->GetDevice(j)->GetObject<MmWaveUeNetDevice>();
        if (mcuedev)
        {
            imsi = int(mcuedev->GetImsi());
            // Old: int serving_cell = ue_assoc_list[imsi - 1];
            int serving_cell = 0;
            auto itAssoc = ueAssocByImsi.find(static_cast<uint64_t>(imsi));
            if (itAssoc != ueAssocByImsi.end())
            {
                serving_cell = static_cast<int>(itAssoc->second);
            }
            if (serving_cell == 0)
            {
                serving_cell = 1;
            }
            Ptr<MobilityModel> model = node->GetObject<MobilityModel>();
            Vector position = model->GetPosition();
            Vector velocity = model->GetVelocity();
            double speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y +
                                     velocity.z * velocity.z); // speed in m/s
            double speedKmh = speed * 3.6;
            speedKmh = std::round(speedKmh * 10.0) / 10.0;
            /*NS_LOG_UNCOND("Position of UE with IMSI "
                          << imsi << " is " << model->GetPosition() << " at time "
                          << Simulator::Now().GetSeconds()
                          << ", UE connected to Cell: " << serving_cell);*/
            NS_LOG_UNCOND(Simulator::Now().GetSeconds()
                          << ": Position of UE with IMSI " << imsi << " is " << position
                          << ", Speed: " << speedKmh << " km/h"
                          << ", UE connected to Cell: " << serving_cell);
            outFile.open(filename.c_str(), std::ios_base::out | std::ios_base::app);
            if (!outFile.is_open())
            {
                NS_LOG_ERROR("Can't open file " << filename);
                return;
            }

            outFile << timestamp << "," << imsi << "," << position.x << "," << position.y << ",mc,"
                    << serving_cell << "," << m_startTime << std::endl;
            outFile.close();
        }
        else
        {
            //
        }
    }
    NS_LOG_UNCOND("---------------------------------------------");
}

//This function allows tracing current power consumption of cells
void
EnergyConsumptionUpdate(int nodeIndex,
                        std::string filename,
                        double totaloldEnergyConsumption,
                        double totalnewEnergyConsumption)
{
    // std::cout << "mmWave cell " << nodeIndex+2 << ": Total Energy Consumption " <<
    // totalnewEnergyConsumption << "J" << std::endl;
    Time currentTime = Simulator::Now();
    std::ofstream outFile;
    outFile.open(filename, std::ios_base::out | std::ios_base::app);
    outFile << currentTime.GetSeconds() << "," << totalnewEnergyConsumption << ","
            << (totalnewEnergyConsumption - totaloldEnergyConsumption) << std::endl;
    totalnewEnergyConsumption_storage[nodeIndex] = totalnewEnergyConsumption;
}

//This function allows observing current power consumption of cells
void
EnergyConsumptionPrint(int nodeIndex)
{
    //nodeIndex + 2 - Cell naming offset
    NS_LOG_UNCOND("Total energy consumption for mmWave cell "
                  << nodeIndex + 2 << ": " << totalnewEnergyConsumption_storage[nodeIndex] << "J"
                  << " at time " << Simulator::Now().GetSeconds()
                  << ", diff from last measurement is: "
                  << (totalnewEnergyConsumption_storage[nodeIndex] -
                      totaloldEnergyConsumption_storage[nodeIndex])
                  << "J");
    current_energy_consumption[nodeIndex] =
        totalnewEnergyConsumption_storage[nodeIndex] - totaloldEnergyConsumption_storage[nodeIndex];
    totaloldEnergyConsumption_storage[nodeIndex] = totalnewEnergyConsumption_storage[nodeIndex];
    NS_LOG_UNCOND("---------------------------------------------");
}

// Update configuration of cells from the O1 server
double (*O1_get_config(int argc, char* argv[], bool update))[matrix_cells_columns]
{
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
        // matrix_cells[cell_count][x] = entry.second.srsHoppingBandwidth();

        if (!update)
            NS_LOG_UNCOND("Using csiReportFormat = " << entry.second.csiReportFormat());
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
        if (update && matrix_cells[cell_count][16] != energyControl) {
            if (energyControl == -1)
            {
                NS_LOG_UNCOND("energySavingRState = Uninitialized");
            }
            else
            {
                NS_LOG_UNCOND("energySavingRState = " << energyControl);
            }
}
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
        {
            if (energyState == -1)
            {
                NS_LOG_UNCOND("energySavingRState = Uninitialized");
            }
            else
            {
                NS_LOG_UNCOND("energySavingRState = " << energyState);
            }
        }

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

// this function is needed to override mmwave limitation of RIC TaaP which blocks sectorized cells for mmwave module, it helps to handle sector-related commands
std::string
GetBaseSectorName(const std::string& name)
{
    if (name.size() > 3 &&
        (name.substr(name.size() - 3) == "_T2" || name.substr(name.size() - 3) == "_T3"))
        return name.substr(0, name.size() - 3) + "_T1";

    return name; // already T1 or no suffix
}

// Update energy saving state of cells from the O1 server
void
Update_O1_ES_Cells(int argc, char* argv[])
{
    static std::vector<std::string> prevESS(matrix_cells_rows, "0");

    static bool firstRun = true;

    // Load latest O1 config
    O1_get_config(argc, argv, true);

    std::vector<std::string> newESS(matrix_cells_rows);

    // Extract ESS
    for (int i = 0; i < matrix_cells_rows; i++)
    {
        int ess_val = static_cast<int>(matrix_cells[i][17]);
        newESS[i] = std::to_string(ess_val);
    }

    /*// Print the matrix
    std::cout << "Cell matrix contents:\n";
    for (int i = 0; i < matrix_cells_rows; i++)
    {
        for (int j = 0; j < matrix_cells_columns; j++)
        {
            std::cout << matrix_cells[i][j] << " ";
        }
        std::cout << "\n"; // Go to the next row after printing all columns in current row
    }
    std::cout << "\n";*/

    // First run: only initialize
    if (firstRun)
    {
        prevESS = newESS;
        firstRun = false;
        return;
    }

    // ---------------------------------------
    // STRUCT for storing all ESS changes
    // ---------------------------------------
    struct EsChange
    {
        int row;
        std::string oldVal;
        std::string newVal;
        std::string cellName;
        std::string baseName;
        int targetCellId;
    };

    std::vector<EsChange> changes;

    // ---------------------------------------
    // COLLECT ALL ESS CHANGES FIRST
    // ---------------------------------------

    for (int i = 0; i < matrix_cells_rows; i++)
    {
        if (prevESS[i] == newESS[i])
            continue;

        int rawCellValue = static_cast<int>(matrix_cells[i][19]);
        std::string cellName = matrix_cell_names[rawCellValue];
        std::string baseName = GetBaseSectorName(cellName);

        int targetCellId = (rawCellValue / 3) + 2; // current formula

        changes.push_back({i, prevESS[i], newESS[i], cellName, baseName, targetCellId});
    }

    if (changes.empty())
        return;

    // ---------------------------------------
    // PRINT UPDATE SUMMARY
    // ---------------------------------------
    std::cout << "\n==============================================\n";
    std::cout << "      ENERGY SAVING STATE UPDATES\n";
    std::cout << "==============================================\n";

    for (auto& c : changes)
    {
        std::cout << "Cell: " << c.cellName << " | Base: " << c.baseName << " | ESS: " << c.oldVal
                  << " -> " << c.newVal << " | Target CellId: " << c.targetCellId << std::endl;
    }

    std::cout << "----------------------------------------------\n";


    // ---------------------------------------
    // APPLY ALL CHANGES IN ONE PASS
    // ---------------------------------------
    for (NodeList::Iterator it = NodeList::Begin(); it != NodeList::End(); ++it)
    {
        Ptr<Node> node = *it;

        for (uint32_t j = 0; j < node->GetNDevices(); j++)
        {
            Ptr<MmWaveEnbNetDevice> mmdev = node->GetDevice(j)->GetObject<MmWaveEnbNetDevice>();

            if (!mmdev)
                continue;

            uint16_t cell_id = mmdev->GetCellId();

            // Match against any changed cell
            for (auto& c : changes)
            {
                if (cell_id != c.targetCellId)
                    continue;

                Ptr<MmWaveEnbPhy> enbPhy = mmdev->GetPhy();
                if (!enbPhy)
                {
                    NS_LOG_UNCOND("No PHY for cell " << cell_id);
                    continue;
                }

                // Apply energy saving state
                bool isOn = (c.newVal == "0");
                int txPower = isOn ? 30 : 0;
                int noiseFigure = isOn ? 5 : 100;

                enbPhy->SetTxPower(txPower);
                enbPhy->SetNoiseFigure(noiseFigure);

                if (!isOn)
                    NS_LOG_UNCOND("Cell OFF: " << cell_id);
                else
                    NS_LOG_UNCOND("Cell ON: " << cell_id);

                std::cout << "Applied ESS → CellId " << cell_id << " | TxPower=" << txPower
                          << " | NoiseFigure=" << noiseFigure << std::endl;
            }
        }
    }

    std::cout << "==============================================\n";

    // Save updated ESS state
    prevESS = newESS;
}

//section with global values for mmwave simulation
static ns3::GlobalValue g_bufferSize("bufferSize",
                                     "RLC tx buffer size (MB)",
                                     ns3::UintegerValue(10),
                                     ns3::MakeUintegerChecker<uint32_t>());

static ns3::GlobalValue g_enableTraces("enableTraces",
                                       "If true, generate ns-3 traces",
                                       ns3::BooleanValue(true),
                                       ns3::MakeBooleanChecker());

static ns3::GlobalValue g_e2lteEnabled("e2lteEnabled",
                                       "If true, send LTE E2 reports",
                                       ns3::BooleanValue(false),
                                       ns3::MakeBooleanChecker());

static ns3::GlobalValue g_e2nrEnabled("e2nrEnabled",
                                      "If true, send NR E2 reports",
                                      ns3::BooleanValue(false),
                                      ns3::MakeBooleanChecker());

static ns3::GlobalValue g_e2du("e2du",
                               "If true, send DU reports",
                               ns3::BooleanValue(true),
                               ns3::MakeBooleanChecker());

static ns3::GlobalValue g_e2cuUp("e2cuUp",
                                 "If true, send CU-UP reports",
                                 ns3::BooleanValue(true),
                                 ns3::MakeBooleanChecker());

static ns3::GlobalValue g_e2cuCp("e2cuCp",
                                 "If true, send CU-CP reports",
                                 ns3::BooleanValue(true),
                                 ns3::MakeBooleanChecker());

static ns3::GlobalValue g_reducedPmValues("reducedPmValues",
                                          "If true, use a subset of the the pm containers",
                                          ns3::BooleanValue(false),
                                          ns3::MakeBooleanChecker());

static ns3::GlobalValue g_hoSinrDifference(
    "hoSinrDifference",
    "The value for which an handover between MmWave eNB is triggered",
    ns3::DoubleValue(2),
    ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_indicationPeriodicity(
    "indicationPeriodicity",
    "E2 Indication Periodicity reports (value in seconds)",
    ns3::DoubleValue(0.1),
    ns3::MakeDoubleChecker<double>(0.01, 2.0));

static ns3::GlobalValue g_simTime("simTime",
                                  "Simulation time in seconds",
                                  ns3::DoubleValue(1000),
                                  ns3::MakeDoubleChecker<double>(0.1, 100000.0));

static ns3::GlobalValue g_outageThreshold(
    "outageThreshold",
    "SNR threshold for outage events [dB]", // use -1000.0 with NoAuto
    ns3::DoubleValue(-50.0),
    ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_numberOfRaPreambles(
    "numberOfRaPreambles",
    "how many random access preambles are available for the contention based RACH process",
    ns3::UintegerValue(40), // Indicated for TS use case, 52 is default
    ns3::MakeUintegerChecker<uint8_t>());

static ns3::GlobalValue g_handoverMode(
    "handoverMode",
    "HO euristic to be used,"
    "can be only \"NoAuto\", \"FixedTtt\", \"DynamicTtt\",   \"Threshold\"",
    ns3::StringValue("DynamicTtt"),
    ns3::MakeStringChecker());

static ns3::GlobalValue g_e2TermIp("e2TermIp",
                                   "The IP address of the RIC E2 termination",
                                   ns3::StringValue("127.0.0.1"),
                                   ns3::MakeStringChecker());

static ns3::GlobalValue g_enableE2FileLogging(
    "enableE2FileLogging",
    "If true, generate offline file logging instead of connecting to RIC",
    ns3::BooleanValue(false),
    ns3::MakeBooleanChecker());
static ns3::GlobalValue g_e2_func_id("KPM_E2functionID",
                                     "Function ID to subscribe",
                                     ns3::DoubleValue(2),
                                     ns3::MakeDoubleChecker<double>());
static ns3::GlobalValue g_rc_e2_func_id("RC_E2functionID",
                                        "Function ID to subscribe",
                                        ns3::DoubleValue(3),
                                        ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue g_controlFileName("controlFileName",
                                          "The path to the control file (can be absolute)",
                                          ns3::StringValue(""),
                                          ns3::MakeStringChecker());

static ns3::GlobalValue ue_s("N_Ues",
                             "Number of User Equipments",
                             ns3::UintegerValue(3),
                             ns3::MakeUintegerChecker<uint32_t>());

static ns3::GlobalValue center_freq("CenterFrequency",
                                    "Center Frequency Value",
                                    ns3::DoubleValue(3.5e9),
                                    ns3::MakeDoubleChecker<double>());

static ns3::GlobalValue bandwidth_value("Bandwidth",
                                        "Bandwidth Value",
                                        ns3::DoubleValue(20e6),
                                        ns3::MakeDoubleChecker<double>());

int
main(int argc, char* argv[])
{
    // Load latest O1 config
    O1_get_config(argc, argv, true);

    double minX = std::numeric_limits<int>::max();
    double minY = std::numeric_limits<int>::max();

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

    LogComponentEnableAll(LOG_PREFIX_ALL);
    //  LogComponentEnable ("RicControlMessage", LOG_LEVEL_ALL);
    //  LogComponentEnable ("KpmIndication", LOG_LEVEL_DEBUG);
    // LogComponentEnable("KpmIndication", LOG_LEVEL_INFO);

    // LogComponentEnable ("Asn1Types", LOG_LEVEL_LOGIC);
    //   LogComponentEnable ("E2Termination", LOG_LEVEL_LOGIC);
    //  LogComponentEnable ("E2Termination", LOG_LEVEL_DEBUG);

    // LogComponentEnable ("LteEnbNetDevice", LOG_LEVEL_ALL);
    // LogComponentEnable ("MmWaveEnbNetDevice", LOG_LEVEL_INFO);
    //  LogComponentEnable ("LteEnbRrc", LOG_LEVEL_INFO);
    // LogComponentEnable ("EpcX2", LOG_LEVEL_LOGIC);

    // Command line arguments
    CommandLine cmd;
    cmd.Parse(argc, argv);

    bool harqEnabled = true;

    UintegerValue uintegerValue;
    BooleanValue booleanValue;
    StringValue stringValue;
    DoubleValue doubleValue;

    GlobalValue::GetValueByName("hoSinrDifference", doubleValue);
    double hoSinrDifference = doubleValue.Get();
    GlobalValue::GetValueByName("bufferSize", uintegerValue);
    uint32_t bufferSize = uintegerValue.Get();
    GlobalValue::GetValueByName("enableTraces", booleanValue);
    bool enableTraces = booleanValue.Get();
    GlobalValue::GetValueByName("outageThreshold", doubleValue);
    double outageThreshold = doubleValue.Get();
    GlobalValue::GetValueByName("handoverMode", stringValue);
    std::string handoverMode = stringValue.Get();
    GlobalValue::GetValueByName("e2TermIp", stringValue);
    std::string e2TermIp = stringValue.Get();
    GlobalValue::GetValueByName("enableE2FileLogging", booleanValue);
    bool enableE2FileLogging = booleanValue.Get();
    GlobalValue::GetValueByName("KPM_E2functionID", doubleValue);
    double g_e2_func_id = doubleValue.Get();
    GlobalValue::GetValueByName("RC_E2functionID", doubleValue);
    double g_rc_e2_func_id = doubleValue.Get();

    GlobalValue::GetValueByName("numberOfRaPreambles", uintegerValue);
    uint8_t numberOfRaPreambles = uintegerValue.Get();

    NS_LOG_UNCOND("bufferSize " << bufferSize << " OutageThreshold " << outageThreshold
                                << " HandoverMode " << handoverMode << " e2TermIp " << e2TermIp
                                << " enableE2FileLogging " << enableE2FileLogging
                                << " E2 Function ID " << g_e2_func_id);

    GlobalValue::GetValueByName("e2lteEnabled", booleanValue);
    bool e2lteEnabled = booleanValue.Get();
    GlobalValue::GetValueByName("e2nrEnabled", booleanValue);
    bool e2nrEnabled = booleanValue.Get();
    GlobalValue::GetValueByName("e2du", booleanValue);
    bool e2du = booleanValue.Get();
    GlobalValue::GetValueByName("e2cuUp", booleanValue);
    bool e2cuUp = booleanValue.Get();
    GlobalValue::GetValueByName("e2cuCp", booleanValue);
    bool e2cuCp = booleanValue.Get();

    GlobalValue::GetValueByName("reducedPmValues", booleanValue);
    bool reducedPmValues = booleanValue.Get();

    GlobalValue::GetValueByName("indicationPeriodicity", doubleValue);
    double indicationPeriodicity = doubleValue.Get();
    GlobalValue::GetValueByName("controlFileName", stringValue);
    std::string controlFilename = stringValue.Get();

    NS_LOG_UNCOND("e2lteEnabled " << e2lteEnabled << " e2nrEnabled " << e2nrEnabled << " e2du "
                                  << e2du << " e2cuCp " << e2cuCp << " e2cuUp " << e2cuUp
                                  << " controlFilename " << controlFilename
                                  << " indicationPeriodicity " << indicationPeriodicity);

    Config::SetDefault("ns3::LteEnbNetDevice::ControlFileName", StringValue(controlFilename));
    Config::SetDefault("ns3::LteEnbNetDevice::E2Periodicity", DoubleValue(indicationPeriodicity));
    Config::SetDefault("ns3::MmWaveEnbNetDevice::E2Periodicity",
                       DoubleValue(indicationPeriodicity));

    Config::SetDefault("ns3::MmWaveHelper::E2ModeLte", BooleanValue(e2lteEnabled));
    Config::SetDefault("ns3::MmWaveHelper::E2ModeNr", BooleanValue(e2nrEnabled));

    // The DU PM reports should come from both NR gNB as well as LTE eNB,
    // since in the RLC/MAC/PHY entities are present in BOTH NR gNB as well as LTE eNB.
    // DU reports from LTE eNB are not implemented in this release
    Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableDuReport", BooleanValue(e2du));

    // The CU-UP PM reports should only come from LTE eNB, since in the NS3 “EN-DC
    // simulation (Option 3A)”, the PDCP is only in the LTE eNB and NOT in the NR gNB
    Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuUpReport", BooleanValue(e2cuUp));
    Config::SetDefault("ns3::LteEnbNetDevice::EnableCuUpReport", BooleanValue(e2cuUp));

    Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableCuCpReport", BooleanValue(e2cuCp));
    Config::SetDefault("ns3::LteEnbNetDevice::EnableCuCpReport", BooleanValue(e2cuCp));

    Config::SetDefault("ns3::MmWaveEnbNetDevice::ReducedPmValues", BooleanValue(reducedPmValues));
    Config::SetDefault("ns3::LteEnbNetDevice::ReducedPmValues", BooleanValue(reducedPmValues));

    Config::SetDefault("ns3::LteEnbNetDevice::EnableE2FileLogging",
                       BooleanValue(enableE2FileLogging));
    Config::SetDefault("ns3::MmWaveEnbNetDevice::EnableE2FileLogging",
                       BooleanValue(enableE2FileLogging));

    Config::SetDefault("ns3::LteEnbNetDevice::KPM_E2functionID", DoubleValue(g_e2_func_id));
    Config::SetDefault("ns3::MmWaveEnbNetDevice::KPM_E2functionID", DoubleValue(g_e2_func_id));

    Config::SetDefault("ns3::LteEnbNetDevice::RC_E2functionID", DoubleValue(g_rc_e2_func_id));

    Config::SetDefault("ns3::MmWaveEnbMac::NumberOfRaPreambles",
                       UintegerValue(numberOfRaPreambles));

    Config::SetDefault("ns3::MmWaveHelper::HarqEnabled", BooleanValue(harqEnabled));
    Config::SetDefault("ns3::MmWaveHelper::UseIdealRrc", BooleanValue(true));
    Config::SetDefault("ns3::MmWaveHelper::E2TermIp", StringValue(e2TermIp));

    Config::SetDefault("ns3::MmWaveFlexTtiMacScheduler::HarqEnabled", BooleanValue(harqEnabled));
    Config::SetDefault("ns3::MmWavePhyMacCommon::NumHarqProcess", UintegerValue(100));
    // Config::SetDefault ("ns3::MmWaveBearerStatsCalculator::EpochDuration", TimeValue
    // (MilliSeconds (10.0)));

    // set to false to use the 3GPP radiation pattern (proper configuration of the bearing and
    // downtilt angles is needed)
    Config::SetDefault("ns3::PhasedArrayModel::AntennaElement",
                       PointerValue(CreateObject<IsotropicAntennaModel>()));
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(MilliSeconds(100.0)));
    Config::SetDefault("ns3::ThreeGppChannelConditionModel::UpdatePeriod",
                       TimeValue(MilliSeconds(100)));

    Config::SetDefault("ns3::LteRlcAm::ReportBufferStatusTimer", TimeValue(MilliSeconds(10.0)));
    Config::SetDefault("ns3::LteRlcUmLowLat::ReportBufferStatusTimer",
                       TimeValue(MilliSeconds(10.0)));
    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));
    Config::SetDefault("ns3::LteRlcUmLowLat::MaxTxBufferSize",
                       UintegerValue(bufferSize * 1024 * 1024));
    Config::SetDefault("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue(bufferSize * 1024 * 1024));

    Config::SetDefault("ns3::LteEnbRrc::OutageThreshold", DoubleValue(outageThreshold));
    Config::SetDefault("ns3::LteEnbRrc::SecondaryCellHandoverMode", StringValue(handoverMode));
    Config::SetDefault("ns3::LteEnbRrc::HoSinrDifference", DoubleValue(hoSinrDifference));
    Config::SetDefault("ns3::ThreeGppPropagationLossModel::Frequency", DoubleValue(3.5e9));
    Config::SetDefault("ns3::ThreeGppPropagationLossModel::ShadowingEnabled", BooleanValue(false));

    Config::SetDefault("ns3::MmWaveEnbPhy::TxPower", DoubleValue(30));
    Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(250));
    Config::SetDefault("ns3::LteEnbPhy::NoiseFigure", DoubleValue(0));

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

    // Carrier bandwidth in Hz
    double bandwidth = 20e6;
    // Center frequency in Hz
    double centerFrequency = 3.5e9;

    if (matrix_cells[0][1] == 3.5e9)
    {
        // Carrier bandwidth in Hz
        bandwidth = 20e6;
        // Center frequency in Hz
        centerFrequency = 3.5e9;
    }
    else if (matrix_cells[0][1] > 0.7e9)
    {
        // Carrier bandwidth in Hz
        bandwidth = 20e6;
        // Center frequency in Hz
        centerFrequency = 0.7e9;
    }
    else
    {
        //
    }

    // Number of antennas in each UE
    // GlobalValue::GetValueByName ("N_AntennasMcUe", uintegerValue);
    int numAntennasMcUe = 1; // uintegerValue.Get();
    // Number of antennas in each mmWave BS
    // GlobalValue::GetValueByName ("N_AntennasMmWave", uintegerValue);
    int numAntennasMmWave = 1; // uintegerValue.Get();

    /*NS_LOG_INFO("Bandwidth " << bandwidth << " centerFrequency " << double(centerFrequency)
                             << " isd_ue " << isd_ue << " numAntennasMcUe " << numAntennasMcUe
                             << " numAntennasMmWave " << numAntennasMmWave);*/

    // Set the number of antennas in the devices
    Config::SetDefault("ns3::McUeNetDevice::AntennaNum", UintegerValue(numAntennasMcUe));
    Config::SetDefault("ns3::MmWaveNetDevice::AntennaNum", UintegerValue(numAntennasMmWave));
    Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(bandwidth));
    Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue(centerFrequency));

    Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
    mmwaveHelper->SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");
    mmwaveHelper->SetChannelConditionModelType("ns3::ThreeGppUmiStreetCanyonChannelConditionModel");

    Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper>();
    mmwaveHelper->SetEpcHelper(epcHelper);

    // GlobalValue::GetValueByName ("N_LteEnbNodes", uintegerValue);
    uint8_t nLteEnbNodes = 1; // uintegerValue.Get();

    // Get SGW/PGW and create a single RemoteHost
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create the Internet by connecting remoteHost to pgw. Setup routing too
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(2500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    // interface 0 is localhost, 1 is the p2p device
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    // create LTE, mmWave eNB nodes and UE node

    NodeContainer mmWaveEnbNodes;
    NodeContainer lteEnbNodes;
    NodeContainer allEnbNodes;

    lteEnbNodes.Create(nLteEnbNodes);
    allEnbNodes.Add(lteEnbNodes);

    uint8_t nMmWaveEnbNodes = 0;

    // Position setup
    Vector centerPosition = Vector(maxXAxis / 2, maxYAxis / 2, 30);
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    // One LTE eNB in the center
    positionAlloc->Add(centerPosition);

    // === Handle 3500 MHz ===
    if (!matrix_3500.empty() && matrix_cells[0][1] == 3.5e9)
    {
        // Count how many isotropic mmWave cells exist (sector 1 per cell)
        for (uint32_t i = 0; i < matrix_3500.size(); i++)
        {
            if (matrix_3500[i][8] == 1)
            {
                nMmWaveEnbNodes++;
                i += 2; // skip the other two sectors of this cell
            }
        }

        // Create exactly as many mmWave nodes as isotropic cells
        mmWaveEnbNodes.Create(nMmWaveEnbNodes);
        allEnbNodes.Add(mmWaveEnbNodes);

        uint32_t nodeIndex = 0;
        for (uint32_t i = 0; i < matrix_3500.size(); i++)
        {
            if (matrix_3500[i][8] == 1)
            {
                Ptr<Node> gNB = mmWaveEnbNodes.Get(nodeIndex++);

                double x = matrix_3500[i][6];
                double y = matrix_3500[i][7];
                double z = matrix_3500[i][4];

                positionAlloc->Add(Vector(x, y, z));
                i += 2; // jump to next cell
            }
        }
    }

    // === Handle 700 MHz ===
    else if (!matrix_700.empty() && matrix_700[0][1] == 1)
    {
        for (uint32_t i = 0; i < matrix_700.size(); i++)
        {
            if (matrix_700[i][8] == 1)
            {
                nMmWaveEnbNodes++;
                i += 2;
            }
        }

        mmWaveEnbNodes.Create(nMmWaveEnbNodes);
        allEnbNodes.Add(mmWaveEnbNodes);

        uint32_t nodeIndex = 0;
        for (uint32_t i = 0; i < matrix_700.size(); i++)
        {
            if (matrix_700[i][8] == 1)
            {
                Ptr<Node> gNB = mmWaveEnbNodes.Get(nodeIndex++);

                double x = matrix_700[i][6];
                double y = matrix_700[i][7];
                double z = matrix_700[i][4];

                positionAlloc->Add(Vector(x, y, z));
                i += 2;
            }
        }
    }

    // Apply mobility
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(allEnbNodes);

    NodeContainerManager::GetInstance().SetMmWaveEnbNodes(mmWaveEnbNodes);

    // Define number of UEs based on number of cells
    uint32_t ueFactor = 1; // UEs per cell
    uint32_t numUes = nMmWaveEnbNodes * ueFactor;

    // Create UE container
    NodeContainer ueNodes;
    ueNodes.Create(numUes);

    // Create position allocator for UEs
    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();

    // Random offset generators (so UEs are near eNBs, within ~50–100m radius)
    Ptr<UniformRandomVariable> offset = CreateObject<UniformRandomVariable>();
    offset->SetAttribute("Min", DoubleValue(-50.0));
    offset->SetAttribute("Max", DoubleValue(50.0));

    // Assign UEs close to mmWave eNBs
    for (uint32_t i = 0; i < nMmWaveEnbNodes; ++i)
    {
        Ptr<Node> enb = mmWaveEnbNodes.Get(i);
        Vector enbPos = enb->GetObject<MobilityModel>()->GetPosition();

        for (uint32_t j = 0; j < ueFactor; ++j)
        {
            double x = enbPos.x + offset->GetValue();
            double y = enbPos.y + offset->GetValue();
            double z = 1.5;
            uePositionAlloc->Add(Vector(x, y, z));
        }
    }

    // Configure UE mobility
    MobilityHelper uemobility;
    uemobility.SetPositionAllocator(uePositionAlloc);

    // Slight random walk within small area around starting point
    Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable>();
    speed->SetAttribute("Min", DoubleValue(50.0));
    speed->SetAttribute("Max", DoubleValue(100.0));

    uemobility.SetMobilityModel("ns3::RandomWalk2dOutdoorMobilityModel",
                                "Speed",
                                PointerValue(speed),
                                "Bounds",
                                RectangleValue(Rectangle(0, maxXAxis, 0, maxYAxis)));

    // Install on UEs
    uemobility.Install(ueNodes);

    /*MobilityHelper uemobility;

    Ptr<UniformDiscPositionAllocator> uePositionAlloc =
        CreateObject<UniformDiscPositionAllocator>();

    uePositionAlloc->SetX(centerPosition.x);
    uePositionAlloc->SetY(centerPosition.y);
    uePositionAlloc->SetRho(isd_ue);
    Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable>();
    speed->SetAttribute("Min", DoubleValue(2.0));
    speed->SetAttribute("Max", DoubleValue(4.0));

    uemobility.SetMobilityModel("ns3::RandomWalk2dOutdoorMobilityModel",
                                "Speed",
                                PointerValue(speed),
                                "Bounds",
                                RectangleValue(Rectangle(0, maxXAxis, 0, maxYAxis)));
    uemobility.SetPositionAllocator(uePositionAlloc);
    uemobility.Install(ueNodes);*/

    // Install mmWave, lte, mc Devices to the nodesnumAntennasMmWave
    NetDeviceContainer lteEnbDevs = mmwaveHelper->InstallLteEnbDevice(lteEnbNodes);
    NetDeviceContainer mmWaveEnbDevs = mmwaveHelper->InstallEnbDevice(mmWaveEnbNodes);
    NetDeviceContainer mcUeDevs = mmwaveHelper->InstallMcUeDevice(ueNodes);

    // Install the IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(mcUeDevs));
    // Assign IP address to UEs, and install applications
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        // Set the default gateway for the UE
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Add X2 interfaces
    mmwaveHelper->AddX2Interface(lteEnbNodes, mmWaveEnbNodes);

    // for (uint16_t i = 0; i < mmWaveEnbNodes.GetN(); ++i)
    //{
    // for (uint16_t j = i+1; j < mmWaveEnbNodes.GetN(); ++j)
    //{
    // if (i != j)
    //{
    // mmwaveHelper->AddX2Interface(mmWaveEnbNodes.Get(i), mmWaveEnbNodes.Get(j));
    //}
    //}
    //}

    // Manual attachment
    mmwaveHelper->AttachToClosestEnb(mcUeDevs, mmWaveEnbDevs, lteEnbDevs);

    BasicEnergySourceHelper basicEnergySourceHelper;
    basicEnergySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1000000000000));
    basicEnergySourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(5.0));
    energy::EnergySourceContainer sources = basicEnergySourceHelper.Install(mmWaveEnbNodes);
    MmWaveRadioEnergyModelEnbHelper nrEnbHelper;

    energy::DeviceEnergyModelContainer deviceEModel = nrEnbHelper.Install(mmWaveEnbDevs, sources);

    GlobalValue::GetValueByName("simTime", doubleValue);
    double simTime = doubleValue.Get();
    int numPrints = simTime / 0.1;

    std::vector<std::ofstream> outFiles;
    for (int x = 0; x < nMmWaveEnbNodes; ++x)
    {
        std::ostringstream energyFileName;
        energyFileName << "energyfilecell" << x + 2 << ".csv";

        std::ofstream outFile;
        outFile.open(energyFileName.str(), std::ios_base::out | std::ios_base::trunc);
        outFile << "Time,NetEnergy,DiffEnergy" << std::endl;

        outFiles.push_back(std::move(outFile));
    }

    for (int x = 0; x < nMmWaveEnbNodes; ++x)
    {
        std::ostringstream filename;
        filename << "energyfilecell" << x + 2 << ".csv";
        deviceEModel.Get(x)->TraceConnectWithoutContext(
            "TotalEnergyConsumption",
            MakeBoundCallback(&EnergyConsumptionUpdate, x, filename.str()));
        for (int i = 0; i < numPrints; i++)
        {
            Simulator::Schedule(Seconds(i * simTime / numPrints), &EnergyConsumptionPrint, x);
        }
    }

    // Install and start applications
    // On the remoteHost there is UDP OnOff Application

    uint16_t portUdp = 60000;
    Address sinkLocalAddressUdp(InetSocketAddress(Ipv4Address::GetAny(), portUdp));
    PacketSinkHelper sinkHelperUdp("ns3::UdpSocketFactory", sinkLocalAddressUdp);
    AddressValue serverAddressUdp(InetSocketAddress(remoteHostAddr, portUdp));

    ApplicationContainer sinkApp;
    sinkApp.Add(sinkHelperUdp.Install(remoteHost));

    ApplicationContainer clientApp;

    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        // Full traffic
        PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory",
                                            InetSocketAddress(Ipv4Address::GetAny(), 1234));
        sinkApp.Add(dlPacketSinkHelper.Install(ueNodes.Get(u)));
        UdpClientHelper dlClient(ueIpIface.GetAddress(u), 1234);
        dlClient.SetAttribute("Interval", TimeValue(MicroSeconds(500)));
        dlClient.SetAttribute("MaxPackets", UintegerValue(UINT32_MAX));
        dlClient.SetAttribute("PacketSize", UintegerValue(200)); // defult 1280
        clientApp.Add(dlClient.Install(remoteHost));
    }

    // Start applications

    sinkApp.Start(Seconds(0));

    clientApp.Start(MilliSeconds(100));
    clientApp.Stop(Seconds(simTime - 0.1));

    struct timeval time_now{};
    gettimeofday(&time_now, nullptr);
    uint64_t t_startTime_simid = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
    std::string ue_poss_out = "ue_position.txt";
    ClearFile(ue_poss_out, t_startTime_simid);
    ClearFile("enbs.txt", t_startTime_simid);
    ClearFile("gnbs.txt", t_startTime_simid);
    // Since nodes are randomly allocated during each run we always need to print their
    // positions
    PrintGnuplottableUeListToFile("ues.txt");

    int nodecount = int(NodeList::GetNNodes());
    // NS_LOG_UNCOND ("number of nodes: " << nodecount);
    int UE_iterator = nodecount - int(numUes);

    for (int i = 1; i < numPrints; i++)
    {
        Simulator::Schedule(Seconds(i * simTime / numPrints),
                            &PrintGnuplottableEnbListToFile,
                            t_startTime_simid);
        for (uint32_t j = 0; j < ueNodes.GetN(); j++)
        {
            Simulator::Schedule(Seconds(i * simTime / numPrints),
                                &PrintPosition,
                                ueNodes.Get(j),
                                j + UE_iterator,
                                ue_poss_out,
                                t_startTime_simid);
        }
        Simulator::Schedule(Seconds(i * simTime / numPrints), &Update_O1_ES_Cells, argc, argv);
    }

    if (enableTraces)
    {
        mmwaveHelper->EnableTraces();
    }

    // trick to enable PHY traces for the LTE stack
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->Initialize();
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();

    bool run = true;
    if (run)
    {
        NS_LOG_UNCOND("Simulation time is " << simTime << " seconds ");
        Simulator::Stop(Seconds(simTime));
        NS_LOG_INFO("Run Simulation.");
        Simulator::Run();
    }

    NS_LOG_INFO(lteHelper);
    Simulator::Destroy();
    NS_LOG_INFO("Done.");
    return 0;
}