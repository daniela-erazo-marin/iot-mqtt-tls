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

#include <libiot.h>
#include <Wire.h>
#include "Adafruit_CCS811.h"
#include <libota.h>
#include <libstorage.h>

// Versión del firmware (debe coincidir con main.cpp)
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v1.1.1"
#endif

//#define PRINT
#ifdef PRINT
#define PRINTD(x, y) Serial.println(x, y)
#define PRINTLN(x) Serial.println(x)
#define PRINT(x) Serial.print(x)
#else
#define PRINTD(x, y)
#define PRINTLN(x)
#define PRINT(x)
#endif

Adafruit_CCS811 ccs;     //Sensor CCS811
String alert = ""; //Mensaje de alerta
extern const char * client_id;  //ID del cliente MQTT

// Pines I2C para CCS811
#define SDA_PIN 8
#define SCL_PIN 7

// Buffer para lectura del PMS7003
uint8_t pmsBuffer[32];

// Variable para rastrear si el CCS811 está inicializado correctamente
bool ccs811_initialized = false;


/**
 * Consulta y guarda el tiempo actual con servidores SNTP.
 */
time_t setTime() {
  //Sincroniza la hora del dispositivo con el servidor SNTP (Simple Network Time Protocol)
  Serial.print("Ajustando el tiempo usando SNTP");
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov"); //Configura la zona horaria y los servidores SNTP
  now = time(nullptr);              //Obtiene la hora actual
  while (now < 1700000000) {        //Espera a que el tiempo sea mayor a 1700000000 (1 de enero de 2024)
    delay(500);                     //Espera 500ms antes de volver a intentar obtener la hora
    Serial.print(".");
    now = time(nullptr);            //Obtiene la hora actual
  }
  Serial.println(" hecho!");
  struct tm timeinfo;               //Estructura que almacena la información de la hora
  gmtime_r(&now, &timeinfo);        //Obtiene la hora actual
  Serial.print("Tiempo actual: ");  //Una vez obtiene la hora, imprime en el monitor el tiempo actual
  Serial.print(asctime(&timeinfo));
  return now;
}

// Variable para debugging periódico
static unsigned long lastMQTTDebug = 0;
static const unsigned long MQTT_DEBUG_INTERVAL = 30000; // 30 segundos

/**
 * Conecta el dispositivo con el bróker MQTT usando
 * las credenciales establecidas.
 * Si ocurre un error lo imprime en la consola.
 */
void checkMQTT() {
  if (!client.connected()) {
    reconnect();
  }
  // Procesa mensajes MQTT entrantes (esto es crítico para recibir mensajes)
  // IMPORTANTE: client.loop() debe llamarse frecuentemente para recibir mensajes
  bool loopResult = client.loop();
  if (!loopResult && client.connected()) {
    // Si loop() retorna false pero estamos conectados, podría haber un problema
    Serial.println("⚠ client.loop() retornó false (podría indicar problema de conexión)");
  }
  
  // Debug periódico cada 30 segundos
  unsigned long now = millis();
  if (now - lastMQTTDebug > MQTT_DEBUG_INTERVAL) {
    lastMQTTDebug = now;
    Serial.println("=== Healthcheck MQTT (cada 30s) ===");
    Serial.print("Conectado: ");
    Serial.println(client.connected() ? "✅UP" : "❌DOWN");
  }
}

/**
 * Adquiere la dirección MAC del dispositivo y la retorna en formato de cadena.
 */
String getMacAddress() {
  uint8_t mac[6];
  char macStr[18];
  WiFi.macAddress(mac);
  snprintf(macStr, sizeof(macStr), "ESP32-%02X%02X%02X%02X%02X%02X", 
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}


/**
 * Función que se ejecuta cuando se establece conexión con el servidor MQTT
 */
void reconnect() {
  while (!client.connected()) { //Mientras no esté conectado al servidor MQTT
    Serial.println("=== Intentando conectar a MQTT ===");
    Serial.print("Servidor: ");
    Serial.println(mqtt_server);
    Serial.print("Puerto: ");
    Serial.println(mqtt_port);
    Serial.print("Usuario: ");
    Serial.println(mqtt_user);
    Serial.print("Client ID: ");
    Serial.println(client_id);
    Serial.print("Conectando...");
    if (client.connect(client_id, mqtt_user, mqtt_password)) { //Intenta conectarse al servidor MQTT
      Serial.println(" ✓ CONECTADO");
      
      // CRÍTICO: Reconfigurar el callback después de reconectar
      client.setCallback(receivedCallback);
      Serial.println("✓ Callback reconfigurado después de reconexión");
      
      // Imprimir versión del firmware al conectar (usar versión guardada)
      String firmwareVersion = getFirmwareVersion();
      Serial.print("Firmware: ");
      Serial.println(firmwareVersion);
      
      Serial.println("=== Suscripciones MQTT ===");
      // Se suscribe al tópico de suscripción con QoS 1
      bool subResult = client.subscribe(MQTT_TOPIC_SUB, 1);
      if (subResult) {
        Serial.println("✓ Suscrito exitosamente a " + String(MQTT_TOPIC_SUB));
      } else {
        Serial.println("✗ Error al suscribirse a " + String(MQTT_TOPIC_SUB));
      }
      
      // Procesar mensajes para confirmar suscripciones
      client.loop();
      delay(100); // Dar tiempo para procesar
      
      setupOTA(client); //Configura la funcionalidad OTA
      
      // Procesar mensajes nuevamente después de suscribirse a OTA
      client.loop();
      delay(100);
      
      Serial.println("==========================");
      Serial.println("Listo para recibir mensajes MQTT");
    } else {
      Serial.println(" ✗ FALLÓ");
      Serial.println("Problema con la conexión, revise los valores de las constantes MQTT");
      int state = client.state();
      Serial.print("Código de error = ");
      alert = "MQTT error: " + String(state);
      Serial.println(state);
      if ( client.state() == MQTT_CONNECT_UNAUTHORIZED ) ESP.deepSleep(0);
      delay(5000); // Espera 5 segundos antes de volver a intentar
    }
  }
}


/**
 * Función setupIoT que configura el certificado raíz, el servidor MQTT y el puerto
 */
void setupIoT() {
  // I2C se inicializa en setupSensors() con los pines específicos
  espClient.setCACert(root_ca); //Configura el certificado raíz de la autoridad de certificación
  client.setServer(mqtt_server, mqtt_port);   //Configura el servidor MQTT y el puerto seguro
  
  // Configurar buffer más grande para mensajes grandes (por defecto es 256 bytes)
  client.setBufferSize(1024);
  
  client.setCallback(receivedCallback);       //Configura la función que se ejecutará cuando lleguen mensajes a la suscripción
  Serial.println("=== Configuración MQTT ===");
  Serial.print("Servidor MQTT: ");
  Serial.println(mqtt_server);
  Serial.print("Puerto MQTT: ");
  Serial.println(mqtt_port);
  Serial.print("Usuario MQTT: ");
  Serial.println(mqtt_user);
  Serial.print("Client ID: ");
  Serial.println(client_id);
  Serial.print("Buffer size: ");
  Serial.println(client.getBufferSize());
  Serial.println("Callback MQTT configurado: receivedCallback");
  Serial.println("==========================");
  setTime();                    //Ajusta el tiempo del dispositivo con servidores SNTP
  setupSensors();               //Configura los sensores CCS811 y PMS7003
}


/**
 * Escanea el bus I2C y muestra los dispositivos encontrados
 */
void scanI2C() {
  Serial.println("\n=== Escaneo I2C ===");
  byte error, address;
  int nDevices = 0;

  // Limpiar cualquier error previo del bus
  Wire.clearWriteError();
  
  for(address = 1; address < 127; address++ ) {
    // Usar endTransmission con true para liberar el bus
    Wire.beginTransmission(address);
    error = Wire.endTransmission(true);
    
    // Pequeño delay para evitar saturar el bus
    delay(1);

    if (error == 0) {
      Serial.print("Dispositivo I2C encontrado en direccion 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    }
    // No imprimir errores 2 (NACK) o 3 (otros) ya que son normales durante el escaneo
    // Solo mostrar errores críticos (4 = timeout u otro error)
    else if (error == 4) {
      Serial.print("Error crítico en direccion 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(" (código: ");
      Serial.print(error);
      Serial.println(")");
    }
  }
  
  // Limpiar errores al finalizar el escaneo
  Wire.clearWriteError();
  
  if (nDevices == 0) {
    Serial.println("No se encontraron dispositivos I2C");
    Serial.println("Verifica las conexiones I2C");
  } else {
    Serial.print("Total de dispositivos encontrados: ");
    Serial.println(nDevices);
  }
  Serial.println("==================\n");
}

/**
 * Configura los sensores CCS811 y PMS7003
 */
void setupSensors() {
  Serial.println("=== Inicializando Sensores ===");
  Serial.print("Pines I2C - SDA: ");
  Serial.print(SDA_PIN);
  Serial.print(", SCL: ");
  Serial.println(SCL_PIN);
  
  // I2C ya debería estar inicializado en main.cpp antes de startDisplay()
  // Solo configurar el clock si es necesario (evitar re-inicializar)
  Wire.setClock(100000);
  delay(200); // Dar más tiempo para estabilización

  // Escanear bus I2C para diagnóstico
  scanI2C();

  Serial.println("Intentando inicializar CCS811...");
  ccs811_initialized = false;
  
  // Intentar inicializar el CCS811 con manejo de errores
  if (!ccs.begin()) {
    Serial.println("ERROR: CCS811 no detectado");
    Serial.println("Verifica:");
    Serial.println("  1. Conexiones I2C (SDA y SCL)");
    Serial.println("  2. Alimentación del sensor");
    Serial.println("  3. Dirección I2C del sensor (0x5A o 0x5B)");
    Serial.println("Continuando sin CCS811...");
    ccs811_initialized = false;
  } else {
    Serial.println("CCS811 init(): Exitoso");
    ccs.setDriveMode(CCS811_DRIVE_MODE_1SEC);
    delay(2000); // Esperar a que el sensor se estabilice
    ccs811_initialized = true;
  }

  // Inicializar Serial2 para PMS7003 (RX=17, TX=18)
  Serial.println("Inicializando PMS7003...");
  Serial2.begin(9600, SERIAL_8N1, 17, 18);
  delay(100);
  Serial.println("PMS7003 init(): Exitoso");
  Serial.println("=============================\n");
}


/**
 * Lee datos del sensor PMS7003
 */
bool readPMS7003(PMS7003Data * data) {
  if (Serial2.available() < 32) return false;

  // Buscar cabecera 0x42 0x4D
  while (Serial2.available() && Serial2.peek() != 0x42) {
    Serial2.read();
  }

  if (Serial2.available() < 32) return false;

  for (int i = 0; i < 32; i++) {
    pmsBuffer[i] = Serial2.read();
  }

  // Validar encabezado
  if (pmsBuffer[0] != 0x42 || pmsBuffer[1] != 0x4D) return false;

  // Checksum
  uint16_t checksum = 0;
  for (int i = 0; i < 30; i++) checksum += pmsBuffer[i];
  uint16_t check_code = (pmsBuffer[30] << 8) | pmsBuffer[31];

  if (checksum != check_code) return false;

  // Parsear datos
  data->pm1_0_cf1  = (pmsBuffer[4] << 8) | pmsBuffer[5];
  data->pm2_5_cf1  = (pmsBuffer[6] << 8) | pmsBuffer[7];
  data->pm10_cf1   = (pmsBuffer[8] << 8) | pmsBuffer[9];
  data->pm1_0_atm  = (pmsBuffer[10] << 8) | pmsBuffer[11];
  data->pm2_5_atm  = (pmsBuffer[12] << 8) | pmsBuffer[13];
  data->pm10_atm   = (pmsBuffer[14] << 8) | pmsBuffer[15];
  data->num_part_03 = (pmsBuffer[16] << 8) | pmsBuffer[17];
  data->num_part_05 = (pmsBuffer[18] << 8) | pmsBuffer[19];
  data->num_part_1  = (pmsBuffer[20] << 8) | pmsBuffer[21];
  data->num_part_25 = (pmsBuffer[22] << 8) | pmsBuffer[23];
  data->num_part_5  = (pmsBuffer[24] << 8) | pmsBuffer[25];
  data->num_part_10 = (pmsBuffer[26] << 8) | pmsBuffer[27];

  return true;
}

/**
 * Verifica si ya es momento de hacer las mediciones de las variables.
 * si ya es tiempo, mide y envía las mediciones.
 */
bool measure(SensorData * data) {
  if ((millis() - measureTime) >= MEASURE_INTERVAL * 1000 ) {
    PRINTLN("\nMidiendo variables...");
    measureTime = millis();
    
    // Inicializar valores
    data->ccs811_valido = false;
    data->pms7003_valido = false;
    data->co2 = 0;
    data->tvoc = 0;
    
    // Leer datos del CCS811 (solo si está inicializado)
    if (ccs811_initialized) {
      // Limpiar cualquier error previo del bus I2C
      Wire.clearWriteError();
      delay(10); // Pequeño delay para estabilizar el bus
      
      // Verificar si hay datos disponibles antes de leer
      if (ccs.available()) {
        // Intentar leer datos con manejo de errores
        uint8_t error = ccs.readData();
        if (error == 0) {
          // Lectura exitosa
          data->co2 = ccs.geteCO2();
          data->tvoc = ccs.getTVOC();
          
          if (data->co2 != 0xFFFF && data->tvoc != 0xFFFF && data->co2 > 0 && data->tvoc >= 0) {
            data->ccs811_valido = true;
          }
        } else {
          // Error en la lectura - limpiar el bus y continuar
          Wire.clearWriteError();
          delay(10);
        }
      }
    } else {
      // CCS811 no está inicializado, mantener valores en 0
    }
    
    // Leer datos del PMS7003
    if (readPMS7003(&data->pms7003)) {
      data->pms7003_valido = true;
    }
    
    // Imprimir datos organizados
    Serial.println("\n========================================");
    Serial.println("      LECTURA DE SENSORES");
    Serial.println("========================================");
    
    // Datos del CCS811
    Serial.println("--- CCS811 (Calidad del Aire) ---");
    if (data->ccs811_valido) {
      Serial.println("  Estado:  disponible");
    } else {
      Serial.println("  Estado: No disponible");
    }
    Serial.print("  CO2 : ");
    Serial.print(data->co2);
    Serial.println(" ppm");
    Serial.print("  TVOC: ");
    Serial.print(data->tvoc);
    Serial.println(" ppb");
    
    // Datos del PMS7003
    Serial.println("--- PMS7003 (Partículas) ---");
    if (data->pms7003_valido) {
      Serial.print("  PM1.0: ");
      Serial.print(data->pms7003.pm1_0_atm);
      Serial.println(" µg/m³");
      Serial.print("  PM2.5: ");
      Serial.print(data->pms7003.pm2_5_atm);
      Serial.println(" µg/m³");
      Serial.print("  PM10 : ");
      Serial.print(data->pms7003.pm10_atm);
      Serial.println(" µg/m³");
    } else {
      Serial.println("  Estado: No disponible");
    }
    
    Serial.println("========================================\n");
    
    // Retornar true si al menos uno de los sensores tiene datos válidos
    return (data->ccs811_valido || data->pms7003_valido);
  }
  return false;
}

/**
 * Verifica si ha llegdo alguna alerta al dispositivo.
 * Si no ha llegado devuelve OK, de lo contrario retorna la alerta.
 * También asigna el tiempo en el que se dispara la alerta.
 */
String checkAlert() {
  if (alert.length() != 0) {
    if ((millis() - alertTime) >= ALERT_DURATION * 1000 ) {
      alert = "";
      alertTime = millis();
    }
    return alert;
  } else return "OK";
}

/**
 * Publica los datos de los sensores al tópico configurado usando el cliente MQTT.
 */
void sendSensorData(SensorData * data) {
  // Verificar que el cliente MQTT esté conectado antes de publicar
  if (!client.connected()) {
    Serial.println("⚠ ERROR: Cliente MQTT no conectado. No se puede publicar.");
    Serial.println("Intentando reconectar...");
    reconnect();
    // Si aún no está conectado después de intentar reconectar, salir
    if (!client.connected()) {
      Serial.println("✗ No se pudo reconectar. Datos no enviados.");
      return;
    }
  }
  
  String json = "{";
  
  // Datos del CCS811
  if (data->ccs811_valido) {
    json += "\"co2\": " + String(data->co2) + ", ";
    json += "\"tvoc\": " + String(data->tvoc) + ", ";
  } else {
    json += "\"co2\": 0, ";
    json += "\"tvoc\": 0, ";
  }
  
  // Datos del PMS7003
  if (data->pms7003_valido) {
    json += "\"pm1_0\": " + String(data->pms7003.pm1_0_atm) + ", ";
    json += "\"pm2_5\": " + String(data->pms7003.pm2_5_atm) + ", ";
    json += "\"pm10\": " + String(data->pms7003.pm10_atm);
  } else {
    json += "\"pm1_0\": 0, ";
    json += "\"pm2_5\": 0, ";
    json += "\"pm10\": 0";
  }
  
  json += "}";
  char payload[json.length()+1];
  json.toCharArray(payload, json.length()+1);
  
  Serial.println("\n=== Publicando datos MQTT ===");
  Serial.print("Client ID: ");
  Serial.println(client_id);
  Serial.print("Topic: ");
  Serial.println(MQTT_TOPIC_PUB);
  Serial.print("Payload: ");
  Serial.println(json);
  
  // Publicar con QoS 1 para garantizar entrega
  bool publishResult = client.publish(MQTT_TOPIC_PUB, payload, false);
  
  if (publishResult) {
    Serial.println("✓ Mensaje publicado exitosamente");
    // Procesar mensajes para asegurar que se envíe
    client.loop();
  } else {
    Serial.println("✗ ERROR: Fallo al publicar mensaje MQTT");
    Serial.print("Estado del cliente: ");
    Serial.println(client.state());
    Serial.println("Verificando conexión...");
    if (!client.connected()) {
      Serial.println("Cliente desconectado. Intentando reconectar...");
      reconnect();
    }
  }
  Serial.println("============================\n");
}


/**
 * Función que se ejecuta cuando llega un mensaje a la suscripción MQTT.
 * Construye el mensaje que llegó y si contiene ALERT lo asgina a la variable 
 * alert que es la que se lee para mostrar los mensajes.
 * También verifica si el mensaje es para actualización OTA.
 */
void receivedCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("\n*** CALLBACK MQTT DISPARADO ***");
  Serial.print("Topic recibido: [");
  Serial.print(topic);
  Serial.print("] (longitud payload: ");
  Serial.print(length);
  Serial.println(")");
  
  // Crear buffer para el payload (agregar null terminator)
  // Usar String para evitar problemas con VLA
  String data = "";
  for (unsigned int i = 0; i < length; i++) {
    data += (char)payload[i];
  }
  
  Serial.print("Payload: ");
  Serial.println(data);
  
  // Compara el topic recibido con el topic OTA
  String topicStr = String(topic);
  String otaTopicStr = String(OTA_TOPIC);
  
  Serial.println("--- Comparación de topics ---");
  Serial.print("Topic recibido: '");
  Serial.print(topicStr);
  Serial.print("' (longitud: ");
  Serial.print(topicStr.length());
  Serial.println(")");
  Serial.print("OTA_TOPIC esperado: '");
  Serial.print(otaTopicStr);
  Serial.print("' (longitud: ");
  Serial.print(otaTopicStr.length());
  Serial.println(")");
  Serial.print("¿Coinciden? ");
  Serial.println(topicStr == otaTopicStr ? "SÍ" : "NO");
  
  // Verifica si el mensaje es para actualización OTA
  if (topicStr == otaTopicStr) {
    Serial.println("✓✓✓ Mensaje OTA detectado, procesando...");
    checkOTAUpdate(data.c_str());
    return;
  }
  
  // Verifica si el mensaje contiene una alerta
  if (data.indexOf("ALERT") >= 0) {
    Serial.println("✓ Mensaje ALERT detectado");
    alert = data; // Si el mensaje contiene la palabra ALERT, se asigna a la variable alert
  } else {
    Serial.println("⚠ Mensaje recibido pero no es OTA ni ALERT");
  }
  Serial.println("*** FIN CALLBACK ***\n");
}

/**
 * Función de prueba: Publica un mensaje de prueba y verifica recepción
 * Útil para diagnosticar problemas de MQTT
 */
void testMQTTCallback() {
  if (!client.connected()) {
    Serial.println("⚠ No se puede probar: cliente MQTT no conectado");
    return;
  }
  
  Serial.println("=== TEST MQTT CALLBACK ===");
  Serial.println("Este test verifica que el callback funciona correctamente");
  Serial.println("Publicando mensaje de prueba...");
  
  // Publicar un mensaje de prueba al topic de entrada (para que el dispositivo lo reciba)
  String testTopic = String(MQTT_TOPIC_SUB);
  String testMessage = "TEST_MESSAGE_FROM_SELF";
  
  bool pubResult = client.publish(testTopic.c_str(), testMessage.c_str());
  if (pubResult) {
    Serial.println("✓ Mensaje de prueba publicado");
    Serial.println("Esperando recibirlo en el callback...");
    Serial.println("(Si el callback funciona, deberías ver '*** CALLBACK MQTT DISPARADO ***' arriba)");
  } else {
    Serial.println("✗ Error al publicar mensaje de prueba");
  }
  
  // Procesar mensajes varias veces para asegurar recepción
  for (int i = 0; i < 10; i++) {
    client.loop();
    delay(100);
  }
  
  Serial.println("=== FIN TEST ===");
}