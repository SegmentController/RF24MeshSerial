/**
   RF24MeshSerial
**/

#define RF24MESHSERIAL_VERSION      "1.1.0"

#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include <RF24Mesh.h>

/**
   SKETCH-WIDE CONFIGURATION

   Set these values before upload
   Some parameter can be set at runtime
**/
#define SERIAL_SPEED                115200
//#define SERIAL_COMMAND_ECHO                           // Enable echo of input (use SmarTTY or Arduino IDE COM window instead)

#define START_DELAY_MS              500               // Some hardware bootup time
#define LOOP_DELAY_MS               5                 // Some cool-down sleep

//#define AUTOBEGIN_AS_MASTER                           // Autostart as master (nodeID = 0)
#define DEFAULT_CHANNEL             90                // 0..125 (2.400 to 2.525)
#define DEFAULT_SPEED               RF24_250KBPS      // RF24_250KBPS, RF24_1MBPS or RF24_2MBPS
#define MASH_AUTORENEW_INTERVAL_MS  30 * 1000         // Non-master node automatic mesh connection check (and renew if needed) in every x ms

#ifdef ARDUINO_AVR_NANO                               // CE&CS: RFnano=(10, 9) or (9, 10);
#define RADIO_CE_CS_PIN            {9, 10}
#elif STM32_CORE_VERSION                              // CE&CS: STM32=(PB0, PA4)
#define RADIO_CE_CS_PIN            {PB0, PA4}
#else                                                 // CE&CS: Nano typical=(7, 8)
#define RADIO_CE_CS_PIN            {7, 8}
#endif

#define RADIO_POWER                 RF24_PA_HIGH      // RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX
#define NETWORK_TIMEOUT_MS          350               // Network operation (dhcp, renew) timeout 1000..15000 (original default 7500, tipical 750)
//#define NETWORK_DYNAMIC_TX_TIMEOUT                  // If enabled, random additional txTimeout will set

#define MESH_PAYLOAD_MAX_SIZE       128               // Maximum available payload size

#define NETWORK_SEND_RETRY          3                 // Default retry of send
/**
   END OF CONFIGURATION
**/

#include "SerialCommand.h"

typedef uint16_t pin_ce_cs_t[2];

SerialCommand serialCmd;
RF24 radio(RADIO_CE_CS_PIN);
RF24Network network(radio);
RF24Mesh mesh(radio, network);
bool hasbegin = false;
uint8_t nodeid = 0;
uint8_t channel = DEFAULT_CHANNEL;
uint8_t retry = NETWORK_SEND_RETRY;
rf24_datarate_e speed = DEFAULT_SPEED;
unsigned long lastcheck = 0;

#if defined(ARDUINO_AVR_NANO)
void (*rebootFunc)(void) = 0;
#endif

#ifdef STM32_CORE_VERSION
void rebootFunc()
{
  NVIC_SystemReset();
}
#endif

void setup() {
  while (!Serial);

  Serial.begin(SERIAL_SPEED);

  cmdVersion();

#ifdef START_DELAY_MS
  delay(START_DELAY_MS);
#endif

  if (!radio.begin()) {
    Serial.print(F("ERROR "));
    Serial.println(F("Radio HW not found"));
    while (1);
  }
  radio.setPALevel(RADIO_POWER);

#ifdef NETWORK_DYNAMIC_TX_TIMEOUT
  network.txTimeout = random(400, 750);
#endif

  cmdHello();

#ifdef AUTOBEGIN_AS_MASTER
  nodeid = 0;
  mesh.setNodeID(nodeid);
  cmdBegin();
  cmdNodeId();
  cmdChannel();
  cmdSpeed();
#endif

  serialCmd.addCommand("HELLO", cmdHello);
  serialCmd.addCommand("BEGIN", cmdBegin);
  serialCmd.addCommand("SEND", cmdSend);
  serialCmd.addCommand("HEARTBEAT", cmdHeartbeat);
  serialCmd.addCommand("CHECK", cmdCheck);
  serialCmd.addCommand("RENEW", cmdRenew);
  serialCmd.addCommand("NODEID", cmdNodeId);
  serialCmd.addCommand("CHANNEL", cmdChannel);
  serialCmd.addCommand("SPEED", cmdSpeed);
  serialCmd.addCommand("RETRY", cmdRetry);
  serialCmd.addCommand("NODELIST", cmdNodeList);
  serialCmd.addCommand("VERSION", cmdVersion);
  serialCmd.addCommand("UPTIME", []() {
    Serial.print(F("UPTIME "));
    Serial.println(millis());
  });
  serialCmd.addCommand("RESET", []() {
    Serial.println(F("RESET"));
    Serial.flush();
    delay(1500);
    rebootFunc();
  });

  serialCmd.addCommand("HELP", []() {
    Serial.println(F("HELLO"));
    Serial.println(F("BEGIN"));
    Serial.println(F("SEND NodeId Type [Data]"));
    Serial.println(F("ex: SEND 0x20 0x10 0x9D3CE3CBAC8352541647D2417942F56B"));
    Serial.println(F("HEARTBEAT IntervalMSec NodeId Type"));
    Serial.println(F("ex: HEARTBEAT 500 0x00 0x10"));
    Serial.println(F("HEARTBEAT STOP"));
    if (nodeid)
    {
      Serial.println(F("CHECK"));
      Serial.println(F("RENEW"));
    }
    Serial.println(F("NODEID [0 | 0x00 | 0x01..0xFE]"));
    Serial.println(F("CHANNEL [0..125]"));
    Serial.println(F("SPEED [0..2]"));
    Serial.println(F("RETRY [0..16]"));
    if (!nodeid)
      Serial.println(F("NODELIST"));
    Serial.println(F("VERSION"));
    Serial.println(F("UPTIME"));
    Serial.println(F("RESET"));
  });

  serialCmd.setDefaultHandler([](const char *command) {
    Serial.print(F("ERROR "));
    Serial.print(F("Invalid command: "));
    Serial.println(command);
  });
}

void loop() {
  mesh.update();

  if (!nodeid)
    if (hasbegin)
      mesh.DHCP();

#ifdef MASH_AUTORENEW_INTERVAL_MS
  if (nodeid)
    if (millis() - lastcheck > MASH_AUTORENEW_INTERVAL_MS)
      if (hasbegin)
      {
        if (!mesh.checkConnection())
          cmdRenew(true);
        lastcheck = millis();
      }
#endif

  processReceived();

  processHeartbeat();

  serialCmd.readSerial();

#ifdef LOOP_DELAY_MS
  delay(LOOP_DELAY_MS);
#endif
}

RF24NetworkHeader rcvHeader;
byte rcvData[MESH_PAYLOAD_MAX_SIZE];
void processReceived()
{
  while (network.available())
  {
    network.peek(rcvHeader);

    int16_t from_node_id = mesh.getNodeID(rcvHeader.from_node);
    unsigned char type = rcvHeader.type;

    uint16_t size = network.read(rcvHeader, &rcvData, sizeof(rcvData));

    Serial.print(F("RECEIVE"));

    Serial.print(F(" "));

    Serial.print(F("0x"));
    if (from_node_id < 0x10)
      Serial.print(F("0"));
    Serial.print(from_node_id, HEX);

    Serial.print(F(" "));

    Serial.print(F("0x"));
    if (type < 0x10)
      Serial.print(F("0"));
    Serial.print(type, HEX);

    Serial.print(F(" "));

    if (size)
    {
      Serial.print(F("0x"));
      for (uint16_t i = 0; i < size; i++)
      {
        if (rcvData[i] < 0x10)
          Serial.print(F("0"));
        Serial.print(rcvData[i], HEX);
      }
    }
    Serial.println();
  }
}

void cmdHello() {
  Serial.println(F("READY"));
}

void cmdVersion() {
  Serial.print(F("VERSION "));
  Serial.println(RF24MESHSERIAL_VERSION);
}

byte sndData[MESH_PAYLOAD_MAX_SIZE];
int sndLength = 0;
void cmdSend() {
  int16_t to_node_id = 0;
  unsigned char type = 0;
  char *arg;

  {
    arg = serialCmd.next();
    if (arg == NULL) {
      Serial.print(F("ERROR "));
      Serial.println(F("NodeId missing"));
      return;
    }
    if (strncmp(arg, "0x", strlen("0x")) != 0)
    {
      Serial.print(F("ERROR "));
      Serial.println(F("NodeId not in HEX format"));
      return;
    }
    if (strlen(arg) != 4)
    {
      Serial.print(F("ERROR "));
      Serial.println(F("NodeId invalid length"));
      return;
    }
    to_node_id = (int)strtol(&arg[2], NULL, 16);
  }

  {
    arg = serialCmd.next();
    if (arg == NULL) {
      Serial.print(F("ERROR "));
      Serial.println(F("Type missing"));
      return;
    }
    if (strncmp(arg, "0x", strlen("0x")) != 0)
    {
      Serial.print(F("ERROR "));
      Serial.println(F("Type not in HEX format"));
      return;
    }
    if (strlen(arg) != 4)
    {
      Serial.print(F("ERROR "));
      Serial.println(F("Type invalid length"));
      return;
    }
    type = (char)strtol(&arg[2], NULL, 16);
    if (type > 127)
    {
      Serial.print(F("ERROR "));
      Serial.println(F("Type invalid value"));
      return;
    }
  }

  {
    sndLength = 0;
    arg = serialCmd.next();
    if (arg != NULL) {
      if (strncmp(arg, "0x", strlen("0x")) != 0)
      {
        Serial.print(F("ERROR "));
        Serial.println(F("Data not in HEX format"));
        return;
      }
      if (strlen(arg) < 4 || (strlen(arg) % 2) != 0)
      {
        Serial.print(F("ERROR "));
        Serial.println(F("Data invalid length"));
        return;
      }
      for (int i = 2; i < strlen(arg); i += 2)
      {
        byte d = 0;
        for (int j = 0; j < 2; j++)
        {
          d *= 16;

          char c = arg[i + j];
          if (c >= '0' && c <= '9') d += c - '0';
          else if (c >= 'A' && c <= 'F') d += c - 'A' + 10;
          else if (c >= 'a' && c <= 'f') d += c - 'a' + 10;
          else
          {
            Serial.print(F("ERROR "));
            Serial.print(F("Data invalid char: "));
            Serial.println(c);
            return;
          }
        }

        sndData[sndLength] = d;
        sndLength++;
      }
    }
  }

  bool success = false;
  int trycount = 0;
  while (!success && trycount <= retry)
  {
    if (mesh.write(&sndData, type, sndLength, to_node_id))
    {
      success = true;
      Serial.print(F("SENT"));
      if (trycount)
      {
        Serial.print(F(" "));
        Serial.println(trycount + 1);
      }
      else
        Serial.println();
    }
    trycount++;
    delay(random(5, 20));
  }

  if (!success)
  {
    Serial.print(F("ERROR "));
    Serial.println(F("Not sent"));
  }
}

int sndHeartbeatInterval = 0;
int sndHeartbeatNodeId = 0;
unsigned char sndHeartbeatType = 0;
unsigned long sndHeartbeatLastSent = 0;
void processHeartbeat()
{
  if (!sndHeartbeatInterval)
    return;

  if (millis() - sndHeartbeatLastSent < sndHeartbeatInterval)
    return;

  unsigned long now = millis();
  bool success = false;
  int trycount = 0;
  while (!success && trycount <= retry)
  {
    if (mesh.write(&now, sndHeartbeatType, sizeof(now), sndHeartbeatNodeId))
    {
      success = true;
      Serial.print(F("HEARTBEAT SENT"));
      if (trycount)
      {
        Serial.print(F(" "));
        Serial.println(trycount + 1);
      }
      else
        Serial.println();
    }
    trycount++;
    delay(random(5, 20));
  }

  if (!success)
  {
    Serial.print(F("ERROR "));
    Serial.println(F("Heartbeat not sent"));
  }

  sndHeartbeatLastSent = millis();
}

void cmdHeartbeat()
{
  sndHeartbeatInterval = 0;

  int16_t intervalms = 0;
  int16_t to_node_id = 0;
  unsigned char type = 0;
  char *arg;

  {
    arg = serialCmd.next();
    if (arg == NULL) {
      Serial.print(F("ERROR "));
      Serial.println(F("Interval ms missing"));
      return;
    }
    if (strcmp(arg, "STOP") == 0)
      intervalms = 0;
    else if (strlen(arg) < 2  || strlen(arg) > 4)
    {
      Serial.print(F("ERROR "));
      Serial.println(F("Interval ms invalid length (10..9999ms)"));
      return;
    }
    intervalms = (int)strtol(&arg[0], NULL, 10);
  }

  if (intervalms)
  {
    arg = serialCmd.next();
    if (arg == NULL) {
      Serial.print(F("ERROR "));
      Serial.println(F("NodeId missing"));
      return;
    }
    if (strncmp(arg, "0x", strlen("0x")) != 0)
    {
      Serial.print(F("ERROR "));
      Serial.println(F("NodeId not in HEX format"));
      return;
    }
    if (strlen(arg) != 4)
    {
      Serial.print(F("ERROR "));
      Serial.println(F("NodeId invalid length"));
      return;
    }
    to_node_id = (int)strtol(&arg[2], NULL, 16);
  }

  if (intervalms)
  {
    arg = serialCmd.next();
    if (arg == NULL) {
      Serial.print(F("ERROR "));
      Serial.println(F("Type missing"));
      return;
    }
    if (strncmp(arg, "0x", strlen("0x")) != 0)
    {
      Serial.print(F("ERROR "));
      Serial.println(F("Type not in HEX format"));
      return;
    }
    if (strlen(arg) != 4)
    {
      Serial.print(F("ERROR "));
      Serial.println(F("Type invalid length"));
      return;
    }
    type = (char)strtol(&arg[2], NULL, 16);
    if (type > 127)
    {
      Serial.print(F("ERROR "));
      Serial.println(F("Type invalid value"));
      return;
    }
  }

  if (intervalms)
  {
    sndLength = 0;
    arg = serialCmd.next();
    if (arg != NULL) {
      if (strncmp(arg, "0x", strlen("0x")) != 0)
      {
        Serial.print(F("ERROR "));
        Serial.println(F("Data not in HEX format"));
        return;
      }
      if (strlen(arg) < 4 || (strlen(arg) % 2) != 0)
      {
        Serial.print(F("ERROR "));
        Serial.println(F("Data invalid length"));
        return;
      }
      for (int i = 2; i < strlen(arg); i += 2)
      {
        byte d = 0;
        for (int j = 0; j < 2; j++)
        {
          d *= 16;

          char c = arg[i + j];
          if (c >= '0' && c <= '9') d += c - '0';
          else if (c >= 'A' && c <= 'F') d += c - 'A' + 10;
          else if (c >= 'a' && c <= 'f') d += c - 'a' + 10;
          else
          {
            Serial.print(F("ERROR "));
            Serial.print(F("Data invalid char: "));
            Serial.println(c);
            return;
          }
        }

        sndData[sndLength] = d;
        sndLength++;
      }
    }
  }

  sndHeartbeatInterval = intervalms;
  sndHeartbeatType = type;
  sndHeartbeatNodeId = to_node_id;

  if (intervalms)
    Serial.println(F("HEARTBEAT SCHEDULED"));
  else
    Serial.println(F("HEARTBEAT STOPPED"));
}

void cmdCheck() {
  if (!nodeid)
  {
    Serial.print(F("ERROR "));
    Serial.println(F("Check available on non-master only"));
    return;
  }

  if (!hasbegin)
  {
    Serial.print(F("ERROR "));
    Serial.println(F("Connection not started (BEGIN)"));
    return;
  }

  if (mesh.checkConnection())
    Serial.println(F("CHECK OK"));
  else
  {
    Serial.print(F("ERROR "));
    Serial.println(F("Check failed"));
  }
}

void cmdRenew() {
  cmdRenew(false);
}

void cmdRenew(bool autorenew) {
  if (!nodeid)
  {
    Serial.print(F("ERROR "));
    Serial.println(F("Renew available on non-master only"));
    return;
  }

  if (!hasbegin)
    cmdBegin();
  else
  {
    mesh.renewAddress(NETWORK_TIMEOUT_MS);
    if (mesh.checkConnection())
      Serial.println(autorenew ? F("AUTORENEW OK") : F("RENEW OK"));
    else
    {
      Serial.print(F("ERROR "));
      Serial.println(autorenew ? F("Auto renew failed") : F("Renew failed"));
    }
  }
}

void cmdBegin() {
  hasbegin = true;
  mesh.begin(channel, speed, NETWORK_TIMEOUT_MS);

  if (!nodeid || mesh.checkConnection())
    Serial.println(F("BEGIN OK"));
  else
  {
    Serial.print(F("ERROR "));
    Serial.println(F("Begin failed"));
  }
}

void cmdNodeId() {
  int num;
  char *arg;

  arg = serialCmd.next();
  if (arg != NULL) {
    if (strcmp(arg, "0") == 0)
    {
      num = 0;
    }
    else
    {
      if (strncmp(arg, "0x", strlen("0x")) != 0)
      {
        Serial.print(F("ERROR "));
        Serial.println(F("NodeId not in HEX format"));
        return;
      }
      if (strlen(arg) != 4)
      {
        Serial.print(F("ERROR "));
        Serial.println(F("NodeId invalid length"));
        return;
      }
      num = (char)strtol(&arg[2], NULL, 16);
    }
    if (num > 253)
    {
      Serial.print(F("ERROR "));
      Serial.println(F("NodeId invalid value"));
      return;
    }
    nodeid = num;
    mesh.setNodeID(nodeid);
  }
  Serial.print(F("NODEID "));
  Serial.print(F("0x"));
  if (nodeid < 0x10)
    Serial.print(F("0"));
  Serial.println(nodeid, HEX);
}

void cmdChannel() {
  int num;
  char *arg;

  arg = serialCmd.next();
  if (arg != NULL) {
    num = atoi(arg);
    if (num < 0 || num > 125) {
      Serial.print(F("ERROR "));
      Serial.print(F("Invalid channel (0..125): "));
      Serial.println(arg);
      return;
    }
    channel = num;
    mesh.setChannel(channel);
  }
  Serial.print(F("CHANNEL "));
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
        Serial.print(F("ERROR "));
        Serial.print(F("Invalid speed (0..2): "));
        Serial.println(arg);
        return;
    }
    if (hasbegin)
      mesh.begin(channel, speed, NETWORK_TIMEOUT_MS);
  }
  Serial.print(F("SPEED "));
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

void cmdRetry() {
  int num;
  char *arg;

  arg = serialCmd.next();
  if (arg != NULL) {
    num = atoi(arg);
    if (num < 0 || num > 16) {
      Serial.print(F("ERROR "));
      Serial.print(F("Invalid retry (0..16): "));
      Serial.println(arg);
      return;
    }
    retry = num;
  }
  Serial.print(F("RETRY "));
  Serial.println(retry);
}

void cmdNodeList()
{
  if (nodeid)
  {
    Serial.print(F("ERROR "));
    Serial.println(F("Node list available on master only"));
    return;
  }
  Serial.println(F("NODELIST"));
  for (int i = 0; i < mesh.addrListTop; i++) {
    Serial.print(F("0x"));
    if (mesh.addrList[i].nodeID < 0x10)
      Serial.print(F("0"));
    Serial.print(mesh.addrList[i].nodeID, HEX);
    Serial.print(F(" "));
    Serial.println(mesh.addrList[i].address, OCT);
  }
  Serial.print(F("NODELIST"));
  Serial.println(F("END"));
}
