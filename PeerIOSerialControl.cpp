/************************************************************************//**
 * @file PeerIOSerialControl.cpp
 * @brief Arduino Peer IO-Control through Serial Port Communications.
 * @authors 
 *    tgit23        1/2017       Original
 ******************************************************************************/
#include "PeerIOSerialControl.h"
#define ID_MASK 0x0F    // Bytes[0] [0000 1111] ArduinoID ( 0-15 )
#define REPLY_BIT 4     // Bytes[0] [0001 0000] Reply-1, Send-0
#define RW_BIT 5        // Bytes[0] [0010 0000] Read-1, Write-0
#define DA_BIT 6        // Bytes[0] [0100 0000] Digital-1, Analog-0
#define DPIN_MASK 0x3F  // Bytes[1] [0011 1111] Digital Pins ( 0 - 63 )
#define APIN_MASK 0x7F  // Bytes[1] [0111 1111] Analog Pins ( 0 - 127 )
#define HL_BIT 6        // Bytes[1] [0100 0000] High-1, Low-0
#define END_BIT 7       // Bytes[?} [1000 0000] Any set 8th bit flags END-OF-PACKET

//-----------------------------------------------------------------------------------------------------
// Initializer
//-----------------------------------------------------------------------------------------------------
PeerIOSerialControl::PeerIOSerialControl(int ThisArduinoID, Stream &CommunicationPort, Stream &DebugPort) {
  ArduinoID = ThisArduinoID;
  COMPort = &CommunicationPort;
  DBPort = &DebugPort;
}


void PeerIOSerialControl::digitalWriteB(uint8_t Pin, uint8_t Value) {
  int packetID = SendPacket(DIGITAL,WRITE,Pin,Value);
  unsigned long Start = millis();
  do {
    if ( Available() ) break;
  } while ( (millis() - Start) < BlockingTimeoutMS );
}
int PeerIOSerialControl::digitalReadB(uint8_t Pin) {
  int packetID = SendPacket(DIGITAL,READ,Pin);
  unsigned long Start = millis();
  do {
    if ( Available() ) return GetReply(packetID);
  } while ( (millis() - Start) < BlockingTimeoutMS );
  return -1;
}
int PeerIOSerialControl::analogReadB(uint8_t Pin) {
  int packetID = SendPacket(ANALOG,READ,Pin);
  unsigned long Start = millis();
  do {
    if ( Available() ) return GetReply(packetID);
  } while ( (millis() - Start) < BlockingTimeoutMS );
  return -1;
}
void PeerIOSerialControl::analogWriteB(uint8_t Pin, int Value) {
  int packetID = SendPacket(ANALOG,WRITE,Pin,Value);
  unsigned long Start = millis();
  do {
    if ( Available() ) break;
  } while ( (millis() - Start) < BlockingTimeoutMS );
}


int PeerIOSerialControl::digitalWriteNB(uint8_t Pin, uint8_t Value) {
  return SendPacket(DIGITAL,WRITE,Pin,Value);
}
int PeerIOSerialControl::digitalReadNB(uint8_t Pin) {
  return SendPacket(DIGITAL,READ,Pin);
}
int PeerIOSerialControl::analogReadNB(uint8_t Pin) {
  return SendPacket(ANALOG,READ,Pin);
}
int PeerIOSerialControl::analogWriteNB(uint8_t Pin, int Value) {
  return SendPacket(ANALOG,WRITE,Pin,Value);
}

void PeerIOSerialControl::TargetArduinoID(int ID) {
  iTargetArduinoID = ID;
}
int PeerIOSerialControl::TargetArduinoID() {
  return iTargetArduinoID;
}

void PeerIOSerialControl::Timeout(int milliseconds) {
  BlockingTimeoutMS = milliseconds;
}
int PeerIOSerialControl::Timeout() {
  return BlockingTimeoutMS;
}
void PeerIOSerialControl::VirtualPin(int Pin, int Value) {
  if ( Pin > 63 && Pin < 128 ) iVirtualPin[Pin-64] = Value;
}
int PeerIOSerialControl::VirtualPin(int Pin) {
  if ( Pin > 63 && Pin < 128 ) return iVirtualPin[Pin-64];
}
  
//-----------------------------------------------------------------------------------------------------
// SendPacket()
//-----------------------------------------------------------------------------------------------------
int PeerIOSerialControl::SendPacket(bool DA, bool RW, byte Pin, int Value) {
  DBL(("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"));
  DBL(("SendPacket()"));
  byte SBytes[4] = { 0,0,0,0 };
  if ( DA ) bitSet(SBytes[0],DA_BIT);
  if ( RW ) bitSet(SBytes[0],RW_BIT);
  SBytes[0] = SBytes[0] | (iTargetArduinoID & ID_MASK);
  
  if ( DA == DIGITAL ) {
    SBytes[1] = (Pin & DPIN_MASK);                             // 6-bit Pin for Digital
    if ( RW != READ ) bitWrite(SBytes[1],HL_BIT,(Value>0));    // Digital Write - Set H/L Bit
    bitSet(SBytes[1],END_BIT);                                 // Digital only uses 2-Bytes  
  
  } else {
    SBytes[1] = (Pin & APIN_MASK);                             // 7-bit Pin for Analog
    if ( Value > -1 ) {
      Value = ValueTo7bits(Value);                            // Conversion marks the END_BIT
      SBytes[2] = lowByte(Value);
      SBytes[3] = highByte(Value);
    } else {
      bitSet(SBytes[1],END_BIT);                               // Set END_BIT if not sending Value
    }
  }  
  
  DB(("SendBytes( "));
  COMPort->write(SBytes[0]);
  COMPort->write(SBytes[1]);
  if ( SBytes[2] != 0 ) COMPort->write(SBytes[2]);
  if ( SBytes[3] != 0 ) COMPort->write(SBytes[3]);
  DB((SBytes[0],HEX));DBC;DB((SBytes[1],HEX));DBC;DB((SBytes[2],HEX));DBC;DB((SBytes[3],HEX));DBL((" )"));
  
  return ( SBytes[1] << 8 ) | SBytes[0];                        // Return Bytes 0, 1 for tracking
}

//-----------------------------------------------------------------------------------------------------
// GetReply()
//-----------------------------------------------------------------------------------------------------
int PeerIOSerialControl::GetReply(int packetID) {
  DB(("GetReply("));DB((packetID,HEX));DBL((")"));
  int UseRBI = RBI - 1;
  if ( UseRBI < 1 ) UseRBI = 0;
  
  // Find the Reply for this Command
  if ( packetID != -1 ) {
    byte Byte0 = lowByte(packetID);
    byte Byte1 = highByte(packetID);
    int i = UseRBI; UseRBI = -1;
    do {
      DB(("\tRBytes["));DB((i));DB(("][0] = "));DBL((RBytes[i][0],HEX));
      DB(("\tRBytes["));DB((i));DB(("][1] = "));DBL((RBytes[i][1],HEX));
      if ( (Byte0 & 0xEF) == (RBytes[i][0] & 0xEF) && 
           (Byte1 & 0x3F) == (RBytes[i][1] & 0x3F) ) { 
                UseRBI = i; 
                break;
      }
      i--; if ( i < 0 ) i = 9;
    } while ( i != RBI );
  }

  if ( UseRBI < 0 ) return -1;
  if ( bitRead(RBytes[UseRBI][0],RW_BIT) == WRITE ) { 
    return 0;                                         // Okay Status for a WRITE COMMAND
  } else {
    if ( bitRead(RBytes[UseRBI][0],DA_BIT) == DIGITAL ) {              // Value of the Reply
      return bitRead(RBytes[UseRBI][1],HL_BIT);
    } else {
      return ValueTo8bits(RBytes[UseRBI][2],RBytes[UseRBI][3]);
    }
  }
}

//-----------------------------------------------------------------------------------------------------
// ValueTo8bits()   ValueTo7bits()
//  - Encodes / Decodes numeric values into (2)7-bit bytes for the 14-bit 'AV' (Analog Value) Bytes
//  - This function automatically attaches the END_BIT at the appropriate location.
//-----------------------------------------------------------------------------------------------------
int PeerIOSerialControl::ValueTo8bits(byte lByte, byte hByte) {
  bitClear(hByte,7);                        // Clear any End-Of-Packet flag
  bitWrite(lByte,7,bitRead(hByte,0));       // Transfer hByte<0> onto lByte<7>
  return (hByte<<7) | lByte;                // Left shift 7 overwrites lByte<7>
}
int PeerIOSerialControl::ValueTo7bits(int From8BitValue) {
  byte lByte = lowByte(From8BitValue);
  byte hByte = highByte(From8BitValue);
  if ( From8BitValue > 0x3FFF ) return -1;  //  Value is too big for a 14-bit Value
  hByte = hByte << 1;                       // Make Room on hByte for bit-7 of lByte
  bitWrite(hByte,0,bitRead(lByte,7));       // Transfer lByte<7> onto hByte<0>
  if ( From8BitValue > 0x7F ) { 
    bitSet(hByte,7);        // Value > 7-bits so Set 'END_BIT' @ hByte
    bitClear(lByte,7);
  } else {
    bitSet(lByte,7);        // Value <= 7-bits so Set 'END_BIT' @ lByte
    bitClear(hByte,7);
  }
  return (hByte<<8) | lByte;        
}

//-----------------------------------------------------------------------------------------------------
// DecodePacket()
//-----------------------------------------------------------------------------------------------------
void PeerIOSerialControl::DecodePacket(long lPacket) {
  byte Byte0; byte Byte1; byte Byte2; byte Byte3;
  if ( lPacket = -1 ) { 
    Byte0=Bytes[0];Byte1=Bytes[1];Byte2=Bytes[2];Byte3=Bytes[3];
  } else {
    Byte0 = ( lPacket >> 24 ) & 0xFF;
    Byte1 = ( lPacket >> 16 ) & 0xFF;
    Byte2 = ( lPacket >> 8 ) & 0xFF;
    Byte3 = lPacket & 0xFF;  
  }
  DB(("D/A Flag = "));if ( bitRead(Byte0,DA_BIT) ) { DBL(("DIGITAL")); } else { DBL(("ANALOG")); }
  DB(("R/W Flag = "));if ( bitRead(Byte0,RW_BIT) ) { DBL(("READ")); } else { DBL(("WRITE")); }
  DB(("S/R Flag = "));if ( bitRead(Byte0,REPLY_BIT) ) { DBL(("REPLY")); } else { DBL(("SEND")); }
  DB(("Arduino ID = "));DBL(( (Byte0 & ID_MASK) ));
  if ( bitRead(Byte0,DA_BIT) ) { 
    DB(("H/L Flag = "));
    if ( bitRead(Byte0,HL_BIT) ) { DBL(("HIGH")); } else { DBL(("LOW")); }
    DB(("PIN = "));DBL(( (Byte1 & DPIN_MASK) ));
  } else {
    DB(("Value = "));DBL(( ValueTo8bits(Byte2, Byte3) ));
    DB(("PIN = "));DBL(( (Byte1 & APIN_MASK) ));
  }
}
  
//-----------------------------------------------------------------------------------------------------
// ProcessPacket()
//-----------------------------------------------------------------------------------------------------
void PeerIOSerialControl::ProcessPacket() {
  DB(("ProcessPacket( "));
  DB((Bytes[0],HEX));DBC;DB((Bytes[1],HEX));DBC;DB((Bytes[2],HEX));DBC;DB((Bytes[3],HEX));
  DB((" ) - "));
  
  // REPLY PACKET RECEIVED
  if ( bitRead(Bytes[0],REPLY_BIT) ) {
    DBL(("Packet Type REPLY"));
    for ( int i=0;i<4;i++ ) RBytes[RBI][i] = Bytes[i];   // Put Replies in RBytes Buffer
#if defined(DEBUG)
  #if DEBUG>0
      DecodePacket();
  #endif
#endif


  // COMMAND PACKET RECEIVED
  } else if ( (Bytes[0] & ID_MASK) == ArduinoID ) {
    
    DBL(("Packet Type SEND"));
    // DIGITAL
    if ( bitRead(Bytes[0],DA_BIT) == DIGITAL ) {
      int pin = Bytes[1] & DPIN_MASK;
      if ( bitRead(Bytes[0],RW_BIT) == READ ) {
        DB(("digitalRead("));DB((pin));DBL((")"));
        bitWrite(Bytes[1],HL_BIT,digitalRead(pin));
      } else {
        DB(("digitalWrite("));DB((pin));DB((","));DB((bitRead(Bytes[1],HL_BIT)));DBL((")"));
        digitalWrite(pin,bitRead(Bytes[1],HL_BIT));
      }
      bitSet(Bytes[1],END_BIT);

    // ANALOG
    } else {
      int pin = Bytes[1] & APIN_MASK;
      int val = 0;
      if ( bitRead(Bytes[0],RW_BIT) == READ ) {
        DB(("analogRead("));DB((pin));DBL((")"));
        if ( pin > 63 && pin < 128 ) { 
          val = ValueTo7bits(iVirtualPin[pin-64]); 
        } else { 
          val = ValueTo7bits(analogRead(pin)); 
        }
        Bytes[2] = lowByte(val);
        Bytes[3] = highByte(val);
      } else { 
        DB(("analogWrite("));DB((pin));DB((","));DB((ValueTo8bits(Bytes[2],Bytes[3])));DBL((")"));
        if ( pin > 63 && pin < 128 ) { 
          iVirtualPin[pin-64] = ValueTo8bits(Bytes[2],Bytes[3]); 
        } else { 
          analogWrite(pin,ValueTo8bits(Bytes[2],Bytes[3]));
        }
      }
    }
    
    // Send out the Reply Packet
    bitSet(Bytes[0],REPLY_BIT); // Set the Reply Bit
    DB(("SendBytes( "));
    COMPort->write(Bytes[0]);
    COMPort->write(Bytes[1]);
    if ( Bytes[2] != 0 ) COMPort->write(Bytes[2]);
    if ( Bytes[3] != 0 ) COMPort->write(Bytes[3]);
    DB((Bytes[0],HEX));DBC;DB((Bytes[1],HEX));DBC;DB((Bytes[2],HEX));DBC;DB((Bytes[3],HEX));DBL((" )"));
  }   

}

//-----------------------------------------------------------------------------------------------------
// Available()
//-----------------------------------------------------------------------------------------------------
bool PeerIOSerialControl::Available() {
  // Receive Bytes
  while(COMPort->available() > 0) {
    Bytes[idx] = COMPort->read();
    if ( Bytes[idx] != -1 ) {
      //DBL((Bytes[idx],HEX));
      if ( bitRead(Bytes[idx],END_BIT) ) {
        DBL(("-----------------------------------------------------------"));
        DB(("Packet Received @ Size: "));DBL((idx+1));
        bitClear(Bytes[idx],END_BIT);           // Clear the END_BIT
        for(int i=(idx+1);i<4;i++) Bytes[i]=0;  // Clear unused bytes
        idx = 0;
        ProcessPacket();
        return true;
      } else {
        idx++;
      }
    }
  }
}

