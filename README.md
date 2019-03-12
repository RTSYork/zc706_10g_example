Zynq Ten-Gigabit Example
========================

This repository contains an example project for two ZC706 boards communicating over a 10-Gigabit network, using optical transceivers in the SFP+ cages on the boards.

The project structure is as follows:

Master
------
The master hardware is a reasonably simply DMA-based design to transmit packets over the 10G network. This DMA is connected to the high-performance 64-bit memory interface on the processing system to access the PS memory. Of course, if required, a separate memory controller can be instantiated in the PL to drive the other DDR3 module present on the board in order to prevent memory contention between the DMA core and tasks running on the PS.

The DMA controller's AXI Stream ports are then connected to the 10G subsystem through two AXI-Stream data FIFOs. These FIFOs are configured in packet mode, hence they receive an entire packet and await TLAST before transmitting the packet on to the 10G subsystem. These buffers are required to prevent starvation or overflow; the transmit side of the 10G subsystem requires that valid data is transferred on every cycle for which TREADY is asserted after the first word of a packet is transferred, and similarly the receive side expects that data is consumed on every cycle that TVALID is asserted (and actually ignores the TREADY single from a slave). It is forseeable that the DMA core may not be able to provide packet data every cycle after beginning a transaction, for example, when changing memory rows (and thus having to re-issue commands to re-open the next memory row) and hence these fast block-RAM based buffers are required. Similarly, it is forseeable that the receive side may not be able to receive a data word every cycle.

The software project then sets up the Si5324 clock oscillator on the board which is connected to the GTX banks for the SFP+ cage to provide a 156.25MHz clock, required for the operation of the 10G Ethernet Subsystem. It then creates four packets, and sets up the DMA core to transmit these cyclically as fast as possible.

Slave
-----
The slave is a basic packet processor written in Vivado HLS. The processor simply receives the destination/source MAC and the ethertype, constructs a reply packet from them, and then adds 0xAA to the first two bytes of the payload, and echoes the remainder of the packet back verbatim. Because the packet processor constructs a reply packet by simply flipping the dest/source MAC address fields, it must have received the whole MAC header before a reply can be created. To do so, it stores the destination MAC address in one cycle, along with part of the source MAC. When it receives the remainder of the source MAC address, it constructs the reply packet, sends the first word and saves the second word in a buffer to be sent in the next cycle. In subsequent cycles, the buffer is transmitted and replaced with the next word to be sent. Because of this buffering, the reply is effectively delayed by a cycle.

The hardware design is then reasonably simple again - the accelerator is connected to two AXI-Stream FIFOs again. While the packet processor can emit a data word every cycle, it is possible that the receiving end of the 10G ethernet core may not provide valid packet data every cycle (and hence TVALID may go low for a cycle in the middle of a packet). If this happens, it will prevent the packet processor from emitting a data word in that cycle and thus may cause the transmit side of the 10G core to starve. These data FIFOs again prevent this situation from occuring.

Side note: It is probably OK to remove one of these FIFOs if required; because the FIFO on the receive side will only start relaying data when it has a full packet, and can provide valid data on every cycle, it will not cause the situation where the packet processor is prevented from relaying a data word on one cycle because it is starved, as described above. Similarly, if a FIFO is used on the transmit side but not the receive side, it will wait for a complete transmit packet before relaying and hence it does not matter if the packet processor does not emit a data word on every cycle.

The software project in this case is extremely simple - it just sets up the Si5324 clock.

Building
--------
The project can be built using the two "build_project.sh" scripts in the hardware directories. These will create a Vivado project, create the block design, synthesize and implement the design. To perform this process, you must have a license for the Xilinx Ten-Gigabit MAC core.

After building, the projects can then be opened in Vivado and exported to SDK to build the software projects. The code in the {master,slave}_sw folders can then be imported into SDK to be built and run on the platforms.
