/*
 * The MIT License
 *
 * Copyright 2024 Alvaro Salazar <alvaro@denkitronik.com>.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef LIBIOT_H
#define LIBIOT_H

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "Adafruit_CCS811.h"

#define MEASURE_INTERVAL 2          ///< Intervalo en segundos de las mediciones
#define ALERT_DURATION 60           ///< Duración aproximada en la pantalla de las alertas que se reciban

extern const char* MQTT_TOPIC_PUB; ///< El tópico de publicación debe tener estructura: <país>/<estado>/<ciudad>/<usuario>/out
extern const char* MQTT_TOPIC_SUB; ///< El tópico de publicación debe tener estructura: <país>/<estado>/<ciudad>/<usuario>/out
extern const char* mqtt_server;     ///< Cambia por la dirección de tu servidor MQTT
extern const int mqtt_port;         ///< Puerto seguro (TLS)
extern const char* mqtt_user;       ///< Cambia por tu usuario MQTT
extern const char* mqtt_password;   ///< Cambia por tu contraseña MQTT
extern const char* root_ca;         ///< Certificado raíz de la autoridad de certificación en formato PEM
extern WiFiClientSecure espClient;  ///< Conexión TLS/SSL
extern PubSubClient client;         ///< Cliente MQTT

extern time_t now;                  ///< Timestamp de la fecha actual.
extern long long int measureTime;   ///< Tiempo de la última medición
extern long long int alertTime;     ///< Tiempo en que inició la última alerta
extern String alert;                ///< Mensaje para mostrar en la pantalla
extern Adafruit_CCS811 ccs;         ///< Sensor CCS811

// Estructura para datos del PMS7003
struct PMS7003Data {
  uint16_t pm1_0_cf1;
  uint16_t pm2_5_cf1;
  uint16_t pm10_cf1;
  uint16_t pm1_0_atm;
  uint16_t pm2_5_atm;
  uint16_t pm10_atm;
  uint16_t num_part_03;
  uint16_t num_part_05;
  uint16_t num_part_1;
  uint16_t num_part_25;
  uint16_t num_part_5;
  uint16_t num_part_10;
};

typedef struct {
  // Datos del CCS811
  uint16_t co2;
  uint16_t tvoc;
  bool ccs811_valido;
  // Datos del PMS7003
  PMS7003Data pms7003;
  bool pms7003_valido;
} SensorData;

time_t setTime();                   ///< Función setTime que ajusta el tiempo del dispositivo con servidores SNTP
bool measure(SensorData * data);    ///< Función measure que verifica si ya es momento de hacer las mediciones de las variables
void reconnect();                   ///< Función que se ejecuta cuando se establece conexión con el servidor MQTT
void setupIoT();                    ///< Función setupIoT que configura el certificado raíz, el servidor MQTT y el puerto
void setupSensors();                ///< Función setupSensors que configura los sensores CCS811 y PMS7003
void scanI2C();                     ///< Función scanI2C que escanea el bus I2C y muestra los dispositivos encontrados
void checkMQTT();                   ///< Función checkMQTT que verifica si el dispositivo está conectado al broker MQTT y si no lo está, intenta reconectar
String checkAlert();                ///< Función checkAlert que verifica si ha llegado alguna alerta al dispositivo
void receivedCallback(char* topic, byte* payload, unsigned int length); ///< Función receivedCallback que se ejecuta cuando llega un mensaje a la suscripción MQTT
void sendSensorData(SensorData * data); ///< Función sendSensorData que publica los datos de los sensores al tópico configurado usando el cliente MQTT
String getMacAddress();             ///< Función getMacAddress que adquiere la dirección MAC del dispositivo y la retorna en formato de cadena  

#endif /* LIBIOT_H */
