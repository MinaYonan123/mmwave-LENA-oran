1. Put CM files from CSON and Topo from OSIRIS into mmwave-LENA-oran/O1_sim/config_data/
2. mmwave-LENA-oran/O1_sim/config_data$ python3 O1_server.py
3. ./ns3 run 'scratch/Energy_saving_with_cell_utilization_scenario_O1.cc' or for Lena5G ./ns3 run 'scratch/opl_nr_mimo_demo.cc'
4. Run ./TurnOff to turn off BRANGES_T2 cell (this means put in Energy Saving State).
5. Change cell ids inside the script to choose other cell. 
6. Run ./TurnOn to turn on BRANGES_T2 cell (this means put in Energy Not Saving State).
7. Change cell ids inside the script to choose other cell. 
8. Run ./EnergySavingState to get the energy state for BRANGES_T2 cell.
