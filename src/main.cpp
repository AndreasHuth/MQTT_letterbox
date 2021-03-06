#include <Arduino.h>

#include <LittleFS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

// needed for OTA & MQTT
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40]          = "192.168.0.192";
char mqtt_port[6]             = "1883";

//Project name
char WifiApName[40]           = "Wifi_LETTERBOX";
char MQTTClientName[40]       = "Wifi_MQTT_LETTERBOX";
char OtaClientName[40]        = "Wifi_OTA_LETTERBOX";

// MQTT
const char* pup_alive         = "/topic_letterbox/active";
const char* pub_value1        = "/topic_letterbox/status";

// WIFI
WiFiClient espClient;
PubSubClient client(espClient);

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


// OTA setup function:
void OTA_setup (void)
{
  // Ergänzung OTA
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname((const char *) OtaClientName);
  //ArduinoOTA.setHostname("LETTERBOX");
  
  // No authentication by default
  ArduinoOTA.setPassword((const char *)"admin");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// MQTT callback function:
void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived @ PUB [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// RECONNECT MQTT Server
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTTClientName)) {
      Serial.println("connected ...");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" MQTT failed to connect and hit timeout");
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      
      //reset and try again, or maybe put it to deep sleep
      // ESP.reset();
      ESP.deepSleep(0); //
      delay(5000);
    }
  }
}

long lastTransferTime = 0;
long SleepTime = 0;
bool startup = true;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  // OUTPUT Definition !
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  //clean FS, for testing
  //LittleFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (LittleFS.begin()) {
    Serial.println("mounted file system");
    if (LittleFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);

  //reset settings - for testing
  wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  //if (!wifiManager.autoConnect("WEMOS D1", "password")) {
  if (!wifiManager.autoConnect(WifiApName)) {
    Serial.println("failed to connect and hit timeout");
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
      digitalWrite(2, LOW); delay(200);
      digitalWrite(2, HIGH); delay(200);
    //reset and try again, or maybe put it to deep sleep
    //ESP.reset();  
    ESP.deepSleep(0); //
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  //Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

//  Serial.println("local ip");
//  Serial.println(WiFi.localIP());

  // OTA starts here!

  OTA_setup();

  // MQTT - Connection:
  client.setServer(mqtt_server, 1883);
  client.setCallback(MQTTcallback);

  SleepTime = millis();
}


void loop() {
  // MQTT connect
  if (!client.connected()) {
    reconnect();}
  client.loop();

  // OTA handler !
  ArduinoOTA.handle();

  // put your main code here, to run repeatedly:
  if (startup == 1)
  {
   client.publish(pub_value1, "1");
   startup= 0; 
   SleepTime = millis();
  }
  // SLEEP !
  if (millis() - SleepTime > (1000)) {  // tbd sec
    ESP.deepSleep(0); //
    delay(100);
    }
}
