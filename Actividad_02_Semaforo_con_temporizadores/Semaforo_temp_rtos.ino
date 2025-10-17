// ------------------------------------------------------------
// CONFIGURACIÓN DEL NÚCLEO DEL ESP32
// ------------------------------------------------------------
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;    // Si el ESP32 usa un solo núcleo, ejecutar en el núcleo 0
#else
  static const BaseType_t app_cpu = 1;    // Si tiene dos núcleos, usar el 1 (libera el 0 para WiFi/Bluetooth)
#endif


// ------------------------------------------------------------
// DECLARACIÓN DE PINES
// ------------------------------------------------------------
// LEDs del semáforo (ajusta según tu conexión real)
const int LED_ROJO = 15;       // LED del rojo
const int LED_AMARILLO = 2;    // LED del amarillo
const int LED_VERDE = 4;       // LED del verde


// ------------------------------------------------------------
// VARIABLES Y TIMERS
// ------------------------------------------------------------
static TimerHandle_t timerSemaforo = NULL;       // Timer principal (para cambio de estados rojo/verde)
static TimerHandle_t timerAmarilloBlink = NULL;  // Timer del parpadeo del amarillo

int estado = 0;        // Estado del semáforo: 0=Rojo, 1=Amarillo, 2=Verde
int blinkCount = 0;    // Contador de parpadeos del amarillo


// ------------------------------------------------------------
// CALLBACK PRINCIPAL DEL SEMÁFORO
// ------------------------------------------------------------
// Esta función se ejecuta automáticamente cuando vence el timerSemaforo
void semaforoCallback(TimerHandle_t xTimer) {
  switch (estado) {

    // --------------------------------------------------------
    // ESTADO 0: ROJO -> pasa a AMARILLO
    // --------------------------------------------------------
    case 0:
      digitalWrite(LED_ROJO, LOW);        // Apaga el LED rojo
      estado = 1;                         // Cambia al estado amarillo
      blinkCount = 0;                     // Reinicia el contador de parpadeos
      // Inicia el temporizador de parpadeo del LED amarillo
      xTimerStart(timerAmarilloBlink, 0);
      break;

    // --------------------------------------------------------
    // ESTADO 2: VERDE -> pasa a ROJO
    // --------------------------------------------------------
    case 2:
      digitalWrite(LED_VERDE, LOW);       // Apaga el LED verde
      estado = 0;                         // Vuelve al estado rojo
      digitalWrite(LED_ROJO, HIGH);       // Enciende el LED rojo
      // Cambia la duración del timer principal a 2 segundos
      xTimerChangePeriod(timerSemaforo, pdMS_TO_TICKS(2000), 0);
      break;
  }
}


// ------------------------------------------------------------
// CALLBACK DEL PARPADEO AMARILLO
// ------------------------------------------------------------
// Este callback se ejecuta cada cierto tiempo mientras el amarillo está parpadeando
void amarilloBlinkCallback(TimerHandle_t xTimer) {
  // Invierte el estado actual del LED amarillo (enciende/apaga)
  digitalWrite(LED_AMARILLO, !digitalRead(LED_AMARILLO));
  blinkCount++;                            // Aumenta el contador de parpadeos

  // Cada parpadeo completo equivale a 2 llamadas (on+off),
  // así que 6 ticks = 3 parpadeos completos
  if (blinkCount >= 6) {
    // Apagar el LED amarillo al terminar los parpadeos
    digitalWrite(LED_AMARILLO, LOW);
    // Detener el timer de parpadeo
    xTimerStop(timerAmarilloBlink, 0);

    // Cambiar al estado verde
    estado = 2;
    digitalWrite(LED_VERDE, HIGH);         // Enciende el verde

    // Configurar duración del verde (2 segundos)
    xTimerChangePeriod(timerSemaforo, pdMS_TO_TICKS(2000), 0);
    // Iniciar el timer principal de nuevo
    xTimerStart(timerSemaforo, 0);
  }
}


// ------------------------------------------------------------
// CONFIGURACIÓN INICIAL
// ------------------------------------------------------------
void setup() {
  Serial.begin(115200);                    // Inicializa la comunicación serial

  // Configura los pines de los LEDs como salidas
  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);

  // Inicia el semáforo encendido en rojo
  digitalWrite(LED_ROJO, HIGH);

  // --------------------------------------------------------
  // CREACIÓN DE TIMERS DE FreeRTOS
  // --------------------------------------------------------

  // Timer principal: controla el cambio de rojo <-> verde
  timerSemaforo = xTimerCreate(
    "Semaforo",                // Nombre del timer
    pdMS_TO_TICKS(2000),       // 2 segundos de duración
    pdFALSE,                   // Timer de un solo uso (one-shot)
    (void*)0,                  // ID del timer (no se usa)
    semaforoCallback           // Función callback cuando vence el tiempo
  );

  // Timer del parpadeo amarillo (~333 ms por cambio)
  timerAmarilloBlink = xTimerCreate(
    "AmarilloBlink",           // Nombre del timer
    pdMS_TO_TICKS(333),        // 333 ms de intervalo
    pdTRUE,                    // Repetitivo mientras esté activo
    (void*)0,                  // ID (no se usa)
    amarilloBlinkCallback      // Función callback que alterna el LED amarillo
  );

  // --------------------------------------------------------
  // INICIO DE LA SECUENCIA
  // --------------------------------------------------------
  // El semáforo empieza con el rojo encendido
  // y después de 2 segundos cambia automáticamente a amarillo
  xTimerStart(timerSemaforo, 0);
}


// ------------------------------------------------------------
// LOOP PRINCIPAL VACÍO
// ------------------------------------------------------------
void loop() {
  // No se usa, ya que todo se maneja con temporizadores de FreeRTOS
}
