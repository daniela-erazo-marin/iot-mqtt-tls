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

#include <libdisplay.h>

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // Pantalla OLED vinculada al dispositivo

/**
 * Vincula la pantalla al dispositivo y asigna el color de texto blanco como predeterminado.
 * Si no es exitosa la vinculación, se muestra un mensaje en consola.
 * NOTA: I2C debe estar inicializado antes de llamar a esta función.
 */
void startDisplay() {
  // I2C debe estar inicializado antes (se hace en setupSensors() o manualmente)
  // Si no está inicializado, Wire.begin() se llamará automáticamente con pines por defecto
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Dirección 0x3C para 128x64
    Serial.println(F("SSD1306 allocation failed"));
    Serial.println(F("Verifica la conexión I2C de la pantalla OLED"));
    for(;;); // No continúa si no se puede vincular la pantalla
  }
  display.setTextColor(SSD1306_WHITE); // Color de texto blanco
}

/**
 * Imprime en la pantalla un mensaje de "No hay señal".
 */
void displayNoSignal() {
  display.clearDisplay(); // Limpia la pantalla
  display.setTextSize(2); // Tamaño de texto 2
  display.setCursor(10, 10); // Posición del cursor
  display.println("No hay señal"); 
  display.display();
}

/**
 * Agrega a la pantalla el header con mensaje "IOT Sensors" y en seguida la hora actual
 */
void displayHeader(time_t now) {  // Se recibe el tiempo actual como parámetro
  display.setTextSize(1);         // Tamaño de texto 1
  long int milli = now + millis() / 1000; // Se obtiene el tiempo en milisegundos 
  struct tm* tinfo;               // Estructura para almacenar la información de la hora
  tinfo = localtime(&milli);      // Se obtiene la hora actual
  String hour = String(asctime(tinfo)).substring(11, 19); // Se obtiene la hora en formato hh:mm:ss
  String title = "IOT Sensors  " + hour;  // Se crea el título con la hora
  display.println(title);         // Se imprime el título en la pantalla
}

/**
 * Agrega los valores medidos de temperatura y humedad a la pantalla.
 */
void displayMeasures(float temp, float humi) {
  display.println("");
  display.print("Co2: ");
  display.print(temp);  // Se imprime la temperatura
  display.print("    ");
  display.print("tvoc: ");
  display.print(humi);  // Se imprime la humedad
  display.println("");
}

/**
 * Agrega el mensaje indicado a la pantalla.
 * Si el mensaje es OK, se busca mostrarlo centrado.
 */
void displayMessage(String message) {
  display.setTextSize(1);     // Tamaño de texto 1
  display.println("\nMsg:");  // Se imprime el mensaje
  display.setTextSize(2);     // Tamaño de texto 2
  if (message.equals("OK")) { // Si el mensaje es OK
    display.println("    " + message);  // Se imprime centrado
  } else {
    display.println("");      // Se imprime un salto de línea
    display.setTextSize(1);   // Tamaño de texto 1
    display.println(message); // Se imprime el mensaje en la pantalla
  }
}

/**
 * Muestra en la pantalla el mensaje de "Connecting to:" 
 * y luego el nombre de la red a la que se conecta.
 */
void displayConnecting(String ssid) {
  display.clearDisplay();      // Limpia la pantalla
  display.setTextSize(1);      // Tamaño de texto 1
  display.println("Conectando a:\n"); 
  display.println(ssid);      // Se imprime el nombre de la red
  display.display();          // Se muestra el contenido en la pantalla
}

/**
 * Muestra en la pantalla el mensaje recibido.
 * Se recibe el mensaje, la hora actual, la temperatura y la humedad.
 */
void displayLoop(String message, time_t now, float temp, float humi) {
  display.clearDisplay();
  display.setCursor(0,0); 
  displayHeader(now);
  displayMeasures(temp, humi);
  displayMessage(message);
  display.display();
}