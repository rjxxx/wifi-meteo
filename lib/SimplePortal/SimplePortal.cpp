#include "SimplePortal.h"
#include <LittleFS.h>

static DNSServer SP_dnsServer;
#ifdef ESP8266
static ESP8266WebServer SP_server(80);
#else
static WebServer SP_server(80);
#endif

const char SP_connect_page[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
<style>
    input[type="text"], input[type="number"] {
        margin-bottom: 8px;
        font-size: 20px;
    }
    input[type="submit"] {
        width: 180px;
        height: 60px;
        margin-bottom: 8px;
        font-size: 20px;
    }
    iframe{
        display: none;
    }
</style>
<div style="text-align: center;">
    <h3>WiFi</h3>
    <form action="/wifi" method="POST" target="frame">
        <input type="text" name="wifi_ssid" placeholder="SSID">
        <input type="text" name="wifi_pass" placeholder="Password">
        <br>
        <input type="submit" value="Save">
    </form>
    <br>
    <h3>MQTT</h3>
    <form action="/mqtt" method="POST" target="frame">
        <input type="text" name="mqtt_host" placeholder="Host">
        <input type="number" name="mqtt_port" placeholder="Port" value="0">
        <br>
        <input type="submit" value="Save">
    </form>
    <form action="/exit" method="POST">
        <input type="submit" value="Reboot">
    </form>
    <iframe name="frame"></iframe>
</div>
</body>
</html>)rawliteral";

static byte SP_status = 0;
static bool SP_started = false;
WifiConfig wifiConfig;
MqttConfig mqttConfig;

bool writeMqttConfig() {
    File f = LittleFS.open("/mqtt.txt", "w");
    if (!f) {
        return false;
    }
    f.print(mqttConfig.host);
    f.print('\n');
    f.print(mqttConfig.port);
    f.print('\n');
    f.close();
    return true;
}

bool writeWifiConfig() {
    File f = LittleFS.open("/wifi.txt", "w");
    if (!f) {
        Serial.println("Failed to open file 'wifi.txt'");
        return false;
    }
    f.print(wifiConfig.ssid);
    f.print('\n');
    f.print(wifiConfig.pass);
    f.print('\n');
    f.close();
    return true;
}

void SP_handleSaveWifi() {
    Serial.println("Saving WIFI settings");
    strcpy(wifiConfig.ssid, SP_server.arg("wifi_ssid").c_str());
    strcpy(wifiConfig.pass, SP_server.arg("wifi_pass").c_str());

    if (writeWifiConfig()) {
        wifiConfig.success = true;
    } else {
        mqttConfig.success = false;
        Serial.println("Failed to open file 'wifi.txt'");
    }
}

void SP_handleSaveMqtt() {
    Serial.println("Saving MQTT settings");
    strcpy(mqttConfig.host, SP_server.arg("mqtt_host").c_str());
    mqttConfig.port = SP_server.arg("mqtt_port").toInt();

    if (writeMqttConfig()) {
        mqttConfig.success = true;
    } else {
        mqttConfig.success = false;
        Serial.println("Failed to open file 'mqtt.txt'");
    }
}

void SP_handleExit() {
    SP_status = SP_EXIT;
}

void portalStart() {
    WiFi.softAPdisconnect();
    WiFi.disconnect();
    IPAddress apIP(SP_AP_IP);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, subnet);
    WiFi.softAP(SP_AP_NAME);
    SP_dnsServer.start(53, "*", apIP);

    SP_server.onNotFound([]() {
        SP_server.send(200, "text/html", SP_connect_page);
    });
    SP_server.on("/wifi", HTTP_POST, SP_handleSaveWifi);
    SP_server.on("/mqtt", HTTP_POST, SP_handleSaveMqtt);
    SP_server.on("/exit", HTTP_POST, SP_handleExit);
    SP_server.begin();
    SP_started = true;
    SP_status = SP_RUN;
}

void portalStop() {
    WiFi.softAPdisconnect();
    SP_server.stop();
    SP_dnsServer.stop();
    SP_status = SP_EXIT;
    SP_started = false;
}

bool portalTick() {
  if (SP_started) {
    SP_dnsServer.processNextRequest();
    SP_server.handleClient();
    yield();
    if (SP_status) {
      portalStop();
      return 1;
    }
  }
  return 0;
}

void portalRun(uint32_t prd) {
  uint32_t tmr = millis();
  portalStart();
  while (!portalTick()) {
    if (millis() - tmr > prd) {
      SP_status = 5;
      portalStop();
      break;
    }
    yield();
  }
}

byte portalStatus() {
    return SP_status;
}

bool loadWifiConfig() {
    File f = LittleFS.open("/wifi.txt", "r");
    if (!f) {
        Serial.println("Failed to open file 'wifi.txt'");
        return false;
    }
    strcpy(wifiConfig.ssid, f.readStringUntil('\n').c_str());
    strcpy(wifiConfig.pass, f.readStringUntil('\n').c_str());
    wifiConfig.success = true;
    f.close();
    return true;
}

bool loadMqttConfig() {
    File f = LittleFS.open("/mqtt.txt", "r");
    if (!f) {
        Serial.println("Failed to open file 'mqtt.txt'");
        return false;
    }
    strcpy(mqttConfig.host, f.readStringUntil('\n').c_str());
    mqttConfig.port = f.readStringUntil('\n').toInt();
    mqttConfig.success = true;
    f.close();
    return true;
}

void deleteMqttConfig() {
    LittleFS.remove("/mqtt.txt");
}

void deleteWifiConfig() {
    LittleFS.remove("/wifi.txt");
}
