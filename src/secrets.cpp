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

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <libiot.h>
#include <libwifi.h>
#include <stdio.h>

/*********** Inicio de parametros configurables por el usuario *********/

// Variables de entorno - se configuran en platformio.ini o .env
// Los topicos deben tener la estructura: <país>/<estado>/<ciudad>/<usuario>/out
#ifndef COUNTRY
#define COUNTRY "colombia"                        ///< País (definir vía .env)
#endif
#ifndef STATE
#define STATE "valle"                           ///< Estado/Departamento (definir vía .env)
#endif
#ifndef CITY
#define CITY "tulua"                            ///< Ciudad (definir vía .env)
#endif
#ifndef MQTT_SERVER
#define MQTT_SERVER "mqtt.daniela.freeddns.org"                     ///< Servidor MQTT (definir vía .env)
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 8883                            ///< Puerto seguro (TLS)
#endif
#ifndef MQTT_USER
#define MQTT_USER "admin"                         ///< Usuario MQTT (definir vía .env)
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD "admin*"                    ///< Contraseña MQTT (definir vía .env)
#endif

// Variables de configuración de la red WiFi
#ifndef WIFI_SSID
#define WIFI_SSID "MARIN_G"                       ///< SSID por defecto vacío; usar aprovisionamiento
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "Jacobo20035"                   ///< Password por defecto vacío; usar aprovisionamiento
#endif

// Alias para compatibilidad con el código existente
#define SSID WIFI_SSID
#define PASSWORD WIFI_PASSWORD

// Certificado raíz - se configura como variable de entorno
#ifndef ROOT_CA
#define ROOT_CA "-----BEGIN CERTIFICATE-----MIIDnzCCAyWgAwIBAgISBaenrZXmRt9jw0c5zp/lgXLLMAoGCCqGSM49BAMDMDIxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MQswCQYDVQQDEwJFNzAeFw0yNTExMjEyMjA0MDlaFw0yNjAyMTkyMjA0MDhaMCQxIjAgBgNVBAMTGW1xdHQuZGFuaWVsYS5mcmVlZGRucy5vcmcwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQ+QjlcbDx5B0mSe7psuwebOhVLIIRarat2OkC/dThYbgj1JMeoCQP89BZxNuSI191SLIiU5BfEUsCWh0yRRrFLo4ICJzCCAiMwDgYDVR0PAQH/BAQDAgeAMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAMBgNVHRMBAf8EAjAAMB0GA1UdDgQWBBTRmA9XZeA1rYs1IP35Keb4BmOedDAfBgNVHSMEGDAWgBSuSJ7chx1EoG/aouVgdAR4wpwAgDAyBggrBgEFBQcBAQQmMCQwIgYIKwYBBQUHMAKGFmh0dHA6Ly9lNy5pLmxlbmNyLm9yZy8wJAYDVR0RBB0wG4IZbXF0dC5kYW5pZWxhLmZyZWVkZG5zLm9yZzATBgNVHSAEDDAKMAgGBmeBDAECATAtBgNVHR8EJjAkMCKgIKAehhxodHRwOi8vZTcuYy5sZW5jci5vcmcvNzYuY3JsMIIBBAYKKwYBBAHWeQIEAgSB9QSB8gDwAHYAyzj3FYl8hKFEX1vB3fvJbvKaWc1HCmkFhbDLFMMUWOcAAAGaqKfJaAAABAMARzBFAiEA2VmxrENtA0576gBAADaaG499tdyqtSCm21tMGhOJNPkCIBhmugre3BlBNwPGo5sWjfzjUNy3NkrzQfZQfr0se9BnAHYADleUvPOuqT4zGyyZB7P3kN+bwj1xMiXdIaklrGHFTiEAAAGaqKfJKAAABAMARzBFAiAJuSA0hl5nkFxdqFyoFLlBIsmWiGduaQbCU2pIRp7pcQIhAO4TDte1KkpYb+TnHVoGayeoYw8U1rr/HCOrylXA04TGMAoGCCqGSM49BAMDA2gAMGUCME/CMMo+dhYXCFt5gLxM2yjZTfAOOpDxJwdYe8ECZJtTypPui0LS8R1BqVbu6tCyUAIxANybOhY8XFaECwga7m5OF0W7xl5OLMMLL+7wGQ54ISFrwx1ryPa60Et7G8WLlmGlWg==-----END CERTIFICATE----------BEGIN CERTIFICATE-----MIIEVzCCAj+gAwIBAgIRAKp18eYrjwoiCWbTi7/UuqEwDQYJKoZIhvcNAQELBQAwTzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2VhcmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjQwMzEzMDAwMDAwWhcNMjcwMzEyMjM1OTU5WjAyMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3MgRW5jcnlwdDELMAkGA1UEAxMCRTcwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAARB6ASTCFh/vjcwDMCgQer+VtqEkz7JANurZxLP+U9TCeioL6sp5Z8VRvRbYk4P1INBmbefQHJFHCxcSjKmwtvGBWpl/9ra8HW0QDsUaJW2qOJqceJ0ZVFT3hbUHifBM/2jgfgwgfUwDgYDVR0PAQH/BAQDAgGGMB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcDATASBgNVHRMBAf8ECDAGAQH/AgEAMB0GA1UdDgQWBBSuSJ7chx1EoG/aouVgdAR4wpwAgDAfBgNVHSMEGDAWgBR5tFnme7bl5AFzgAiIyBpY9umbbjAyBggrBgEFBQcBAQQmMCQwIgYIKwYBBQUHMAKGFmh0dHA6Ly94MS5pLmxlbmNyLm9yZy8wEwYDVR0gBAwwCjAIBgZngQwBAgEwJwYDVR0fBCAwHjAcoBqgGIYWaHR0cDovL3gxLmMubGVuY3Iub3JnLzANBgkqhkiG9w0BAQsFAAOCAgEAjx66fDdLk5ywFn3CzA1w1qfylHUDaEf0QZpXcJseddJGSfbUUOvbNR9N/QQ16K1lXl4VFyhmGXDT5Kdfcr0RvIIVrNxFh4lqHtRRCP6RBRstqbZ2zURgqakn/Xip0iaQL0IdfHBZr396FgknniRYFckKORPGyM3QKnd66gtMst8I5nkRQlAg/Jb+Gc3egIvuGKWboE1G89NTsN9LTDD3PLj0dUMrOIuqVjLB8pEC6yk9enrlrqjXQgkLEYhXzq7dLafv5Vkig6Gl0nuuqjqfp0Q1bi1oyVNAlXe6aUXw92CcghC9bNsKEO1+M52YY5+ofIXlS/SEQbvVYYBLZ5yeiglV6t3SM6H+vTG0aP9YHzLn/KVOHzGQfXDP7qM5tkf+7diZe7o2fw6O7IvN6fsQXEQQj8TJUXJxv2/uJhcuy/tSDgXwHM8Uk34WNbRT7zGTGkQRX0gsbjAea/jYAoWv0ZvQRwpqPe79D/i7Cep8qWnA+7AE/3B3S/3dEEYmc0lpe1366A/6GEgk3ktr9PEoQrLChs6Itu3wnNLB2euC8IKGLQFpGtOO/2/hiAKjyajaBP25w1jF0Wl8Bbqne3uZ2q1GyPFJYRmT7/OXpmOH/FVLtwS+8ng1cAmpCujPwteJZNcDG0sF2n/sc0+SQf49fdyUK0ty+VUwFj9tmWxyR/M=-----END CERTIFICATE-----"                     ///< CA vacía por defecto; definir vía .env
#endif

const char* root_ca = ROOT_CA;

/*********** Fin de parametros configurables por el usuario ***********/


/* Constantes de configuración del servidor MQTT, no cambiar */
const char* mqtt_server = MQTT_SERVER;            ///< Dirección de tu servidor MQTT
const int mqtt_port = MQTT_PORT;                  ///< Puerto seguro (TLS)
const char* mqtt_user = MQTT_USER;                ///< Usuario MQTT
const char* mqtt_password = MQTT_PASSWORD;        ///< Contraseña MQTT

// Obtener la MAC Address
String macAddress = getMacAddress();
const char * client_id = macAddress.c_str();      ///< ID del cliente MQTT

// Tópicos de publicación y suscripción
String mqtt_topic_pub( String(COUNTRY) + "/" + String(STATE) + "/"+ String(CITY) + "/" + String(client_id) + "/" + String(mqtt_user) + "/out");
String mqtt_topic_sub( String(COUNTRY) + "/" + String(STATE) + "/"+ String(CITY) + "/" + String(client_id) + "/" + String(mqtt_user) + "/in");

// Convertir los tópicos a constantes de tipo char*
const char * MQTT_TOPIC_PUB = mqtt_topic_pub.c_str();
const char * MQTT_TOPIC_SUB = mqtt_topic_sub.c_str();

long long int measureTime = millis();   // Tiempo de la última medición
long long int alertTime = millis();     // Tiempo en que inició la última alerta
WiFiClientSecure espClient;             // Conexión TLS/SSL con el servidor MQTT
PubSubClient client(espClient);         // Cliente MQTT para la conexión con el servidor
time_t now;                             // Timestamp de la fecha actual.
const char* ssid = SSID;                // Cambia por el nombre de tu red WiFi
const char* password = PASSWORD;        // Cambia por la contraseña de tu red WiFi