#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ezTime.h>
#include <ESP8266WiFi.h>
#include <ESP8266MDNS.h>
#include <WiFiUdp.h>
#include <MQTT.h>

///////////////////////////////////////////////////////////////////
//                           Config                              //
///////////////////////////////////////////////////////////////////
String VERSION = "v1.0.0"; // Spaced out to not exceed this length
String HOSTNAME = "VF-ESP8266";
String SSID = "SSID";
String PASSWORD = "PASSWORD";
String OTA_PASSWORD = "testing123";
String TIMEZONE = "America/Los_Angeles";
boolean MQTT_ENABLED = true; // Enable MQTT
char *MQTT_BROKER = "192.168.1.4";
char *MQTT_USERNAME = "mqtt";
char *MQTT_PASSWORD = "realsecurepassword";

// Defines
Timezone myTZ;
bool carrots[20];
int currentBrightness = 0;
int dispDelay = 500; // Delay between display updates (microseconds)

WiFiClient net;
MQTTClient client;

void setBrightness(int brightness)
{
  Serial.write(0x04);           // DIM Command
  delayMicroseconds(dispDelay); // Delay between commands
  // Convert 0-100 to 5 steps
  if (brightness >= 100)
  {
    currentBrightness = 100;
    Serial.write(0xFF); // Brightness Command 100%
  }
  else if (brightness >= 80)
  {
    currentBrightness = 80;
    Serial.write(0x80); // Brightness Command 80%
  }
  else if (brightness >= 60)
  {
    currentBrightness = 60;
    Serial.write(0x60); // Brightness Command 60%
  }
  else if (brightness >= 40)
  {
    currentBrightness = 40;
    Serial.write(0x40); // Brightness Command 40%
  }
  else if (brightness >= 20)
  {
    currentBrightness = 20;
    Serial.write(0x20); // Brightness Command 20%
  }
  else
  {
    currentBrightness = 0;
    Serial.write(0x00); // Brightness Command 0%
  }
  delayMicroseconds(dispDelay); // Delay between commands
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
  Serial.write(0x0D);   // CLR Command
  delayMicroseconds(dispDelay);
}

void displayTime()
{
  updateDisplay(myTZ.dateTime("l   M Y A h:i:s.v"));
}

// MQTT Handler
void messageReceivedMQTT(String &topic, String &payload)
{
  // Parse Topic
  if (topic.equals(HOSTNAME + "/status"))
  {
    client.publish(HOSTNAME + "/status", "1");
  }
  else if (topic.equals(HOSTNAME + "/relay/0/set"))
  {
    if (payload.equals("1"))
    {
      setBrightness(0);
      client.publish(HOSTNAME + "/relay/0", "1");
    }
    else if (payload.equals("0"))
    {
      setBrightness(100);
      client.publish(HOSTNAME + "/relay/0", "0");
    }
    else if (payload.equals("1"))
    {
      client.publish(HOSTNAME + "/relay/0", "1");
    }
  }
  else if (topic.equals(HOSTNAME + "/brightness/set"))
  {
    int brightness = payload.toInt();
    setBrightness(brightness);
    client.publish("Panel-Badge/brightness", String(currentBrightness));
  }
}

boolean mqttSetup = 0;
char mqttHost[20];
void handleMQTT()
{
  if (MQTT_ENABLED)
  {
    HOSTNAME.toCharArray(mqttHost, HOSTNAME.length());
    if (!mqttSetup)
    {
      client.disconnect();
      if (WiFi.SSID() == SSID)
      {
        client.begin(MQTT_BROKER, net);
        client.onMessage(messageReceivedMQTT);
        int i = 0;
        while (!client.connect(mqttHost, MQTT_USERNAME, MQTT_PASSWORD))
        {
          delay(1000);
          i++;
          if (i > 5)
          {
            MQTT_ENABLED = false;
            return;
          }
        }
        Serial.println("Connected!");

        // Publish Current Status before Subscribing
        client.publish(HOSTNAME + "/status", "1");
        if (currentBrightness > 0)
        {
          client.publish(HOSTNAME + "/relay/0", "1");
        }
        client.publish(HOSTNAME + "/brightness", String(currentBrightness));

        client.subscribe(HOSTNAME + "/status");
        client.subscribe(HOSTNAME + "/relay/0/set");
        client.subscribe(HOSTNAME + "/brightness/set");

        client.publish(HOSTNAME, HOSTNAME + " online!");
        mqttSetup = true;
      }
      else
      {
        Serial.println("Wifi in AP mode! Disabling MQTT Handler");
        MQTT_ENABLED = false;
      }
    }
    else
    {
      client.loop();

      // Reconnect to MQTT if Disconnected.
      if (!client.connected())
      {
        mqttSetup = false;
      }
    }
  }
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

void yieldBackground()
{
  ArduinoOTA.handle(); // Arduino OTA
  handleMQTT();        // MQTT Handler
  events();            // ezTime Events
  yield();             // Yield to other processes (wifi)
}

void loop()
{
  yieldBackground();
  displayTime();
}
