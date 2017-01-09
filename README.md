# PeerIOSerialControl
Arduino Peer IO Control through Serial Communications, like XBEE AT mode or direct board to board

# Usage
- Create Serial Connection using XBEE modules for Serial Communication or connecting RX-TX, TX-RX on two boards
- Start an Arduino Sketch
- Download the .zip file and use Sketch IDE to add it as a library
```
#include "PeerIOSerialControl.h"

// Setup a Software Serial for XBEE (Allows Debug)
#include <SoftwareSerial.h>
SoftwareSerial IOSerial(SS_RX_PIN,SS_TX_PIN);   // ( rxPin, txPin )
PeerIOSerialControl XBee(1,IOSerial,Serial);    // ArduinoID, IOSerial, DebugSerial

void setup(){
    IOSerial.begin(9600);
    Serial.begin(9600);
    XBee.Timeout(WAIT_REPLY_MS);
    XBee.TargetArduinoID(2);
}

void loop(){
  XBee.Available(); // Check for Communication
  
  Serial.println(XBee.analogReadB(5);
  Serial.println(XBee.digitalReadB(10);
  XBee.analogWriteB(5, 200);
  XBee.digitalWriteB(10, HIGH);
}
```
# Features
- Each Call is validated by an echo response
- Calls can be used as 'B' blocking or 'NB' non-blocking where a reply is queried later
- Uses only 2-bytes for digital and 4-bytes for analog calls and replies
