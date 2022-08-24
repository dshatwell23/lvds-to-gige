# Data Transmission Through Gigabit Ethernet from a LVDS Interface Using a System On Chip (Single Board Computer + FPGA)

![teaser figure](images/teaser.png)
**Anthropometric measurement estimation algorithm:** Block diagram of the LVDS to Gigabit Ethernet data transmission system. The system is implemented on a system-on-chip (SoC), which has a FPGA and a ARM Cortex-A9 microprocessor. Custom hardware created in the FPGA receives data from the LVDS signals and stores it in memory. Then, the processor access the memory and sends the data through Gigabit Ethernet.</p> 

This repository contains the files used to implement the LVDS to Gigabit Ethernet data transmission system, developed in the summer internship program of the Jicamarca Radio Observatory. 

## Description

The objective of this project was to design and implement a system capable of transmitting data at high speeds from the JARS 2.0 radar to a remote computer through Gigabit Ethernet using a system on chip (SoC). The system has two main stages: (i) data acquisition from the LVDS interface and (ii) data transmission to the computer through a communication protocol. In order to acquire data from the LVDS interface, the FPGA was used to implement a system capable of multiplexing and copying the data to a memory shared with the processor. Then, a program running on the processor was used to read the data from memory and send it to the PC with the UDP protocol.

### Hardware

The data transmission system is implemented in DE0-Nano board, which contains a Cyclone V system-on-chip (SoC), consisting of a single-board computer and an FPGA. The microprocessor of the single-board computer is a dual core ARM Cortex-A9. By using the FPGA, users of the DE0-Nano board can develop custom hardware and interact with the GPIO ports.

