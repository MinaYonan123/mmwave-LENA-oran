from typing import Optional

from src.simulation_objects.simulation import Simulation


class SimulationManager:
    _simulation: Optional[Simulation] = None
    scenario: str = ''

    @classmethod
    def get_simulation(cls) -> Simulation:
        if cls._simulation is None:
            cls._simulation = Simulation(number_of_ues=0, number_of_cells=0)
        return cls._simulation

    @classmethod
    def refresh_simulation(cls) -> Simulation:
        if cls._simulation.number_of_ues == 0 and cls._simulation.number_of_cells == 0:
            return cls._simulation
        simulation = cls.get_simulation()
        new_ues, new_cells, sim_id = simulation.get_simulation_data(
            number_of_ues=simulation.number_of_ues,
            number_of_cells=simulation.number_of_cells
        )
        if not simulation.sim_id:
            simulation.sim_id = sim_id
        new_max_x, new_max_y = simulation.get_charts_max_axis_value()
        simulation.ues = new_ues
        simulation.cells = new_cells
        simulation.max_x = new_max_x
        simulation.max_y = new_max_y
        simulation.ue_history.append(new_ues)
        simulation.cell_history.append(new_cells)
        if len(simulation.ue_history) > 50:
            simulation.ue_history.pop(0)
        if len(simulation.cell_history) > 50:
            simulation.cell_history.pop(0)
        power_usage = 0
        if simulation.starting_power == 0 or simulation.starting_power is None:
            starting_power = 0
            for cell in simulation.cells:
                if cell.es_state == 1 or cell.es_power is None:
                    starting_power = 0
                    break
                else:
                    if cell.type == 'lte':
                        cell_power = simulation.starting_power = simulation.get_first_value_from_measurement(
                            f'enbs_espower_{cell.cell_id}')
                    elif cell.type == 'mmwave':
                        cell_power = simulation.starting_power = simulation.get_first_value_from_measurement(
                            f'gnbs_espower_{cell.cell_id}')
                    starting_power += cell_power
            simulation.starting_power = starting_power
        maxec = 0
        totalcurrec = 0
        for cell in simulation.cells:
            if cell.es_power:
                power_usage += cell.es_power
            if cell.maxec:
                maxec = max(maxec, cell.maxec)
            if cell.totalcurrec:
                totalcurrec = max(totalcurrec, cell.totalcurrec)
        simulation.current_power = power_usage
        simulation.maxec = maxec
        simulation.totalcurrec = totalcurrec

        return simulation

    @classmethod
    def reset_simulation(cls):
        if cls._simulation is not None:
            cls._simulation.ues = []
            cls._simulation.cells = []
            cls._simulation.simulation_start_time = None
            cls._simulation.number_of_ues = 0
            cls._simulation.number_of_cells = 0
            cls._simulation.max_x = 6000
            cls._simulation.max_y = 6000
            cls._simulation.sim_id = None
            cls._simulation.cell_history = []
            cls._simulation.ue_history = []
            cls._simulation.starting_power = 0
            cls._simulation.current_power = 0
            cls._simulation.simulation_status = 'off'

    @classmethod
    def delete_simulation(cls):
        cls._simulation = None

    @classmethod
    def start_simulation(cls, scenario):
        cls._simulation.simulation_status = 'on'
        cls.scenario = scenario
        if not scenario:
            raise Exception("Empty scenario")


    @classmethod
    def stop_simulation(cls):
        cls._simulation.simulation_status = 'off'
        cls.scenario = ''

    @classmethod
    def get_scenario(cls):
        return cls.scenario