/*
  Board: NODEMCU 1.0
  upload speed 30000

  //next version: http://www.esp8266learning.com/esp8266-mcp23017-example.php
https://arduino-esp8266.readthedocs.io/en/latest/libraries.html#i2c-wire-library

  * MCP23017 Connections: *
  SDA(13) to D2 (4) on Node MCU
  SCK(12) to D1 (5) on Node MCU
  A0-A2 to GND
  
  * HDLO-3416 Connections: *
  D0-D6 connected to MCP23017 GPA0-GPA6
  CUE to ground
  BL to V+

  MCP23017 GPB0 to HDLO-3416 A0
  MCP23017 GPB1 to HDLO-3416 A1
  MCP23017 GPB2 to HDLO-3416 CE Disp #0
  MCP23017 GPB3 to HDLO-3416 CE Disp #1
  MCP23017 GPB4 to HDLO-3416 CE Disp #2
  MCP23017 GPB5 to HDLO-3416 CE Disp #3
  MCP23017 GPB6 to HDLO-3416 CU
  Character order HDL starts with normal ASCII at 0x20 continues untill 0x7E

  node vs arduino pin mapping:
  0-3
  1-10
  2-4
  3-9
  4-2
  5-1
  9-11
  10-12
  12-6
  13-7
  14-5
  15-8
  16-0
*/
#include "Wire.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <AutoConnect.h>
#include <AutoConnectCredential.h>
#include <ESP8266HTTPClient.h>


#define D_WR 12 //12 = D6 on MCU, WR on Display HDLO-3416
#define D_CLR 13 //12 = D7 on MCU, CLR on Display HDLO-3416

ESP8266WebServer Server;          //Server object
AutoConnect Portal; //Autoconnect portal object
AutoConnectConfig ACConfig; //Autoconnect portal configure object

//Delete all autoconnect credentials
void deleteAllCredentials(void) {
  AutoConnectCredential credential;
  station_config_t cfg;
  uint8_t ent = credential.entries();

  while (ent--) {
    credential.load((int8_t)0, &cfg);
    credential.del((const char*)&cfg.ssid[0]);
  }
}

//Display an entire string
void dispString(String str) {
  /*//Clear display (removed since it introduced a "blink"
  digitalWrite(D_CLR, LOW);
  delayMicroseconds(12);
  digitalWrite(D_CLR, HIGH);
  delayMicroseconds(1);*/
    
  byte i; //Counter for for loop
  char c; //Character buffer
  for (i=0; i<16; i++) {
    if (i >= str.length()) { //If string has run out
      c = ' '; //Use space as filler
    }
    else { //Otherwise get said character
      c = str.charAt(i);
    }
    dispChar(i % 4, c, i/4); //Display character
  }
}

//Display a single character at position X
void dispChar(byte addr, byte chr, byte disp) {
  //Prepare control lines
  Wire.beginTransmission(0x20);
  Wire.write(0x13); // address GPIOA
  /* First, we set everything high (including CU) apart from the Address: 0b11111100
   *   This is because CU needs to be high and
   *   the display enable is active low
   *  
   * Next, we set the display, this is done using some conversion and inversion to get the right channnel to set low
   *    in such a way that the top row is 0 and 1
   *    and the bottom row is 2 and 3: (~(0b1 << (((disp + 2) %4) + 2))))
   *  
   * Lastly, we setup the right address. This is done using an inversion since the display counts from left to right
   *    We are however latin speaking, and therefore choose left to right numbering for ease of use: (3-addr)
  */
  Wire.write((0b11111100 & (~(0b1 << (((disp + 2) %4) + 2))))  | (3-addr)); //Setup control lines, see above
  Wire.endTransmission();

  //Prepare data lines
  Wire.beginTransmission(0x20);
  Wire.write(0x12); // address GPIOA
  Wire.write(0b00000000 | chr);   //D0 - D6: set character
  Wire.endTransmission();
  
  digitalWrite(D_WR, LOW); //Lower WR in preparation of data loding
  delayMicroseconds(1);
  digitalWrite(D_WR, HIGH); //Rising edge on WR, latches in data
  delayMicroseconds(10);
  
}

void setup() {
  //Use below when needed to write eeprom
  /*deleteAllCredentials(); //Clears all credentials at start..
  ACConfig.immediateStart = true;*/
  //Set soft AP SSID & Password
  ACConfig.apid = "textmeter";
  ACConfig.psk = "12345678";
  
  pinMode(LED_BUILTIN, OUTPUT);  // LED_BUILTIN pin as output
  pinMode(D_WR, OUTPUT);         // Display Write as output
  pinMode(D_CLR, OUTPUT);        // Display Clear as output

  digitalWrite(D_WR, HIGH); //set to high because Display Write is active 0
  digitalWrite(D_CLR, HIGH);//set to high because Display Clear is active 0


  Serial.begin(115200); //Setup Serial (for debugging)
  Serial.println("Hello World!"); //Start of program, welcome message.

  Wire.begin(); // wake up I2C bus
  
  Wire.beginTransmission(0x20);
  Wire.write(0x00); // IODIRA register
  Wire.write(0x00); // set all of GPIO A to outputs
  Wire.endTransmission();  
  
  Wire.beginTransmission(0x20);
  Wire.write(0x01); // IODIRB register
  Wire.write(0x00); // set all of GPIOB to outputs
  Wire.endTransmission();  

  dispString("Startup");

  Serial.println("Starting Portal");

  Portal.config(ACConfig);
  if (Portal.begin()) { //Will block until we set the AP using auto connect
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    dispString("Connected!");
    delay(1000);
  }
}

// the loop function runs over and over again forever
void loop() {
  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
    HTTPClient http;  //Declare an object of class HTTPClient
    http.begin("http://192.168.178.43/textmeter/");  //Specify request destination
    int httpCode = http.GET();                                                                  //Send the request
      if (httpCode > 0) { //Check the returning code
        if (httpCode == HTTP_CODE_OK) {
          String payload = http.getString();
          Serial.println("Received data:");
          Serial.println(payload);                     //Print the response payload
          dispString(payload);
        }
        else {
          Serial.println("HTTP code not OK:");
          Serial.println(httpCode);
        }
      }
      else {
        Serial.println("HTTP code not larger then 0");
      }
      http.end();   //Close connection
      delay(200);
  }
  else {
    dispString("Not Connected");
  }
  Portal.handleClient();
}
