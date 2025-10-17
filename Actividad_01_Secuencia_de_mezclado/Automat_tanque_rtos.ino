// ============================================================
// PROGRAMA: Secuencia neumática con máquina de estados (ESP32 + FreeRTOS)
// Descripción: Control de válvulas, motor y sensores mediante tareas paralelas.
// ============================================================

// ------------------------------------------------------------
// CONFIGURACIÓN DEL NÚCLEO DEL ESP32
// ------------------------------------------------------------
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;    // Si el ESP32 usa un solo núcleo -> usar núcleo 0
#else
  static const BaseType_t app_cpu = 1;    // Si usa dos núcleos -> usar el 1 (deja el 0 libre para WiFi/Bluetooth)
#endif


// ------------------------------------------------------------
// DECLARACIÓN DE PINES
// ------------------------------------------------------------

// Entradas (botones y sensores)
const int btnA = 32;   // Botón de inicio
const int sp1  = 33;   // Sensor de posición 1
const int sp2  = 25;   // Sensor de posición 2
const int dl   = 26;   // Sensor límite DL
const int dt   = 27;   // Sensor límite DT

// Salidas (válvulas, motor, etc.)
const int VA = 4;      // Válvula A
const int VB = 2;      // Válvula B
const int V2 = 5;      // Válvula 2
const int V1 = 18;     // Válvula 1
const int M  = 19;     // Motor
const int VS = 21;     // Válvula S


// ------------------------------------------------------------
// VARIABLES DE CONTROL
// ------------------------------------------------------------
int estado = 0;         // Representa el estado actual de la máquina de estados
bool start = false;     // Bandera (actualmente no utilizada)


// ------------------------------------------------------------
// TAREA PRINCIPAL: Secuencia del proceso
// ------------------------------------------------------------
void tareaSecuencia(void *parameter) {
  while (1) {   // Ciclo infinito de la tarea
    switch (estado) {

      // ------------------------------------------------------
      // ESTADO 0: Esperar botón de inicio
      // ------------------------------------------------------
      case 0:
        if (digitalRead(btnA) == LOW) {         // Si el botón se presiona (activo en bajo)
          Serial.println("Inicio de ciclo");
          estado = 1;                           // Avanza al siguiente estado
        }
        break;

      // ------------------------------------------------------
      // ESTADO 1: Activar válvula VA hasta que SP1 se active
      // ------------------------------------------------------
      case 1:
        digitalWrite(VA, HIGH);                 // Enciende válvula A
        if (digitalRead(sp1) == LOW) {          // Si el sensor SP1 se activa
          digitalWrite(VA, LOW);                // Apaga VA
          Serial.println("SP1 detectado -> VA OFF, VB ON");
          estado = 2;                           // Avanza al estado 2
        }
        break;

      // ------------------------------------------------------
      // ESTADO 3: (Versión alternativa, activa motor 10 s)
      // ------------------------------------------------------
      case 3:
        digitalWrite(VB, HIGH);                 // Enciende válvula B
        if (digitalRead(sp2) == LOW) {          // Si SP2 se activa
          digitalWrite(VB, LOW);                // Apaga VB
          Serial.println("SP2 detectado -> VB OFF, V2 ON (5s)");
          digitalWrite(M, HIGH);                // Enciende motor
          vTaskDelay(pdMS_TO_TICKS(10000));     // Espera 10 segundos
          digitalWrite(M, LOW);                 // Apaga motor
          estado = 4;                           // Pasa al estado 4
        }
        break;

      // ------------------------------------------------------
      // ESTADO 2: Activar VB y luego V1 hasta detectar DL
      // ------------------------------------------------------
      case 2:
        digitalWrite(VB, HIGH);                 // Enciende válvula B
        if (digitalRead(sp2) == LOW) {          // Si SP2 detecta posición
           digitalWrite(VB, LOW);               // Apaga VB
           Serial.println("SP2 detectado -> VB OFF, V2 ON (5s)");
        }

        digitalWrite(V1, HIGH);                 // Enciende válvula 1
        if (digitalRead(dl) == LOW) {           // Si sensor DL detecta
           digitalWrite(V1, LOW);               // Apaga V1
           Serial.println("DL detectado -> V1 OFF, M ON (10s)");
        }

        // Nota: Aquí el código pasa directamente al estado 4,
        //       aunque se podrían agregar más pasos intermedios.
        estado = 4;                             // Avanza al estado 4
        break;

      // ------------------------------------------------------
      // ESTADO 4: Activar VS hasta que DT se active
      // ------------------------------------------------------
      case 4:
        digitalWrite(VS, HIGH);                 // Enciende válvula S
        if (digitalRead(dt) == LOW) {           // Si sensor DT detecta
          digitalWrite(VS, LOW);                // Apaga VS
          Serial.println("DT detectado -> VS OFF, ciclo finalizado");
          estado = 0;                           // Regresa al inicio del ciclo
        }
        break;
    }

    // Pausa corta para no saturar la CPU
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}


// ------------------------------------------------------------
// TAREA DE SEGURIDAD: Apagar todo si no hay ciclo activo
// ------------------------------------------------------------
void tareaSeguridad(void *parameter) {
  while (1) {
    if (estado == 0) {                   // Si el sistema está en reposo
      digitalWrite(VA, LOW);             // Apaga todas las salidas
      digitalWrite(VB, LOW);
      digitalWrite(V2, LOW);
      digitalWrite(V1, LOW);
      digitalWrite(M, LOW);
      digitalWrite(VS, LOW);
    }
    vTaskDelay(pdMS_TO_TICKS(100));      // Revisa cada 100 ms
  }
}


// ------------------------------------------------------------
// TAREA DE MONITOREO SERIAL
// ------------------------------------------------------------
void tareaMonitor(void *parameter) {
  while (1) {
    Serial.print("Estado actual: ");      // Imprime el estado actual
    Serial.println(estado);
    vTaskDelay(pdMS_TO_TICKS(1000));      // Cada 1 segundo
  }
}


// ------------------------------------------------------------
// CONFIGURACIÓN INICIAL
// ------------------------------------------------------------
void setup() {
  Serial.begin(115200);                   // Inicializa comunicación serial

  // Configura entradas con resistencias internas pull-up
  pinMode(btnA, INPUT_PULLUP);
  pinMode(sp1, INPUT_PULLUP);
  pinMode(sp2, INPUT_PULLUP);
  pinMode(dl, INPUT_PULLUP);
  pinMode(dt, INPUT_PULLUP);

  // Configura salidas
  pinMode(VA, OUTPUT);
  pinMode(VB, OUTPUT);
  pinMode(V2, OUTPUT);
  pinMode(V1, OUTPUT);
  pinMode(M, OUTPUT);
  pinMode(VS, OUTPUT);

  // --------------------------------------------------------
  // CREACIÓN DE TAREAS EN FreeRTOS
  // --------------------------------------------------------
  xTaskCreatePinnedToCore(tareaSecuencia, "Secuencia", 4096, NULL, 1, NULL, app_cpu); // Tarea principal
  xTaskCreatePinnedToCore(tareaSeguridad, "Seguridad", 2048, NULL, 1, NULL, app_cpu); // Apagado de seguridad
  xTaskCreatePinnedToCore(tareaMonitor,   "Monitor",   2048, NULL, 1, NULL, app_cpu); // Monitor serial
}


// ------------------------------------------------------------
// LOOP PRINCIPAL VACÍO
// ------------------------------------------------------------
void loop() {
  // No se usa. Todas las tareas corren en FreeRTOS.
}
