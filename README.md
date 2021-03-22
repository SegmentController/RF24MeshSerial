# RF24MeshSerial
Firmware for run RF24 Mesh network on ARM devices and access over TTY/serial (USB port)

# Why?
I love RF24 devices. With the help of RF24Mesh I can create automatic networks, I can run the network master node on an Arduino Nano with low memory usage. My favorite library is nRF24/RF24Mesh (https://github.com/nRF24/RF24Mesh). But I ran into an obstacle when I wanted to use it on a simple PC.

Although it works on RPi, I have a hard time explaining to my customers what hardware configuration they can use to install and operate nRF24 and python and linux libs and gcc config. Error detection is also cumbersome. And on a Windows PC, I was completely clueless.

# Solution
I want to make an intermediate hardware that:
- manages the entire mesh network
- use lib nRF24/RF24Mesh (https://github.com/nRF24/RF24Mesh)
- not my CPU works with RF24 connection
- low energy and easy to install

This is how the idea of a small firmware that can be installed on an Arduino Nano or RFNano was born. I just upload the firmware and it uses nRF24Mesh to build the network, keep it running, the Nano CPU handles it all. And I can send simple commands to Nano on a serial port (like AT commands with a GPS device):
- GET/SET NODEID
- GET/SET CHANNEL
- GET/SET SPEED
- BEGIN
- SEND
- RECEIVE
- CHECK (network)
- RENEW (address)
- UPTIME
- VERSION
- HELP

After I could manage the device through the COM port and manage the entire RF24Mesh network, I made a library for using it under Node.js: https://github.com/BCsabaEngine/RF24MeshSerialNode 

# Hardware
You can use it on any Arduino to which an nRF24 chip is attached. Or you can use the RFNano module immediately without soldering. The USB port is suitable for upload code, through which it receives the power supply and you can also implement the serial port communication on this. All you need is a USB data cable! A 30cm is enough, but the length doesn't matter, it can be up to 2m.

![RFNano](https://github.com/BCsabaEngine/RF24MeshSerial/blob/main/docs/rfnano.jpg?raw=true)
![NanoExt](https://github.com/BCsabaEngine/RF24MeshSerial/blob/main/docs/nanowithrf24.jpg?raw=true)

# Configuration
Before uploading the firmware, you can change several parameters to work immediately after the resulting hardware connection. (And you can change multiple values at runtime - with COM port commands).
Important config:
- SERIAL_SPEED: COM port speed
- AUTOBEGIN_AS_MASTER: Autostart the nRF24 comm as a master node. Without this you build a slave-node and you must to specify nodeid before begin
- RADIO_CE_CS_PIN: your hardware depending CE and CS pins

# Used libraries
Modified SerialCommand library included/embed in project. It is optimized for this project: memory allocation, buffers length.
You need to install manually the RF24Mesh (and RF24Network and RF24) library into Arduino IDE to compile.

# Memory usage
Default compile for Arduino Nano in Arduino IDE: Sketch size 16612 bytes (54%), variables are 1496 bytes (73%).

# Try it!
Grab an Arduino RF Nano, upload the code on it and use a COM port monitor to see the engine. Try a few commands on the COM port. You are ready to create the RF24Mesh network!


Example, type (bold text) these lines to master node and see results (normal text):

VERSION 1.0

READY

**CHANNEL 90**

CHANNEL 90

**NODEID 0**

NODEID 0x0

**BEGIN**

BEGIN OK

**SEND 0x12 0x00 0x112233**



Example, type (bold text) these lines to node with id 0x12 and see results (normal text):

VERSION 1.0

READY

**CHANNEL 90**

CHANNEL 90

**NODEID 0x12**

NODEID 0x12

**BEGIN**

BEGIN OK

RECEIVE 0x12 0x00 0x112233
