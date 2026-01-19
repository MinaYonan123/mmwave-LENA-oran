(Input for RIC TaaP README)

## 4. Main Updates
### 4.7 O1 interface emulation extension
O1 extension for RIC TaaP has two main functions:
1. Provide a mechanism to simplify building ns3 scenarios using user-defined topology and Configuration Management (CM) external files (.csv) - this function uses O1 method 'get-config'.
2. Provide a mechanism to manipulate node (cell) configuration during simulation runtime (e.g., turn on/off cell) - this function uses O1 method 'edit-config'.

O1 server uses NETCONF protocol, according to RFC 6241 and O-RAN Operations and Maintenance Interface 4.0.

### 6.4 Usage/deployment
#### 6.4.6 O1 get-config demo
Demo presents how to use predefined external topology and CMs to build a simulation scenario without developing code.
Steps to replicate the demo:
1. Modification topology and CMs if needed at: mmwave-LENA-oran/O1_sim/config_data/csv/
2. Run O1 server: 
    ```c
   /O1_sim/config_data$ python3 O1_server.py
3. Run the proper scenario for mmwave:
    ```c
   ./ns3 run 'scratch/Energy_saving_with_cell_utilization_scenario_O1.cc'
4. Run the proper scenario for 5G-LENA:
    ```c
   ./ns3 run 'scratch/O1_autoconfiguration_5GLena_with_MIMO'

#### 6.4.6 O1 edit-config demo
Demo presents how to modification cell properties during simulation run-time with O1 edit-config method.
Steps to replicate the demo:
1. Run O1 server:
    ```c
    /O1_sim/config_data$ python3 O1_server.py
2. Run the proper scenario
    ```c
    ./ns3 run 'scratch/Energy_saving_with_cell_utilization_scenario_O1.cc'

3. Put in Energy Saving State — Cell off
    ```c
    /O1_sim/demo$ ./TurnOff

4. Put in Energy Saving State — Cell on
    ```c
   /O1_sim/demo$ ./TurnOn
   
Users can choose cells to be reconfigured by modification of TurnOff/TurnOn scripts and putting proper values, according to the Topo_Example.csv file. 
