#include <Adafruit_MPU6050.h>  //Requiere tenerlo instalado de "Library Manager"
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

// Definición del pin del LED (El GPIO 2 es el LED azul interno en la mayoría de ESP32)
#define LED_PIN 21  // Ahora el pulso de caída saldrá por el pin físico GPIO 20

// Definición de los pines I2C
const int pinSDA = 8;    // Pin conectado a GPIO8 (SDA para ESP32-S3)
const int pinSCL = 9;  // Pin conectado a GPIO9 (SCL para ESP32-S3)
// Umbrales físicos basados en la firma de la caída en el pecho
const float UMBRAL_CAIDA_LIBRE = 0.50; // Menor a 0.5G (Pérdida de soporte/altura)
const float UMBRAL_IMPACTO      = 3.00; // Mayor a 3.0G (Golpe seco contra el suelo/cuerpo)

// Variables de estado para la secuencia temporal
bool posibleCaidaLibre = false;
unsigned long tiempoCaidaLibre = 0;
const unsigned long VENTANA_TIEMPO = 500; // Tiempo máximo (ms) entre la caída libre y el impacto

void setup(void) {
  Serial.begin(115200);
  
  // Configurar el pin del LED como salida
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Asegurar que empiece apagado

  // Inicializar bus I2C nativo (SDA=8, SCL=9)
  Wire.begin(pinSDA, pinSCL);

  if (!mpu.begin(0x68)) {
    Serial.println("¡Error! No se encuentra el MPU6050.");
    while (1) { delay(10); }
  }

  // Rangos óptimos para no saturar el chip durante el golpe
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("--- SISTEMA DE PRUEBA DE CAÍDAS LISTO ---");
  Serial.println("Sostén el sensor y realiza un movimiento brusco hacia abajo simulando el colapso.");
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Convertir aceleraciones crudas (m/s^2) a unidades G
  float ax_g = a.acceleration.x / 9.81;
  float ay_g = a.acceleration.y / 9.81;
  float az_g = a.acceleration.z / 9.81;

  // Calcular la Magnitud Total del Vector Espacial
  float A_total = sqrt(ax_g * ax_g + ay_g * ay_g + az_g * az_g);

  unsigned long tiempoActual = millis();

  // FASE 1: Detectar si el vector cae por debajo del umbral de ingravidez
  if (A_total < UMBRAL_CAIDA_LIBRE) {
    posibleCaidaLibre = true;
    tiempoCaidaLibre = tiempoActual; // Registrar el momento exacto
  }

  // Cancelar la bandera si pasa demasiado tiempo sin que ocurra un impacto
  if (posibleCaidaLibre && (tiempoActual - tiempoCaidaLibre > VENTANA_TIEMPO)) {
    posibleCaidaLibre = false;
  }

  // FASE 2: Detectar el pico del impacto posterior
  if (A_total > UMBRAL_IMPACTO) {
    // Si el impacto ocurre justo después de una caída libre, es una CAÍDA VÁLIDA
    if (posibleCaidaLibre) {
      Serial.print("¡ALERTA DE CAÍDA DETECTADA! Magnitud del impacto: ");
      Serial.print(A_total);
      Serial.println(" G");

      // INDICADOR FÍSICO: Encender LED de alerta
      digitalWrite(LED_PIN, HIGH);
      delay(3000); // Mantener encendido por 3 segundos para que puedan verlo bien
      digitalWrite(LED_PIN, LOW);

      // Reiniciar estados
      posibleCaidaLibre = false;
    }
  }
  delay(10); // Muestreo rápido a 100Hz para no perder el pico del golpe
}