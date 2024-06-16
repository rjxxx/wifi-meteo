/*
    Простой менеджер WiFi для esp8266 для задания логина-пароля WiFi и режима работы
    GitHub: https://github.com/GyverLibs/SimplePortal
    
    AlexGyver, alex@alexgyver.ru
    https://alexgyver.ru/
    MIT License

    Версии:
    v1.0
    v1.1 - совместимость с ESP32
*/

#define SP_AP_NAME "ESP WifiConfig"     // название точки
#define SP_AP_IP 192,168,1,1        // IP точки

#ifndef _SimplePortal_h
#define _SimplePortal_h

#include <DNSServer.h>

#ifdef ESP8266

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#else
#include <WiFi.h>
#include <WebServer.h>
#endif

#define SP_RUN 0
#define SP_SUBMIT 1
#define SP_EXIT 2

struct WifiConfig {
    char ssid[32] = "";
    char pass[32] = "";
    bool success = false;
};

struct MqttConfig {
    char host[128] = "";
    int port = -1;
    bool success = false;
};

extern WifiConfig wifiConfig;
extern MqttConfig mqttConfig;

void portalStart();

void portalStop();

void portalRun();

byte portalStatus();

bool loadWifiConfig();

bool loadMqttConfig();

void deleteMqttConfig();

void deleteWifiConfig();

#endif