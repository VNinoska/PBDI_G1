/*Se cargó el código a un ESP32S NodeMCU (ESP-WROOM-32) con la sgte conexión:
  Herramientas → Placa → ESP32 Arduino → ESP32 Dev Module

Posible Driver USB-Serie faltante (CP2102)
En ese caso, utilizar la sgte página:
https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads
*/

// Definición de los pines según tu esquema
const int pinLO_mas = 32;    // Pin conectado a LO+
const int pinLO_menos = 33;  // Pin conectado a LO-
const int pinECG = 36;       // Pin VP (Sensor OUTPUT)

bool continuar = true;  // OPCIONAL: Controla si se imprimen los datos o no

// Configuración del filtro para suavizar la curva
const int TAMANO_FILTRO = 10;
int lecturas[TAMANO_FILTRO];
int indiceLectura = 0;
long totalSuma = 0;

void setup() {
  // Iniciar la comunicación con la computadora a 115200 baudios
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);  // Rango completo de 0V a 3.3V

  // Configurar los pines de detección de electrodos como entradas
  pinMode(pinLO_mas, INPUT);
  pinMode(pinLO_menos, INPUT);

  // Inicializar el filtro en 0
  for (int i = 0; i < TAMANO_FILTRO; i++) lecturas[i] = 0;
}

void loop() {
  //OPCIONAL: Config. extra para parar a seguir trasmisión con order de Monitor Serial
  // 1. Revisar si el usuario envió algo por el Monitor Serial
  if (Serial.available() > 0) {
    char comando = Serial.read();  // Leer el carácter enviado

    if (comando == 'P' || comando == 'p') {
      continuar = false;
      Serial.println("\n--- IMPRESIÓN PAUSADA ---");
    } else if (comando == 'C' || comando == 'c') {
      continuar = true;
      Serial.println("\n--- IMPRESIÓN REANUDADA ---");
    }
  }
  // 2. Imprimir datos solo si la variable 'continuar' es verdadera
  if (continuar) {
    
    // Verificar si algún electrodo está desconectado
    if ((digitalRead(pinLO_mas) == 1) || (digitalRead(pinLO_menos) == 1)) {
      // Si están desconectados, enviamos un valor constante (línea plana)
      Serial.println("!");
    } else {
      // Si están bien conectados, leemos y enviamos la señal analógica del corazón
      //int valorECG = analogRead(pinECG);
      //Serial.println(valorECG);

      // ---- FILTRO PROMEDIO MÓVIL ----
      totalSuma = totalSuma - lecturas[indiceLectura];
      lecturas[indiceLectura] = analogRead(pinECG);
      totalSuma = totalSuma + lecturas[indiceLectura];
      indiceLectura = (indiceLectura + 1) % TAMANO_FILTRO;

      int senalSuave = totalSuma / TAMANO_FILTRO;
      // --------------------------------

      Serial.println(senalSuave);
    }
  }
  // Pequeña pausa de 10 milisegundos para estabilizar la lectura
  delay(10);
}