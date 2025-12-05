*Put CM files from CSON and Topo from OSIRIS into mmwave-LENA-oran/O1_sim/config_data/
1. /mmwave-LENA-oran/O1_sim/config_data$ python3 O1_server.py
2. ./ns3 run 'scratch/Energy_saving_with_cell_utilization_scenario_O1.cc' or for Lena5G ./ns3 run 'scratch/opl_nr_mimo_demo.cc'
3. (Need to be run first to initialize ES states, need to be fixed) Run ./TurnOn to turn on BRANGES_T2 cell (this means put in Energy Not Saving State).
4. Run ./TurnOff to turn off cell (this means put in Energy Saving State).
5. Change cell ids inside the script to choose other cell according to example and Topo_Example. 
6. Run ./TurnOn to turn on cell (this means put in Energy Not Saving State).
7. Change cell ids inside the script to choose other cell. 
8. Run ./EnergySavingState to get the energy state.
