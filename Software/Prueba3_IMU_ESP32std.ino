#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "arduinoFFT.h"  

Adafruit_MPU6050 mpu;

// DEFINICIÓN DE PINES SEGÚN TUS ESPECIFICACIONES (ESP32)
const int LED_VERDE    = 2;  // GPIO D2: Sistema OK / Reposo
const int LED_AMARILLO = 4;  // GPIO D4: En proceso de Caída Libre
const int LED_ROJO     = 18; // GPIO D18: TRIGGER 1: Caída detectada
const int LED_AZUL     = 5;  // GPIO D5: TRIGGER 2: Convulsión Clónica FFT

// 1. CONFIGURACIÓN DE MUESTREO (Cuppens 2012)
const int FRECUENCIA_MUESTREO = 50; 
const int PERIODO_MUESTREO_MS = 1000 / FRECUENCIA_MUESTREO; 
unsigned long ultimoTiempoMuestreo = 0; 
bool ejecutando = true; 

// 2. PARÁMETROS DE CAÍDA (Paso 2 - Bourke 2007)
const float UMBRAL_CAIDA_LIBRE = 0.61; 
const float UMBRAL_IMPACTO     = 3.50; 
enum EstadoCaida { REPOSO, EN_CAIDA_LIBRE };
EstadoCaida estadoCaidaActual = REPOSO;
unsigned long tiempoInicioCaida = 0;
const unsigned long VENTANA_MAXIMA_CAIDA_MS = 1500; 

// 3. PARÁMETROS DE CONFIGURACIÓN DE FFT (Paso 3 - Enfoque Espectral)
const uint16_t SAMPLES = 128; 
double vReal[SAMPLES];
double vImag[SAMPLES];
uint16_t indiceMuestras = 0;

// Instanciar objeto FFT
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, (double)FRECUENCIA_MUESTREO);

const double UMBRAL_ENERGIA_RELATIVA = 0.45; 
int ventanasClonicasConsecutivas = 0;
const int VENTANAS_REQUERIDAS_ALERTA = 3;    

void dispararTriggerECG(String tipoCrisis, int pinLedAlarma);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10); 

  // CONFIGURAR PINES DE LEDS COMO SALIDAS
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);

  // Encendido inicial de diagnóstico (Verde activo)
  digitalWrite(LED_VERDE, HIGH);
  digitalWrite(LED_AMARILLO, LOW);
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_AZUL, LOW);

  if (!mpu.begin()) {
    Serial.println("¡Error MPU6050!");
    while (1) {
      digitalWrite(LED_AMARILLO, HIGH); delay(200);
      digitalWrite(LED_AMARILLO, LOW);  delay(200);
    }
  } else {
    Serial.println("MPU6050 Inicializado a 50Hz para pruebas físicas.");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    delay(100);
  }
}

void loop() {
  unsigned long tiempoActual = millis();

  // --- CONTROL DE PAUSA POR TECLADO ---
  if (Serial.available() > 0) {
    char tecla = Serial.read(); 
    if (tecla == 't' || tecla == 'T') { 
      ejecutando = false; 
      digitalWrite(LED_VERDE, LOW); 
      Serial.println("\n[PAUSADO]"); 
    } 
    else if (tecla == 's' || tecla == 'S') { 
      ejecutando = true; 
      ultimoTiempoMuestreo = millis(); 
      digitalWrite(LED_VERDE, HIGH); 
      Serial.println("\n[REANUDADO]"); 
    }
  }

  if (ejecutando) {
    if (tiempoActual - ultimoTiempoMuestreo >= PERIODO_MUESTREO_MS) {
      ultimoTiempoMuestreo = tiempoActual;

      sensors_event_t a, g, temp; 
      mpu.getEvent(&a, &g, &temp);

      float aam = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z, 2));
      float aam_g = aam / 9.80665;

      vReal[indiceMuestras] = (double)aam_g;
      vImag[indiceMuestras] = 0.0; 
      indiceMuestras++;

      // ==========================================
      // PASOS B Y C: MAQUINA DE ESTADOS DE CAÍDA
      // ==========================================
      switch(estadoCaidaActual) {
        case REPOSO:
          if (aam_g < UMBRAL_CAIDA_LIBRE) {
            estadoCaidaActual = EN_CAIDA_LIBRE;
            tiempoInicioCaida = tiempoActual;
            digitalWrite(LED_AMARILLO, HIGH); // LED Amarillo ON en Caída Libre
          }
          break;
        case EN_CAIDA_LIBRE:
          if (tiempoActual - tiempoInicioCaida > VENTANA_MAXIMA_CAIDA_MS) {
            estadoCaidaActual = REPOSO;
            digitalWrite(LED_AMARILLO, LOW); // Falsa alarma
          } else if (aam_g > UMBRAL_IMPACTO) {
            digitalWrite(LED_AMARILLO, LOW); 
            dispararTriggerECG("CAIDA / DROP ATTACK", LED_ROJO); // LED Rojo en Impacto
            estadoCaidaActual = REPOSO;
          }
          break;
      }

      // ==========================================
      // PASO D: PROCESAMIENTO SPECTRAL FFT
      // ==========================================
      if (indiceMuestras == SAMPLES) {
        double suma = 0;
        for (int i = 0; i < SAMPLES; i++) suma += vReal[i];
        double media = suma / SAMPLES;
        for (int i = 0; i < SAMPLES; i++) vReal[i] -= media;

        // CORRECCIÓN AQUÍ: Usamos la constante universal de Hamming (FFT_WIN_TYP_HAMMING)
        FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
        FFT.compute(FFT_FORWARD);
        FFT.complexToMagnitude();

        double energiaTotal = 0;
        double energiaBandaCrisis = 0;

        for (int i = 1; i < (SAMPLES / 2); i++) {
          double frecuenciaActual = (i * (double)FRECUENCIA_MUESTREO) / SAMPLES;
          double magnitudSquared = pow(vReal[i], 2);

          energiaTotal += magnitudSquared;

          if (frecuenciaActual >= 2.0 && frecuenciaActual <= 5.0) {
            energiaBandaCrisis += magnitudSquared;
          }
        }

        double ratioEnergia = 0.0;
        if (energiaTotal > 0.01) { 
          ratioEnergia = energiaBandaCrisis / energiaTotal;
        }

        if (ratioEnergia >= UMBRAL_ENERGIA_RELATIVA) {
          ventanasClonicasConsecutivas++;
          // Pequeño parpadeo de validación en el LED azul
          digitalWrite(LED_AZUL, HIGH); delay(20); digitalWrite(LED_AZUL, LOW);
          
          Serial.print("-> [OK SPECTRAL] Ventana rítmica. Consecutivas: ");
          Serial.println(ventanasClonicasConsecutivas);
        } else {
          ventanasClonicasConsecutivas = 0; 
        }

        if (ventanasClonicasConsecutivas >= VENTANAS_REQUERIDAS_ALERTA) {
          dispararTriggerECG("CONVULSIÓN CLÓNICA (FFT 2-5 Hz)", LED_AZUL); // LED Azul en Alerta Rítmica Sostenida
          ventanasClonicasConsecutivas = 0;
        }

        indiceMuestras = 0;
      }
    }
  } 
}

void dispararTriggerECG(String tipoCrisis, int pinLedAlarma) {
  Serial.println("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  Serial.print("!!! TRIGGER DISPARADO EN SEGUNDO PLANO: "); Serial.println(tipoCrisis);
  Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  
  digitalWrite(LED_VERDE, LOW);
  
  // Alarma visual: Parpadeo intenso del LED asignado por 4 segundos
  for(int i = 0; i < 8; i++) {
    digitalWrite(pinLedAlarma, HIGH); delay(250);
    digitalWrite(pinLedAlarma, LOW);  delay(250);
  }
  
  digitalWrite(LED_VERDE, HIGH);
}