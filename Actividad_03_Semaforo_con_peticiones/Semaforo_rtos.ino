
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// Pines semáforo 1
const int ROJO_1 = 19;
const int VERDE_1 = 18;

// Pines semáforo 2
const int ROJO_2 = 21;
const int VERDE_2 = 5;

// Botones
const int BTN_1 = 25;
const int BTN_2 = 26;

// Sensores de presencia
const int SENSOR_1 = 23;
const int SENSOR_2 = 22;

// Semáforo binario para acceso
static SemaphoreHandle_t semaforoAcceso;

// Variables de control
volatile int turno = 0;           // 0 = libre, 1 = semáforo 1, 2 = semáforo 2
volatile int turnoPendiente = 0;  // 0 = ninguno, 1 = espera 1, 2 = espera 2

// Prototipos
void tareaBoton1(void *parameter);
void tareaBoton2(void *parameter);
void tareaSemaforo1(void *parameter);
void tareaSemaforo2(void *parameter);

void setup() {
  Serial.begin(115200);

  // Pines de salida
  pinMode(ROJO_1, OUTPUT);
  pinMode(VERDE_1, OUTPUT);
  pinMode(ROJO_2, OUTPUT);
  pinMode(VERDE_2, OUTPUT);

  // Entradas
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(SENSOR_1, INPUT);
  pinMode(SENSOR_2, INPUT);

  // Estado inicial
  digitalWrite(ROJO_1, HIGH);
  digitalWrite(VERDE_1, LOW);
  digitalWrite(ROJO_2, HIGH);
  digitalWrite(VERDE_2, LOW);

  // Semáforo binario (solo un paso a la vez)
  semaforoAcceso = xSemaphoreCreateBinary();
  xSemaphoreGive(semaforoAcceso);  // libre al inicio

  // Tareas
  xTaskCreatePinnedToCore(tareaBoton1, "Boton1", 2048, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore(tareaBoton2, "Boton2", 2048, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore(tareaSemaforo1, "Semaforo1", 2048, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore(tareaSemaforo2, "Semaforo2", 2048, NULL, 1, NULL, app_cpu);
}

void loop() {}

// ==========================================================
// Tareas
// ==========================================================

// --- BOTÓN 1 ---
void tareaBoton1(void *parameter) {
  bool lastState = HIGH;
  while (1) {
    bool currentState = digitalRead(BTN_1);
    if (lastState == HIGH && currentState == LOW) {
      vTaskDelay(50 / portTICK_PERIOD_MS);  // antirrebote
      if (digitalRead(BTN_1) == LOW) {
        if (turno == 0) {  // nadie tiene el turno
          if (xSemaphoreTake(semaforoAcceso, 0) == pdTRUE) {
            turno = 1;
            Serial.println("Semáforo 1 ACTIVADO");
          }
        } else if (turno != 1) {  // ya hay otro activo
          turnoPendiente = 1;
          Serial.println("Semáforo 1 EN ESPERA");
        }
      }
    }
    lastState = currentState;
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// --- BOTÓN 2 ---
void tareaBoton2(void *parameter) {
  bool lastState = HIGH;
  while (1) {
    bool currentState = digitalRead(BTN_2);
    if (lastState == HIGH && currentState == LOW) {
      vTaskDelay(50 / portTICK_PERIOD_MS);
      if (digitalRead(BTN_2) == LOW) {
        if (turno == 0) {
          if (xSemaphoreTake(semaforoAcceso, 0) == pdTRUE) {
            turno = 2;
            Serial.println("Semáforo 2 ACTIVADO");
          }
        } else if (turno != 2) {
          turnoPendiente = 2;
          Serial.println("Semáforo 2 EN ESPERA");
        }
      }
    }
    lastState = currentState;
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// --- SEMÁFORO 1 ---
void tareaSemaforo1(void *parameter) {
  while (1) {
    if (turno == 1) {
      // Estado verde para semáforo 1
      digitalWrite(ROJO_1, LOW);
      digitalWrite(VERDE_1, HIGH);
      digitalWrite(ROJO_2, HIGH);
      digitalWrite(VERDE_2, LOW);
      Serial.println("Semáforo 1 EN VERDE");

      // Espera sensor 1
      while (digitalRead(SENSOR_1) == LOW) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }

      // Sensor detectado → liberar
      Serial.println("Sensor 1 detectó salida → Liberando control");
      digitalWrite(VERDE_1, LOW);
      digitalWrite(ROJO_1, HIGH);

      // Revisa si hay un turno pendiente
      if (turnoPendiente == 2) {
        turnoPendiente = 0;
        turno = 2;
        Serial.println("Cambio automático a Semáforo 2");
      } else {
        turno = 0;
        xSemaphoreGive(semaforoAcceso);
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// --- SEMÁFORO 2 ---
void tareaSemaforo2(void *parameter) {
  while (1) {
    if (turno == 2) {
      // Estado verde para semáforo 2
      digitalWrite(ROJO_2, LOW);
      digitalWrite(VERDE_2, HIGH);
      digitalWrite(ROJO_1, HIGH);
      digitalWrite(VERDE_1, LOW);
      Serial.println("Semáforo 2 EN VERDE");

      // Espera sensor 2
      while (digitalRead(SENSOR_2) == LOW) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }

      Serial.println("Sensor 2 detectó salida → Liberando control");
      digitalWrite(VERDE_2, LOW);
      digitalWrite(ROJO_2, HIGH);

      // Revisa si hay turno pendiente
      if (turnoPendiente == 1) {
        turnoPendiente = 0;
        turno = 1;
        Serial.println("Cambio automático a Semáforo 1");
      } else {
        turno = 0;
        xSemaphoreGive(semaforoAcceso);
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
