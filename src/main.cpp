#include <Arduino.h>
#include <Wire.h>
#include <GyverBME280.h>
#include <SoftwareSerial.h>
#include <MHZ19.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <SimplePortal.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <GyverNTP.h>

#define MHZ_RX D6
#define MHZ_TX D7
String MAC = WiFi.macAddress();
String MQTT_CLIENT_ID = "ESP8266Client-" + MAC;
uint8_t BUFFER[200];

GyverBME280 bme;
SoftwareSerial ss(MHZ_RX, MHZ_TX);
MHZ19 mhz19;
unsigned long getDataTimer = 0;
WiFiClient espClient;
PubSubClient client(espClient);
StaticJsonDocument<200> jsonResponse;
GyverNTP ntp;

void mqttReconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (client.connect(MQTT_CLIENT_ID.c_str())) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(9600);                               // Запуск последовательного порта

    //BME280
    bme.setFilter(FILTER_COEF_8);                     // Настраиваем коофициент фильтрации
    bme.setTempOversampling(OVERSAMPLING_8);          // Настраиваем передискретизацию для датчика температуры
    bme.setPressOversampling(OVERSAMPLING_16);        // Настраиваем передискретизацию для датчика давления
    bme.setStandbyTime(
            STANDBY_500MS);                 // Устанавливаем время сна между измерениями (у нас обычный циклический режим)
    bme.begin();                                      // Если на этом настройки окончены - инициализируем датчик

    //MHZ19
    ss.begin(9600);
//    mhz19.begin(ss);

    LittleFS.begin();

    if (!loadWifiConfig()) {
        Serial.println("Wifi settings not load");
    }
    if (!loadMqttConfig()){
        Serial.println("Mqtt settings not load");
    }
    if (!wifiConfig.success || !mqttConfig.success) {
        Serial.println("Start portal");
        portalStart();
    }

    WiFi.softAPdisconnect();
    WiFi.disconnect();

    WiFi.setAutoReconnect(true);
    WiFi.setAutoConnect(false);

    WiFi.hostname("weather-station");

    WiFi.mode(WIFI_STA);

    Serial.print("Connecting to ");
    Serial.print(wifiConfig.ssid);

    WiFi.begin(wifiConfig.ssid, wifiConfig.pass);

    uint8_t i = 0;
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(500);
        if (i > 60) {
            Serial.println("not connected, restart");
            deleteWifiConfig();
            EspClass::restart();
        }
        i++;
    }
    Serial.println("connected");
    ntp.begin();
    ntp.updateNow();

    client.setServer(mqttConfig.host, mqttConfig.port);
    i = 0;
    while (!client.connected()) {
        Serial.print("Connecting to ");
        Serial.print(mqttConfig.host);
        Serial.print(" ");
        if (client.connect(MQTT_CLIENT_ID.c_str())) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
        if (i > 5) {
            deleteMqttConfig();
            EspClass::restart();
        }
        i++;
    }
}

void loop() {
    float temperature = bme.readTemperature();
    float pressure = pressureToMmHg(bme.readPressure());
    float humidity = bme.readHumidity();

//    if (millis() - getDataTimer >= 2000) {
//        int CO2;
//
//        CO2 = mhz19.getCO2();                             // Request CO2 (as ppm)
//
//        Serial.print("CO2 (ppm): ");
//        Serial.println(CO2);
//
//        float Temp = mhz19.getTemperature();                     // Request Temperature (as Celsius)
//        Serial.print("Temperature (C): ");
//        Serial.println(Temp);
//
//        getDataTimer = millis();
//    }

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" *C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    Serial.print("Pressure: ");
    Serial.print(pressure);
    Serial.println(" mm Hg");
    Serial.println("");

    if (!client.connected()) {
        mqttReconnect();
    }
    client.loop();

    ntp.tick();

    jsonResponse["sensorId"] = MAC;
    jsonResponse["sensorData"]["temperature"] = temperature;
    jsonResponse["sensorData"]["humidity"] = humidity;
    jsonResponse["sensorData"]["pressure"] = pressure;
    jsonResponse["datetime"] = ntp.unix();

    size_t bytesWritten = serializeJson(jsonResponse, BUFFER);
    client.publish("weather", BUFFER, bytesWritten);

    delay(10000);
}
