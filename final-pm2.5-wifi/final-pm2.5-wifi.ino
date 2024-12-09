#include <ArduinoJson.h>
int incomingByte = 0; // for incoming serial data

const int MAX_FRAME_LEN = 64;
char frameBuf[MAX_FRAME_LEN];
int detectOff = 0;
int frameLen = MAX_FRAME_LEN;
bool inFrame = false;
char printbuf[256];

const bool DEBUG = false;

uint16_t calcChecksum = 0;

struct PMSA003_framestruct {
  uint8_t  frameHeader[2];
  uint16_t frameLen = MAX_FRAME_LEN;
  uint16_t concPM1_0_CF1;
  uint16_t concPM2_5_CF1;
  uint16_t concPM10_0_CF1;
  uint16_t concPM1_0_amb;
  uint16_t concPM2_5_amb;
  uint16_t concPM10_0_amb;
  uint16_t rawGt0_3um;
  uint16_t rawGt0_5um;
  uint16_t rawGt1_0um;
  uint16_t rawGt2_5um;
  uint16_t rawGt5_0um;
  uint16_t rawGt10_0um;
  uint8_t  version;
  uint8_t  errorCode;
  uint16_t checksum;
} thisFrame;



// your network name also called SSID
char ssid[] = "SSID";
// your network password
char password[] = "PASSWORD";


void printWifiStatus();

bool PMSA003_read() {
//   Serial2.begin(9600);   
  bool packetReceived = false;
  while (!packetReceived) {
    if (Serial2.available() > 32) {
      int drain = Serial2.available();
      if (DEBUG) {
        Serial.print("-- Draining buffer: ");
        Serial.println(Serial2.available(), DEC);
      }
      for (int i = drain; i > 0; i--) {
        Serial2.read();
      }
    }
    if (Serial2.available() > 0) {
      if (DEBUG) {
        Serial.print("-- Available: ");
        Serial.println(Serial2.available(), DEC);
      }
      incomingByte = Serial2.read();
      if (DEBUG) {
        Serial.print("-- READ: ");
        Serial.println(incomingByte, HEX);
      }
      if (!inFrame) {
        if (incomingByte == 0x42 && detectOff == 0) {
          frameBuf[detectOff] = incomingByte;
          thisFrame.frameHeader[0] = incomingByte;
          calcChecksum = incomingByte; // Checksum init!
          detectOff++;
        }
        else if (incomingByte == 0x4D && detectOff == 1) {
          frameBuf[detectOff] = incomingByte;
          thisFrame.frameHeader[1] = incomingByte;
          calcChecksum += incomingByte;
          inFrame = true;
          detectOff++;
        }
        else {
//               Serial.print("-- Frame syncing... ");
//             Serial.print(incomingByte, HEX);
          // Serial.println();
        }
      }
      else {
        frameBuf[detectOff] = incomingByte;
        calcChecksum += incomingByte;
        detectOff++;
        uint16_t val = frameBuf[detectOff - 1] + (frameBuf[detectOff - 2] << 8);
        switch (detectOff) {
          case 4:
            thisFrame.frameLen = val;
            frameLen = val + detectOff;
            break;
          case 6:
            thisFrame.concPM1_0_CF1 = val;
            break;
          case 8:
            thisFrame.concPM2_5_CF1 = val;
            break;
          case 10:
            thisFrame.concPM10_0_CF1 = val;
            break;
          case 12:
            thisFrame.concPM1_0_amb = val;
            break;
          case 14:
            thisFrame.concPM2_5_amb = val;
            break;
          case 16:
            thisFrame.concPM10_0_amb = val;
            break;
          case 18:
            thisFrame.rawGt0_3um = val;
            break;
          case 20:
            thisFrame.rawGt0_5um = val;
            break;
          case 22:
            thisFrame.rawGt1_0um = val;
            break;
          case 24:
            thisFrame.rawGt2_5um = val;
            break;
          case 26:
            thisFrame.rawGt5_0um = val;
            break;
          case 28:
            thisFrame.rawGt10_0um = val;
            break;
          case 29:
            val = frameBuf[detectOff - 1];
            thisFrame.version = val;
            break;
          case 30:
            val = frameBuf[detectOff - 1];
            thisFrame.errorCode = val;
            break;
          case 32:
            thisFrame.checksum = val;
            calcChecksum -= ((val >> 8) + (val & 0xFF));
            break;
          default:
            break;
        }

        if (detectOff >= frameLen) {
          /*
            sprintf(printbuf, "PMS7003 ");
            sprintf(printbuf, "%s[%02x %02x] (%04x) ", printbuf,
                thisFrame.frameHeader[0], thisFrame.frameHeader[1], thisFrame.frameLen);
            sprintf(printbuf, "%sCF1=[%04d %04d %04d] ", printbuf,
                thisFrame.concPM1_0_CF1, thisFrame.concPM2_5_CF1, thisFrame.concPM10_0_CF1);
            sprintf(printbuf, "%samb=[%04x %04x %04x] ", printbuf,
                thisFrame.concPM1_0_amb, thisFrame.concPM2_5_amb, thisFrame.concPM10_0_amb);
            sprintf(printbuf, "%sraw=[%04x %04x %04x %04x %04x %04x] ", printbuf,
                thisFrame.rawGt0_3um, thisFrame.rawGt0_5um, thisFrame.rawGt1_0um,
                thisFrame.rawGt2_5um, thisFrame.rawGt5_0um, thisFrame.rawGt10_0um);
            sprintf(printbuf, "%sver=%02x err=%02x ", printbuf,
                thisFrame.version, thisFrame.errorCode);
            sprintf(printbuf, "%scsum=%04x %s xsum=%04x", printbuf,
                thisFrame.checksum, (calcChecksum == thisFrame.checksum ? "==" : "!="), calcChecksum);
          */
          //Serial.println(printbuf);

          packetReceived = true;
          detectOff = 0;
          inFrame = false;
        }
      }
    }
  }
//Serial2.end();
  return (calcChecksum == thisFrame.checksum);
}



void setup() {
  Serial.begin(115200);      // initialize serial communication

  Serial2.begin(9600); 
 


}



void loop() {
  int i = 0;


  if (!PMSA003_read()) {
    Serial.println("PMSA003 not ready, wait..");
    delay(4000);
  }
    Serial.print("PM 1.0 (ug/m3): ");
  Serial.print(thisFrame.concPM1_0_CF1);
  Serial.println();
   Serial.print("PM 2.5 (ug/m3): ");
  Serial.print(thisFrame.concPM2_5_CF1); 
  Serial.println();
    Serial.print("PM 10.0 (ug/m3): ");
    Serial.print(thisFrame.concPM10_0_CF1);
  Serial.println();  
//  StaticJsonBuffer<200> jsonBuffer;
//  JsonObject &root = jsonBuffer.createObject();
//  root["pm1.0"] = thisFrame.concPM1_0_CF1;
//  root["pm2.5"] = thisFrame.concPM2_5_CF1;
//  root["pm10"] = thisFrame.concPM10_0_CF1;
//
//  //                    root["pm1.0_at"] = thisFrame.concPM1_0_amb;
//  //                    root["pm2.5_at"] = thisFrame.concPM2_5_amb;
//  //                    root["pm10_at"] = thisFrame.concPM10_0_amb;
//
//  root["ver"] = thisFrame.version;
//  root["error-code"] = thisFrame.errorCode;
//
//
//  if (thisFrame.checksum == calcChecksum) {
//    root["pass"] = true;
//  } else {
//    root["pass"] = false;
//  }
//
//  root["temperature"] = int(temperature);
//  root["humidity"] = int(relative_humidity);

//  root["SSID"] = WiFi.SSID();
//  root["IP"] = String(WiFi.localIP());
//  root["RSSI"] = int(WiFi.RSSI());

//  root.printTo(Serial);







}
