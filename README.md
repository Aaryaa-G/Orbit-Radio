# OrbitRadio

OrbitRadio is a dedicated payload developed by New Leap Labs, K. J. Somaiya Institute of Technology, hosted on MOI-1 by Take Me 2 Space Technologies Pvt. Ltd. It has been designed to serve the HAM (Amateur Radio) community in India and globally, offering a reliable APRS-based telemetry service in the UHF amateur band.

The payload integrates telemetry collection, data processing, and RF transmission into a compact and efficient system, ensuring that satellite health and mission data are available to ground users via standard HAM equipment.


## Purpose

The OrbitRadio payload acts as a space-based APRS digipeater and telemetry transmitter, enabling:

* Real-time collection of telemetry data from satellite subsystems.
* Forwarding of this data over the UHF amateur band at a central frequency of 435.248 MHz.
* Supporting the HAM radio community by expanding access to telemetry data and promoting satellite-ground interaction.

This payload not only strengthens satellite operations but also promotes educational and experimental opportunities for amateur operators worldwide.


## Why This Payload is Needed

### Amateur Radio Outreach

OrbitRadio extends support to the global HAM community, giving radio amateurs the chance to receive and decode real-time telemetry from a satellite in orbit.

### Telemetry Reliability

By transmitting key health and mission data from subsystems, the payload ensures that satellite operators can monitor performance and detect anomalies efficiently.

### Educational Value

Amateur operators, students, and researchers gain hands-on experience in receiving and interpreting APRS packets over AX.25, fostering learning in RF communication, telemetry, and satellite operations.

### Spectrum Efficiency

Operating within the 435–438 MHz amateur band, the payload uses AFSK with FM modulation and a narrow 12.5 kHz bandwidth, ensuring efficient use of radio spectrum and compliance with global standards.


## OrbitRadio Output

* **Service:** APRS-based telemetry downlink
* **Frequency Band:** 435–438 MHz (central frequency: 435.248 MHz, subject to IARU/ITU finalization)
* **Modulation Scheme:** AFSK (Audio Frequency Shift Keying) using FM
* **Encoding:** APRS over AX.25 protocol
* **Transmit Power:** +30 dBm (1 W) RF output
* **Target Users:** Global HAM operators, ground stations, and educational institutions

The final transmitted signal is received on the ground using a standard amateur radio setup, decoded as AX.25 APRS packets, and displayed as meaningful telemetry data.

### The main purpose of OrbitRadio is to:

* Collect and transmit satellite telemetry to ground stations.
* Promote educational and research opportunities in HAM radio and satellite communication.
* Support both Indian and global HAM operators in accessing real-time satellite data.

The system is strictly for telemetry and educational outreach and does not involve any encrypted or restricted communications.
