/*
 * ============================================================
 *  WiFi Telescope Focuser – ESP8266
 *  MyFocuserPro2 Full Protocol (numeric commands :00# … :C5#)
 *  Compatible with ASCOM myFP2ESP driver and INDI myFocuserPro2 driver
 *
 *  Hardware: ESP8266 (NodeMCU / Wemos D1 mini)
 *  Motor:    28BYJ-48 via ULN2003 driver board (half-step)
 *  Pins:     IN1=D5(14), IN2=D6(12), IN3=D7(13), IN4=D8(15)
 *
 *  Protocol port : 7624  (TCP)
 *  Web interface : port 80
 *  OTA update    : ArduinoOTA (hostname = deviceName)
 * ============================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ElegantOTA.h>
#include <EEPROM.h>
#include "secrets.h"

// ============================================================
//  USER CONFIGURATION – edit before flashing
// ============================================================
const char* ssid       = WIFI_SSID;
const char* password   = WIFI_PASSWORD;
const char* deviceName = "TelescopeFocuser";
const int   tcpPort    = 7624;

// Motor pins (ULN2003)
const int IN1 = 14; // D5
const int IN2 = 12; // D6
const int IN3 = 13; // D7
const int IN4 = 15; // D8

// ============================================================
//  FIRMWARE VERSION
// ============================================================
const char* FW_VERSION       = "300";   // major version string
const char* FW_MINOR_VERSION = "100";   // minor version string

// ============================================================
//  PROTOCOL CONSTANTS
// ============================================================
const char _RTOKEN = 'r';   // generic reply token (matches tcpip_server.cpp _RTOKEN)
const char _EOC    = '#';   // end-of-command character

const int DEFAULTTEMPRESOLUTION = 9;  // DS18B20 default bits

// ============================================================
//  MOTOR CONSTANTS
// ============================================================
const int STEPS_PER_REV = 2048;  // 28BYJ-48 full revolution (half-step)
const int SPEED_SLOW    = 4000;  // µs between steps (slowest)
const int SPEED_MED     = 3000;
const int SPEED_FAST    = 2000;  // fastest

// Half-step sequence
const int stepSeq[8][4] = {
  {1,0,0,0},{1,1,0,0},{0,1,0,0},{0,1,1,0},
  {0,0,1,0},{0,0,1,1},{0,0,0,1},{1,0,0,1}
};

// ============================================================
//  EEPROM LAYOUT
// ============================================================
const int EEPROM_SIZE      = 256;
const int ADDR_POS         = 0;   // long  (4 bytes)
const int ADDR_MAXSTEP     = 4;   // long  (4 bytes)
const int ADDR_SPEED       = 8;   // int   (4 bytes, stored as µs)
const int ADDR_STEPMODE    = 12;  // int
const int ADDR_COILPWR     = 16;  // byte
const int ADDR_REVERSE     = 17;  // byte
const int ADDR_BACKIN      = 18;  // byte  (backlash IN steps)
const int ADDR_BACKOUT     = 19;  // byte  (backlash OUT steps)
const int ADDR_BKIN_EN     = 20;  // byte  (backlash IN enabled)
const int ADDR_BKOUT_EN    = 21;  // byte  (backlash OUT enabled)
const int ADDR_STEPSIZE    = 22;  // float (4 bytes) step size in µm
const int ADDR_STEPSIZE_EN = 26;  // byte
const int ADDR_VALID       = 30;  // byte  magic number
const int ADDR_TEMPMODE    = 31;  // byte  (1=Celsius, 0=Fahrenheit)
const int ADDR_UPDMOVE     = 32;  // byte  (display update on move)
const int ADDR_MOTORSPEED  = 33;  // byte  (0=slow,1=med,2=fast)

const byte EEPROM_MAGIC = 0xA5;

// ============================================================
//  FOCUSER STATE
// ============================================================
long  currentPos   = 0;
long  targetPos    = 0;
long  maxStep      = 8000;
bool  isMoving     = false;
int   stepMode     = 2;           // 1=full, 2=half
int   motorSpeedUs = SPEED_MED;   // µs between steps
int   motorSpeed   = 1;           // 0=slow, 1=med, 2=fast (matches tcpip protocol)
bool  coilPower    = false;       // keep coils energised after move?
bool  reverseDir   = true;
bool  tempComp     = false;
int   tempCoeff    = 0;
bool  tempCompDir  = false;        // false=IN, true=OUT
float temperature  = 20.0f;
bool  tempProbeOK  = false;        // no physical probe → false
bool  displayOn    = true;
bool  displayUpdateOnMove = false;
int   displayTime  = 2000;         // ms
byte  tempMode     = 1;            // 1=Celsius, 0=Fahrenheit
bool  stepSizeEn   = false;
float stepSizeMicrons = 1.0f;
byte  backlashInSteps  = 0;
byte  backlashOutSteps = 0;
bool  backlashInEn     = false;
bool  backlashOutEn    = false;
bool  parkEnabled      = false;
bool  parked           = false;
int   parkTime         = 30;        // seconds
bool  pushbtnEn        = false;
int   pushbtnSteps     = 1;
int   delayAfterMove   = 0;        // ms
long  presets[10]      = {0};       // :90x / :91x
int   lastMoveDir      = 0;         // +1=OUT, -1=IN, 0=none
long  backlashTarget   = -1L;       // >=0 → phase-2 target pending

// internal step tracking
int   currentStepIdx = 0;
unsigned long lastStepUs = 0;

// temperature compensation accumulator
float lastCompTemp   = 20.0f;

// ============================================================
//  WEB & TCP SERVERS
// ============================================================
ESP8266WebServer webServer(80);
WiFiServer  tcpServer(tcpPort);
WiFiClient  tcpClient;

String inputBuffer = "";

// ============================================================
//  EEPROM HELPERS
// ============================================================
void eepromScheduleSave();   // forward declaration

void eepromSave() {
  EEPROM.put(ADDR_POS,         currentPos);
  EEPROM.put(ADDR_MAXSTEP,     maxStep);
  EEPROM.put(ADDR_SPEED,       motorSpeedUs);
  EEPROM.put(ADDR_STEPMODE,    stepMode);
  EEPROM.put(ADDR_COILPWR,     (byte)coilPower);
  EEPROM.put(ADDR_REVERSE,     (byte)reverseDir);
  EEPROM.put(ADDR_BACKIN,      backlashInSteps);
  EEPROM.put(ADDR_BACKOUT,     backlashOutSteps);
  EEPROM.put(ADDR_BKIN_EN,     (byte)backlashInEn);
  EEPROM.put(ADDR_BKOUT_EN,    (byte)backlashOutEn);
  EEPROM.put(ADDR_STEPSIZE,    stepSizeMicrons);
  EEPROM.put(ADDR_STEPSIZE_EN, (byte)stepSizeEn);
  EEPROM.put(ADDR_VALID,       EEPROM_MAGIC);
  EEPROM.put(ADDR_TEMPMODE,    tempMode);
  EEPROM.put(ADDR_UPDMOVE,     (byte)displayUpdateOnMove);
  EEPROM.put(ADDR_MOTORSPEED,  (byte)motorSpeed);
  EEPROM.commit();
}

void eepromLoad() {
  byte magic = 0;
  EEPROM.get(ADDR_VALID, magic);
  if (magic != EEPROM_MAGIC) {
    Serial.println("EEPROM: no valid data, using defaults");
    eepromScheduleSave();
    return;
  }
  EEPROM.get(ADDR_POS,      currentPos);
  EEPROM.get(ADDR_MAXSTEP,  maxStep);
  EEPROM.get(ADDR_SPEED,    motorSpeedUs);
  EEPROM.get(ADDR_STEPMODE, stepMode);
  byte b;
  EEPROM.get(ADDR_COILPWR,  b); coilPower    = (b != 0);
  EEPROM.get(ADDR_REVERSE,  b); reverseDir   = (b != 0);
  EEPROM.get(ADDR_BACKIN,   backlashInSteps);
  EEPROM.get(ADDR_BACKOUT,  backlashOutSteps);
  EEPROM.get(ADDR_BKIN_EN,  b); backlashInEn  = (b != 0);
  EEPROM.get(ADDR_BKOUT_EN, b); backlashOutEn = (b != 0);
  EEPROM.get(ADDR_STEPSIZE, stepSizeMicrons);
  EEPROM.get(ADDR_STEPSIZE_EN, b); stepSizeEn = (b != 0);
  EEPROM.get(ADDR_TEMPMODE,    tempMode);
  EEPROM.get(ADDR_UPDMOVE,  b); displayUpdateOnMove = (b != 0);
  EEPROM.get(ADDR_MOTORSPEED, b); motorSpeed = (int)b;

  // sanity checks
  if (currentPos < 0 || currentPos > 200000)    currentPos   = 0;
  if (maxStep < 100  || maxStep > 200000)        maxStep      = 8000;
  if (motorSpeedUs < 600 || motorSpeedUs > 5000) motorSpeedUs = SPEED_MED;
  if (stepMode < 1   || stepMode > 8)            stepMode     = 2;
  if (stepSizeMicrons <= 0)                      stepSizeMicrons = 1.0f;
  if (motorSpeed < 0 || motorSpeed > 2)          motorSpeed   = 1;

  targetPos = currentPos;
}

// ============================================================
//  MOTOR CONTROL
// ============================================================
void motorPins(int a, int b, int c, int d) {
  digitalWrite(IN1, a);
  digitalWrite(IN2, b);
  digitalWrite(IN3, c);
  digitalWrite(IN4, d);
}

void motorOff() {
  motorPins(0, 0, 0, 0);
}

void motorStep(int dir) {
  if (reverseDir) dir = -dir;
  if (dir > 0) { currentStepIdx = (currentStepIdx + 1) % 8; }
  else          { currentStepIdx = (currentStepIdx + 7) % 8; }
  motorPins(stepSeq[currentStepIdx][0],
            stepSeq[currentStepIdx][1],
            stepSeq[currentStepIdx][2],
            stepSeq[currentStepIdx][3]);
}

void motorLoop() {
  if (!isMoving) return;

  unsigned long now = micros();
  if (now - lastStepUs < (unsigned long)motorSpeedUs) return;

  if (currentPos == targetPos) {
    if (backlashTarget >= 0) {
      long real      = backlashTarget;
      backlashTarget = -1L;
      targetPos      = real;
      return;
    }
    isMoving = false;
    if (!coilPower) motorOff();
    eepromScheduleSave();
    parked = false;
    return;
  }

  int dir = (targetPos > currentPos) ? 1 : -1;
  motorStep(dir);
  currentPos += dir;
  lastStepUs  = now;
}

void moveTo(long pos) {
  if (pos < 0)       pos = 0;
  if (pos > maxStep) pos = maxStep;

  int dir = (pos > currentPos) ? 1 : (pos < currentPos ? -1 : 0);
  if (dir == 0) { targetPos = pos; isMoving = false; parked = false; return; }

  long adjPos = pos;
  backlashTarget = -1L;

  if (lastMoveDir != 0 && dir != lastMoveDir) {
    if (dir < 0 && backlashInEn && backlashInSteps > 0) {
      long over = pos - (long)backlashInSteps;
      if (over < 0) over = 0;
      if (over != pos) { adjPos = over; backlashTarget = pos; }
    } else if (dir > 0 && backlashOutEn && backlashOutSteps > 0) {
      long over = pos + (long)backlashOutSteps;
      if (over > maxStep) over = maxStep;
      if (over != pos) { adjPos = over; backlashTarget = pos; }
    }
  }

  lastMoveDir = dir;
  targetPos   = adjPos;
  isMoving    = (targetPos != currentPos);
  parked      = false;
}

void moveRelative(long steps) {
  moveTo(currentPos + steps);
}

void stopMotor() {
  isMoving       = false;
  targetPos      = currentPos;
  backlashTarget = -1L;
  if (!coilPower) motorOff();
}

void homeMotor() {
  moveTo(0);
}

void setCurrentPos(long pos) {
  currentPos  = pos;
  targetPos   = pos;
  lastMoveDir = 0;
}

// ============================================================
//  TCP SEND
// ============================================================
void tcpSend(const String& s) {
  if (tcpClient && tcpClient.connected()) {
    tcpClient.print(s);
    tcpClient.flush();
    Serial.print("RSP: "); Serial.println(s);
  }
}

// Build a reply: <token><value>#
// Overloads for convenience
void sendReply(char token, long val) {
  tcpSend(String(token) + String(val) + "#");
}
void sendReply(char token, bool val) {
  tcpSend(String(token) + String(val ? "1" : "0") + "#");
}
void sendReply(char token, const char* val) {
  tcpSend(String(token) + String(val) + "#");
}
void sendReply(char token, const String& val) {
  tcpSend(String(token) + val + "#");
}
void sendReplyFloat(char token, float val, int decimals) {
  char buf[16];
  dtostrf(val, 1, decimals, buf);
  tcpSend(String(token) + String(buf) + "#");
}

// ============================================================
//  PROTOCOL COMMAND DISPATCH
//  tcpip_server.cpp compatible:
//    commands :00#-:99# and :A0#-:C5#
//    responses: <token><value>#
// ============================================================
void dispatchCommand(const String& raw) {
  if (raw.length() < 2) return;

  String pfx = raw.substring(0, 2);
  String arg = (raw.length() > 2) ? raw.substring(2) : "";

  Serial.print("CMD: "); Serial.print(pfx);
  if (arg.length()) { Serial.print(" arg="); Serial.print(arg); }
  Serial.println();

  // ── :00# Get current focuser position
  if (pfx == "00") {
    sendReply('P', currentPos);

  // ── :01# Get motor moving status (I1=moving, I0=stopped)
  } else if (pfx == "01") {
    sendReply('I', isMoving);

  // ── :02# Controller status
  } else if (pfx == "02") {
    sendReply('E', "OK");

  // ── :03# Firmware version
  } else if (pfx == "03") {
    sendReply('F', FW_VERSION);

  // ── :04# Firmware name + version  → F<name>\r\n<version>#
  } else if (pfx == "04") {
    tcpSend("F" + String(deviceName) + "\r\n" + String(FW_VERSION) + "#");

  // ── :05xxxxxx# Set target position and move (no reply)
  } else if (pfx == "05") {
    if (!isMoving) {
      long pos = arg.toInt();
      moveTo(pos);
      isMoving = true;
    }

  // ── :06# Get temperature (3 decimal places)
  } else if (pfx == "06") {
    sendReplyFloat('Z', temperature, 3);

  // ── :07xxxxxx# Set MaxStep (no reply)
  } else if (pfx == "07") {
    long ms = arg.toInt();
    if (ms > currentPos && ms <= 200000) { maxStep = ms; eepromScheduleSave(); }

  // ── :08# Get MaxStep
  } else if (pfx == "08") {
    sendReply('M', maxStep);

  // ── :09# Get in/out LED mode (not supported on ESP8266)
  } else if (pfx == "09") {
    sendReply(_RTOKEN, (long)0);

  // ── :10# Get MaxIncrement (same as MaxStep)
  } else if (pfx == "10") {
    sendReply('Y', maxStep);

  // ── :11# Get Coil Power setting
  } else if (pfx == "11") {
    sendReply('O', coilPower);

  // ── :12x# Set Coil Power (no reply)
  } else if (pfx == "12") {
    coilPower = (arg.toInt() != 0);
    if (!coilPower && !isMoving) motorOff();
    else if (coilPower) { /* coils already active during move */ }

  // ── :13# Get reverse direction
  } else if (pfx == "13") {
    sendReply('R', reverseDir);

  // ── :14x# Set reverse direction (no reply)
  } else if (pfx == "14") {
    if (!isMoving) {
      reverseDir = (arg.toInt() != 0);
      eepromScheduleSave();
    }

  // ── :15x# Set motor speed 0=slow,1=med,2=fast (no reply)
  } else if (pfx == "15") {
    int sp = constrain(arg.toInt(), 0, 2);
    motorSpeed = sp;
    if      (sp == 0) motorSpeedUs = SPEED_SLOW;
    else if (sp == 1) motorSpeedUs = SPEED_MED;
    else              motorSpeedUs = SPEED_FAST;
    eepromScheduleSave();

  // ── :16# Set temperature display to Celsius (no reply)
  } else if (pfx == "16") {
    tempMode = 1;  // Celsius=1

  // ── :17# Set temperature display to Fahrenheit (no reply)
  } else if (pfx == "17") {
    tempMode = 0;  // Fahrenheit=0

  // ── :18# Set stepsize enable (not supported on ESP8266, no reply)
  } else if (pfx == "18") {
    // always enabled on ESP8266

  // ── :19xxx# Set step size value (µm, float, no reply)
  } else if (pfx == "19") {
    float v = arg.toFloat();
    if (v >= 0.1f && v <= 100.0f) { stepSizeMicrons = v; eepromScheduleSave(); }

  // ── :20# Set temperature probe resolution (not supported, no reply)
  } else if (pfx == "20") {
    // not supported on ESP8266

  // ── :21# Get temperature probe resolution
  } else if (pfx == "21") {
    sendReply('Q', (long)DEFAULTTEMPRESOLUTION);

  // ── :22xxx# Set temperature coefficient (no reply)
  } else if (pfx == "22") {
    tempCoeff = constrain(arg.toInt(), 0, 400);

  // ── :23x# Set temperature compensation ON/OFF (no reply)
  } else if (pfx == "23") {
    if (tempProbeOK) {
      tempComp = (arg.toInt() != 0);
    } else {
      tempComp = false;
    }

  // ── :24# Get temperature compensation state  (token '1')
  } else if (pfx == "24") {
    sendReply('1', tempComp);

  // ── :25# Get temperature compensation available
  } else if (pfx == "25") {
    sendReply('A', tempProbeOK);

  // ── :26# Get temperature coefficient
  } else if (pfx == "26") {
    sendReply('B', (long)tempCoeff);

  // ── :27# Stop motor (no reply)
  } else if (pfx == "27") {
    stopMotor();

  // ── :28# Home motor to position 0 (no reply)
  } else if (pfx == "28") {
    if (!isMoving) { targetPos = 0; isMoving = (currentPos != 0); }

  // ── :29# Get stepmode
  } else if (pfx == "29") {
    sendReply('S', (long)stepMode);

  // ── :30xx# Set stepmode (no reply)
  } else if (pfx == "30") {
    int sm = arg.toInt();
    if (sm >= 1 && sm <= 8) { stepMode = sm; eepromScheduleSave(); }

  // ── :31xxxxxx# Set current focuser position (no move, no reply)
  } else if (pfx == "31") {
    if (!isMoving) {
      long pos = arg.toInt();
      if (pos >= 0 && pos <= maxStep) {
        setCurrentPos(pos);
        eepromScheduleSave();
      }
    }

  // ── :32# Get if step size enabled (always 1 on ESP8266)
  } else if (pfx == "32") {
    tcpSend("U1#");

  // ── :33# Get step size in microns (2 decimal places)
  } else if (pfx == "33") {
    sendReplyFloat('T', stepSizeMicrons, 2);

  // ── :34# Get display page time
  } else if (pfx == "34") {
    sendReply('X', (long)displayTime);

  // ── :35# Set display page time (not supported on ESP8266, no reply)
  } else if (pfx == "35") {
    // not supported

  // ── :36x# Enable/disable display (no reply)
  } else if (pfx == "36") {
    displayOn = (arg.toInt() != 0);

  // ── :37# Get display state
  } else if (pfx == "37") {
    sendReply('D', displayOn);

  // ── :38# Get temperature mode (1=Celsius, 0=Fahrenheit)
  } else if (pfx == "38") {
    sendReply('b', (long)tempMode);

  // ── :39# Get target motor position
  } else if (pfx == "39") {
    sendReply('N', targetPos);

  // ── :40# Reboot controller (no reply – reboots immediately)
  } else if (pfx == "40") {
    delay(100);
    ESP.restart();

  // ── :41# Set in-out LED mode (not supported on ESP8266, no reply)
  } else if (pfx == "41") {
    // not supported

  // ── :42# Reset focuser to defaults (no reply)
  } else if (pfx == "42") {
    if (!isMoving) {
      currentPos = 0; targetPos = 0; maxStep = 8000;
      motorSpeedUs = SPEED_MED; motorSpeed = 1;
      stepMode = 2; coilPower = false; reverseDir = true;
      backlashInSteps = 0; backlashOutSteps = 0;
      backlashInEn = false; backlashOutEn = false;
      stepSizeMicrons = 1.0f; stepSizeEn = false;
      tempCoeff = 0; tempComp = false; tempMode = 1;
      eepromScheduleSave();
    }

  // ── :43# Get motorspeed (0=slow,1=med,2=fast)
  } else if (pfx == "43") {
    sendReply('C', (long)motorSpeed);

  // ── :44# Get powerdown enable state
  } else if (pfx == "44") {
    sendReply(_RTOKEN, parkEnabled);

  // ── :45x# Set powerdown enable (no reply)
  } else if (pfx == "45") {
    parkEnabled = (arg.toInt() != 0);

  // ── :46# Get in-out LED enable state (not supported)
  } else if (pfx == "46") {
    sendReply(_RTOKEN, (long)0);

  // ── :47# Set in-out LED enable (not supported, no reply)
  } else if (pfx == "47") {
    // not supported

  // ── :48# Save settings to EEPROM (no reply)
  } else if (pfx == "48") {
    if (!isMoving) { eepromScheduleSave(); }

  // ── :49# Controller signature / hash
  } else if (pfx == "49") {
    tcpSend("ab552efd#");

  // ── :50# Get home position switch enabled (none → 0)
  } else if (pfx == "50") {
    tcpSend("l0#");

  // ── :51# Get WiFi IP address
  } else if (pfx == "51") {
    sendReply(_RTOKEN, WiFi.localIP().toString());

  // ── :52# Get powerdown status (parked)
  } else if (pfx == "52") {
    sendReply(_RTOKEN, parked);

  // ── :53# Get number of display pages (deprecated → 0)
  } else if (pfx == "53") {
    sendReply(_RTOKEN, (long)0);

  // ── :54# Get WiFi SSID
  } else if (pfx == "54") {
    sendReply(_RTOKEN, ssid);

  // ── :55# Get motor speed delay in µs  (token '0')
  } else if (pfx == "55") {
    sendReply('0', (long)motorSpeedUs);

  // ── :56xxxx# Set motor speed delay in µs (no reply)
  } else if (pfx == "56") {
    int us = arg.toInt();
    if (us >= 600 && us <= 5000) { motorSpeedUs = us; eepromScheduleSave(); }

  // ── :57# Get pushbutton enable state (not supported)
  } else if (pfx == "57") {
    sendReply(_RTOKEN, (long)0);

  // ── :58# Set pushbutton enable (not supported, no reply)
  } else if (pfx == "58") {
    // not supported

  // ── :59# Get powerdown time (seconds)
  } else if (pfx == "59") {
    sendReply(_RTOKEN, (long)parkTime);

  // ── :60xx# Set powerdown time in seconds [30-120] (no reply)
  } else if (pfx == "60") {
    parkTime = constrain(arg.toInt(), 30, 120);

  // ── :61x# Set display update on move (no reply)
  } else if (pfx == "61") {
    displayUpdateOnMove = (arg.toInt() != 0);

  // ── :62# Get display update on move
  } else if (pfx == "62") {
    sendReply('L', displayUpdateOnMove);

  // ── :63# Get home position switch status (not supported)
  } else if (pfx == "63") {
    tcpSend("H0#");

  // ── :64xxx# Move relative number of steps (no reply)
  } else if (pfx == "64") {
    if (!isMoving) {
      long steps = arg.toInt();
      long pos = currentPos + steps;
      if (pos < 0) pos = 0;
      if (pos > maxStep) pos = maxStep;
      targetPos = pos;
      isMoving  = (targetPos != currentPos);
    }

  // ── :65# Set jogging state (not supported, no reply)
  } else if (pfx == "65") {
    // not supported on ESP8266

  // ── :66# Get jogging state (not supported → 0)
  } else if (pfx == "66") {
    tcpSend("K0#");

  // ── :67# Set jogging direction (not supported, no reply)
  } else if (pfx == "67") {
    // not supported on ESP8266

  // ── :68# Get jogging direction (not supported → 0)
  } else if (pfx == "68") {
    tcpSend("V0#");

  // ── :69# Get push button steps
  } else if (pfx == "69") {
    sendReply('?', (long)pushbtnSteps);

  // ── :70# Set push button steps (not supported, no reply)
  } else if (pfx == "70") {
    // not supported on ESP8266

  // ── :71xxx# Set delay after move time [0-255] ms (no reply)
  } else if (pfx == "71") {
    delayAfterMove = constrain(arg.toInt(), 0, 255);

  // ── :72# Get delay after move time  (token '3')
  } else if (pfx == "72") {
    sendReply('3', (long)delayAfterMove);

  // ── :73# Set backlash IN enable (not supported on ESP8266, no reply)
  } else if (pfx == "73") {
    // not supported

  // ── :74# Get backlash IN enabled status  (token '4')
  } else if (pfx == "74") {
    sendReply('4', backlashInSteps > 0);

  // ── :75# Set backlash OUT enable (not supported on ESP8266, no reply)
  } else if (pfx == "75") {
    // not supported

  // ── :76# Get backlash OUT enabled status  (token '4')
  } else if (pfx == "76") {
    sendReply('4', backlashOutSteps > 0);

  // ── :77xx# Set backlash IN steps [0-255] (no reply)
  } else if (pfx == "77") {
    backlashInSteps = (byte)constrain(arg.toInt(), 0, 255);
    eepromScheduleSave();

  // ── :78# Get backlash IN steps  (token '6')
  } else if (pfx == "78") {
    sendReply('6', (long)backlashInSteps);

  // ── :79xx# Set backlash OUT steps [0-255] (no reply)
  } else if (pfx == "79") {
    backlashOutSteps = (byte)constrain(arg.toInt(), 0, 255);
    eepromScheduleSave();

  // ── :80# Get backlash OUT steps  (token '7')
  } else if (pfx == "80") {
    sendReply('7', (long)backlashOutSteps);

  // ── :81# Get STALL_VALUE (TMC – not supported)  (token '8')
  } else if (pfx == "81") {
    tcpSend("80#");

  // ── :82# Set STALL_VALUE (not supported, no reply)
  } else if (pfx == "82") {
    // not supported

  // ── :83# Get temperature probe present
  } else if (pfx == "83") {
    sendReply('c', tempProbeOK);

  // ── :84# Reserved (no reply)
  } else if (pfx == "84") {
    // reserved

  // ── :85# Get delay after move enable state
  } else if (pfx == "85") {
    sendReply(_RTOKEN, delayAfterMove > 0);

  // ── :86# Set delay after move enable (not supported, no reply)
  } else if (pfx == "86") {
    // not supported on ESP8266

  // ── :87# Get temperature compensation direction  (token 'k')
  } else if (pfx == "87") {
    sendReply('k', tempCompDir);

  // ── :88x# Set temperature compensation direction (no reply)
  } else if (pfx == "88") {
    tempCompDir = (arg.toInt() != 0);

  // ── :89# Get stepper power (always on)  (token '9')
  } else if (pfx == "89") {
    tcpSend("91#");

  // ── :90xyyyy# Set preset position (no reply)
  } else if (pfx == "90") {
    if (arg.length() >= 2) {
      int slot  = arg.substring(0, 1).toInt();
      long val  = arg.substring(1).toInt();
      if (slot >= 0 && slot <= 9) presets[slot] = val;
    }

  // ── :91x# Get preset position
  } else if (pfx == "91") {
    int slot = constrain(arg.toInt(), 0, 9);
    sendReply(_RTOKEN, presets[slot]);

  // ── :92xxx# Set display page option (no reply)
  } else if (pfx == "92") {
    // not supported on this build

  // ── :93# Get display page option  (token 'l')
  } else if (pfx == "93") {
    tcpSend("l111111#");

  // ── :94# Get TMC interpolation state (not supported → 0)  (token 'n')
  } else if (pfx == "94") {
    tcpSend("n0#");

  // ── :95# Set TMC interpolation state (not supported, no reply)
  } else if (pfx == "95") {
    // not supported

  // ── :96# Get firmware major,minor version
  } else if (pfx == "96") {
    tcpSend("F" + String(FW_VERSION) + "," + String(FW_MINOR_VERSION) + "#");

  // ── :97# Get display type (0=none)  (token 'n')
  } else if (pfx == "97") {
    tcpSend("n0#");

  // ── :98# Get network RSSI
  } else if (pfx == "98") {
    sendReply(_RTOKEN, (long)WiFi.RSSI());

  // ── :99# Set home position switch enable (not supported, no reply)
  } else if (pfx == "99") {
    // not supported

  // ── Extended commands :A0#-:A9# (joysticks, temp probe, ALPACA)
  } else if (pfx == "A0") {
    // Get joystick1 enable (not supported)
    sendReply(_RTOKEN, (long)0);
  } else if (pfx == "A1") {
    // Set joystick1 enable (not supported, no reply)
    ;
  } else if (pfx == "A2") {
    // Get joystick2 enable (not supported)
    sendReply(_RTOKEN, (long)0);
  } else if (pfx == "A3") {
    // Set joystick2 enable (not supported, no reply)
    ;
  } else if (pfx == "A4") {
    // Get temp probe enabled state
    sendReply(_RTOKEN, tempProbeOK);
  } else if (pfx == "A5") {
    // Set temp probe enabled state (no reply)
    ;
  } else if (pfx == "A6") {
    // Get ALPACA server enabled (not supported)
    sendReply(_RTOKEN, (long)0);
  } else if (pfx == "A7") {
    // Set ALPACA server enabled (not supported, no reply)
    ;
  } else if (pfx == "A8") {
    // Get ALPACA server running status (not supported)
    sendReply(_RTOKEN, (long)0);
  } else if (pfx == "A9") {
    // Set ALPACA server start/stop (not supported, no reply)
    ;

  // ── Extended commands :B0#-:B9# (Web server, Management server, config files)
  } else if (pfx == "B0") {
    // Get Web server enabled (always running)
    sendReply(_RTOKEN, (long)1);
  } else if (pfx == "B1") {
    // Set Web server enabled (no reply)
    ;
  } else if (pfx == "B2") {
    // Get Web server running status (always running)
    sendReply(_RTOKEN, (long)1);
  } else if (pfx == "B3") {
    // Set Web server start/stop (no reply)
    ;
  } else if (pfx == "B4") {
    // Get Management server enabled (not applicable)
    sendReply(_RTOKEN, (long)0);
  } else if (pfx == "B5") {
    // Set Management server enabled (no reply)
    ;
  } else if (pfx == "B6") {
    // Get Management server running status
    sendReply(_RTOKEN, (long)0);
  } else if (pfx == "B7") {
    // Set Management server start/stop (no reply)
    ;
  } else if (pfx == "B8") {
    // Get controller config json (not available in standalone build)
    sendReply(_RTOKEN, (long)0);
  } else if (pfx == "B9") {
    // Get board config json (not available in standalone build)
    sendReply(_RTOKEN, (long)0);

  // ── Extended commands :C0#-:C5# (OTA, brightness)
  } else if (pfx == "C0") {
    // Get OTA state (not supported)
    sendReply(_RTOKEN, (long)0);
  } else if (pfx == "C1") {
    // Set OTA state (not supported, no reply)
    ;
  } else if (pfx == "C2") {
    // Get OTA running status (not supported)
    sendReply(_RTOKEN, (long)0);
  } else if (pfx == "C3") {
    // Set OTA start/stop (not supported, no reply)
    ;
  } else if (pfx == "C4") {
    // Get brightness level (always max, no hardware support)
    sendReply(_RTOKEN, (long)255);
  } else if (pfx == "C5") {
    // Set brightness (not supported, no reply)
    ;

  } else {
    Serial.print("TCP: unknown CMD: "); Serial.println(pfx);
  }
}

// ============================================================
//  TCP INPUT HANDLER
//  Protocol: each command starts with ':' and ends with '#'
// ============================================================
unsigned long lastClientActivity = 0;
const unsigned long CLIENT_TIMEOUT_MS = 10000;

void handleTCP() {
  // accept new connection
  if (tcpServer.hasClient()) {
    WiFiClient newClient = tcpServer.available();
    if (newClient) {
      if (tcpClient && tcpClient.connected()) {
        tcpClient.stop();
        Serial.println("TCP: replaced old client");
      }
      tcpClient = newClient;
      tcpClient.setNoDelay(true);
      inputBuffer = "";
      lastClientActivity = millis();
      Serial.println("TCP: connected from " + tcpClient.remoteIP().toString());
    }
  }

  if (!tcpClient) return;

  if (!tcpClient.connected()) {
    tcpClient.stop();
    inputBuffer = "";
    Serial.println("TCP: client disconnected");
    return;
  }
  if (millis() - lastClientActivity > CLIENT_TIMEOUT_MS) {
    tcpClient.stop();
    inputBuffer = "";
    Serial.println("TCP: idle timeout, dropped");
    return;
  }

  int cmdCount = 0;
  while (tcpClient.available() && cmdCount < 8) {
    char c = tcpClient.read();
    lastClientActivity = millis();

    if (c == ':') {
      inputBuffer = "";
    } else if (c == _EOC) {
      if (inputBuffer.length() > 0) {
        dispatchCommand(inputBuffer);
        inputBuffer = "";
        cmdCount++;
      }
    } else if (c >= 32) {
      inputBuffer += c;
      if (inputBuffer.length() > 64) inputBuffer = "";
    }
  }
}

// ============================================================
//  TEMPERATURE COMPENSATION (called each second)
// ============================================================
unsigned long lastTempCheck = 0;
void updateTempComp() {
  if (!tempComp || !tempProbeOK) return;
  unsigned long now = millis();
  if (now - lastTempCheck < 1000) return;
  lastTempCheck = now;

  float delta = temperature - lastCompTemp;
  if (fabs(delta) >= 1.0f) {
    long steps = (long)(delta * tempCoeff);
    if (tempCompDir) steps = -steps;
    if (steps != 0) moveRelative(steps);
    lastCompTemp = temperature;
  }
}

// ============================================================
//  WEB INTERFACE
// ============================================================
#include "html_page.h"

void webHandleRoot() {
  webServer.send_P(200, "text/html", HTML_PAGE);
}

void webHandleStatus() {
  String json = "{";
  json += "\"pos\":"     + String(currentPos)   + ",";
  json += "\"target\":"  + String(targetPos)    + ",";
  json += "\"moving\":"  + String(isMoving)     + ",";
  json += "\"speed\":"   + String(motorSpeedUs) + ",";
  json += "\"temp\":"    + String(temperature)  + ",";
  json += "\"maxstep\":" + String(maxStep)      + ",";
  json += "\"rssi\":"    + String(WiFi.RSSI())  + ",";
  json += "\"ip\":\""    + WiFi.localIP().toString() + "\"";
  json += "}";
  webServer.send(200, "application/json", json);
}

void webHandleMoveTo() {
  if (webServer.hasArg("pos")) moveTo(webServer.arg("pos").toInt());
  webServer.send(200, "text/plain", "OK");
}

void webHandleStep() {
  if (webServer.hasArg("steps")) moveRelative(webServer.arg("steps").toInt());
  webServer.send(200, "text/plain", "OK");
}

void webHandleStop() {
  stopMotor();
  webServer.send(200, "text/plain", "OK");
}

void webHandleZero() {
  setCurrentPos(0);
  webServer.send(200, "text/plain", "OK");
}

void webHandleHome() {
  homeMotor();
  webServer.send(200, "text/plain", "OK");
}

void webHandleSave() {
  eepromScheduleSave();
  webServer.send(200, "text/plain", "OK");
}

void webHandleReset() {
  currentPos = 0; targetPos = 0; maxStep = 8000;
  motorSpeedUs = SPEED_MED; motorSpeed = 1;
  stepMode = 2; coilPower = false; reverseDir = true;
  eepromScheduleSave();
  webServer.send(200, "text/plain", "OK");
}

void webHandleSetSpeed() {
  if (webServer.hasArg("us")) {
    int us = webServer.arg("us").toInt();
    if (us >= 600 && us <= 5000) { motorSpeedUs = us; eepromScheduleSave(); }
  }
  webServer.send(200, "text/plain", "OK");
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n\n=== MyFocuserPro2 ESP8266 v3.00 ===");

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  motorOff();

  EEPROM.begin(EEPROM_SIZE);
  eepromLoad();
  Serial.printf("Loaded pos=%ld  maxStep=%ld\n", currentPos, maxStep);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 40) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi failed – running in AP mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  }

  webServer.on("/",         webHandleRoot);
  webServer.on("/status",   webHandleStatus);
  webServer.on("/moveto",   webHandleMoveTo);
  webServer.on("/step",     webHandleStep);
  webServer.on("/stop",     webHandleStop);
  webServer.on("/zero",     webHandleZero);
  webServer.on("/home",     webHandleHome);
  webServer.on("/save",     webHandleSave);
  webServer.on("/reset",    webHandleReset);
  webServer.on("/setspeed", webHandleSetSpeed);
  ElegantOTA.begin(&webServer);
  webServer.begin();

  tcpServer.begin();
  tcpServer.setNoDelay(true);

  Serial.printf("MyFocuserPro2 TCP port : %d\n", tcpPort);
  Serial.printf("Web interface          : http://%s\n",
                WiFi.localIP().toString().c_str());
}

// ============================================================
//  DEFERRED EEPROM WRITE
// ============================================================
bool eepromDirty = false;
unsigned long eepromDirtyAt = 0;
const unsigned long EEPROM_DEFER_MS = 3000;

void eepromScheduleSave() {
  eepromDirty   = true;
  eepromDirtyAt = millis();
}

void eepromFlushIfDue() {
  if (!eepromDirty) return;
  if (millis() - eepromDirtyAt < EEPROM_DEFER_MS) return;
  eepromSave();
  eepromDirty = false;
}

// ============================================================
//  MAIN LOOP
// ============================================================
void loop() {
  ElegantOTA.loop();
  webServer.handleClient();
  handleTCP();
  motorLoop();
  updateTempComp();
  eepromFlushIfDue();
}
