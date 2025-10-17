#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif


// Pines
// =============================
// Botones
const int btnA = 32;
const int sp1  = 33;
const int sp2  = 25;
const int dl   = 26;
const int dt   = 27;

// LEDs (válvulas, motor, etc.)
const int VA = 4;   // Válvula A
const int VB = 2;   // Válvula B
const int V2 = 5;   // Válvula 2
const int V1 = 18;  // Válvula 1
const int M  = 19;  // Motor
const int VS = 21;  // Válvula S


// Variables de control

int estado = 0;   // Máquina de estados
bool start = false;


// Tarea: Secuencia principal

void tareaSecuencia(void *parameter) {
  while (1) {
    switch (estado) {
      case 0: // Esperar botón A
        if (digitalRead(btnA) == LOW) {  // Botón presionado
          Serial.println("Inicio de ciclo");
          estado = 1;
        }
        break;

      case 1: // Activar VA
        digitalWrite(VA, HIGH);
        if (digitalRead(sp1) == LOW) {  // Sensor SP1 detectado
          digitalWrite(VA, LOW);
          Serial.println("SP1 detectado -> VA OFF, VB ON");
          estado = 2;
        }

        break;

      case 3: // Activar VB
        digitalWrite(VB, HIGH);
        if (digitalRead(sp2) == LOW) {  // Sensor SP2 detectado
          digitalWrite(VB, LOW);
          Serial.println("SP2 detectado -> VB OFF, V2 ON (5s)");

          digitalWrite(M, HIGH);
          vTaskDelay(pdMS_TO_TICKS(10000));
          digitalWrite(M, LOW);
          estado = 4;

          //digitalWrite(V2, HIGH);
          //vTaskDelay(pdMS_TO_TICKS(5000));
          //digitalWrite(V2, LOW);
          //estado = 3;
        }
        break;

      case 2: // Activar V1 hasta DL
        digitalWrite(VB, HIGH);
        if (digitalRead(sp2) == LOW) {
           digitalWrite(VB, LOW);
           Serial.println("SP2 detectado -> VB OFF, V2 ON (5s)");
        }

           digitalWrite(V1, HIGH);
           if (digitalRead(dl) == LOW) {
           digitalWrite(V1, LOW);
           Serial.println("DL detectado -> V1 OFF, M ON (10s)");
           }

        //   digitalWrite(V2, HIGH);
        //   vTaskDelay(pdMS_TO_TICKS(5000));
        //   digitalWrite(V2, LOW);
        //   estado = 3;
        //   //digitalWrite(M, HIGH);
        //   //vTaskDelay(pdMS_TO_TICKS(10000));
        //   //digitalWrite(M, LOW);
        estado = 4;
        // }
         break;

      case 4: // Activar VS hasta DT
        digitalWrite(VS, HIGH);
        if (digitalRead(dt) == LOW) {
          digitalWrite(VS, LOW);
          Serial.println("DT detectado -> VS OFF, ciclo finalizado");
          estado = 0;  // Reiniciar, esperando botón A
        }
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // Evita saturar CPU
  }
}


// Tarea: LEDs de seguridad (apagar todo si no está en secuencia)

void tareaSeguridad(void *parameter) {
  while (1) {
    if (estado == 0) {
      // Apagar todo si no está en ciclo
      digitalWrite(VA, LOW);
      digitalWrite(VB, LOW);
      digitalWrite(V2, LOW);
      digitalWrite(V1, LOW);
      digitalWrite(M, LOW);
      digitalWrite(VS, LOW);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}


// Tarea: Monitor Serial

void tareaMonitor(void *parameter) {
  while (1) {
    Serial.print("Estado actual: ");
    Serial.println(estado);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


// Setup

void setup() {
  Serial.begin(115200);

  // Configurar botones
  pinMode(btnA, INPUT_PULLUP);
  pinMode(sp1, INPUT_PULLUP);
  pinMode(sp2, INPUT_PULLUP);
  pinMode(dl, INPUT_PULLUP);
  pinMode(dt, INPUT_PULLUP);

  // Configurar LEDs
  pinMode(VA, OUTPUT);
  pinMode(VB, OUTPUT);
  pinMode(V2, OUTPUT);
  pinMode(V1, OUTPUT);
  pinMode(M, OUTPUT);
  pinMode(VS, OUTPUT);

  // Crear tareas
  xTaskCreatePinnedToCore(tareaSecuencia, "Secuencia", 4096, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore(tareaSeguridad, "Seguridad", 2048, NULL, 1, NULL, app_cpu);
  xTaskCreatePinnedToCore(tareaMonitor,   "Monitor",   2048, NULL, 1, NULL, app_cpu);
}

void loop() {
  // Nada aquí
}
