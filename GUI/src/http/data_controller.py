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
    print(updated_simulation.ues)
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


