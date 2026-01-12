#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <FirebaseESP32.h>

/*================= credenciales =================*/
#define WIFI_SSID "MORONGUITAS"
#define WIFI_PASSWORD "delunoalnueve234"


#define FIREBASE_HOST "https://rtosmagda-default-rtdb.firebaseio.com/" 
#define FIREBASE_AUTH "PGjWDtXFOFzWcA6hG47zAVqaplhyevNx4sYiarlr"

/*================= DEFINICIÓN DE PINES =================*/
#define PIN_HEATER      23  // Salida SSR (Slow PWM)
#define PIN_FAN         19  // Salida Ventilador (PWM Rápido)

// Pines del Keypad 4x4
#define R1 32
#define R2 33
#define R3 25
#define R4 26
#define C1 14
#define C2 27
#define C3 12
#define C4 13

Adafruit_SHT31 sht31 = Adafruit_SHT31();
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Configuración Teclado
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'}, // A: Siguiente Sistema
  {'4','5','6','B'}, // B: Anterior Sistema
  {'7','8','9','C'}, // C: Entrar / Editar
  {'*','0','#','D'}  // D: Atrás / Cancelar
};
byte rowPins[ROWS] = {R1, R2, R3, R4};
byte colPins[COLS] = {C1, C2, C3, C4};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Objetos Firebase
FirebaseData fbData;
FirebaseAuth auth;
FirebaseConfig config;


struct ControlParams {
    float setpoint;   
    float Kp;
    float Ki;
    float Kd;
    float output;     
    float integral;
    float lastError;
    bool  useFuzzy;   
};

ControlParams heater = {50.0, 30.0, 0.1, 100.0, 0, 0, 0, false}; 
ControlParams fan    = {40.0, 5.0,  0.05, 10.0,  0, 0, 0, false}; 

float sharedTemp = 0.0;
float sharedHum = 0.0;
SemaphoreHandle_t xDataMutex;

enum MenuState { HOME_VIEW, SELECT_SYSTEM, SELECT_PARAM, EDIT_VALUE };
MenuState currentMenu = HOME_VIEW;
int selectedSystem = 0; 
int selectedParam = 0;  
float tempEditValue = 0;

/*=================LÓGICA DE CONTROL=================*/

float trimf(float x, float a, float b, float c) {
    if (x <= a || x >= c) return 0;
    if (x == b) return 1;
    if (x < b) return (x - a) / (b - a);
    return (c - x) / (c - b);
}

float computeFuzzyHeater(float error) {
    float eNeg = trimf(error, -50, -20, 0); 
    float eZero = trimf(error, -5, 0, 5);   
    float ePos = trimf(error, 0, 20, 50);   
    float num = (eNeg * 0) + (eZero * 80) + (ePos * 255);
    float den = eNeg + eZero + ePos;
    if (den == 0) return (error > 0) ? 255 : 0; 
    return constrain(num / den, 0, 255);
}

float computePID(ControlParams &sys, float input, bool reverseLogic) {
    float error = sys.setpoint - input;
    if (reverseLogic) error = input - sys.setpoint; 
    sys.integral += error * 0.1; 
    sys.integral = constrain(sys.integral, -100, 100); 
    float derivative = error - sys.lastError;
    sys.lastError = error;
    float out = (sys.Kp * error) + (sys.Ki * sys.integral) + (sys.Kd * derivative);
    return constrain(out, 0, 255);
}

/*=================TAREAS FREERTOS=================*/

// --- TAREA 1: Lectura de Sensores ---
void TaskSensors(void *pvParameters) {
    if (!sht31.begin(0x44)) {
        if (!sht31.begin(0x45)) {
             Serial.println("Error: No se encuentra SHT31.");
        }
    }

    while(1) {
        float t = sht31.readTemperature();
        float h = sht31.readHumidity();

        if (!isnan(t) && !isnan(h)) {
            if (xSemaphoreTake(xDataMutex, portMAX_DELAY)) {
                sharedTemp = t;
                sharedHum = h;
                xSemaphoreGive(xDataMutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

// --- TAREA 2: Control de Actuadores ---
void TaskControl(void *pvParameters) {
    ledcSetup(0, 20000, 8); 
    ledcAttachPin(PIN_FAN, 0);
    pinMode(PIN_HEATER, OUTPUT);
    digitalWrite(PIN_HEATER, LOW);

    unsigned long windowStartTime = millis();
    const int WindowSize = 1000; 
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(50); 
    xLastWakeTime = xTaskGetTickCount();

    while(1) {
        float t, h;
        if (xSemaphoreTake(xDataMutex, portMAX_DELAY)) {
            t = sharedTemp;
            h = sharedHum;
            xSemaphoreGive(xDataMutex);
        }

        // HEATER
        float heaterVal = 0;
        if (heater.useFuzzy) heaterVal = computeFuzzyHeater(heater.setpoint - t);
        else heaterVal = computePID(heater, t, false);
        heater.output = heaterVal;

        // SSR Logic
        long onTime = (long)((heaterVal / 255.0) * WindowSize);
        unsigned long now = millis();
        if (now - windowStartTime > WindowSize) windowStartTime += WindowSize;
        if ((now - windowStartTime) < onTime) digitalWrite(PIN_HEATER, HIGH);
        else digitalWrite(PIN_HEATER, LOW);

        // FAN
        float fanVal = 0;
        if (fan.useFuzzy) fanVal = (h > fan.setpoint) ? map(h, fan.setpoint, fan.setpoint+20, 60, 255) : 0;
        else fanVal = computePID(fan, h, true);
        fan.output = fanVal;
        ledcWrite(0, (int)fanVal);

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// --- TAREA 3: Interfaz de Usuario ---
void TaskUI(void *pvParameters) {
    lcd.init();
    lcd.backlight();
    char key;
    unsigned long lastUpdate = 0;

    while(1) {
        key = keypad.getKey();
        switch(currentMenu) {
            case HOME_VIEW:
                if (millis() - lastUpdate > 800) {
                    lcd.setCursor(0,0); lcd.printf("T:%.1f H:%.1f", sharedTemp, sharedHum);
                    lcd.setCursor(0,1); lcd.printf("SP_T:%.0f SP_H:%.0f", heater.setpoint, fan.setpoint);
                    lastUpdate = millis();
                }
                if (key == 'C') { currentMenu = SELECT_SYSTEM; lcd.clear(); }
                break;
            case SELECT_SYSTEM:
                lcd.setCursor(0,0); lcd.print("CONFIGURAR:");
                lcd.setCursor(0,1); lcd.print(selectedSystem == 0 ? "> CALENTADOR" : "> VENTILADOR");
                if (key == 'A' || key == 'B') selectedSystem = !selectedSystem;
                if (key == 'C') { currentMenu = SELECT_PARAM; selectedParam = 0; lcd.clear(); }
                if (key == 'D') currentMenu = HOME_VIEW;
                break;
            case SELECT_PARAM:
                lcd.setCursor(0,0); lcd.print(selectedSystem==0 ? "CALENTADOR:" : "VENTILADOR:");
                lcd.setCursor(0,1);
                switch(selectedParam) {
                    case 0: lcd.print("> Setpoint"); break;
                    case 1: lcd.print("> Kp"); break;
                    case 2: lcd.print("> Ki"); break;
                    case 3: lcd.print("> Kd"); break;
                    case 4: lcd.print("> Modo Control"); break;
                }
                if (key == 'A') selectedParam = (selectedParam + 1) % 5;
                if (key == 'B') selectedParam = (selectedParam - 1 < 0) ? 4 : selectedParam - 1;
                if (key == 'D') currentMenu = SELECT_SYSTEM; 
                if (key == 'C') { 
                    currentMenu = EDIT_VALUE;
                    xSemaphoreTake(xDataMutex, portMAX_DELAY);
                    ControlParams *sys = (selectedSystem==0) ? &heater : &fan;
                    if(selectedParam == 0) tempEditValue = sys->setpoint;
                    else if(selectedParam == 1) tempEditValue = sys->Kp;
                    else if(selectedParam == 2) tempEditValue = sys->Ki;
                    else if(selectedParam == 3) tempEditValue = sys->Kd;
                    else if(selectedParam == 4) tempEditValue = (float)sys->useFuzzy;
                    xSemaphoreGive(xDataMutex);
                    lcd.clear();
                }
                break;
            case EDIT_VALUE:
                lcd.setCursor(0,0); lcd.print("Valor Actual:");
                lcd.setCursor(0,1); 
                if (selectedParam == 4) lcd.print(tempEditValue > 0.5 ? "FUZZY" : "PID CLASICO");
                else lcd.print(tempEditValue);
                float step = (selectedParam == 0) ? 1.0 : 0.1;
                if (key == 'A') tempEditValue += step;
                if (key == 'B') tempEditValue -= step;
                if (selectedParam == 4 && (key == 'A' || key == 'B')) tempEditValue = !((bool)tempEditValue);
                if (key == 'D') currentMenu = SELECT_PARAM;
                if (key == 'C') { 
                    xSemaphoreTake(xDataMutex, portMAX_DELAY);
                    ControlParams *sys = (selectedSystem==0) ? &heater : &fan;
                    if(selectedParam == 0) sys->setpoint = tempEditValue;
                    else if(selectedParam == 1) sys->Kp = tempEditValue;
                    else if(selectedParam == 2) sys->Ki = tempEditValue;
                    else if(selectedParam == 3) sys->Kd = tempEditValue;
                    else if(selectedParam == 4) sys->useFuzzy = (bool)tempEditValue;
                    xSemaphoreGive(xDataMutex);
                    currentMenu = SELECT_PARAM;
                    lcd.clear(); lcd.print("Guardado!"); vTaskDelay(500);
                }
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- TAREA 4: Sincronización Firebase 
void TaskFirebase(void *pvParameters) {
    Serial.println("Esperando WiFi para Firebase...");
    while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));
    
    config.database_url = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH; 
    fbData.setResponseSize(4096); 

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    Serial.println("Firebase Iniciado. Conectando...");

    while(1) {
        if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
            
            // 1. PREPARAR DATOS
            FirebaseJson json;
            float t_local, h_local, h_sp_local, f_sp_local, h_out_local, f_out_local;
            
            xSemaphoreTake(xDataMutex, portMAX_DELAY);
            t_local = sharedTemp; h_local = sharedHum;
            h_sp_local = heater.setpoint; h_out_local = heater.output;
            f_sp_local = fan.setpoint; f_out_local = fan.output;
            xSemaphoreGive(xDataMutex);

            json.set("temperatura", t_local);
            json.set("humedad", h_local);
            json.set("heater_sp", h_sp_local);
            json.set("heater_out", h_out_local);
            json.set("fan_sp", f_sp_local);
            json.set("fan_out", f_out_local);
            
            // 2. ENVIAR DATOS (ESTO SE MANTIENE PARA MONITOREO)
            Serial.print("Enviando a Firebase... ");
            if (Firebase.updateNode(fbData, "/secadora/estado", json)) {
                Serial.println("OK!");
            } else {
                Serial.print("ERROR: ");
                Serial.println(fbData.errorReason());
            }

         
            

        }
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

/*=================SETUP Y LOOP =================*/
void setup() {
    Serial.begin(115200);
    Wire.begin(); 

    Serial.print("Conectando WiFi a: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    while(WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nWiFi Conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    xDataMutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(TaskSensors,  "Sensors",  4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(TaskControl,  "Control",  4096, NULL, 3, NULL, 1); 
    xTaskCreatePinnedToCore(TaskUI,       "UI",       4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskFirebase, "Firebase", 8192, NULL, 1, NULL, 0); 
}

void loop() {
    vTaskDelete(NULL); 
}