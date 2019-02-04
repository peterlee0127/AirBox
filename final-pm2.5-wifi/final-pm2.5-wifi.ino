/* 
 * Defaultly disabled. More details: https://docs.particle.io/reference/firmware/photon/#system-thread 
 */
// SYSTEM_THREAD(ENABLED);

/*
 * Defaultly disabled. If BLE setup is enabled, when the Duo is in the Listening Mode, it will de-initialize and re-initialize the BT stack.
 * Then it broadcasts as a BLE peripheral, which enables you to set up the Duo via BLE using the RedBear Duo App or customized
 * App by following the BLE setup protocol: https://github.com/redbear/Duo/blob/master/docs/listening_mode_setup_protocol.md#ble-peripheral 
 * 
 * NOTE: If enabled and upon/after the Duo enters/leaves the Listening Mode, the BLE functionality in your application will not work properly.
 */
//BLE_SETUP(ENABLED);

/*
 * SYSTEM_MODE:
 *     - AUTOMATIC: Automatically try to connect to Wi-Fi and the Particle Cloud and handle the cloud messages.
 *     - SEMI_AUTOMATIC: Manually connect to Wi-Fi and the Particle Cloud, but automatically handle the cloud messages.
 *     - MANUAL: Manually connect to Wi-Fi and the Particle Cloud and handle the cloud messages.
 *     
 * SYSTEM_MODE(AUTOMATIC) does not need to be called, because it is the default state. 
 * However the user can invoke this method to make the mode explicit.
 * Learn more about system modes: https://docs.particle.io/reference/firmware/photon/#system-modes .
 */
#if defined(ARDUINO) 
SYSTEM_MODE(SEMI_AUTOMATIC); 
#endif


#include <Wire.h>
#include <SeeedOLED.h>

#include <ArduinoJson.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#define DHTTYPE           DHT11     // DHT 11 
#define DHTPIN 2
DHT_Unified dht(DHTPIN, DHTTYPE);

int relative_humidity = 0;
int temperature = 0;

int incomingByte = 0; // for incoming serial data
int refreshOLEDCount = 0;

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
char ssid[] = "PeterAP";
// your network password
char password[] = "peter810127";

TCPServer server = TCPServer(80);

#define BLUE_LED  7

void printWifiStatus();

bool PMSA003_read() {
   
    Serial1.begin(9600);
    bool packetReceived = false;
    while (!packetReceived) {
        if (Serial1.available() > 32) {
            int drain = Serial1.available();
            if (DEBUG) {
                Serial.print("-- Draining buffer: ");
                Serial.println(Serial1.available(), DEC);
            }
            for (int i = drain; i > 0; i--) {
                Serial1.read();
            }
        }
        if (Serial1.available() > 0) {
            if (DEBUG) {
                Serial.print("-- Available: ");
                Serial.println(Serial1.available(), DEC);
            }
            incomingByte = Serial1.read();
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
               //     Serial.print("-- Frame syncing... ");
                 //   Serial.print(incomingByte, HEX);
                    if (DEBUG) {
                    }
                   // Serial.println();
                }
            }
            else {
                frameBuf[detectOff] = incomingByte;
                calcChecksum += incomingByte;
                detectOff++;
                uint16_t val = frameBuf[detectOff-1]+(frameBuf[detectOff-2]<<8);
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
                        val = frameBuf[detectOff-1];
                        thisFrame.version = val;
                        break;
                    case 30:
                        val = frameBuf[detectOff-1];
                        thisFrame.errorCode = val;
                        break;
                    case 32:
                        thisFrame.checksum = val;
                        calcChecksum -= ((val>>8)+(val&0xFF));
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
    Serial1.end();
    return (calcChecksum == thisFrame.checksum);
}

void setup() {
  Serial.begin(115200);      // initialize serial communication
  
  Wire.begin();
  SeeedOled.init();  //initialze SEEED OLED display

  SeeedOled.clearDisplay();          //clear the screen and set start position to top left corner
  SeeedOled.setNormalDisplay();      //Set display to normal mode (i.e non-inverse mode)
  SeeedOled.setPageMode();           //Set addressing mode to Page Mode
  SeeedOled.setTextXY(0,0); 
  SeeedOled.putString("init..."); //Print the String

  dht.begin();
  
 printWifiStatus();

  // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
  WiFi.on();
  WiFi.setCredentials(ssid,password);
  WiFi.connect();
  
  while ( WiFi.connecting()) {
    // print dots while we wait to connect
    delay(300);
  }
  
  
  IPAddress localIP = WiFi.localIP();
  while (localIP[0] == 0) {
    localIP = WiFi.localIP();
    Serial.println("waiting for an IP address");
    delay(1000);
  }

  
  // you're connected now, so print out the status  
  printWifiStatus();

  Serial.println("Starting webserver on port 80");
  server.begin();                           // start the web server on port 80

}

void DHT_read() {
   sensors_event_t temp_event;  
  dht.temperature().getEvent(&temp_event);
  if (isnan(temp_event.temperature)) {
    //Serial.println("Error reading temperature!");
  }
  else {
    //Serial.print("Temperature: ");
    //Serial.print(temp_event.temperature);
    temperature = int(temp_event.temperature);
    //Serial.println(" *C");
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&temp_event);
  if (isnan(temp_event.relative_humidity)) {
    Serial.println("Error reading humidity!");
  }
  else {
    //Serial.print("Humidity: ");
    //Serial.print(temp_event.relative_humidity);
    relative_humidity = int(temp_event.relative_humidity);
    //Serial.println("%");
  }
  
  }

void loop() {
  int i = 0;
  
  
   if (!PMSA003_read()) {
        Serial.println('PMSA003 not ready, wait..');
        delay(4000);
    }
    
    DHT_read();

              StaticJsonBuffer<200> jsonBuffer;
                    JsonObject &root = jsonBuffer.createObject();
                    root["pm1.0"] = thisFrame.concPM1_0_CF1;
                    root["pm2.5"] = thisFrame.concPM2_5_CF1;
                    root["pm10"] = thisFrame.concPM10_0_CF1;

//                    root["pm1.0_at"] = thisFrame.concPM1_0_amb;
//                    root["pm2.5_at"] = thisFrame.concPM2_5_amb;
//                    root["pm10_at"] = thisFrame.concPM10_0_amb;

                    root["ver"] = thisFrame.version;
                    root["error-code"] = thisFrame.errorCode;

                    
                    if(thisFrame.checksum==calcChecksum){
                      root["pass"] = true;  
                    }else {
                      root["pass"] = false;
                     }

                     root["temperature"] = int(temperature);
                     root["humidity"] = int(relative_humidity);

                    root["SSID"] = WiFi.SSID();
                    root["IP"] = String(WiFi.localIP());
                    root["RSSI"] = int(WiFi.RSSI());
                     
                    root.printTo(Serial);        
                    Serial.println();
                    

               if(refreshOLEDCount>=30){
                refreshOLEDCount = 0;
                SeeedOled.clearDisplay();         // Clear Display.
              }
              refreshOLEDCount++;
              
              SeeedOled.setTextXY(0,0);          
              String FirstLine = "Temp:"+String(temperature)+"C RH:"+ String(relative_humidity)+"%";
              SeeedOled.putString(FirstLine); //Print the String

              SeeedOled.setTextXY(2,0);        
              String SecondLine1 = "PM1.0: "+String(thisFrame.concPM1_0_CF1);
              SeeedOled.putString(SecondLine1); //Print the String

              SeeedOled.setTextXY(3,0);        
              String SecondLine2 = "PM2.5: "+String(thisFrame.concPM2_5_CF1);
              SeeedOled.putString(SecondLine2); //Print the String

              SeeedOled.setTextXY(4,0);        
              String SecondLine3 = "PM10: "+String(thisFrame.concPM10_0_CF1);
              SeeedOled.putString(SecondLine3); //Print the String

              SeeedOled.setTextXY(6,0);        
              String ThirdLine = String(WiFi.SSID());
              SeeedOled.putString(ThirdLine); //Print the String

              SeeedOled.setTextXY(7,0);        
              String ForthLine = WiFi.localIP();
              SeeedOled.putString(ForthLine); //Print the String   if(refreshOLEDCount>=10){

       
  TCPClient client = server.available();   // listen for incoming clients
  if (client) {                             // if you get a client,
    Serial.println("new client");           // print a message out the serial port
    char buffer[150] = {0};                 // make a buffer to hold incoming data
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (strlen(buffer) == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            /*
            client.println("<html><head><title>RedBear Duo WiFi Web Server</title></head><body align=center>");
            client.println("<h1 align=center><font color=\"blue\">Welcome to the Duo WiFi Web Server</font></h1>");
            client.print("BLUE LED <button onclick=\"location.href='/H'\">HIGH</button>");
            client.println(" <button onclick=\"location.href='/L'\">LOW</button><br>");

             */
             
              root.prettyPrintTo(client);

           
              
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          }
          else {      // if you got a newline, then clear the buffer:
            memset(buffer, 0, 150);
            i = 0;
          }
        }
        else if (c != '\r') {    // if you got anything else but a carriage return character,
          buffer[i++] = c;      // add it to the end of the currentLine
        }

      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disonnected");
    }
    
  
}


void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}


