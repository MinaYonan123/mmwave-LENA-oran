import time
import tensorflow as tf
import numpy as np
import socket
from sionna.rt import load_scene, PlanarArray, Transmitter, Receiver, PathSolver
SPEED_OF_LIGHT = 3e8  # in meters per second
#from sionna.constants import SPEED_OF_LIGHT
import os, subprocess, signal
import argparse
import matplotlib.pyplot as plt
import traceback

file_name = "scenarios/parisScene/scene.xml"
#local_machine = True
#verbose = False

def manage_location_message(message, sionna_structure):
    try:
        # Handle map_update message
        data = message[len("LOC_UPDATE:"):]
        parts = data.split(",")
        car = int(parts[0].replace("obj", ""))

        new_x = float(parts[1])
        new_y = float(parts[2])
        new_z = float(parts[3]) + 1
        new_angle = float(parts[4])

        # Save in SUMO_live_location_db
        sionna_structure["SUMO_live_location_db"][car] = {"x": new_x, "y": new_y, "z": new_z, "angle": new_angle}

        # Check if the vehicle exists in Sionna_location_db
        if car in sionna_structure["sionna_location_db"]:
            # Fetch the old position and angle
            old_x = sionna_structure["sionna_location_db"][car]["x"]
            old_y = sionna_structure["sionna_location_db"][car]["y"]
            old_z = sionna_structure["sionna_location_db"][car]["z"]
            old_angle = sionna_structure["sionna_location_db"][car]["angle"]

            # Check if the position or angle has changed by more than the thresholds
            position_changed = (
                    abs(new_x - old_x) >= sionna_structure["position_threshold"]
                    or abs(new_y - old_y) >= sionna_structure["position_threshold"]
                    or abs(new_z - old_z) >= sionna_structure["position_threshold"]
            )
            angle_changed = abs(new_angle - old_angle) >= sionna_structure["angle_threshold"]
        else:
            # No previous record, so this is the first update (considered a change)
            position_changed = True
            angle_changed = True

        # If the position or angle has changed, update the dictionary and the scene
        if position_changed or angle_changed:
            # Update Sionna_location_db with the new values
            sionna_structure["sionna_location_db"][car] = sionna_structure["SUMO_live_location_db"][car]
            # Clear the path_loss cache as one of the car's position has changed (must do for vNLOS cases)
            sionna_structure["path_loss_cache"] = {}
            sionna_structure["rays_cache"] = {}
            # print("Pathloss cache cleared.")
            # Print the updated car's information for logging
            if sionna_structure["verbose"]:
                print(f"car_{car} - Position: [{new_x}, {new_y}, {new_z}] - Angle: {new_angle}")
            # Apply changes to the scene
            if sionna_structure["scene"].get(f"car_{car}"):  # Make sure the object exists in the scene
                from_sionna = sionna_structure["scene"].get(f"car_{car}")
                from_sionna.position = [new_x, new_y, new_z]
                # Orientation is not changed because of a SIONNA bug (kernel crashes)
                # new_orientation = (new_angle*np.pi/180, 0, 0)
                # from_sionna.orientation = type(from_sionna.orientation)(new_orientation, device=from_sionna.orientation.device)

                if sionna_structure["verbose"]:
                    print(f"Updated car_{car} position in the scene.")
            else:
                print(f"ERROR: no car_{car} in the scene, use Blender to check")

            sionna_structure["scene"].remove(f"car_{car}_tx_antenna")
            sionna_structure["scene"].remove(f"car_{car}_rx_antenna")
        return car

    except (IndexError, ValueError) as e:
        print(f"EXCEPTION - Location parsing failed: {e}")
        return None


def match_rays_to_cars(paths, sionna_structure):
    try:
        print("Matching rays between cars...")
        matched_paths = {}
        
        # Get list of cars in scene
        cars = list(sionna_structure["sionna_location_db"].keys())
        
        # Initialize rays cache structure if not exists
        for car1_id in cars:
            car1 = f"car_{car1_id}"
            if car1 not in sionna_structure["rays_cache"]:
                sionna_structure["rays_cache"][car1] = {}
            
            for car2_id in cars:
                car2 = f"car_{car2_id}"
                if car1 != car2 and car2 not in sionna_structure["rays_cache"][car1]:
                    sionna_structure["rays_cache"][car1][car2] = {
                        "path_coefficients": [],
                        "delays": [],
                        "is_los": [False]
                    }

        # Extract and store the complex path coefficients
        if hasattr(paths, '_a_real') and hasattr(paths, '_a_imag'):
            a_real = paths._a_real
            a_imag = paths._a_imag
            tau = paths._tau
            
            # Create complex coefficients
            a_complex = a_real + 1j * a_imag
            
            # Store coefficients for each car pair
            for i, car1_id in enumerate(cars):
                car1 = f"car_{car1_id}"
                matched_paths[car1] = {}
                
                for j, car2_id in enumerate(cars):
                    car2 = f"car_{car2_id}"
                    if car1 != car2:
                        try:
                            # Get coefficients for this car pair
                            coeffs = a_complex[i, :, j, :, :, :]
                            delays = tau[i, j, :]
                            
                            # Convert coefficients to numpy array and ensure they're properly stored
                            coeffs_array = np.array(coeffs)
                            
                            # Check if coefficients are non-zero
                            has_valid_paths = np.any(np.abs(coeffs_array) > 0)
                            
                            # Store in cache with explicit conversion to complex128
                            path_data = {
                                "path_coefficients": [coeffs_array.astype(np.complex128)],
                                "delays": delays,
                                "is_los": [has_valid_paths]
                            }
                            sionna_structure["rays_cache"][car1][car2] = path_data
                            matched_paths[car1][car2] = path_data
                            
                            print(f"Cached paths for {car1} to {car2} (valid paths: {has_valid_paths})")
                            if has_valid_paths:
                                print(f"Max coefficient magnitude: {np.max(np.abs(coeffs_array)):.2e}")
                            
                        except Exception as e:
                            print(f"Error processing coefficients for {car1}-{car2}: {e}")
                            traceback.print_exc()
                            continue
        else:
            print("Warning: No path coefficient data available")
            return {}
    
    except Exception as e:
        print(f"Error in match_rays_to_cars: {str(e)}")
        traceback.print_exc()
        return {}
    
    return matched_paths




def list_scene_objects(sionna_structure):
    print("Objects in the scene:")
    for obj_name in sionna_structure["scene"].objects:
        print(f"- {obj_name}")
    
    print("\nCars in sionna_location_db:")
    for car_id in sionna_structure["sionna_location_db"]:
        print(f"- car_{car_id}")

def compute_rays(sionna_structure):
    try:
        print("Starting compute_rays function...")
        t = time.time()
        
        # Set up arrays
        print("Setting up antenna arrays...")
        sionna_structure["scene"].tx_array = sionna_structure["planar_array"]
        sionna_structure["scene"].rx_array = sionna_structure["planar_array"]
        # Debug: Print final antenna configuration
        print("\nFinal antenna configuration:")
        # print(f"Number of TX antennas: {len([obj for obj in sionna_structure['scene'].objects if 'tx_array' in obj])}")
        # print(f"Number of RX antennas: {len([obj for obj in sionna_structure['scene'].objects if 'rx_array' in obj])}")

        # Debug: Print scene configuration
        print("Scene configuration:")
        print(f"Frequency: {sionna_structure['scene'].frequency} Hz")
        print(f"Max depth: {sionna_structure['max_depth']}")
        

        # for car_id in sionna_structure["sionna_location_db"]:
        #     print(f"Car ID: {car_id}")
        #     print(f"Location: {sionna_structure['sionna_location_db'][car_id]}")
        #     print(f"TX Antenna Name: car_{car_id}_tx_antenna")
        #     print(f"RX Antenna Name: car_{car_id}_rx_antenna")
        #     print(f"Car Position: {np.array([sionna_structure['sionna_location_db'][car_id]['x'], sionna_structure['sionna_location_db'][car_id]['y'], sionna_structure['sionna_location_db'][car_id]['z']])}")
           
            # Ensure every car in the simulation has antennas
        print("Setting up car antennas...")
        for car_id in sionna_structure["sionna_location_db"]:
            tx_antenna_name = f"car_{car_id}_tx_antenna"
            rx_antenna_name = f"car_{car_id}_rx_antenna"
            car_position = np.array(
                [sionna_structure["sionna_location_db"][car_id]['x'], 
                 sionna_structure["sionna_location_db"][car_id]['y'],
                 sionna_structure["sionna_location_db"][car_id]['z']])
            tx_position = car_position + np.array(sionna_structure["antenna_displacement"])
            rx_position = car_position + np.array(sionna_structure["antenna_displacement"])
            print(f"TX Position: {car_position + np.array(sionna_structure['antenna_displacement'])}")
            print(f"RX Position: {rx_position + np.array(sionna_structure['antenna_displacement'])}")

            if sionna_structure["scene"].get(tx_antenna_name) is None:
                print(f"Adding TX antenna for car_{car_id} at position {tx_position}")
                sionna_structure["scene"].add(Transmitter(tx_antenna_name, position=tx_position, orientation=[0, 0, 0]))
                sionna_structure["scene"]._tx_array = sionna_structure["scene"].tx_array

            if sionna_structure["scene"].get(rx_antenna_name) is None:
                print(f"Adding RX antenna for car_{car_id} at position {rx_position}")
                sionna_structure["scene"].add(Receiver(rx_antenna_name, position=rx_position, orientation=[0, 0, 0]))
                sionna_structure["scene"]._rx_array = sionna_structure["scene"].rx_array
            print(f"Number of TX antennas: {len([obj for obj in sionna_structure['scene'].objects if 'tx_array' in obj])}")    
        # Debug: Print final antenna configuration
        print("\nFinal antenna configuration:")
       


        # Initialize PathSolver with debug information
        print("\nInitializing PathSolver...")
        p_solver = PathSolver()
        
        print("Calling PathSolver with parameters:")
        print(f"- specular_reflection: True")
        print(f"- refraction: True")
        print(f"- diffuse_reflection: False")
        print(f"- max_depth: {sionna_structure['max_depth']}")
        print(f"- los: Fasle")
        
        paths = p_solver(scene=sionna_structure["scene"],
                        specular_reflection=True,
                        synthetic_array= True, 
                        refraction=True,
                        diffuse_reflection=False,
                        max_depth=sionna_structure["max_depth"],
                        los=True, 
                        seed=41)
        
        print("\nPathSolver execution completed")
        print("Paths object type:", type(paths))
        print("Paths object attributes:", dir(paths))

        # Compute channel impulse response
        print("\nComputing channel impulse response...")
        a, tau = paths.cir(normalize_delays=True, out_type="numpy")
        print("CIR computation completed")
        print(f"a shape: {a.shape}")
        print(f"tau shape: {tau.shape}")
        
        # Store raw paths data for matching
        paths._a_real = np.real(a)
        paths._a_imag = np.imag(a)
        paths._tau = tau
        
        print(f"\nRay tracing took: {(time.time() - t) * 1000:.2f} ms")
        
        t = time.time()
        print("\nMatching rays to cars...")
        matched_paths = match_rays_to_cars(paths, sionna_structure)
        print(f"Matching rays to cars took: {(time.time() - t) * 1000:.2f} ms")

        # Process the matched paths and update the cache
        print("\nProcessing matched paths and updating cache...")
        for src_car_id in sionna_structure["sionna_location_db"]:
            current_source_car_name = f"car_{src_car_id}"
            if current_source_car_name in matched_paths:
                matched_paths_for_source = matched_paths[current_source_car_name]

                for trg_car_id in sionna_structure["sionna_location_db"]:
                    current_target_car_name = f"car_{trg_car_id}"
                    if current_target_car_name != current_source_car_name:
                        if current_target_car_name in matched_paths_for_source:
                            if current_source_car_name not in sionna_structure["rays_cache"]:
                                sionna_structure["rays_cache"][current_source_car_name] = {}
                            sionna_structure["rays_cache"][current_source_car_name][current_target_car_name] = \
                                matched_paths_for_source[current_target_car_name]
                            print(f"Cached paths for {current_source_car_name} to {current_target_car_name}")

        print("compute_rays completed successfully")
        return None

    except Exception as e:
        print(f"ERROR in compute_rays: {str(e)}")
        import traceback
        traceback.print_exc()
        return None


def get_path_loss(sionna_structure, car1_id, car2_id):
    try:
        # car1_id and car2_id already include the "car_" prefix
        if car1_id not in sionna_structure["rays_cache"] or car2_id not in sionna_structure["rays_cache"][car1_id]:
            print(f"Computing rays for {car1_id}-{car2_id}")
            compute_rays(sionna_structure)

        if car1_id not in sionna_structure["rays_cache"] or car2_id not in sionna_structure["rays_cache"][car1_id]:
            print(f"No cached paths between {car1_id}-{car2_id}")
            return 300

        path_data = sionna_structure["rays_cache"][car1_id][car2_id]
        path_coefficients = path_data["path_coefficients"]

        if not path_coefficients or len(path_coefficients) == 0:
            print(f"No path coefficients found for {car1_id}-{car2_id}")
            return 300

        # Calculate total power from complex coefficients
        total_power = 0
        for coeffs in path_coefficients:
            coeffs = np.array(coeffs, dtype=np.complex128)  # Ensure complex data type
            if len(coeffs.shape) > 1:  # Handle multi-dimensional arrays
                coeffs = coeffs.reshape(-1)  # Flatten array
            
            # Calculate power from complex coefficients
            power = np.sum(np.abs(coeffs) ** 2)
            total_power += power
            
            # Debug output
            print(f"Processing coefficients batch:")
            print(f"- Shape: {coeffs.shape}")
            print(f"- Max magnitude: {np.max(np.abs(coeffs)):.2e}")
            print(f"- Power contribution: {power:.2e}")

        if total_power > 0:
            path_loss = -10 * np.log10(total_power)
            print(f"Path loss for {car1_id}-{car2_id}: {path_loss:.2f} dB (total power: {total_power:.2e})")
            return path_loss
        else:
            print(f"Zero total power calculated for {car1_id}-{car2_id}")
            return 300

    except Exception as e:
        print(f"ERROR in get_path_loss for {car1_id}-{car2_id}: {str(e)}")
        traceback.print_exc()
        return 300


def manage_path_loss_request(message, sionna_structure):
    try:
        data = message[len("CALC_REQUEST_PATHGAIN:"):]
        parts = data.split(",")
        car_a_str = parts[0].replace("obj", "")
        car_b_str = parts[1].replace("obj", "")

        # Getting each car_id, the origin is marked as 0
        car_a_id = "origin" if car_a_str == "0" else f"car_{int(car_a_str)}" if car_a_str else "origin"
        car_b_id = "origin" if car_b_str == "0" else f"car_{int(car_b_str)}" if car_b_str else "origin"
        print ("car_a_id ",car_a_id)
        print ("car_b_id ",car_b_id)
        
        if car_a_id == "origin" or car_b_id == "origin":
            # If any, ignoring path_loss requests from the origin, used for statistical calibration
            path_loss_value = 0
        else:
            t = time.time()
            path_loss_value = get_path_loss(sionna_structure, car_a_id, car_b_id)

        return path_loss_value

    except (ValueError, IndexError) as e:
        print(f"EXCEPTION - Error processing path_loss request: {e}")
        return None


def get_delay(car1_id, car2_id, sionna_structure):
    # Check and compute rays only if necessary
    if car1_id not in sionna_structure["rays_cache"] or car2_id not in sionna_structure["rays_cache"][car1_id]:
        compute_rays(sionna_structure)

    delays = np.abs(sionna_structure["rays_cache"][car1_id][car2_id]["delays"])
    delays_flat = delays.flatten()

    # Filter positive values
    positive_values = delays_flat[delays_flat >= 0]

    if positive_values.size > 0:
        min_positive_value = np.min(positive_values)
    else:
        min_positive_value = 1e5

    return min_positive_value


def manage_delay_request(message, sionna_structure):
    try:
        data = message[len("CALC_REQUEST_DELAY:"):]
        parts = data.split(",")
        car_a_str = parts[0].replace("obj", "")
        car_b_str = parts[1].replace("obj", "")

        # Getting each car_id, the origin is marked as 0
        car_a_id = "origin" if car_a_str == "0" else f"car_{int(car_a_str)}" if car_a_str else "origin"
        car_b_id = "origin" if car_b_str == "0" else f"car_{int(car_b_str)}" if car_b_str else "origin"

        if car_a_id == "origin" or car_b_id == "origin":
            # If any, ignoring path_loss requests from the origin, used for statistical calibration
            delay = 0
        else:
            delay = get_delay(car_a_id, car_b_id, sionna_structure)

        return delay

    except (ValueError, IndexError) as e:
        print(f"EXCEPTION - Error processing delay request: {e}")
        return None


def manage_los_request(message, sionna_structure):
    try:
        data = message[len("CALC_REQUEST_LOS:"):]
        parts = data.split(",")
        car_a_str = parts[0].replace("obj", "")
        car_b_str = parts[1].replace("obj", "")

        # Getting each car_id, the origin is marked as 0
        car_a_id = "origin" if car_a_str == "0" else f"car_{int(car_a_str)}" if car_a_str else "origin"
        car_b_id = "origin" if car_b_str == "0" else f"car_{int(car_b_str)}" if car_b_str else "origin"

        if car_a_id == "origin" or car_b_id == "origin":
            # If any, ignoring requests from the origin, used for statistical calibration
            los = [False]
        else:
            # Check if rays need to be computed
            if car_a_id not in sionna_structure["rays_cache"] or car_b_id not in sionna_structure["rays_cache"][car_a_id]:
                print(f"Computing rays for {car_a_id} and {car_b_id}")
                compute_rays(sionna_structure)

            # Check if we have the cache entry after computing rays
            if car_a_id in sionna_structure["rays_cache"] and car_b_id in sionna_structure["rays_cache"][car_a_id]:
                # Initialize "is_los" if it doesn't exist
                if "is_los" not in sionna_structure["rays_cache"][car_a_id][car_b_id]:
                    sionna_structure["rays_cache"][car_a_id][car_b_id]["is_los"] = [False]
                los = sionna_structure["rays_cache"][car_a_id][car_b_id]["is_los"]
            else:
                print(f"No ray data available for {car_a_id} and {car_b_id}")
                los = [False]

        return los

    except Exception as e:
        print(f"EXCEPTION - Error processing LOS request: {e}")
        traceback.print_exc()
        return [False]


# Function to kill processes using a specific port
def kill_process_using_port(port, verbose=False):
    try:
        result = subprocess.run(['lsof', '-i', f':{port}'], stdout=subprocess.PIPE)
        for line in result.stdout.decode('utf-8').split('\n'):
            if 'LISTEN' in line:
                pid = int(line.split()[1])
                os.kill(pid, signal.SIGKILL)
                if verbose:
                    print(f"Killed process {pid} using port {port}")
    except Exception as e:
        print(f"Error killing process using port {port}: {e}")


# Configure GPU settings
def configure_gpu(verbose=False):
    if os.getenv("CUDA_VISIBLE_DEVICES") is None:
        gpu_num = 2  # Default GPU setting
        os.environ["CUDA_VISIBLE_DEVICES"] = f"{gpu_num}"
    os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'

    gpus = tf.config.list_physical_devices('GPU')
    if gpus:
        try:
            for gpu in gpus:
                tf.config.experimental.set_memory_growth(gpu, True)
        except RuntimeError as e:
            print(e)

    tf.get_logger().setLevel('ERROR')
    if verbose:
        print("Configured TensorFlow and GPU settings.")

# Main function to manage initialization and variables
def main():
    # Argument parser setup
    parser = argparse.ArgumentParser(description='ns3-rt - Sionna Server Script: use the following options to configure the server.')
    parser.add_argument('--path-to-xml-scenario', type=str, default='scenarios/SionnaExampleScenario/scene.xml',
                        help='Path to the .xml file of the scenario (see Sionna documentation for the creation of custom scenarios)')
    parser.add_argument('--local-machine', action='store_true',
                        help='Flag to indicate if Sionna and ns3-rt are running on the same machine (locally)')
    parser.add_argument('--verbose', action='store_true', help='Flag for verbose output')
    parser.add_argument('--frequency', type=float, help='Frequency of the simulation in Hz', default=5.89e9)
    args = parser.parse_args()
    file_name = args.path_to_xml_scenario
    print (file_name)
    local_machine = args.local_machine
    verbose = args.verbose
    frequency = args.frequency
    # Kill any process using the port
    kill_process_using_port(8103, verbose)

    # Configure GPU
    configure_gpu(verbose)

    sionna_structure = dict()

    sionna_structure["verbose"] = verbose

    # Load scene and configure radio settings
    sionna_structure["scene"] = load_scene(file_name)
     # Add these lines here to print object materials
    print("Objects and their radio materials:")
    for i, obj in enumerate(sionna_structure["scene"].objects.values()):
        print(f"{obj.name} : {obj.radio_material.name}")
        if i >= 10:
            break
   
    sionna_structure["scene"].frequency = frequency  # Frequency in Hz
    #sionna_structure["scene"].synthetic_array = True  # Enable synthetic array processing
    #element_spacing = SPEED_OF_LIGHT / sionna_structure["scene"].frequency / 2
    element_spacing = 0.5 
    # Create a larger antenna array for better path detection
    sionna_structure["planar_array"] = PlanarArray(
        num_rows=4,
        num_cols=4,
        vertical_spacing=element_spacing,
        horizontal_spacing=element_spacing,
        pattern="iso",
        polarization="V"
    )
    # Place antennas slightly above the car to improve reception
    sionna_structure["antenna_displacement"] = [0, 0, 1.5]
    sionna_structure["position_threshold"] = 3  # Position update threshold in meters
    sionna_structure["angle_threshold"] = 90  # Angle update threshold in degrees
    sionna_structure["max_depth"] = 5  # Maximum ray tracing depth
    sionna_structure["num_samples"] = 1e5 # Number of samples for ray tracing

    sionna_structure["path_loss_cache"] = {}
    sionna_structure["delay_cache"] = {}
    sionna_structure["last_path_loss_requested"] = None

    # Set up UDP socket
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    if local_machine:
        udp_socket.bind(("127.0.0.1", 8103))  # Local machine configuration
    else:
        udp_socket.bind(("0.0.0.0", 8103))  # External server configuration

    # Databases for vehicle locations
    sionna_structure["SUMO_live_location_db"] = {}  # Real-time vehicle locations in SUMO
    sionna_structure["sionna_location_db"] = {}  # Vehicle locations in Sionna

    sionna_structure["rays_cache"] = {}  # Cache for ray information
    sionna_structure["path_loss_cache"] = {}  # Cache for path loss values

    # Simulation main loop or function calls could go here
    # Example:
    # process_location_updates(scene, SUMO_live_location_db, Sionna_location_db, ...)
    # manage_requests(udp_socket, rays_cache, ...)

    print(f"Simulation setup complete. Ready to process requests. Ray Tracing is working at {frequency / 1e9} GHz.")

    while True:
        # Receive data from the socket
        payload, address = udp_socket.recvfrom(1024)
        message = payload.decode()
        print (f"Received message: {message} from {address}")
        #list_scene_objects(sionna_structure)
        if message.startswith("LOC_UPDATE:"):
            updated_car = manage_location_message(message, sionna_structure)
            if updated_car is not None:
                response = "LOC_CONFIRM:" + "obj" + str(updated_car)
                udp_socket.sendto(response.encode(), address)

        if message.startswith("CALC_REQUEST_PATHGAIN:"):
            pathloss = manage_path_loss_request(message, sionna_structure)
            if pathloss is not None:
                response = "CALC_DONE_PATHGAIN:" + str(pathloss)
                udp_socket.sendto(response.encode(), address)

        if message.startswith("CALC_REQUEST_DELAY:"):
            delay = manage_delay_request(message, sionna_structure)
            if delay is not None:
                response = "CALC_DONE_DELAY:" + str(delay)
                udp_socket.sendto(response.encode(), address)

        if message.startswith("CALC_REQUEST_LOS:"):
            los = manage_los_request(message, sionna_structure)
            if los is not None:
                response = "CALC_DONE_LOS:" + str(los)
                udp_socket.sendto(response.encode(), address)

        if message.startswith("SHUTDOWN_SIONNA"):
            print("Got SHUTDOWN_SIONNA message. Bye!")
            udp_socket.close()
            break
        print (f"send message: {response} to {address}")


# Entry point
if __name__ == "__main__":
    main()
