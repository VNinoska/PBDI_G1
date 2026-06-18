constexpr byte PIN_LO_MAS = 25;   // LO+
constexpr byte PIN_LO_MENOS = 26; // LO-
constexpr byte PIN_ECG = 36;     // Entrada Analógica (VP)

// --- CONFIGURACIÓN DE SEGURIDAD ---
// Si tu gráfica se queda en 0 fija, cambia esta línea a 'false'
const bool ACTIVAR_DETECCION_ELECTRODOS = true; 

// Variables para el Filtro Pasa-Banda Digital (Nombres corregidos para evitar conflictos)
float v_x0 = 0, v_x1 = 0, v_x2 = 0; // Historial de entrada
float v_y0 = 0, v_y1 = 0, v_y2 = 0; // Historial de salida

void setup() {
  Serial.begin(115200);
  
  // Configuración del ADC del ESP32 para máxima estabilidad
  analogSetAttenuation(ADC_11db); 
  
  pinMode(PIN_LO_MAS, INPUT);
  pinMode(PIN_LO_MENOS, INPUT);
}
void loop() {
  bool electrodoDesconectado = false;
  if (ACTIVAR_DETECCION_ELECTRODOS) {
    electrodoDesconectado = (digitalRead(PIN_LO_MAS) == HIGH || digitalRead(PIN_LO_MENOS) == HIGH);
  }

  if (electrodoDesconectado) {
    Serial.println(2000); // Enviar línea media para evitar saltos bruscos en el Plotter
  } 
  else {
    // Para mitigar el ruido del ADC del ESP32, promediamos 4 lecturas rápidas (Sobremuestreo)
    long suma = 0;
    for(int i=0; i<4; i++) {
      suma += analogRead(PIN_ECG);
      delayMicroseconds(50); 
    }
    float lecturaPura = suma / 4.0f;
    
    // ---- FILTRO DIGITAL PASA-BANDA ADAPTADO ----
    v_x2 = v_x1; v_x1 = v_x0; 
    v_x0 = lecturaPura;
    v_y2 = v_y1; v_y1 = v_y0;
    
    // Ecuación sintonizada para estabilizar la línea base y eliminar armónicos parásitos
    v_y0 = (0.3584f * v_x0) - (0.7168f * v_x1) + (0.3584f * v_x2) + (1.4225f * v_y1) - (0.5222f * v_y2);
    
    // Desplazamiento óptimo para visualización de 12 bits
    int senalFiltrada = (int)v_y0 + 2000; 
    
    // Imprimir lectura pura y filtrada al mismo tiempo para comparar
    // (En el Serial Plotter verán dos líneas: si una se mueve rítmicamente, ¡es el corazón!)
    Serial.print("Pura:"); Serial.print(lecturaPura);
    Serial.print(",");
    Serial.print("Filtrada:"); Serial.println(senalFiltrada);
  }
  
  delay(10); 
}