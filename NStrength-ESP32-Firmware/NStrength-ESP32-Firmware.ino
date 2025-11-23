#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <BluetoothSerial.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SDA_PIN 19
#define SCL_PIN 4
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
BluetoothSerial SerialBT;

#define BUTTON_DOWN 26    // Botón para bajar
#define BUTTON_SELECT 25  // Botón para seleccionar
#define BUTTON_UP 27      // Botón para subir
#define PULSE_SENSOR_PIN 35
#define LED_PIN 13
#define SPEAKER_PIN 15
#define MOTOR_PIN 33

const int PULSE_MAX_VALUE = 3360;
const int PULSE_MIN_VALUE = 0;
const int NUM_READINGS = 300;
const int INTERVALO_LECTURA = 100;
const int INTERVALO_PARLANTE = 1000;

// Variables de configuración
int edad = 25;
int objetivo = 1; // 1: Cardio, 2: Pérdida de peso, 3: Recuperación
bool enReposo = false;
float pulsoPromedio = 0;
int tiempoDescansoCalculado = 0;
bool descansoEnCurso = false;
unsigned long tiempoInicioDescanso = 0;


const char* ssid = "M7";
const char* password = "coreM87Nvme";
const String firebaseURL = "https://proyect-fitness-default-rtdb.firebaseio.com/sensor-data.json";

// Estados del sistema
enum Estado {
    BIENVENIDA,
    CONFIG_EDAD,
    CONFIRMAR_EDAD,
    CONFIG_OBJETIVO,
    CONFIRMAR_OBJETIVO,
    CONFIG_ESTADO,
    CONFIRMAR_ESTADO,
    MEDICION,
    MOSTRAR_RESULTADOS,
    INICIAR_DESCANSO,
    DESCANSO_EN_PROGRESO
};

Estado estadoActual = BIENVENIDA;

void setup() {
    Serial.begin(9600);
    SerialBT.begin("MonitorCardiaco");
    Wire.begin(SDA_PIN, SCL_PIN);

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println(F("Error: No se puede inicializar la pantalla OLED"));
        for (;;);
    }

    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_SELECT, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    pinMode(SPEAKER_PIN, OUTPUT);
    pinMode(MOTOR_PIN, OUTPUT);
    
    digitalWrite(LED_PIN, LOW);
    mostrarBienvenida();
}

// [Las funciones mostrarBienvenida hasta mostrarConfiguracionObjetivo permanecen igual]
void mostrarBienvenida() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Bienvenido al");
    display.println("Monitor Cardiaco");
    display.println("");
    display.println("Presione cualquier");
    display.println("boton para comenzar");
    display.println("la configuracion");
    display.display();
}

void mostrarConfiguracionEdad() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Configurar Edad:");
    display.println("");
    display.setTextSize(2);
    display.print(edad);
    display.println(" anos");
    display.setTextSize(1);
    display.println("");
    display.println("ARRIBA:  +");
    display.println("ABAJO:   -");
    display.println("SELECT: Confirmar");
    display.display();
}

void mostrarConfirmacionEdad() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Confirmar Edad:");
    display.println("");
    display.setTextSize(2);
    display.print(edad);
    display.println(" anos");
    display.setTextSize(1);
    display.println("");
    display.println("Es correcta?");
    display.println("ARRIBA:  Si");
    display.println("ABAJO:   No");
    display.display();
}

void mostrarConfiguracionObjetivo() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Seleccionar Objetivo:");
    display.println("");
    display.println(objetivo == 1 ? "> Cardio" : "  Cardio");
    display.println(objetivo == 2 ? "> Perdida Peso" : "  Perdida Peso");
    display.println(objetivo == 3 ? "> Recuperacion" : "  Recuperacion");
    display.println("");
    display.println("ARRIBA:  Cambiar");
    display.println("SELECT: Seleccionar");
    display.display();
}

void mostrarConfirmacionObjetivo() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Confirmar Objetivo:");
    display.println("");
    String objText;
    switch(objetivo) {
        case 1: objText = "Cardio"; break;
        case 2: objText = "Perdida Peso"; break;
        case 3: objText = "Recuperacion"; break;
    }
    display.setTextSize(2);
    display.println(objText);
    display.setTextSize(1);
    display.println("");
    display.println("Es correcto?");
    display.println("ARRIBA:  Si");
    display.println("ABAJO:   No");
    display.display();
}

void mostrarConfiguracionEstado() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Estado actual:");
    display.println("");
    display.println(enReposo ? "> En reposo" : "> En actividad");
    display.println("");
    display.println("ABAJO:   Cambiar");
    display.println("SELECT: Confirmar");
    display.display();
}

void mostrarConfirmacionEstado() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Confirmar Estado:");
    display.println("");
    display.setTextSize(2);
    display.println(enReposo ? "REPOSO" : "ACTIVIDAD");
    display.setTextSize(1);
    display.println("");
    display.println("Es correcto?");
    display.println("ARRIBA:  Si");
    display.println("ABAJO:   No");
    display.display();
}

int calcularDescansoRecomendado(float pulsoPromedio) {
    int fcMax = 220 - edad;
    float porcentajeFCM = (pulsoPromedio / fcMax) * 100;
    
    if (enReposo && pulsoPromedio < 100) {
        return 0;
    }
    
    int tiempoDescanso;
    if (porcentajeFCM > 90) {
        tiempoDescanso = 15;
    } else if (porcentajeFCM > 80) {
        tiempoDescanso = 10;
    } else if (porcentajeFCM > 70) {
        tiempoDescanso = 5;
    } else {
        tiempoDescanso = 3;
    }
    
    switch(objetivo) {
        case 1: tiempoDescanso = round(tiempoDescanso * 0.8); break;
        case 2: tiempoDescanso = round(tiempoDescanso * 1.2); break;
        case 3: tiempoDescanso = round(tiempoDescanso * 1.5); break;
    }
    
    return tiempoDescanso;
}

// [Las funciones mostrarConfiguracionEstado hasta medirPulso permanecen igual]

void mostrarResultados() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Resultados:");
    display.print("Pulso: ");
    display.print(pulsoPromedio, 1);
    display.println(" BPM");
    display.println("");
    display.print("Descanso sugerido: ");
    display.print(tiempoDescansoCalculado);
    display.println(" min");
    display.println("");
    display.println("BTN UP: Iniciar descanso");
    display.println("BTN DOWN: Cancelar");
    display.display();
}

void mostrarDescansoEnProgreso(int tiempoRestante) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Descanso en curso:");
    display.println("");
    display.setTextSize(2);
    
    // Convertir a minutos y segundos
    int minutos = tiempoRestante / 60;
    int segundos = tiempoRestante % 60;
    
    // Mostrar tiempo restante
    if (minutos < 10) display.print("0");
    display.print(minutos);
    display.print(":");
    if (segundos < 10) display.print("0");
    display.println(segundos);
    
    display.setTextSize(1);
    display.println("");
    display.println("Mant. cualquier");
    display.println("boton para cancelar");
    display.display();
}
void medirPulso() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Midiendo pulso...");
    display.println("No se mueva");
    display.println("");
    display.println("Tiempo restante:");
    display.display();

    static unsigned long ultimoEnvio = 0; // Controla el tiempo del último envío
    const unsigned long intervaloEnvio = 300; // Enviar cada 300 ms

    unsigned long tiempoInicio = millis();
    unsigned long tiempoUltimoParlante = millis();
    int sumaTotal = 0;
    int lecturasTomadas = 0;

    while (millis() - tiempoInicio < 30000) {
        int segundosRestantes = 30 - ((millis() - tiempoInicio) / 1000);

        display.fillRect(0, 32, 128, 16, SSD1306_BLACK);
        display.setCursor(0, 32);
        display.print(segundosRestantes);
        display.println(" segundos");
        display.display();

        int pulseValue = analogRead(PULSE_SENSOR_PIN);
        digitalWrite(LED_PIN, HIGH);

        if (pulseValue >= PULSE_MIN_VALUE && pulseValue <= PULSE_MAX_VALUE) {
            int bpm = map(pulseValue, PULSE_MIN_VALUE, PULSE_MAX_VALUE, 28, 180);
            bpm = constrain(bpm, 28, 180);
            bpm = round(bpm * 0.70);

            sumaTotal += bpm;
            lecturasTomadas++;

            display.fillRect(0, 48, 128, 16, SSD1306_BLACK);
            display.setCursor(0, 48);
            display.print("BPM: ");
            display.println(bpm);
            display.display();

            // Enviar datos a Firebase periódicamente
            if (millis() - ultimoEnvio >= intervaloEnvio) {
                enviarDatosAFirebase(bpm);
                ultimoEnvio = millis();
            }
        }

        if (millis() - tiempoUltimoParlante >= INTERVALO_PARLANTE) {
            tone(SPEAKER_PIN, 1000, 100);
            tiempoUltimoParlante = millis();
        }

        digitalWrite(LED_PIN, LOW);
        delay(INTERVALO_LECTURA);
    }

    if (lecturasTomadas > 0) {
        pulsoPromedio = (float)sumaTotal / lecturasTomadas;
        tiempoDescansoCalculado = calcularDescansoRecomendado(pulsoPromedio);

        tone(SPEAKER_PIN, 500, 1000);
        digitalWrite(MOTOR_PIN, HIGH);
        delay(2000);
        digitalWrite(MOTOR_PIN, LOW);

        // Enviar promedio final a Firebase
        enviarDatosAFirebase(pulsoPromedio);
  }
  
}

// Función para enviar datos a Firebase
void enviarDatosAFirebase(float pulsoPromedio) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        http.begin(firebaseURL);
        http.addHeader("Content-Type", "application/json");

        String jsonData = "{\"pulsoPromedio\":" + String(pulsoPromedio, 2) + "}";

        int httpResponseCode = http.POST(jsonData);

        if (httpResponseCode > 0) {
            Serial.println("Datos enviados correctamente: " + String(httpResponseCode));
        } else {
            Serial.println("Error al enviar datos: " + String(httpResponseCode));
        }
        http.end();
    } else {
        Serial.println("Error: No conectado a WiFi");
 }
  
}
void enviarDatosBluetooth() {
    String datos = "Edad: " + String(edad) + "\n" + 
                   "Objetivo: " + String(objetivo) + "\n" + 
                   "Pulso promedio: " + String(pulsoPromedio, 1) + " BPM\n" + 
                   "Tiempo de descanso calculado: " + String(tiempoDescansoCalculado) + " minutos\n" +
                   "Estado: " + String(enReposo ? "Reposo" : "Actividad");
    SerialBT.println(datos);
}


void loop() {
    static unsigned long tiempoPresionado = 0;
    static bool botonMantenido = false;
    
    bool botonUpPresionado = digitalRead(BUTTON_UP) == LOW;
    bool botonDownPresionado = digitalRead(BUTTON_DOWN) == LOW;
    bool botonSelectPresionado = digitalRead(BUTTON_SELECT) == LOW;
    
    if ((botonUpPresionado || botonDownPresionado || botonSelectPresionado) && !botonMantenido) {
        tiempoPresionado = millis();
        botonMantenido = true;
    }
    
    if (!botonUpPresionado && !botonDownPresionado && !botonSelectPresionado) {
        botonMantenido = false;
    }
    
    switch(estadoActual) {
        case BIENVENIDA:
            if (botonUpPresionado || botonDownPresionado || botonSelectPresionado) {
                estadoActual = CONFIG_EDAD;
                delay(300);
            }
            break;
            
        case CONFIG_EDAD:
            mostrarConfiguracionEdad();
            if (botonUpPresionado) {
                edad = min(99, edad + 1);
                delay(200);
            }
            if (botonDownPresionado) {
                edad = max(10, edad - 1);
                delay(200);
            }
            if (botonSelectPresionado) {
                estadoActual = CONFIRMAR_EDAD;
                delay(300);
            }
            break;
            
        case CONFIRMAR_EDAD:
            mostrarConfirmacionEdad();
            if (botonUpPresionado) {
                estadoActual = CONFIG_OBJETIVO;
                delay(300);
            }
            if (botonDownPresionado) {
                estadoActual = CONFIG_EDAD;
                delay(300);
            }
            break;
            
        case CONFIG_OBJETIVO:
            mostrarConfiguracionObjetivo();
            if (botonUpPresionado) {
                objetivo = objetivo % 3 + 1;
                delay(300);
            }
            if (botonSelectPresionado) {
                estadoActual = CONFIRMAR_OBJETIVO;
                delay(300);
            }
            break;
            
        case CONFIRMAR_OBJETIVO:
            mostrarConfirmacionObjetivo();
            if (botonUpPresionado) {
                estadoActual = CONFIG_ESTADO;
                delay(300);
            }
            if (botonDownPresionado) {
                estadoActual = CONFIG_OBJETIVO;
                delay(300);
            }
            if (botonSelectPresionado) {  // Agregado para manejar el botón SELECT
                estadoActual = CONFIG_ESTADO;
                delay(300);
            }
            break;
            
        case CONFIG_ESTADO:
            mostrarConfiguracionEstado();
            if (botonDownPresionado) {
                enReposo = !enReposo;
                delay(300);
            }
            if (botonSelectPresionado) {
                estadoActual = CONFIRMAR_ESTADO;
                delay(300);
            }
            break;
            
        case CONFIRMAR_ESTADO:
            mostrarConfirmacionEstado();
            if (botonUpPresionado) {
                estadoActual = MEDICION;
                delay(300);
            }
            if (botonDownPresionado) {
                estadoActual = CONFIG_ESTADO;
                delay(300);
            }
            break;
            
        case MEDICION:
            medirPulso();
            estadoActual = MOSTRAR_RESULTADOS;
            break;
            
        case MOSTRAR_RESULTADOS:
            mostrarResultados();
            enviarDatosBluetooth();  // Enviar datos al finalizar la medición
            if (botonSelectPresionado) {
                estadoActual = INICIAR_DESCANSO;
                delay(300);
            }
            if (botonDownPresionado) {
                estadoActual = BIENVENIDA;
                delay(300);
            }
            break;
            
        case INICIAR_DESCANSO:
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.println("Preparando descanso");
            display.println("");
            display.println("Duracion:");
            display.setTextSize(2);
            display.print(tiempoDescansoCalculado);
            display.println(" min");
            display.setTextSize(1);
            display.println("");
            display.println("SELECT: Iniciar");
            display.println("ABAJO:  Cancelar");
            display.display();
            
            if (botonSelectPresionado) {
                estadoActual = DESCANSO_EN_PROGRESO;
                tiempoInicioDescanso = millis();
                descansoEnCurso = true;
                tone(SPEAKER_PIN, 1000, 200);
                delay(300);
            }
            if (botonDownPresionado) {
                estadoActual = BIENVENIDA;
                delay(300);
            }
            break;
            
        case DESCANSO_EN_PROGRESO:
            if (descansoEnCurso) {
                int tiempoTranscurrido = (millis() - tiempoInicioDescanso) / 1000;
                int tiempoRestante = (tiempoDescansoCalculado * 60) - tiempoTranscurrido;
                
                if (tiempoRestante <= 0) {
                    tone(SPEAKER_PIN, 1000, 1000);
                    digitalWrite(MOTOR_PIN, HIGH);
                    delay(1000);
                    digitalWrite(MOTOR_PIN, LOW);
                    descansoEnCurso = false;
                    estadoActual = BIENVENIDA;
                } else {
                    mostrarDescansoEnProgreso(tiempoRestante);
                    
                    if (botonMantenido && (millis() - tiempoPresionado > 2000)) {
                        descansoEnCurso = false;
                        estadoActual = BIENVENIDA;
                        tone(SPEAKER_PIN, 500, 500);
                        delay(300);
                    }
                }
                break;
     }
  
  }

}