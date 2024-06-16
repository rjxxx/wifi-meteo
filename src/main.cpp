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
#include <GyverDS18.h>

#define MHZ_RX D6
#define MHZ_TX D7
#define DS_PIN D5
#define DEBUG true

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
GyverDS18Single ds(DS_PIN);

void mqttReconnect()
{
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        if (client.connect(MQTT_CLIENT_ID.c_str()))
        {
            Serial.println("connected");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void setup()
{
    Serial.begin(9600); // Запуск последовательного порта

    // BME280
    bme.setFilter(FILTER_COEF_8);              // Настраиваем коофициент фильтрации
    bme.setTempOversampling(OVERSAMPLING_8);   // Настраиваем передискретизацию для датчика температуры
    bme.setPressOversampling(OVERSAMPLING_16); // Настраиваем передискретизацию для датчика давления
    bme.setStandbyTime(
        STANDBY_500MS); // Устанавливаем время сна между измерениями (у нас обычный циклический режим)
    bme.begin();        // Если на этом настройки окончены - инициализируем датчик

    // DS12
    ds.requestTemp();

    // MHZ19
    ss.begin(9600);
    mhz19.begin(ss);
    mhz19.autoCalibration();

    LittleFS.begin();

    // if (!loadWifiConfig())
    // {
    //     Serial.println("Wifi settings not load");
    // }
    // if (!loadMqttConfig())
    // {
    //     Serial.println("Mqtt settings not load");
    // }
    // if (!wifiConfig.success || !mqttConfig.success)
    // {
    //     Serial.println("Start portal");
    //     portalStart();
    // }

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
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print('.');
        delay(500);
        if (i > 60)
        {
            Serial.println("not connected, restart");
            deleteWifiConfig();
            EspClass::restart();
        }
        i++;
    }
    Serial.println("connected");

    client.setServer("192.168.1.61", 1883);
    i = 0;
    while (!client.connected())
    {
        Serial.print("Connecting to ");
        Serial.print(mqttConfig.host);
        Serial.print(" ");
        if (client.connect(MQTT_CLIENT_ID.c_str()))
        {
            Serial.println("connected");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
        if (i > 5)
        {
            deleteMqttConfig();
            EspClass::restart();
        }
        i++;
    }
}

void loop()
{
    if (millis() - getDataTimer >= 10000)
    {
        float temperature = bme.readTemperature();
        // float temperature = mhz19.getTemperature();
        static float temperature_ds = 0;

        float pressure = pressureToMmHg(bme.readPressure());
        float humidity = bme.readHumidity();

        int CO2 = mhz19.getCO2(true, false);

        ds.requestTemp(); 
        if (ds.waitReady() && ds.readTemp())
        {
            temperature_ds = ds.getTemp();
        }

#ifdef DEBUG
        float temperature_mhz = mhz19.getTemperature();

        Serial.print("CO2 (ppm): ");
        Serial.println(CO2);

        Serial.print("Temperature (mhz19): ");
        Serial.println(temperature_mhz);

        Serial.print("Temperature (ds18): ");
        Serial.println(temperature_ds);

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
#endif
        if (!client.connected())
        {
            mqttReconnect();
        }
        client.loop();

        jsonResponse["sensorId"] = MAC;
        jsonResponse["sensorData"]["temperature"] = temperature;
        jsonResponse["sensorData"]["humidity"] = humidity;
        jsonResponse["sensorData"]["pressure"] = pressure;
        jsonResponse["sensorData"]["co2"] = CO2;

        size_t bytesWritten = serializeJson(jsonResponse, BUFFER);
        client.publish("weather", BUFFER, bytesWritten);

        getDataTimer = millis();
    }

    delay(10);
}
