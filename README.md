 LoRa-WAN Mesh Network for Remote Environmental Monitoring and Biodiversity Conservation

A multi-hop wireless mesh network architecture leveraging 6LoWPAN over LoRa to extend coverage and ensure reliable data telemetry in dense, remote ecosystems. This project evaluates network stack alignment, routing protocol overhead, and multi-hop performance characteristics using the ns-3 simulator.


 Overview

Standard LoRaWAN operates on a star-of-stars topology, which faces coverage limitations in deep forest canopies or topographically challenging terrain. This project addresses the limitation by implementing an offline-resilient, multi-hop mesh routing framework. 

By embedding IPv6 adaptation via 6LoWPAN, the network enables energy-efficient end-to-end IP routing directly to localized sensors, facilitating long-range environmental telemetry without relying on pervasive cellular infrastructure.

 Key Features
Multi-Hop Relaying: Extends standard LoRa range boundaries across multiple node hops.
6LoWPAN Integration: Allows header compression and IP-packet transmission over resource-constrained LoRa links.
Localized Rule Optimization: Configured for propagation characteristics typical of regional environmental deployments (eg., West African terrain models).
Performance Simulation: Built and validated inside an `ns-3` network simulation environment running over WSL/Linux.



Repository Structure

scratch — Contains the core simulation scripts, multi-hop routing logic, and ns-3 simulation execution binaries.
. gitignore — Configured to filter out heavy ns-3 build artifacts, object files, and local logs.


 Prerequisites & Installation

To compile and run this simulation locally, you need an operational Linux environment (or Windows Subsystem for Linux - WSL) with ns-3 installed.

1. Clone the repository into your ns-3 scratch workspace:
   bash
   git clone [https://github.com/NA-456/LoRa-WAN-Mesh-Network-for-Remote-Environmental-Monitoring-and-Biodiversity-Conservation.git](https://github.com/NA-456/LoRa-WAN-Mesh-Network-for-Remote-Environmental-Monitoring-and-Biodiversity-Conservation.git)
