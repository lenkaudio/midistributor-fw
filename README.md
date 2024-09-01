# Lenkaudio MIDIstributor: *Open-Source 4x/4x MIDI/USB Interface*

## **Project Status**: 

#### **⚠️ Under Development ⚠️**

The hardware **has design errors that will be corrected in V2**, see below.

The software is currently a **work in progress**, as basic features are being implemented.

### Current State - HW
- V1 HW was tested and design errors were found
  - V1 works after performing the following changes:
    1. VDD is externally shorted with VBUS **or** +3V3 is provided externally
    2. R31, R31, R37, R35 are removed, disabling the RX/Thru switch
  - The hardware Rx/Thru switch function likely requires replacing R31-R38 with higher-valued resistors (and possibly adding buffers)
- Planned V2 HW Changes: 
  - The hardware Rx/Thru switch function will be removed and replaced with a software switch

### Current State - SW

- MIDI Routing
  - Messages are transmitted between HW MIDI RX/TX ports and USB MIDI In/Out ports
  - Currently only static routing is supported
    
    **Routing Table:**
    | Rule 	| From            	| To              	| Messages 	|
    |------	|-----------------	|-----------------	|----------	|
    | 1    	| HW MIDI IN  A   	|  USB MIDI IN  1 	| ALL      	|
    | 2    	| HW MIDI IN  B   	|  USB MIDI IN  2 	| ALL      	|
    | 3    	| HW MIDI IN  C   	|  USB MIDI IN  3 	| ALL      	|
    | 4    	| HW MIDI IN  D   	|  USB MIDI IN  4 	| ALL      	|
    | 5    	| USB MIDI OUT 1  	|  HW MIDI OUT A  	| ALL      	|
    | 6    	| USB MIDI OUT 2  	|  HW MIDI OUT B  	| ALL      	|
    | 7    	| USB MIDI OUT 3  	|  HW MIDI OUT C  	| ALL      	|
    | 8    	| USB MIDI OUT 4  	|  HW MIDI OUT D  	| ALL      	|

- USB
  - A Composite Device is defined with the following class definitions: MIDI, CDC, HID
  - Currently only MIDI is correctly supported, CDC and HID are there as placeholders for future functions
  - A custom Windows driver is planned

## Hardware Design

You can find the *Lenkaudio MIDIstributor* hardware design files on [OSHWHub](https://oshwlab.com/lenkaudio/lenkaudio_midistributor_v1). 

JLCPCB PCB printing and assembly service is available and *recommended* as the hardware has been *specifically designed* with JLCPCB Economic PCB Assembly service in mind.

## Acknowledgements

A special thanks to [rppicomidi](https://github.com/rppicomidi/) for the [midi-multistream2usbdev](https://github.com/rppicomidi/midi-multistream2usbdev) project, which has saved me a *significant* amount of time and effort in the development of the MIDIstributor firmware. This repository is a fork of that project.

## License Information

- **Hardware License**: [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)
- **Software License**: MIT License (see LICENSE.txt)