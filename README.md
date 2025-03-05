# mmWave ns-3 module #

This is an [ns-3](https://www.nsnam.org "ns-3 Website") module for the simulation
of 5G cellular networks operating at mmWaves. A description of this module can be found in [this paper](https://ieeexplore.ieee.org/document/8344116/ "mmwave paper").

Main features:

* Support of a wide range of channel models, including the model based on 3GPP TR 38.901 for frequencies between 0.5 and 100 GHz. Ray tracing and measured traces can also be used.

* Custom PHY and MAC classes supporting the 3GPP NR frame structure and numerologies.

* Custom schedulers for supporting dynamic TDD formats

* Carrier Aggregation at the MAC layer

* Enhancements to the RLC layer with re-segmentation of packets for retransmissions

* Dual Connectivity with LTE base stations, with fast secondary cell handover and channel tracking

* Simulation of core network elements (with also the MME as a real node)

## Installation
This repository contains a complete ns-3 installation with the addition of the mmwave module. 

Use these commands to download and build `ns3-mmwave`:
```
git clone https://github.com/nyuwireless-unipd/ns3-mmwave.git
cd ns3-mmwave
./ns3 configure --disable-python --enable-examples && ./ns3 build
```

## Usage example
You can use the following command to run the `mmwave-simple-epc` example. 
```
./ns3 run mmwave-simple-epc
```
Other examples are included in `src/mmwave/examples/`

## Documentation
The documentation of this module is available at [this link](./src/mmwave/doc/mmwave-doc.md).

## Related modules
- MilliCar is an ns-3 module for the simulation of mmWave NR V2X networks. Check [this repo](https://github.com/signetlabdei/millicar) for further details.
- A seperate module is being developed for [mmWave UE Energy Consumption](https://github.com/arghasen10/mmwave-energy "mmwave-energy"). You can use this module for analyzing 
Energy Consumption behaviour of mmwave UE. Check this repository for further details.
- `ns3-mmwave-iab` is an extended version of `ns3-mmWave` adding wireless relaying capabilities to an ns-3 NetDevice, and the possibility of simulating in-band relaying at mmWave frequencies. Check [this repo](https://github.com/signetlabdei/ns3-mmwave-iab) for further details.

## References 
The following papers describe in detail the features implemented in the mmWave
module:
- [End-to-End Simulation of 5G mmWave Networks](https://ieeexplore.ieee.org/document/8344116/ "comst paper") is a comprehensive tutorial with a detailed description of the whole module. We advise the researchers interested in this module to start reading from this paper;
- [Integration of Carrier Aggregation and Dual Connectivity for the ns-3 mmWave Module](https://arxiv.org/abs/1802.06706 "wns3 2018") describes the Carrier Aggregation implementation;
- [Implementation of A Spatial Channel Model for ns-3](https://arxiv.org/abs/2002.09341 "wns3 2020") describes the integration of the spatial channel model based on the 3GPP specifications TR 38.901 V15.0.0;
- [Performance Comparison of Dual Connectivity and Hard Handover for LTE-5G Tight Integration](https://arxiv.org/abs/1607.05425 "simutools paper") describes the Dual Connectivity feature.

These other papers describe features that were implemented in older releases: 
- [ns-3 Implementation of the 3GPP MIMO Channel Model for Frequency Spectrum above 6 GHz](https://dl.acm.org/citation.cfm?id=3067678 "wns3 2017") describes the implementation of the 3GPP channel model based on TR 38.900;
- [Multi-Sector and Multi-Panel Performance in 5G mmWave Cellular Networks](https://arxiv.org/abs/1808.04905 "globecom2018") describes the multi-sector addition to the 3GPP channel model;

If you use this module in your research, please cite:

M. Mezzavilla, M. Zhang, M. Polese, R. Ford, S. Dutta, S. Rangan, M. Zorzi, _"End-to-End Simulation of 5G mmWave Networks,"_ in IEEE Communications Surveys & Tutorials, vol. 20, no. 3, pp. 2237-2263, thirdquarter 2018. [bibtex available here](https://ieeexplore.ieee.org/document/8344116/)

## Future work
We are actively developing new features for the mmWave module, including:
- 3GPP NR beam tracking
- 3GPP NR Integrated Access and Backhaul feature (see [this repo](https://github.com/signetlabdei/ns3-mmwave-iab) for more details)

## About
This module is being developed by [NYU Wireless](http://wireless.engineering.nyu.edu/) and the [University of Padova](http://mmwave.dei.unipd.it/).
This  work  was  supported  in  part by  the  U.S.  Department  of  Commerce  National  Institute  of  Standards  and Technology through the Project “An End-to-End Research Platform for Public Safety  Communications  above  6  GHz”  under  Award  70NANB17H16.



<!-- The new-handover branch offers integration between LTE and mmWave and dual connectivity features.
 -->

## Authors ##

The ns-3 mmWave module is the result of the development effort carried out by different people. The main contributors are: 
- Tommaso Zugno, University of Padova
- Michele Polese, University of Padova
- Matteo Pagin, University of Padova
- Mattia Lecci, University of Padova
- Matteo Drago, University of Padova
- Mattia Rebato, University of Padova
- Menglei Zhang, NYU Wireless
- Marco Giordani, University of Padova
- Marco Mezzavilla, NYU Wireless
- Sourjya Dutta, NYU Wireless
- Russell Ford, NYU Wireless
- Gabriel Arrobo, Intel

## License ##

This software is licensed under the terms of the GNU GPLv2, as like as ns-3. See the LICENSE file for more details.
=======
# RT-enabled ns-3 with NVIDIA Sionna

This is an [ns-3](https://www.nsnam.orghttps:/) version implementing ray tracing for wireless channel simulation using [NVIDIA Sionna RT](https://github.com/NVlabs/sionna).

The integration of Sionna enables **highly accurate propagation modeling** by leveraging GPU-accelerated ray tracing, making it ideal for simulations in complex environments at any frequency, including urban and vehicular scenarios.

For details about this integration running in a possible application example, refer to the following paper:

- [Toward Digital Network Twins: Integrating Sionna RT in ns-3 for 6G Multi-RAT Networks Simulations](https://arxiv.org/abs/2501.00372)

## Key Features

- **Deterministic Wireless Channel Simulations** using the GPU-accelerated RT module in Sionna.
- **Fully Customizable Scenario in Sionna** with the possibility of chosing object meshes, materials (more details about scenes [available here](https://nvlabs.github.io/sionna/api/rt.html)) and antennas.
- **Seamless Mobility Syncronization** between ns-3 Nodes and their correspondent mesh in Sionna.
- **Possibility to run Sionna and ns-3 on two different machines** for example with Sionna in a GPU-powered server farm to reduce computation times

## Installation

ns-3 and Sionna are two separated entities able to communicate with each other via an UDP network socket. For this reason, the installation consists in two steps:

### 1. Installing ns3-rt

A complete ns-3 installation is contained in this repository, as well as the `sionna` module for the integration with Sionna.

Use the following commands to download and build ns3-rt:

```bash
git clone https://github.com/robpegurri/ns3-rt.git
cd ns3-rt
./ns3 configure --disable-python --enable-examples 
./ns3 build
```

### 2. Installing Sionna

To install Sionna, ensure you have Python (versions 3.8 to 3.11) and TensorFlow (versions 2.13 to 2.15) installed. Detailed instructions are available in the official Sionna installation guide. Ubuntu 22.04 is recommended.

**If you are running Sionna on a CPU**, install TensorFlow and LLVM (also required in this case) with:

```bash
sudo apt install llvm
python3 -m pip install tensorflow 
```

and verify the installation with:

```bash
python3 -c "import tensorflow as tf; print(tf.reduce_sum(tf.random.normal([1000, 1000])))"
```

**If you are running Sionna on a GPU**, install the required drivers (refer to your GPU vendor). After the drivers are properly setup, TensorFlow GPU can be installed with:

```bash
python3 -m pip install 'tensorflow[and-cuda]' 
```

and verify the installation with:

```bash
python3 -c "import tensorflow as tf; print(tf.config.list_physical_devices('GPU'))"
```

In case of any issues with TensorFlow or TensorFlow GPU, please refer to the official installation guide [at this page](https://www.tensorflow.org/install).

**At this point, install Sionna** with:

```bash
python3 -m pip install sionna
```

and run the following code in `python3` to check if Sionna was installed properly:

```bash
 >>> import sionna
 >>> print(sionna.__version__)
```


=======
## Running the example

To run the `simple-sionna-example` example, you first need to start Sionna (this example expects Sionna to run locally, see the next section to know how to run Sionna remotely).

Run the Python example script `sionna_server_script.py` from the `/src/sionna` folder with the following command and options:

```bash
python3 'sionna_server_script.py' --local-machine --frequency=2.1e9 --path-to-xml-scenario=scenarios/SionnaExampleScenario/scene.xml
```

After Sionna has started, run the ns-3 simulation in parallel with:

```bash
./ns3 run simple-sionna-example
```

## Simulating with Sionna and ns-3 on separated machines

ns3-sionna was created with the possibility to run Sionna both locally (on the same machine with ns-3) and remotely (in a server with). In your ns-3 script, you can enable this possibility with `SionnaHelper` this way:

```cpp
#include "ns3/sionna-helper.h"
...
SionnaHelper& sionnaHelper = SionnaHelper::GetInstance();
sionnaHelper.SetLocalMachine(false);
sionnaHelper.SetServerIp("YOUR-IP-ADDRESS-HERE");
```

While on Sionna side, just remove the `--local-machine` flag when running the Python script. The default port used by ns3-sionna is **UDP/8103**.

## Notes on using a custom Sionna scene with ns3-rt

ns3-rt links every Node to a specific object in Sionna (associated with a fully customizable mesh). If this mesh is not found in the scene, then Sionna would not know how to calculate any of the requested values by ns3-rt.

In the given example, upon the reception of a *LOC_UPDATE* message from ns3-rt, `sionna_server_script.py` looks for the correspondent object mesh named **car_n**, where **n** is calculated as the ns-3 **Node ID + 1**. The TX and RX antennas are placed on top of the objects (cars, in this case).

To fully understand how to create a custom scene for Sionna, please refer to the [official video tutorial by NVIDIA](https://www.youtube.com/watch?v=7xHLDxUaQ7chttps:/).

## Acknowledgements
If you want to acknowledge our work, please refer to the following pre-print:
```
@misc{pegurri2025digitalnetworktwinsintegrating,
      title={Toward Digital Network Twins: Integrating Sionna RT in ns-3 for 6G Multi-RAT Networks Simulations}, 
      author={Roberto Pegurri and Francesco Linsalata and Eugenio Moro and Jakob Hoydis and Umberto Spagnolini},
      year={2025},
      eprint={2501.00372},
      archivePrefix={arXiv},
      primaryClass={cs.NI},
      url={https://arxiv.org/abs/2501.00372}, 
}
```
