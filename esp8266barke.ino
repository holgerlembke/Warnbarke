/*

   einfache RGB-Warnbarke, RGB-Maschinenturm, Warnturmleuchte 

   ESP-01/esp8266 als Prozessorboard

   3D-Model:
     https://github.com/holgerlembke/OpenSCAD-Modelle/blob/main/rgb3x8barke.scad

*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Adafruit_NeoPixel.h>
#include <privatedata.h>  // my credetial data, https://github.com/holgerlembke/privatedata
#include <PubSubClient.h>

ESP8266WiFiMulti wifiMulti;

WiFiClient wificlient;
PubSubClient mqttclient(wificlient);

const String mqttserver = privatedata_mqttserver;

const byte PinMatrix = 3;
const byte LED_NUM = 3 * 8;

Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_NUM, PinMatrix, NEO_GRB + NEO_KHZ800);

//*****************************************************************************************************************************************
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Serial.println(F("\n\n"));

  Serial.print(F("esp8266Barke "));
  Serial.print(__DATE__);
  Serial.print(F(" "));
  Serial.print(__TIME__);
  Serial.println();

  leds.begin();
  for (int i = 0; i < LED_NUM; i++) {
    leds.setPixelColor(i, 0, 0, 0);
  }
  leds.show();

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(privatedata_mySSID, privatedata_myWAP2);

  mqttclient.setServer(mqttserver.c_str(), 1883);
  mqttclient.setCallback(callback);

  setupHeartBeat();
}

//*****************************************************************************************************************************************
void loopHeartBeat(void) {
  static unsigned long ticker = 0;
  const int flashon = 50;
  const unsigned long flashtime = 1000l * 10l;
  static int flash = flashon;
  static bool ison = false;

  byte state = 0;
  if (WiFi.status() == WL_CONNECTED) {
    state = 1;
    if (mqttclient.connected()) {
      state = 2;
    }
  }

  if (millis() - ticker > flash) {
    if (ison) {
      flash = flashtime - flashon;
      switch (state) {
        case 1:
          {  // blau
            leds.setPixelColor(0, 0, 0, 2);
            break;
          }
        case 2:
          {  // gruen
            leds.setPixelColor(0, 0, 2, 0);
            break;
          }
        default:
          {  // hellrot
            leds.setPixelColor(0, 20, 0, 0);
            break;
          }
      }
      mqttclient.publish("Barke/HerzSchlag", "ping");
      ison = false;
    } else {
      flash = flashon;
      if (state == 2) {
        leds.setPixelColor(0, 20, 0, 0);
      }
      ison = true;
    }
    leds.show();

    ticker = millis();
  }
}

struct barkendaten_t {
  byte r = 0, g = 0, b = 0;  // 0 oder 1
  byte helligkeit = 40;
  bool makeon = false;
  bool blinkmode = true;
} barkendaten;

//*****************************************************************************************************************************************
void processbarke() {
  if ((barkendaten.makeon) || (!barkendaten.blinkmode)) {
    allbarkcolor(barkendaten.r * barkendaten.helligkeit, barkendaten.g * barkendaten.helligkeit, barkendaten.b * barkendaten.helligkeit);
    barkendaten.makeon = false;
  } else {
    allbarkcolor(0, 0, 0);
    barkendaten.makeon = true;
  }
}

//*****************************************************************************************************************************************
void callback(char* Topic, byte* Payload, unsigned int length) {
  String topic = String(Topic);
  String payload = "";
  for (int i = 0; i < length; i++) {
    payload += (char)Payload[i];
  }

  if (topic == "Barke/Modus") {
    barkendaten.blinkmode = payload == "blink";
  }

  if (topic == "Barke/Helligkeit") {
    int h = payload.toInt();
    if ((h > 0) && (h < 256)) {
      barkendaten.helligkeit = h;
    }
  }

  if (topic == "Barke/Farbe") {
    payload.toLowerCase();

    if (payload == "gelb") {
      barkendaten.r = 40;
      barkendaten.g = 40;
      barkendaten.b = 0;
    }
    if (payload == "gruen") {
      barkendaten.r = 0;
      barkendaten.g = 40;
      barkendaten.b = 0;
    }
    if (payload == "rot") {
      barkendaten.r = 40;
      barkendaten.g = 0;
      barkendaten.b = 0;
    }
  }

  /** /
  Serial.print("topic: ");
  Serial.print(topic);
  Serial.print(" payload: ");
  Serial.print(payload);
  Serial.println();
/**/
}

//*****************************************************************************************************************************************
void mqttreconnect() {
  if (mqttclient.connect("arduinoClient")) {
    Serial.println("MQTT connected.");
    mqttclient.subscribe("Barke/#");
  }
}

//*****************************************************************************************************************************************
void allbarkcolor(byte r, byte g, byte b) {
  for (int i = 1; i < LED_NUM; i++) {
    leds.setPixelColor(i, r, g, b);
  }
  leds.show();
}

//*****************************************************************************************************************************************
void loop() {
  const uint32_t connectTimeoutMs = 1000l * 20l;
  static bool connected = false;

  static uint32_t mqtttimeout = 0;
  static uint32_t barkenticker = 0;

  if (wifiMulti.run(connectTimeoutMs) == WL_CONNECTED) {
    if (!connected) {
      Serial.print("WiFi connected: ");
      Serial.println(WiFi.localIP());
      connected = true;
    }
  } else {
    connected = false;
  }

  if (!mqttclient.connected()) {
    if (millis() - mqtttimeout > 15000l) {
      mqtttimeout = millis();
      mqttreconnect();
    }
  }
  mqttclient.loop();

  if (millis() - barkenticker > 1000) {
    barkenticker = millis();
    processbarke();
  }

  loopHeartBeat();
}

//