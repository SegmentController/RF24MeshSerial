# RF24MeshSerial
Firmware for RF24 Mesh devices access by TTY/serial (USB port)

# Why?
I love RF24 devices. With the help of RF24Mesh I can create automatic networks, I can run the network on an Arduino NANO with low memory usage. My favorite is nRF24/RF24Mesh. But I ran into an obstacle when I wanted to use it on a PC.

Although it works on RPi, I have a hard time explaining to my customers what HW configuration they can use to install and operate nRF24 hardware and python. Error detection is also cumbersome. And on a Windows PC, I was completely clueless.

# Solution
The idea came up: you should make some intermediate hardware that:
- manages the entire mesh network
- not my CPU works with RF24 connection
- low energy and easy to install

This is how the idea of a small firmware that can be installed on an Arduino NANO or NanoRF was born. I just upload the firmware and it uses nRF24Mesh to build the network, keep it running, the NANO CPU handles it all. And I can send simple commands to him on a serial port (like AT commands with a GPS device):
- GET/SET NODEID
- GET/SET CHANNEL
- GET/SET SPEED
- BEGIN
- SEND
- RECEIVE
- CHECK (network)
- RENEW (address)

# Hardware
You can use it on any Arduino to which an nRF24 chip is attached. Or you can use the NanoRF module immediately without soldering. The USB port is suitable for upload code, through which it receives the power supply and you can also implement the serial port communication on this. All you need is a USB data cable! A 30cm is enough, but the length doesn't matter, it can be up to 2m.
![RFNano](https://github.com/BCsabaEngine/RF24MeshSerial/blob/main/docs/rfnano.jpg?raw=true)
![NanoExt](https://github.com/BCsabaEngine/RF24MeshSerial/blob/main/docs/nanowithrf24.jpg?raw=true)

# Configuration
Before uploading the firmware, you can change several parameters to work immediately after the resulting hardware connection. You can change multiple values at runtime (with COM port commands).
SERIAL_SPEED: COM port speed
AUTOBEGIN_AS_MASTER: Autostart the nRF24 comm as a master node. Without this you build a slave-node and you must to specify nodeid before begin
RADIO_CE_CS_PIN: your hardware depending CE and CS spin

# Libraries
Modified SerialCommand library included/embed in project. It is optimized for this project: memory allocation, buffers length.
You need to install RF24Mesh (and RF24Network and RF24) library into Arduino IDE to compile.

#Memory usage
Default compile for Arduino NANO in Arduino IDE: Sketch size 16612 bytes (54%), variables are 1496 bytes (73%).
