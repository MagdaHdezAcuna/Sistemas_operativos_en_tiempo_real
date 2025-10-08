// =============================
// Configuración de núcleos
// =============================
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0; // Un solo núcleo
#else
  static const BaseType_t app_cpu = 1; // Doble núcleo
#endif

// =============================
// Pines
// =============================
static const int led_pin1 = 4;  // LED 1
static const int led_pin2 = 2;  // LED 2
static const int boton1   = 22; // Botón 1
static const int boton2   = 23; // Botón 2

// Variables de la máquina de estados
int estado = 0;
unsigned long tiempoInicio = 0;
bool secuenciaCorrecta = false;  // <- Control de habilitación de LEDs

// =============================
// Tarea: LED a 500ms
// =============================
void toggleLED(void *parameter) {
  while (1) {
    if (secuenciaCorrecta) {   // Solo funciona si la secuencia fue correcta
      digitalWrite(led_pin1, HIGH);
      vTaskDelay(pdMS_TO_TICKS(500));
      digitalWrite(led_pin1, LOW);
      vTaskDelay(pdMS_TO_TICKS(500));
    } else {
      digitalWrite(led_pin1, LOW); // Forzar apagado si no hay secuencia
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}

// =============================
// Tarea: LED a 323ms
// =============================
void toggleLED2(void *parameter) {
  while (1) {
    if (secuenciaCorrecta) {   // Solo funciona si la secuencia fue correcta
      digitalWrite(led_pin2, HIGH);
      vTaskDelay(pdMS_TO_TICKS(323));
      digitalWrite(led_pin2, LOW);
      vTaskDelay(pdMS_TO_TICKS(323));
    } else {
      digitalWrite(led_pin2, LOW); // Forzar apagado si no hay secuencia
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}

// =============================
// Tarea: Secuencia con botones
// =============================
void secuenciaBotones(void *parameter) {
  while (1) {
    bool b1 = digitalRead(boton1); // HIGH = suelto, LOW = presionado
    bool b2 = digitalRead(boton2);

    switch (estado) {
      case 0: // Esperar presionar boton1
        if (b1 == LOW) {
          tiempoInicio = millis();
          estado = 1;
          Serial.println("Paso 1: Boton1 presionado");
        }
        break;

      case 1: // Esperar 1s después de presionar boton1
        if (millis() - tiempoInicio >= 1000) {
          if (b2 == LOW) { // Presionar boton2
            tiempoInicio = millis();
            estado = 2;
            Serial.println("Paso 2: Boton2 presionado");
          }
        }
        break;

      case 2: // Esperar 1s y luego soltar boton2
        if (millis() - tiempoInicio >= 1000) {
          if (b2 == HIGH) {
            tiempoInicio = millis();
            estado = 3;
            Serial.println("Paso 3: Boton2 liberado");
          }
        }
        break;

      case 3: // Esperar 1s y luego soltar boton1
        if (millis() - tiempoInicio >= 1000) {
          if (b1 == HIGH) {
            tiempoInicio = millis();
            estado = 4;
            Serial.println("Paso 4: Boton1 liberado");
          }
        }
        break;

      case 4: // Esperar >= 1s después de soltar boton1
        if (millis() - tiempoInicio >= 1000) {
          Serial.println(" Secuencia COMPLETA");
          secuenciaCorrecta = true;  // <-- Habilitamos LEDs
          estado = 0; // Reiniciar
        }
        break;
    }

    // Resetear si no se cumple el orden
    if (estado > 0) {
      if ((estado == 1 && b1 == HIGH) ||   // Se soltó boton1 antes de tiempo
          (estado == 2 && b1 == HIGH) ||   // Se soltó boton1 en lugar de boton2
          (estado == 3 && b2 == LOW)) {    // Se volvió a presionar boton2
        Serial.println(" Secuencia incorrecta -> Reinicio");
        secuenciaCorrecta = false;  // <-- Se deshabilitan LEDs
        estado = 0;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // Pequeña pausa para no saturar CPU
  }
}

// =============================
// Setup
// =============================
void setup() {
  // LEDs
  pinMode(led_pin1, OUTPUT);
  pinMode(led_pin2, OUTPUT);

  // Botones
  pinMode(boton1, INPUT_PULLUP);
  pinMode(boton2, INPUT_PULLUP);

  Serial.begin(9600);

  // Tarea 1: LED 500ms
  xTaskCreatePinnedToCore(
    toggleLED,
    "Toggle LED1",
    1024,
    NULL,
    1,
    NULL,
    app_cpu);

  // Tarea 2: LED 323ms
  xTaskCreatePinnedToCore(
    toggleLED2,
    "Toggle LED2",
    1024,
    NULL,
    1,
    NULL,
    app_cpu);

  // Tarea 3: Secuencia de botones
  xTaskCreatePinnedToCore(
    secuenciaBotones,
    "Secuencia Botones",
    2048,   // Más stack porque usa switch y Serial
    NULL,
    1,
    NULL,
    app_cpu);
}

void loop() {
    Serial.print("B1=");
  Serial.print(digitalRead(22));
  Serial.print("  B2=");
  Serial.println(digitalRead(23));
  delay(200);
}
