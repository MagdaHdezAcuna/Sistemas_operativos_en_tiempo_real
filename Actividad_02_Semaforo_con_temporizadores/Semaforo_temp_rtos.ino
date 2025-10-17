#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// Pines de LEDs (ajústalos a tu conexión)
const int LED_ROJO = 15;
const int LED_AMARILLO = 2;
const int LED_VERDE = 4;

static TimerHandle_t timerSemaforo = NULL;
static TimerHandle_t timerAmarilloBlink = NULL;

int estado = 0;       // 0=Rojo, 1=Amarillo, 2=Verde
int blinkCount = 0;   // contador de parpadeos del amarillo

// --- Callback principal del semáforo ---
void semaforoCallback(TimerHandle_t xTimer) {
  switch (estado) {
    case 0: // ROJO → pasa a AMARILLO
      digitalWrite(LED_ROJO, LOW);
      estado = 1;
      blinkCount = 0;
      // inicia parpadeo amarillo
      xTimerStart(timerAmarilloBlink, 0);
      break;

    case 2: // VERDE → pasa a ROJO
      digitalWrite(LED_VERDE, LOW);
      estado = 0;
      digitalWrite(LED_ROJO, HIGH);
      // reprograma en 2s
      xTimerChangePeriod(timerSemaforo, pdMS_TO_TICKS(200), 0);
      break;
  }
}

// --- Callback de parpadeo amarillo ---
void amarilloBlinkCallback(TimerHandle_t xTimer) {
  digitalWrite(LED_AMARILLO, !digitalRead(LED_AMARILLO));
  blinkCount++;

  if (blinkCount >= 6) { // 3 parpadeos (on+off = 2 ticks cada)
    // Apagar amarillo
    digitalWrite(LED_AMARILLO, LOW);
    xTimerStop(timerAmarilloBlink, 0);

    // Cambiar a verde
    estado = 2;
    digitalWrite(LED_VERDE, HIGH);

    // verde dura 2s
    xTimerChangePeriod(timerSemaforo, pdMS_TO_TICKS(200), 0);
    xTimerStart(timerSemaforo, 0);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);

  // Inicia en rojo
  digitalWrite(LED_ROJO, HIGH);

  // Timer principal (maneja ROJO y VERDE, 2s cada uno)
  timerSemaforo = xTimerCreate(
    "Semaforo", 
    pdMS_TO_TICKS(200), // 2 segundos
    pdFALSE,             // one-shot
    (void*)0,
    semaforoCallback
  );

  // Timer para parpadeo del amarillo (~333ms)
  timerAmarilloBlink = xTimerCreate(
    "AmarilloBlink", 
    pdMS_TO_TICKS(33), 
    pdTRUE,              // auto-reload mientras esté activo
    (void*)0,
    amarilloBlinkCallback
  );

  // arranca el semáforo después de 2s (rojo inicial)
  xTimerStart(timerSemaforo, 0);
}

void loop() {
  // Nada, todo lo maneja FreeRTOS
}
