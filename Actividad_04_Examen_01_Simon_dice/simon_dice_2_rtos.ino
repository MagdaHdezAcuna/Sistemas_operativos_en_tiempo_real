#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// ==========================================================
// CONFIGURACIÓN DE FREERTOS (para asignar tareas a núcleos)
// ==========================================================
// Si el microcontrolador solo tiene un núcleo, se usa el 0.
// Si tiene dos (como el ESP32), se usa el núcleo 1 para las tareas del juego.
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// ==========================================================
// CONFIGURACIÓN DEL LCD (pantalla 16x2 con interfaz I2C)
// ==========================================================
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Dirección I2C 0x27, 16 columnas, 2 filas

// ==========================================================
// CONFIGURACIÓN DEL TECLADO MATRICIAL 4x4
// ==========================================================

// Tamaño del teclado
const byte FILAS = 4;
const byte COLUMNAS = 4;

// Mapa de teclas: define qué carácter tiene cada posición
char teclas[FILAS][COLUMNAS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

// Pines del ESP32 conectados al teclado
byte pinesFilas[FILAS]       = {19, 18, 5, 4};
byte pinesColumnas[COLUMNAS] = {25, 26, 27, 14};

// Crea el objeto Keypad con su mapa de teclas y pines asignados
Keypad keypad = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// ==========================================================
// VARIABLES DEL JUEGO
// ==========================================================
#define MAX_NIVEL 20  // Longitud máxima de la secuencia

char secuencia[MAX_NIVEL];        // Secuencia generada por el sistema
char entradaUsuario[MAX_NIVEL];   // Secuencia ingresada por el jugador

int nivel = 1;        // Nivel actual (empieza en 1)
int indice = 0;       // Índice de entrada del usuario

bool esperandoEntrada = false;  // Indica si se espera al jugador
bool mostrarSecuencia = true;   // Indica si debe mostrarse la secuencia
bool nuevaRonda = true;         // Indica si debe generarse una secuencia nueva

// ==========================================================
// HANDLES DE TAREAS (referencias para FreeRTOS)
// ==========================================================
TaskHandle_t tareaJuego;
TaskHandle_t tareaTeclado;

// ==========================================================
// FUNCIÓN AUXILIAR: Genera la secuencia aleatoria
// ==========================================================
void generarSecuencia() {
  for (int i = 0; i < MAX_NIVEL; i++) {
    // Genera una tecla aleatoria dentro de las 16 posibles
    int idxFila = random(0, FILAS);
    int idxCol = random(0, COLUMNAS);
    secuencia[i] = teclas[idxFila][idxCol];
  }
}

// ==========================================================
// TAREA: Mostrar secuencia (maneja el flujo del juego)
// ==========================================================
void tareaMostrarSecuencia(void *parameter) {
  while (1) {
    if (mostrarSecuencia) {
      esperandoEntrada = false;  // No aceptar teclas durante la demostración
      indice = 0;                // Reinicia el índice de entrada

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Nivel: ");
      lcd.print(nivel);
      delay(1000);

      // Si se inicia una nueva ronda, genera una secuencia diferente
      if (nuevaRonda) {
        generarSecuencia();
        nuevaRonda = false;
      }

      // Muestra la secuencia hasta el nivel actual
      for (int i = 0; i < nivel; i++) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Secuencia:");
        lcd.setCursor(0, 1);
        lcd.print(secuencia[i]);
        delay(1000);  // Muestra cada carácter durante 1 segundo
      }

      // Pide al usuario repetir la secuencia
      lcd.clear();
      lcd.print("Ingrese secuencia");
      esperandoEntrada = true;   // Ahora se habilita la lectura del teclado
      mostrarSecuencia = false;  // Ya no mostrar hasta terminar entrada
    }

    // Espera corta antes de volver a verificar el estado
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// ==========================================================
// TAREA: Leer teclado (maneja las entradas del jugador)
// ==========================================================
void tareaLeerTeclado(void *parameter) {
  while (1) {
    if (esperandoEntrada) {
      char tecla = keypad.getKey();  // Lee una tecla presionada
      if (tecla) {                   // Si se detecta una tecla
        entradaUsuario[indice] = tecla;  // Guarda la tecla en el arreglo
        lcd.setCursor(indice, 1);       // Muestra la tecla en la LCD
        lcd.print(tecla);
        indice++;

        // Cuando el usuario completa la secuencia del nivel actual
        if (indice >= nivel) {
          esperandoEntrada = false;  // Ya no esperar más teclas

          // Comparar secuencia ingresada vs secuencia generada
          bool correcto = true;
          for (int i = 0; i < nivel; i++) {
            if (entradaUsuario[i] != secuencia[i]) {
              correcto = false;
              break;
            }
          }

          // Mostrar resultado en pantalla
          lcd.clear();
          if (correcto) {
            lcd.print("Secuencia correcta!");
            nivel++;  // Avanza al siguiente nivel
            if (nivel > MAX_NIVEL) nivel = MAX_NIVEL;  // Límite superior
            nuevaRonda = false;  // Mantiene la misma secuencia base
          } else {
            lcd.print("Secuencia incorrecta!");
            nivel = 1;          // Reinicia al nivel 1
            nuevaRonda = true;  // Genera nueva secuencia
          }

          delay(1500);  // Pausa para que el jugador lea el mensaje

          // Inicia nueva ronda (muestra nuevamente la secuencia)
          mostrarSecuencia = true;
          indice = 0;
        }
      }
    }
    // Pequeño retardo para no saturar el CPU
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ==========================================================
// CONFIGURACIÓN INICIAL (setup)
// ==========================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  lcd.begin();
  lcd.backlight();  // Enciende la luz del LCD

  randomSeed(analogRead(0));  // Inicializa generador aleatorio

  lcd.print("Iniciando juego...");
  delay(1000);
  lcd.clear();

  generarSecuencia();  // Genera la primera secuencia del juego

  // --- Crear tareas FreeRTOS ---
  // Tarea que muestra la secuencia y controla niveles
  xTaskCreatePinnedToCore(
    tareaMostrarSecuencia,   // Función
    "MostrarSecuencia",      // Nombre
    4096,                    // Tamaño de pila
    NULL,                    // Parámetro
    1,                       // Prioridad
    &tareaJuego,             // Handle
    app_cpu                  // Núcleo asignado
  );

  // Tarea que lee las teclas del usuario
  xTaskCreatePinnedToCore(
    tareaLeerTeclado,
    "LeerTeclado",
    4096,
    NULL,
    1,
    &tareaTeclado,
    app_cpu
  );
}

void loop() {
  // No se usa loop(), todo se maneja con tareas FreeRTOS
}
