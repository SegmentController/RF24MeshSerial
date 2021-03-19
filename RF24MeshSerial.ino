/**
   RF24MeshSerial
**/

#include <SPI.h>
#include <RF24Network.h>
#include <RF24.h>
#include <RF24Mesh.h>
#include <EEPROM.h> //Include eeprom.h by RF24Mesh for AVR (Uno, Nano) etc. except ATTiny

/**
   SKETCH-WIDE CONFIGURATION

   Set these values before upload
   Some parameter can be set at runtime
**/
#define SERIAL_SPEED                115200
//#define SERIAL_COMMAND_ECHO                           // Enable echo of input (use SmarTTY instead)

#define START_DELAY_MS              500               // Some hardware bootup time
#define LOOP_DELAY_MS               20                // Some cool-down sleep

#define AUTOBEGIN_AS_MASTER                           // Autostart as master (nodeID = 0)
#define DEFAULT_CHANNEL             90                // 0..125 (2.400 to 2.525)
#define DEFAULT_SPEED               RF24_250KBPS      // RF24_250KBPS, RF24_1MBPS or RF24_2MBPS

#define RADIO_CE_CS_PIN             10,9              // RFnano=(10,9) or (9,10); Nano typical=(7,8)
#define RADIO_POWER                 RF24_PA_HIGH      // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
#define NETWORK_TIMEOUT_MS          1500              // 1000..15000 (original default 7500, tipical 1500)
#define NETWORK_DYNAMIC_TX_TIMEOUT                    // If enabled, random additional txTimeout will set
/**
   END OF CONFIGURATION
**/

#include "SerialCommand.h"

SerialCommand serialCmd;
RF24 radio(RADIO_CE_CS_PIN);
RF24Network network(radio);
RF24Mesh mesh(radio, network);
bool hasbegin = false;
uint8_t nodeid = 0;
uint8_t channel = DEFAULT_CHANNEL;
rf24_datarate_e speed = DEFAULT_SPEED;

void setup() {
  while (!Serial);

#ifdef START_DELAY_MS
  delay(START_DELAY_MS);
#endif

  Serial.begin(SERIAL_SPEED);

  if (!radio.begin()) {
    Serial.print(F("ERROR="));
    Serial.println(F("Radio HW not found"));
    while (1);
  }
  radio.setPALevel(RADIO_POWER);

#ifdef NETWORK_DYNAMIC_TX_TIMEOUT
  network.txTimeout = random(400, 750);
#endif

  Serial.println(F("READY"));

#ifdef AUTOBEGIN_AS_MASTER
  nodeid = 0;
  mesh.setNodeID(nodeid);
  cmdBegin();
  cmdNodeId();
  cmdChannel();
  cmdSpeed();
#endif

  serialCmd.addCommand("BEGIN", cmdBegin);
  serialCmd.addCommand("NODEID", cmdNodeId);
  serialCmd.addCommand("CHANNEL", cmdChannel);
  serialCmd.addCommand("SPEED", cmdSpeed);
  serialCmd.addCommand("NODELIST", cmdNodeList);
  serialCmd.addCommand("UPTIME", []() {
    Serial.print(F("UPTIME="));
    Serial.println(millis());
  });

  serialCmd.addCommand("HELP", []() {
    Serial.println(F("BEGIN"));
    Serial.println(F("NODEID [0 | 1..253]"));
    Serial.println(F("CHANNEL [0..125]"));
    Serial.println(F("SPEED [0..2]"));
    Serial.println(F("NODELIST"));
    Serial.println(F("UPTIME"));
  });

  serialCmd.setDefaultHandler([](const char *command) {
    Serial.print(F("ERROR="));
    Serial.print(F("Invalid command: "));
    Serial.println(command);
  });
}

void loop() {
  mesh.update();

  if (!nodeid)
    mesh.DHCP();

  serialCmd.readSerial();

  //  // Check for incoming data from the sensors
  //  if (network.available()) {
  //    RF24NetworkHeader header;
  //    network.peek(header);
  //
  //    uint32_t dat = 0;
  //    switch (header.type) {
  //      // Display the incoming millis() values from the sensor nodes
  //      case 'M':
  //        network.read(header, &dat, sizeof(dat));
  //        Serial.print(dat);
  //        Serial.print(" from RF24Network address #0");
  //        Serial.print(header.from_node, OCT);
  //        Serial.print(" to #");
  //        Serial.print(header.to_node, OCT);
  //        Serial.print(" id #");
  //        Serial.print(header.id);
  //        {
  //          uint8_t nodeid = 0;
  //          for (uint8_t i = 0; i < mesh.addrListTop; i++)
  //            if (header.from_node == mesh.addrList[i].address) {
  //              nodeid = mesh.addrList[i].nodeID;
  //              break;
  //            }
  //          if (nodeid)
  //          {
  //            Serial.print(" from ID #");
  //            Serial.print(nodeid);
  //          }
  //        }
  //        Serial.println();
  //        break;
  //      default:
  //        network.read(header, 0, 0);
  //        Serial.println(header.type);
  //        break;
  //    }
  //  }

#ifdef LOOP_DELAY_MS
  delay(LOOP_DELAY_MS);
#endif
}

void cmdBegin() {
  mesh.begin(channel, speed, NETWORK_TIMEOUT_MS);
  hasbegin = true;
  Serial.println(F("BEGIN"));
}

void cmdNodeId() {
  int num;
  char *arg;

  arg = serialCmd.next();
  if (arg != NULL) {
    num = atoi(arg);
    if (num < 0 || num > 253) {
      Serial.print(F("ERROR="));
      Serial.print(F("Invalid nodeid (0..253): "));
      Serial.println(arg);
      return;
    }
    nodeid = num;
    mesh.setNodeID(nodeid);
  }
  Serial.print(F("NODEID="));
  Serial.println(nodeid);
}

void cmdChannel() {
  int num;
  char *arg;

  arg = serialCmd.next();
  if (arg != NULL) {
    num = atoi(arg);
    if (num < 0 || num > 125) {
      Serial.print(F("ERROR="));
      Serial.print(F("Invalid channel (0..125): "));
      Serial.println(arg);
      return;
    }
    channel = num;
    mesh.setChannel(channel);
  }
  Serial.print(F("CHANNEL="));
  Serial.println(channel);
}

void cmdSpeed() {
  int num;
  char *arg;

  arg = serialCmd.next();
  if (arg != NULL) {
    num = atoi(arg);
    switch (num)
    {
      case 0:
        speed  = RF24_250KBPS;
        break;
      case 1:
        speed  = RF24_1MBPS;
        break;
      case 2:
        speed  = RF24_2MBPS;
        break;
      default:
        Serial.print(F("ERROR="));
        Serial.print(F("Invalid speed (0..2): "));
        Serial.println(arg);
        return;
    }
    if (hasbegin)
      mesh.begin(channel, speed, NETWORK_TIMEOUT_MS);
  }
  Serial.print(F("SPEED="));
  switch (speed)
  {
    case RF24_250KBPS:
      Serial.println(F("250KB"));
      break;
    case RF24_1MBPS:
      Serial.println(F("1MB"));
      break;
    case RF24_2MBPS:
      Serial.println(F("2MB"));
      break;
  }
}

void cmdNodeList()
{
  Serial.println(F("NODELIST"));
  for (int i = 0; i < mesh.addrListTop; i++) {
    Serial.print(mesh.addrList[i].nodeID);
    Serial.print(F("="));
    Serial.println(mesh.addrList[i].address, OCT);
  }
  Serial.print(F("NODELIST"));
  Serial.println(F("END"));
}
