#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// =============================
// CONFIGURACIÓN DE FREERTOS
// =============================
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif

// =============================
// LCD CONFIG
// =============================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =============================
// KEYPAD CONFIG
// =============================
const byte FILAS = 4;
const byte COLUMNAS = 4;

char teclas[FILAS][COLUMNAS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

// Usa pines válidos para salida
byte pinesFilas[FILAS]    = {19, 18, 5, 4};
byte pinesColumnas[COLUMNAS] = {25, 26, 27, 14};

Keypad keypad = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// =============================
// VARIABLES DEL JUEGO
// =============================
#define MAX_NIVEL 20

char secuencia[MAX_NIVEL];
char entradaUsuario[MAX_NIVEL];
int nivel = 1;
int indice = 0;

bool esperandoEntrada = false;
bool mostrarSecuencia = true;
bool nuevaRonda = true;

// =============================
// HANDLES DE TAREAS
// =============================
TaskHandle_t tareaJuego;
TaskHandle_t tareaTeclado;

// =============================
// FUNCIÓN AUXILIAR
// =============================
void generarSecuencia() {
  for (int i = 0; i < MAX_NIVEL; i++) {
    int idxFila = random(0, FILAS);
    int idxCol = random(0, COLUMNAS);
    secuencia[i] = teclas[idxFila][idxCol];
  }
}

// =============================
// TAREA: Mostrar secuencia
// =============================
void tareaMostrarSecuencia(void *parameter) {
  while (1) {
    if (mostrarSecuencia) {
      esperandoEntrada = false;
      indice = 0;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Nivel: ");
      lcd.print(nivel);
      delay(1000);

      // Si es nueva ronda, regenerar secuencia base
      if (nuevaRonda) {
        generarSecuencia();
        nuevaRonda = false;
      }

      // Mostrar caracteres uno a uno
      for (int i = 0; i < nivel; i++) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Secuencia:");
        lcd.setCursor(0, 1);
        lcd.print(secuencia[i]);
        delay(1000);
      }

      lcd.clear();
      lcd.print("Ingrese secuencia");
      esperandoEntrada = true;
      mostrarSecuencia = false;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// =============================
// TAREA: Leer teclado
// =============================
void tareaLeerTeclado(void *parameter) {
  while (1) {
    if (esperandoEntrada) {
      char tecla = keypad.getKey();
      if (tecla) {
        entradaUsuario[indice] = tecla;
        lcd.setCursor(indice, 1);
        lcd.print(tecla);
        indice++;

        if (indice >= nivel) {
          esperandoEntrada = false;

          bool correcto = true;
          for (int i = 0; i < nivel; i++) {
            if (entradaUsuario[i] != secuencia[i]) {
              correcto = false;
              break;
            }
          }

          lcd.clear();
          if (correcto) {
            lcd.print("Secuencia correcta!");
            nivel++;
            if (nivel > MAX_NIVEL) nivel = MAX_NIVEL;
            nuevaRonda = false;
          } else {
            lcd.print("Secuencia incorrecta!");
            nivel = 1;
            nuevaRonda = true;
          }
          delay(1500);

          mostrarSecuencia = true;
          indice = 0;
        }
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// =============================
// SETUP
// =============================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  lcd.begin();
  lcd.backlight();

  randomSeed(analogRead(0));

  lcd.print("Iniciando juego...");
  delay(1000);
  lcd.clear();

  generarSecuencia();

  // Crear tareas
  xTaskCreatePinnedToCore(
    tareaMostrarSecuencia,
    "MostrarSecuencia",
    4096,
    NULL,
    1,
    &tareaJuego,
    app_cpu
  );

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
  // No hace falta nada aquí
}
