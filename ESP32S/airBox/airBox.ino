#include <WiFi.h>
#include <ArduinoJson.h> 
#include <Adafruit_Sensor.h>
#include <DHT.h>

#define DHTTYPE DHT22
#define DHTPIN 4
DHT dht(DHTPIN, DHTTYPE);

int relative_humidity = 0;
int temperature = 0;

const int MAX_FRAME_LEN = 64;
char frameBuf[MAX_FRAME_LEN];
int detectOff = 0;
int frameLen = MAX_FRAME_LEN;
bool inFrame = false;

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

char ssid[] = "SSID";
char password[] = "PASSWORD";

WiFiServer server(80);

DynamicJsonDocument root(256);

TaskHandle_t Task1;

unsigned long lastPMSA003Check = 0;
const unsigned long checkInterval = 4000;

bool PMSA003_read() {
    Serial1.begin(9600);
    bool packetReceived = false;
    while (!packetReceived) {
        if (Serial1.available() > 32) {
            while (Serial1.available()) {
                Serial1.read();
            }
        }
        if (Serial1.available() > 0) {
            int incomingByte = Serial1.read();
            if (!inFrame) {
                if (incomingByte == 0x42 && detectOff == 0) {
                    frameBuf[detectOff] = incomingByte;
                    thisFrame.frameHeader[0] = incomingByte;
                    calcChecksum = incomingByte;
                    detectOff++;
                } else if (incomingByte == 0x4D && detectOff == 1) {
                    frameBuf[detectOff] = incomingByte;
                    thisFrame.frameHeader[1] = incomingByte;
                    calcChecksum += incomingByte;
                    inFrame = true;
                    detectOff++;
                }
            } else {
                frameBuf[detectOff] = incomingByte;
                calcChecksum += incomingByte;
                detectOff++;
                uint16_t val = frameBuf[detectOff - 1] + (frameBuf[detectOff - 2] << 8);
                switch (detectOff) {
                    case 4: thisFrame.frameLen = val; frameLen = val + detectOff; break;
                    case 6: thisFrame.concPM1_0_CF1 = val; break;
                    case 8: thisFrame.concPM2_5_CF1 = val; break;
                    case 10: thisFrame.concPM10_0_CF1 = val; break;
                    case 12: thisFrame.concPM1_0_amb = val; break;
                    case 14: thisFrame.concPM2_5_amb = val; break;
                    case 16: thisFrame.concPM10_0_amb = val; break;
                    case 18: thisFrame.rawGt0_3um = val; break;
                    case 20: thisFrame.rawGt0_5um = val; break;
                    case 22: thisFrame.rawGt1_0um = val; break;
                    case 24: thisFrame.rawGt2_5um = val; break;
                    case 26: thisFrame.rawGt5_0um = val; break;
                    case 28: thisFrame.rawGt10_0um = val; break;
                    case 29: thisFrame.version = val; break;
                    case 30: thisFrame.errorCode = val; break;
                    case 32:
                        thisFrame.checksum = val;
                        calcChecksum -= ((val >> 8) + (val & 0xFF));
                        break;
                    default: break;
                }

                if (detectOff >= frameLen) {
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

void DHT_read(void *pvParameters) {
    for (;;) {
        float h = dht.readHumidity();
        if (h > 0) relative_humidity = int(h);
        float t = dht.readTemperature();
        if (t > 0) temperature = int(t);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("WiFi disconnected, attempting to reconnect...");
    if (WiFi.status() != WL_CONNECTED) {
        delay(50);
        WiFi.begin(ssid, password);
    }
}

void checkWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.println("WiFi lost, reconnecting...");
        WiFi.begin(ssid, password);
    }
}

void setup() {
    Serial.begin(115200);
    dht.begin();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    server.begin();
    if (server) {
      Serial.println("Server started successfully.");
    } else {
      Serial.println("Server failed to start.");
    }

    xTaskCreatePinnedToCore(DHT_read, "DHT_read", 2048, NULL, 2, &Task1, 1);
}

void loop() {
    checkWiFi();

    if (millis() - lastPMSA003Check >= checkInterval) {
        lastPMSA003Check = millis();
        if (!PMSA003_read()) {
            Serial.println("PMSA003 not ready, retrying...");
        } else {
          root["pm1.0"] = thisFrame.concPM1_0_CF1;
          root["pm2.5"] = thisFrame.concPM2_5_CF1;
          root["pm10"] = thisFrame.concPM10_0_CF1;
          root["temperature"] = temperature;
          root["humidity"] = relative_humidity;
          root["RSSI"] = WiFi.RSSI();
          root["pass"] = (thisFrame.checksum == calcChecksum);
        }
    }
  WiFiClient client = server.available();
    if (client) {                             // if you get a client,
      String currentLine = "";       
      while (client.connected()) {            // loop while the client's connected
        if (client.available()) {             // if there's bytes to read from the client,
          char c = client.read();             // read a byte, then
          Serial.write(c);                    // print it out the serial monitor
          if (c == '\n') {                    // if the byte is a newline character
            // if the current line is blank, you got two newline characters in a row.
            // that's the end of the client HTTP request, so send a response:
            if (currentLine.length() == 0) {
              // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
              // and a content-type so the client knows what's coming, then a blank line:
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:application/json");
              client.println();

              serializeJson(root, client);

              // The HTTP response ends with another blank line:
              client.println();
              // break out of the while loop:
              break;
            }
            else {      // if you got a newline, then clear the buffer:
              currentLine = "";
            }
          }
          else if (c != '\r') {    // if you got anything else but a carriage return character,
             currentLine += c;       // add it to the end of the currentLine
          }

        }
      }
      // close the connection:
      client.stop();
      // Serial.println("client disonnected");
  }


    
}
