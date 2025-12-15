#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
 
// --- CONFIGURACIÓN DE NÚCLEO (BEST PRACTICE) ---
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t app_cpu = 0;
#else
  static const BaseType_t app_cpu = 1;
#endif
 
// DEFINICIÓN DE PINES
const int PIN_ENCODER_A = 18; 
const int PIN_ENCODER_B = 19;
const int PIN_MOTOR_1 = 25; 
const int PIN_MOTOR_2 = 26;
 
// CONFIGURACIÓN PWM
const int PWM_FREQ = 20000;      // 20 KHz
const int PWM_RES = 8;           // 8 bits (0-255)
// Nota: En la versión nueva ya no definimos canales manualmente, el ESP32 lo gestiona.
 
// ==========================================
// VARIABLES GLOBALES Y OBJETOS
// ==========================================
 
volatile long Np = 0;
 
// Variables Compartidas
float th_des = 0;           
float th_actual_debug = 0;  
float pwm_debug = 0;        
 
SemaphoreHandle_t xMutexConsigna; 
portMUX_TYPE muxEncoder = portMUX_INITIALIZER_UNLOCKED;
 
// Constantes del Modelo
const float R = 0.1428;     
const float alpha = 0.05;   
 
// Ganancias PID
const float kp = 1.25; 
const float kd = 0.25; 
const float ki = 0.07;
 
// ==========================================
// INTERRUPCIONES
// ==========================================
 
void IRAM_ATTR ISR_Encoder_A() {
    portENTER_CRITICAL_ISR(&muxEncoder); 
    if (digitalRead(PIN_ENCODER_B) == LOW) {
        Np++;
    } else {
        Np--;
    }
    portEXIT_CRITICAL_ISR(&muxEncoder);
}
 
void IRAM_ATTR ISR_Encoder_B() {
    portENTER_CRITICAL_ISR(&muxEncoder);
    if (digitalRead(PIN_ENCODER_A) == HIGH) {
        Np++;
    } else {
        Np--;
    }
    portEXIT_CRITICAL_ISR(&muxEncoder);
}
 
// ==========================================
// TAREA 1: LAZO DE CONTROL PID
// ==========================================
void TaskPID(void *pvParameters) {
    float th = 0, thp = 0;          
    float dth_d = 0, dth_f = 0;     
    float e = 0, de = 0, inte = 0;  
    float u = 0, usat = 0;          
    float PWM = 0;                  
    long Np_local = 0;              
 
    const int dt_ms = 2;            
    const float dt = dt_ms * 0.001; 
    TickType_t xLastWakeTime;       
    xLastWakeTime = xTaskGetTickCount();
 
    for (;;) { 
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(dt_ms));
 
        // 1. Lectura Segura
        portENTER_CRITICAL(&muxEncoder); 
        Np_local = Np; 
        portEXIT_CRITICAL(&muxEncoder);
 
        // 2. Lectura de Consigna
        float target = 0;
        if(xSemaphoreTake(xMutexConsigna, (TickType_t) 10) == pdTRUE) {
            target = th_des;
            xSemaphoreGive(xMutexConsigna);
        } else {
            target = th; 
        }
 
        // 3. Cálculos
        th = R * Np_local;              
        dth_d = (th - thp) / dt;        
        dth_f = alpha * dth_d + (1.0 - alpha) * dth_f;
 
        // 4. PID
        e = target - th;                
        de = -dth_f;                    
        inte = inte + (e * dt);         
        u = (kp * e) + (kd * de) + (ki * inte);
 
        // 5. Saturación
        usat = constrain(u, -12.0, 12.0); 
        PWM = usat * 21.25;               
 
        // 6. Actuación (SINTAXIS NUEVA V3.0)
        int pwm_out = abs(PWM); 
        if (pwm_out > 255) pwm_out = 255;
 
        if (PWM > 0) {
            // Escribimos directamente al PIN, no al canal
            ledcWrite(PIN_MOTOR_1, pwm_out);
            ledcWrite(PIN_MOTOR_2, 0);
        } else {
            ledcWrite(PIN_MOTOR_1, 0);
            ledcWrite(PIN_MOTOR_2, pwm_out);
        }
 
        // 7. Actualizar
        thp = th; 
        th_actual_debug = th;
        pwm_debug = PWM;
    }
}
 
// ==========================================
// TAREA 2: COMUNICACIÓN SERIAL
// ==========================================
void TaskSerial(void *pvParameters) {
    String inputString = "";
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));
 
        if (Serial.available() > 0) {
            inputString = Serial.readStringUntil('\n');
            float nueva_consigna = inputString.toFloat();
 
            if(xSemaphoreTake(xMutexConsigna, (TickType_t) 10) == pdTRUE) {
                th_des = nueva_consigna;
                xSemaphoreGive(xMutexConsigna);
            }
        }
        Serial.print(th_actual_debug);
        Serial.print(",");
        Serial.println(pwm_debug);
    }
}
 
// ==========================================
// SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
 
    pinMode(PIN_ENCODER_A, INPUT_PULLUP); 
    pinMode(PIN_ENCODER_B, INPUT_PULLUP);
 
    // --- CORRECCIÓN AQUÍ: NUEVA SINTAXIS PWM (ESP32 Core 3.0+) ---
    // ledcSetup y ledcAttachPin YA NO EXISTEN.
    // Usamos ledcAttach(pin, freq, resolution)
    if (!ledcAttach(PIN_MOTOR_1, PWM_FREQ, PWM_RES)) {
        Serial.println("Error configurando PWM Motor 1");
    }
    if (!ledcAttach(PIN_MOTOR_2, PWM_FREQ, PWM_RES)) {
        Serial.println("Error configurando PWM Motor 2");
    }
 
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A), ISR_Encoder_A, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B), ISR_Encoder_B, RISING);
 
    xMutexConsigna = xSemaphoreCreateMutex();
 
    // Crear Tareas con app_cpu
    xTaskCreatePinnedToCore(TaskPID, "PID_Control", 4096, NULL, 2, NULL, app_cpu);
    xTaskCreatePinnedToCore(TaskSerial, "Serial_Com", 4096, NULL, 1, NULL, app_cpu);
    Serial.println("Sistema PID RTOS Iniciado (Core v3.0)...");
}
 
void loop() {
    vTaskDelete(NULL); 
}