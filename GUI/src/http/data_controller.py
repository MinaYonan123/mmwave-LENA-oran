import os
import subprocess
from dataclasses import asdict

import requests
import json

import paramiko
from fastapi import APIRouter, Request, Depends
from fastapi.responses import JSONResponse
from fastapi.templating import Jinja2Templates

from src.simulation_objects.simulation import Simulation
from src.simulation_objects.simulation_manager import SimulationManager

influx_data_router = APIRouter()
templates = Jinja2Templates(directory="src/templates")


def get_simulation() -> Simulation:
    return SimulationManager.get_simulation()


@influx_data_router.get("/")
async def root(request: Request, simulation: Simulation = Depends(get_simulation)):
    host_ns3 = os.getenv('NS3_HOST')
    return templates.TemplateResponse(
        "chart.html",
        {
            "request": request,
            "ues": simulation.ues,
            "cells": simulation.cells,
            "sim_id": simulation.sim_id,
            "chart_dimensions": (simulation.max_x, simulation.max_y),
            "host_ns3": host_ns3,
        },
    )

@influx_data_router.get("/scenarios")
async def scenarios(request: Request):
    remote_host = os.getenv('NS3_HOST')
    response = requests.get( f'http://{remote_host}:38866')
    files = {}
    if response.status_code == 200:
        files = json.loads(response.text)
    else:
        files = {"0":"scratch/scenario-zero-with_parallel_loging.cc",
            "1":"scratch/scenario-one.cc",
            "2":"scratch/scenario-zero.cc"}
    return files

@influx_data_router.get("/refresh-data")
async def refresh_data(request: Request, simulation: Simulation = Depends(get_simulation)):
    SimulationManager.refresh_simulation()
    updated_simulation = SimulationManager.get_simulation()
    if (updated_simulation.number_of_ues == 0 or updated_simulation.number_of_cells == 0) and updated_simulation.simulation_status == 'on':
        updated_simulation.set_ue_cell_number()
    es_state = {}
    sinr = {}
    retx = {}
    prb = {}
    for cell in updated_simulation.cells:
        es_state[cell.cell_id] = cell.es_state
        prb[cell.cell_id] = cell.dlPrbUsage_percentage
    for ue in updated_simulation.ues:
        sinr[ue.ue_id] = ue.L3servingSINR_dB
        retx[ue.ue_id] = ue.ErrTotalNbrDl
    
    # Query InfluxDB for RF config data (NR cell)
    # Initialize with default values to ensure rf_config is always populated
    cell_id = 1
    rrc_conn = 0
    ue_throughput = 0.0
    averagepower = 0.0 
    try:
        from influxdb import InfluxDBClient
        import os
        # Use environment variable for Docker network, fallback to localhost for local dev
        influx_host = os.getenv('INFLUXDB_HOST', 'localhost')
        influx_port = int(os.getenv('INFLUXDB_PORT', '8086'))
        influx_user = os.getenv('INFLUXDB_USERNAME', 'root')
        influx_pass = os.getenv('INFLUXDB_PASSWORD', 'root')
        influx_db = os.getenv('INFLUXDB_DATABASE', 'influx')
        
        print(f"[RF_CONFIG] Connecting to InfluxDB at {influx_host}:{influx_port}")
        client = InfluxDBClient(host=influx_host, port=influx_port, username=influx_user, password=influx_pass, database=influx_db)
        
        # Query for numActiveUes (RRC connections) - try NR format first
        try:
            rrc = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_numactiveues"').get_points())
            if rrc and len(rrc) > 0 and rrc[0].get('last') is not None:
                rrc_conn = int(rrc[0]['last'])
                print(f"[RF_CONFIG] Found RRC connections from NR: {rrc_conn}")
        except Exception as e:
            print(f"[RF_CONFIG] Error querying NR RRC: {e}")
        
        # Query for cellId - try NR format first
        try:
            cid = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_cellid"').get_points())
            if cid and len(cid) > 0 and cid[0].get('last') is not None:
                cell_id = int(cid[0]['last'])
                print(f"[RF_CONFIG] Found cell_id from NR: {cell_id}")
        except Exception as e:
            print(f"[RF_CONFIG] Error querying NR cellId: {e}")
                # Initialize portson and portsoff before querying
         # Initialize portson and portsoff before querying
        portson = 0
        portsoff = 0
        # Query for portson - try NR format first
        try:
            result = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_portson"').get_points())
            if result and len(result) > 0 and result[0].get('last') is not None:
                portson = int(result[0]['last'])
                print(f"[RF_CONFIG] Found portson from NR: {portson}")
        except Exception as e:
            print(f"[RF_CONFIG] Error querying NR portson: {e}")
        
        # Query for portsoff - try NR format first
        try:
            result = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_portsoff"').get_points())
            if result and len(result) > 0 and result[0].get('last') is not None:
                portsoff = int(result[0]['last'])
                print(f"[RF_CONFIG] Found portsoff from NR: {portsoff}")
        except Exception as e:
            print(f"[RF_CONFIG] Error querying NR portsoff: {e}")
        
        # Query for pdcpThroughput - try UE level first, then cell level as fallback
        try:
            result = list(client.query('SELECT LAST("value") FROM "ue_1_drb.pdcpsdubitratedl.ueid(pdcpthroughput)"').get_points())
            if result and len(result) > 0 and result[0].get('last') is not None:
                ue_throughput = float(result[0]['last'])
                print(f"[RF_CONFIG] Found UE throughput (ue-level): {ue_throughput}")
            else:
                result = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_drb.pdcpsdubitratedl.ueid(pdcpthroughput)"').get_points())
                if result and len(result) > 0 and result[0].get('last') is not None:
                    ue_throughput = float(result[0]['last'])
                    print(f"[RF_CONFIG] Found UE throughput (cell-level fallback): {ue_throughput}")
        except Exception as e:
            print(f"[RF_CONFIG] Error querying throughput: {e}")
       # Query for average power - try NR format first
        try:
            averagepower = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_averagepower"').get_points())
            if averagepower and len(averagepower) > 0 and averagepower[0].get('last') is not None:
                averagepower = float(averagepower[0]['last'])
                print(f"[RF_CONFIG] Found average power from NR: {averagepower}")
        except Exception as e:
            averagepower = 0.0  # ADD THIS LINE - set default value on error
            print(f"[RF_CONFIG] Error querying NR averagepower: {e}")
        # Query for indicationflag - try NR format first
        try:
            indicationflag = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_indicationflag"').get_points())
            if indicationflag and len(indicationflag) > 0 and indicationflag[0].get('last') is not None:
                indicationflag = int(indicationflag[0]['last'])
                print(f"[RF_CONFIG] Found indicationflag from NR: {indicationflag}")
        except Exception as e:
            indicationflag = 0
            print(f"[RF_CONFIG] Error querying NR indicationflag: {e}")
        # Query for controlflag - try NR format first
        try:
            controlflag = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_controlflag"').get_points())
            if controlflag and len(controlflag) > 0 and controlflag[0].get('last') is not None:
                controlflag = int(controlflag[0]['last'])
                print(f"[RF_CONFIG] Found controlflag from NR: {controlflag}")
        except Exception as e:
            controlflag = 0
            print(f"[RF_CONFIG] Error querying NR controlflag: {e}")
        # Query for newportson - try NR format first
        try:
            newportson = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_newportson"').get_points())
            if newportson and len(newportson) > 0 and newportson[0].get('last') is not None:
                newportson = int(newportson[0]['last'])
                print(f"[RF_CONFIG] Found newportson from NR: {newportson}")
        except Exception as e:
            newportson = 0
            print(f"[RF_CONFIG] Error querying NR newportson: {e}")
        # Query for newportsoff - try NR format first
        try:
            newportsoff = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_newportsoff"').get_points())
            if newportsoff and len(newportsoff) > 0 and newportsoff[0].get('last') is not None:
                newportsoff = int(newportsoff[0]['last'])
                print(f"[RF_CONFIG] Found newportsoff from NR: {newportsoff}")
        except Exception as e:
            newportsoff = 0
            print(f"[RF_CONFIG] Error querying NR newportsoff: {e}")
        # Query for power comparison stats
        power_comparison = {
            "baselineminpower": 0.0,
            "baselinemaxpower": 0.0,
            "baselineaccumulatedpower": 0.0,
            "baselinecurrentpower": 0.0,
            "xappminpower": 0.0,
            "xappmaxpower": 0.0,
            "xappaccumulatedpower": 0.0,
            "xappcurrentpower": 0.0,
            "powersaving": 0.0,
            "powersavingpercent": 0.0,
            "xappactive": 0
        }

        try:
            baselineminpower = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_baselineminpower"').get_points())
            if baselineminpower and len(baselineminpower) > 0 and baselineminpower[0].get('last') is not None:
                power_comparison["baselineminpower"] = float(baselineminpower[0]['last'])
                
            baselinemaxpower = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_baselinemaxpower"').get_points())
            if baselinemaxpower and len(baselinemaxpower) > 0 and baselinemaxpower[0].get('last') is not None:
                power_comparison["baselinemaxpower"] = float(baselinemaxpower[0]['last'])
                
            baselineaccumulatedpower = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_baselineaccumulatedpower"').get_points())
            if baselineaccumulatedpower and len(baselineaccumulatedpower) > 0 and baselineaccumulatedpower[0].get('last') is not None:
                power_comparison["baselineaccumulatedpower"] = float(baselineaccumulatedpower[0]['last'])
                
            baselinecurrentpower = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_baselinecurrentpower"').get_points())
            if baselinecurrentpower and len(baselinecurrentpower) > 0 and baselinecurrentpower[0].get('last') is not None:
                power_comparison["baselinecurrentpower"] = float(baselinecurrentpower[0]['last'])
                
            xappminpower = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_xappminpower"').get_points())
            if xappminpower and len(xappminpower) > 0 and xappminpower[0].get('last') is not None:
                power_comparison["xappminpower"] = float(xappminpower[0]['last'])
                
            xappmaxpower = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_xappmaxpower"').get_points())
            if xappmaxpower and len(xappmaxpower) > 0 and xappmaxpower[0].get('last') is not None:
                power_comparison["xappmaxpower"] = float(xappmaxpower[0]['last'])
                
            xappaccumulatedpower = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_xappaccumulatedpower"').get_points())
            if xappaccumulatedpower and len(xappaccumulatedpower) > 0 and xappaccumulatedpower[0].get('last') is not None:
                power_comparison["xappaccumulatedpower"] = float(xappaccumulatedpower[0]['last'])
                
            xappcurrentpower = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_xappcurrentpower"').get_points())
            if xappcurrentpower and len(xappcurrentpower) > 0 and xappcurrentpower[0].get('last') is not None:
                power_comparison["xappcurrentpower"] = float(xappcurrentpower[0]['last'])
                
            powersaving = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_powersaving"').get_points())
            if powersaving and len(powersaving) > 0 and powersaving[0].get('last') is not None:
                power_comparison["powersaving"] = float(powersaving[0]['last'])
                
            powersavingpercent = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_powersavingpercent"').get_points())
            if powersavingpercent and len(powersavingpercent) > 0 and powersavingpercent[0].get('last') is not None:
                power_comparison["powersavingpercent"] = float(powersavingpercent[0]['last'])
                
            xappactive = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_xappactive"').get_points())
            if xappactive and len(xappactive) > 0 and xappactive[0].get('last') is not None:
                power_comparison["xappactive"] = int(xappactive[0]['last'])
                
            baselinesamplecount = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_baselinesamplecount"').get_points())
            if baselinesamplecount and len(baselinesamplecount) > 0 and baselinesamplecount[0].get('last') is not None:
                power_comparison["baselinesamplecount"] = int(baselinesamplecount[0]['last'])
                
            xappsamplecount = list(client.query(f'SELECT LAST("value") FROM "nr-cu-up-cell-{cell_id}_xappsamplecount"').get_points())
            if xappsamplecount and len(xappsamplecount) > 0 and xappsamplecount[0].get('last') is not None:
                power_comparison["xappsamplecount"] = int(xappsamplecount[0]['last'])
                
            
            print(f"[RF_CONFIG] Found power comparison: min={power_comparison['baselineminpower']}, max={power_comparison['baselinemaxpower']}")
        except Exception as e:
            print(f"[RF_CONFIG] Error querying power comparison: {e}")
            
    except Exception as e:
        print(f"[RF_CONFIG] Error connecting to InfluxDB: {e}")
    
    # Always build rf_config with the values we have (defaults or queried)
    rf_config = {
        "cell_id": cell_id,
        "indication": {
            "rrc_connections": rrc_conn,
            "ue_dl_throughput": ue_throughput,
            "old_antenna_on": portson,
            "old_antenna_off": portsoff
        },
        "ue_throughput": {"value": ue_throughput},
        "control": {"new_antenna_on": newportson, "new_antenna_off": newportsoff},
        "e2ap_messages": {"Indication_Message": indicationflag, "Control_Messages": controlflag},
        "cell_power": {"avg_power": averagepower},
        "power_comparison": power_comparison
    }
    print(f"[RF_CONFIG] Final rf_config: cell_id={cell_id}, rrc={rrc_conn}, throughput={ue_throughput}")
    
    return {
        "ues": [asdict(ue) for ue in updated_simulation.ues],
        "cells": [asdict(cell) for cell in updated_simulation.cells],
        "max_x_max_y": (updated_simulation.max_x, updated_simulation.max_y),
        "sim_id": updated_simulation.sim_id if updated_simulation.sim_id else 'off',
        "es_state": es_state,
        "sinr": sinr,
        "retx": retx,
        "prb": prb,
        "starting_power": updated_simulation.starting_power,
        "current_power": updated_simulation.current_power,
        "maxec": updated_simulation.maxec,
        "totalcurrec": updated_simulation.totalcurrec,
        "simulation_status": updated_simulation.simulation_status,
        "rf_config": rf_config
    }


@influx_data_router.post("/start_simulation")
async def start_simulation(request: Request):
    form_data = await request.json()
    SimulationManager.reset_simulation()
    remote_host = os.getenv('NS3_HOST')
    if not remote_host:
        print("NS3_HOST environment variable is not set.")
        return
    fields = [
        "e2TermIp",
        "hoSinrDifference",
        "indicationPeriodicity",
        "simTime",
        "KPM_E2functionID",
        "RC_E2functionID",
        "N_MmWaveEnbNodes",
        #"N_LteEnbNodes",
        "N_Ues",
        "CenterFrequency",
        "Bandwidth",
        "N_AntennasMcUe",
        "N_AntennasMmWave",
        "IntersideDistanceUEs",
        "IntersideDistanceCells"
    ]
    scenario = form_data.get('scenario')
    if not scenario:
        return
    flags = False
    if form_data.get('flags') == 'true':
        flags = True
    if form_data.get('flexric') == 'true':
        arguments = ' '
    else:
        arguments = '--enableE2FileLogging=1 '
    for field in fields:
        value = form_data.get(field)
        if value is not None:
            arguments += f"--{field}={value} "
        elif value is None and field == 'simTime':
            arguments += f"--simTime=100 "
    if flags:
        command = f'./ns3 run "{scenario} {arguments}"'
    else:
        command = f'./ns3 run "{scenario}"'
    command = f'curl -X POST -d \'{command}\' http://{remote_host}:38866'
    try:
        print(f'Sending start command: {command}')
        result = subprocess.run(command, shell=True, capture_output=True, text=True)
        print("Response from server:")
        print(result.stdout)
        scenario = os.path.split(scenario)[1].split(".")[0]
        SimulationManager.start_simulation(scenario)
    except Exception as e:
        print(f"An error occurred: {e}")
    number_of_ues = int(form_data.get('N_Ues', 2))
    number_of_cells = int(form_data.get('N_LteEnbNodes', 1)) + int(form_data.get('N_MmWaveEnbNodes', 4))
    if not flags:
        number_of_ues = 0
        number_of_cells = 0
    SimulationManager._simulation = Simulation(number_of_ues, number_of_cells)



@influx_data_router.post("/reset_simulation")
async def reset_simulation():
    SimulationManager.reset_simulation()
    return {"message": "Simulation reset"}


@influx_data_router.post("/stop_simulation")
async def stop_simulation():
    remote_host = os.getenv('NS3_HOST')
    scenario = SimulationManager.get_scenario()
    if not scenario:
        return    
    if not remote_host:
        print("NS3_HOST environment variable is not set.")
        return

    command = f"curl -X POST -d '{scenario}' http://{remote_host}:38867"

    try:
        print(f'Sending stop command: {command}')
        result = subprocess.run(command, shell=True, capture_output=True, text=True)
        print("Response from server:")
        print(result.stdout)
        SimulationManager.stop_simulation()
    except Exception as e:
        print(f"An error occurred: {e}")


