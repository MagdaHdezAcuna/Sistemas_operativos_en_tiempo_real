// ==========================================================
// SEMÁFOROS CON BOTONES Y SENSORES - CONTROL MUTUO
// ==========================================================
// Descripción:
// Este código controla dos semáforos con prioridad de paso.
// Cada uno tiene un botón para solicitar el turno y un sensor
// que indica cuándo ha terminado su paso. Solo un semáforo puede
// estar en verde a la vez, y si el otro pide turno mientras tanto,
// queda en espera hasta que el primero libere el paso.
// ==========================================================

// --- Selección del núcleo del ESP32 ---
// Si el microcontrolador solo tiene un núcleo (unicore), se usa el 0.
// En ESP32 de dos núcleos, usamos el núcleo 1 para las tareas de usuario.
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// ==========================================================
// Definición de pines
// ==========================================================

// Pines del semáforo 1
const int ROJO_1 = 19;
const int VERDE_1 = 18;

// Pines del semáforo 2
const int ROJO_2 = 21;
const int VERDE_2 = 5;

// Botones de solicitud
const int BTN_1 = 25;
const int BTN_2 = 26;

// Sensores de presencia o detección
const int SENSOR_1 = 23;
const int SENSOR_2 = 22;

// ==========================================================
// Variables globales y semáforos FreeRTOS
// ==========================================================

// Semáforo binario: controla el acceso (solo un paso activo)
static SemaphoreHandle_t semaforoAcceso;

// Variables de control de turno
volatile int turno = 0;           // 0 = libre, 1 = semáforo 1 activo, 2 = semáforo 2 activo
volatile int turnoPendiente = 0;  // 0 = ninguno, 1 o 2 = espera para su turno

// ==========================================================
// Prototipos de tareas (funciones que ejecuta FreeRTOS)
// ==========================================================
void tareaBoton1(void *parameter);
void tareaBoton2(void *parameter);
void tareaSemaforo1(void *parameter);
void tareaSemaforo2(void *parameter);

// ==========================================================
// CONFIGURACIÓN INICIAL
// ==========================================================
void setup() {
  Serial.begin(115200);

  // Configura pines de salida (LEDs)
  pinMode(ROJO_1, OUTPUT);
  pinMode(VERDE_1, OUTPUT);
  pinMode(ROJO_2, OUTPUT);
  pinMode(VERDE_2, OUTPUT);

  // Configura pines de entrada (botones y sensores)
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(SENSOR_1, INPUT);
  pinMode(SENSOR_2, INPUT);

  // Estado inicial: ambos en rojo
  digitalWrite(ROJO_1, HIGH);
  digitalWrite(VERDE_1, LOW);
  digitalWrite(ROJO_2, HIGH);
  digitalWrite(VERDE_2, LOW);

  // Crea el semáforo binario (solo un paso a la vez)
  semaforoAcceso = xSemaphoreCreateBinary();
  xSemaphoreGive(semaforoAcceso);  // se libera al inicio (sistema libre)

  // Crea las tareas y las asigna al núcleo definido
  xTaskCreatePinnedToCore(tareaBoton1, "Boton1", 2048, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore(tareaBoton2, "Boton2", 2048, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore(tareaSemaforo1, "Semaforo1", 2048, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore(tareaSemaforo2, "Semaforo2", 2048, NULL, 1, NULL, app_cpu);
}

void loop() {
  // Nada aquí, todo lo maneja FreeRTOS mediante tareas
}

// ==========================================================
// TAREAS
// ==========================================================

// ---------------- BOTÓN 1 ----------------
void tareaBoton1(void *parameter) {
  bool lastState = HIGH;  // Estado anterior del botón (por defecto sin presionar)
  while (1) {
    bool currentState = digitalRead(BTN_1);  // Lee el estado actual

    // Detecta flanco de bajada (presión)
    if (lastState == HIGH && currentState == LOW) {
      vTaskDelay(50 / portTICK_PERIOD_MS);  // Retardo antirrebote

      if (digitalRead(BTN_1) == LOW) {  // Confirmar que sigue presionado
        if (turno == 0) {  // Si nadie tiene el turno
          if (xSemaphoreTake(semaforoAcceso, 0) == pdTRUE) {  // Toma el semáforo
            turno = 1;
            Serial.println("Semáforo 1 ACTIVADO");
          }
        } else if (turno != 1) {  // Si el otro semáforo está activo
          turnoPendiente = 1;
          Serial.println("Semáforo 1 EN ESPERA");
        }
      }
    }

    lastState = currentState;  // Actualiza estado anterior
    vTaskDelay(50 / portTICK_PERIOD_MS);  // Pequeña espera
  }
}

// ---------------- BOTÓN 2 ----------------
void tareaBoton2(void *parameter) {
  bool lastState = HIGH;
  while (1) {
    bool currentState = digitalRead(BTN_2);

    if (lastState == HIGH && currentState == LOW) {
      vTaskDelay(50 / portTICK_PERIOD_MS);
      if (digitalRead(BTN_2) == LOW) {
        if (turno == 0) {  // Si no hay nadie en turno
          if (xSemaphoreTake(semaforoAcceso, 0) == pdTRUE) {
            turno = 2;
            Serial.println("Semáforo 2 ACTIVADO");
          }
        } else if (turno != 2) {  // Si el otro semáforo está activo
          turnoPendiente = 2;
          Serial.println("Semáforo 2 EN ESPERA");
        }
      }
    }

    lastState = currentState;
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ---------------- SEMÁFORO 1 ----------------
void tareaSemaforo1(void *parameter) {
  while (1) {
    if (turno == 1) {  // Si este semáforo tiene el turno

      // Enciende verde del semáforo 1, rojo del 2
      digitalWrite(ROJO_1, LOW);
      digitalWrite(VERDE_1, HIGH);
      digitalWrite(ROJO_2, HIGH);
      digitalWrite(VERDE_2, LOW);
      Serial.println("Semáforo 1 EN VERDE");

      // Espera hasta que el sensor detecte paso
      while (digitalRead(SENSOR_1) == LOW) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }

      // Al detectar salida, regresa a rojo
      Serial.println("Sensor 1 detectó salida → Liberando control");
      digitalWrite(VERDE_1, LOW);
      digitalWrite(ROJO_1, HIGH);

      // Si el otro semáforo estaba esperando, le pasa el turno
      if (turnoPendiente == 2) {
        turnoPendiente = 0;
        turno = 2;
        Serial.println("Cambio automático a Semáforo 2");
      } else {
        turno = 0;
        xSemaphoreGive(semaforoAcceso);  // Libera el semáforo global
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// ---------------- SEMÁFORO 2 ----------------
void tareaSemaforo2(void *parameter) {
  while (1) {
    if (turno == 2) {  // Si este semáforo tiene el turno

      // Enciende verde del semáforo 2, rojo del 1
      digitalWrite(ROJO_2, LOW);
      digitalWrite(VERDE_2, HIGH);
      digitalWrite(ROJO_1, HIGH);
      digitalWrite(VERDE_1, LOW);
      Serial.println("Semáforo 2 EN VERDE");

      // Espera a que el sensor detecte paso
      while (digitalRead(SENSOR_2) == LOW) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }

      // Detectó salida
      Serial.println("Sensor 2 detectó salida → Liberando control");
      digitalWrite(VERDE_2, LOW);
      digitalWrite(ROJO_2, HIGH);

      // Si hay solicitud pendiente del otro lado, cambia turno
      if (turnoPendiente == 1) {
        turnoPendiente = 0;
        turno = 1;
