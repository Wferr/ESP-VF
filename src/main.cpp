#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ezTime.h>
#include <ESP8266WiFi.h>
#include <ESP8266MDNS.h>
#include <WiFiUdp.h>

///////////////////////////////////////////////////////////////////
//                           Config                              //
///////////////////////////////////////////////////////////////////
String VERSION = "v1.0.0"; // Spaced out to not exceed this length
String HOSTNAME = "VF-ESP8266";
String SSID = "SSID";
String PASSWORD = "PASSWORD";
String OTA_PASSWORD = "testing123";
String TIMEZONE = "America/Los_Angeles";

// Defines
Timezone myTZ;
bool carrots[20];
int dispDelay = 500; // Delay between display updates (microseconds)

void setBrightness(int brightness)
{                                  // 0-100 in steps of 20?
  Serial.write(0x04);              // DIM Command
  delayMicroseconds(dispDelay);    // Delay between commands
  Serial.write(0x00 + brightness); // Brightness Command
  delayMicroseconds(dispDelay);    // Delay between commands
}

void updateCarrots()
{
  for (int i = 0; i < 20; i++)
  {
    if (carrots[i])
    {
      Serial.write(0x18); // TON Triangle Mark On
    }
    else
    {
      Serial.write(0x19); // TOF Triangle Mark Off
    }
    delayMicroseconds(dispDelay);
    Serial.write(0x14 + i);
    delayMicroseconds(dispDelay);
  }
}

void clearCarrots()
{
  for (int i = 0; i < 20; i++)
  {
    carrots[i] = false;
  }
  Serial.write(0x1A); // TFF Triangle Mark All Off
  delayMicroseconds(dispDelay);
}

void fillCarrots()
{
  for (int i = 0; i < 20; i++)
  {
    carrots[i] = true;
  }
  updateCarrots();
}

String previousMessage;
void updateDisplay(String displayMessage)
{
  if (displayMessage.length() >= 40) // If the message is too long, truncate it
  {
    displayMessage = displayMessage.substring(0, 39);
  }

  // Compare diffrences in messages and display only the difference
  if (displayMessage != previousMessage)
  {
    for (int i = 0; i < 40; i++)
    {
      if (displayMessage[i] != previousMessage[i])
      {
        Serial.write(0x10); // Display Position Command
        delayMicroseconds(dispDelay);
        Serial.write(0x00 + i); // What Position to write digit
        delayMicroseconds(dispDelay);
        Serial.write(displayMessage[i]); // Write
        delayMicroseconds(dispDelay);
      }
    }
    previousMessage = displayMessage;
  }
}

void clearDisplay()
{
  previousMessage = ""; // Clear the previous message
  Serial.write(0x0D); // CLR Command
  delayMicroseconds(dispDelay);
}

void yieldBackground(){
  ArduinoOTA.handle(); // Arduino OTA
  events();            // ezTime Events
  yield();             // Yield to other processes (wifi)
}

void displayTime(){
  updateDisplay(myTZ.dateTime("l   M Y A h:i:s.v"));
}
void setup()
{

  Serial.begin(31250); // Serial Rate of Futaba M202MD07HB Display

  delay(100); // Start delay

  clearDisplay();
  setBrightness(40);

  // Start String
  String startString = ("VF-ESP    FW: " + VERSION + "WiFi: " + SSID);
  updateDisplay(startString);

  // Connect to Wifi
  WiFi.begin(SSID, PASSWORD);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    carrots[i] = true;
    i++;
    updateCarrots();
  }
  if (WiFi.SSID() == SSID)
  {
    // Setup MDNS Responder
    if (!MDNS.begin(HOSTNAME))
    { // Start the mDNS responder for <string>.local
      clearDisplay();
      updateDisplay("MDNS ERROR!");
      delay(10000);
    }

    // Setup NTP
    // setInterval(60); // Using Default interval of 30 Mins
    clearDisplay();
    updateDisplay("Getting Time...");
    waitForSync();
    myTZ.setLocation(TIMEZONE);

    // Setup OTA
    ArduinoOTA.setHostname(HOSTNAME.c_str());
    ArduinoOTA.setPassword(OTA_PASSWORD.c_str());

    ArduinoOTA.onStart([]()
                       {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    clearDisplay();
    updateDisplay("Start updating " + type); });
    ArduinoOTA.onEnd([]()
                     { Serial.println("End"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          {
      int disProgress = (progress / (total / 100));
      updateDisplay("Progress: " + disProgress); });
    ArduinoOTA.onError([](ota_error_t error)
                       {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    } });
    ArduinoOTA.begin();
  }
  else
  {
    clearDisplay();
    updateDisplay("Wifi Error! Cant Connect to " + String(SSID));
    delay(2000);
  }
  clearDisplay();
}

void loop()
{
  yieldBackground();
  displayTime();
}
