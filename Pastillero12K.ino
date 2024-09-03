

// Librerías
#include <Wire.h>
#include <LiquidCrystal_I2C.h>  //Biblioteca para el display LCD
#include <DS1302.h> //Biblioteca para el reloj
#include <AccelStepper.h>//Biblioteca para el stepper BYJ48
#include <DistanceSensor.h>  //Biblioteca para el sensor de ultrasonidos HC-SR04
#include <EEPROM.h>
#include <WiFi.h>
#include <Arduino.h>
#include <HTTPClient.h> //Biblioteca usada para la generación y envío de mensajes por Telegram
#include <ArduinoJson.h> //Biblioteca usada para la generación y envío de mensajes por Telegram
#include <ESP_Mail_Client.h> //Biblioteca pa enviar mensajes po email
#include "Conectividad.h" //Archivo que contiene los datos para las notificaciones web

//Version del Firmware
const String firmwareVersion = "V12K";

//Variables para los mensajes web
bool telegramMessagesEnabled = false;
bool emailMessagesEnabled = false;
String message;

// Configuración de Telegram
const String telegramURL = "https://api.telegram.org/bot" + botToken + "/sendMessage";

//Email:  Configuración del servidor SMTP
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT esp_mail_smtp_port_587

//Email
SMTPSession smtp; // Declarar el objeto global SMTPSession
Session_Config config; // Configuración de la sesión



// Definir los pines para los motores
const int motorPin1 = 16;
const int motorPin2 = 17;
const int motorPin3 = 25;
const int motorPin4 = 26;

// Declaración del motor
AccelStepper motor(4, motorPin1, motorPin2, motorPin3, motorPin4);  

// Definir los pasos por posición para el motor
const int stepsPerPosition = 545; // Motor 28BYJ-48 - 2048 steps por vuelta - Engranajes dispensador 1/5.8571 - 22 compartimentos por vuelta - 2048*5.8571/22 

// Definir la dirección de la pantalla LCD I2C y el tamaño (20x4)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Definir los pines del reloj DS1302
const int kCePin = 4;  // Chip Enable
const int kIoPin = 19;  // Input/Output
const int kSclkPin = 18;  // Serial Clock

// Crear un objeto reloj DS1302
DS1302 rtc(kCePin, kIoPin, kSclkPin);

// Definir los pines del sensor de ultrasonidos HC-SR04
const int echoPin = 12;
const int trigPin = 13;

// Inicializar el sensor de ultrasonidos HR-SC04
DistanceSensor sensor(trigPin, echoPin);

// Variables para el control del sensor de ultrasonidos
bool proximitySensorEnabled = true;
bool proximityTriggered = false;



// Definir los pines de los botones
const int buttonSetPin = 27;
const int buttonOptionPin = 14;

// Definir el PIN del Buzzer
const int buzzerPin = 32;


// Variables para manejar el estado de los botones

bool buttonSetPressed = false; //*******
bool buttonOptionPressed = false;

int buttonSetState = HIGH;
int buttonOptionState = HIGH;
int lastButtonSetState = HIGH;
int lastButtonOptionState = HIGH;


unsigned long lastButtonOptionPress = 0; // Tiempo de la última pulsación registrada para el botón Option
unsigned long lastButtonSetPress = 0;    // Tiempo de la última pulsación registrada para el botón Set
const unsigned long debounceDelay = 50;  // Tiempo mínimo entre pulsaciones (en ms)
const unsigned long ignoreDelay = 500;   // Tiempo para ignorar pulsaciones tras una pulsación simultánea (en ms)
const unsigned long longPressTime = 1000; // Tiempo para considerar una pulsación como larga (en ms)
const unsigned long simultaneousPressTime = 300; // Tiempo para considerar una pulsación simultánea (en ms)

bool buttonOptionHeld = false; // Bandera para saber si el botón Option está siendo mantenido
bool buttonSetHeld = false;    // Bandera para saber si el botón Set está siendo mantenido
unsigned long lastSimultaneousPress = 0; // Tiempo de la última pulsación simultánea detectada
bool simultaneousPressDetected = false;  // Bandera para saber si ya se detectó una pulsación simultánea

//Variables para el control de la luz y el sleep-mode
unsigned long backlightSleepTime = 30000;
unsigned long backlightConnectedTime = 0;


int fieldToSet = 0; // Campo que estamos configurando
//int programmingMode = 0; // 0: Set Time, 1: Set Alarms

// Variables para la introducción de cadenas
bool isItLowerCase = false;    // Indica si el alfabeto es en minúsculas
bool reverseScroll = false;    // Indica si el scroll se realiza en reversa
// Alfabetos en mayúsculas y minúsculas
const char* alfabetUpper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ 1234567890@/:_#!\"$%&()=?º.-,*ç¡[]";
const char* alfabetLower = "abcdefghijklmnopqrstuvwxyz 1234567890@/:_#!\"$%&()=?º.-,*ç¡[]";
// Cadena de salida
String cadena = "";
int charIndex = 0;  // Índice del carácter seleccionado en el alfabeto


//Variables para las alarmas
struct AlarmTime {
  int hour;
  int minute;
};

AlarmTime morningAlarm = {9, 0};
AlarmTime afternoonAlarm = {15, 0};
AlarmTime eveningAlarm = {23, 0};

bool morningAlarmEnabled = true;
bool afternoonAlarmEnabled = true;
bool eveningAlarmEnabled = true;

bool morningAlarmTriggered = false;
bool afternoonAlarmTriggered = false;
bool eveningAlarmTriggered = false;


// Variables para manejo de contraseña
char currentPassword[5] = "1234";
char enteredPassword[5];
int passwordIndex = 0;
bool passwordCorrect = false;
bool newPasswordSet = false;
char newPassword[5];
char confirmPassword[5];

// Estados del sistema

bool inProgrammingMode = false;

enum SystemMode { NORMAL, PROGRAMMING, DISPENSING };
SystemMode currentSystemMode = NORMAL;

// Modos de programación
enum ProgrammingMode { SET_TIME, REFILL, SET_ALARMS, CHANGE_PASSWORD, ADVANCE, SET_SOUND, SET_SENSOR, SET_MESSAGES, CONFIG_WEB, CONFIG_MODE };
ProgrammingMode currentProgrammingMode = CONFIG_MODE;

// Valores iniciales para el reloj
Time timeNow(2024, 23, 6, 16, 30, 0, Time::kSunday);

String dayAsString(const Time::Day day) {
  switch (day) {
    case Time::kSunday: return "Domingo  ";
    case Time::kMonday: return "Lunes    ";
    case Time::kTuesday: return "Martes   ";
    case Time::kWednesday: return "Miercoles";
    case Time::kThursday: return "Jueves   ";
    case Time::kFriday: return "Viernes  ";
    case Time::kSaturday: return "Sabado   ";
  }
  return "";
}

// Variables para la recarga
int refillCount = 0;
bool refillComplete = false;

// Tabla de nombres para las ingestas en la recarga
const char* timesOfDay[] = {"MANANA", "TARDE", "NOCHE"};
const char* daysOfWeek[] = {"DOMINGO", "LUNES", "MARTES", "MIERCOLES", "JUEVES", "VIERNES", "SABADO"};

//  EEPROM
const int EEPROM_SIZE = 512; //Ajusta el tamaño de la EEPROM
const int EEPROM_CURRENT_HOUR = 0;
const int EEPROM_CURRENT_MINUTE = 1;
const int EEPROM_CURRENT_SECOND = 2;
const int EEPROM_CURRENT_DAY = 3;

const int EEPROM_MORNING_ALARM_ENABLED = 4;
const int EEPROM_AFTERNOON_ALARM_ENABLED = 5;
const int EEPROM_EVENING_ALARM_ENABLED = 6;

const int EEPROM_MORNING_ALARM_HOUR = 7;
const int EEPROM_MORNING_ALARM_MINUTE = 8;
const int EEPROM_AFTERNOON_ALARM_HOUR = 9;
const int EEPROM_AFTERNOON_ALARM_MINUTE = 10;
const int EEPROM_EVENING_ALARM_HOUR = 11;
const int EEPROM_EVENING_ALARM_MINUTE = 12;

const int EEPROM_PROXIMITY_SENSOR_ENABLED = 13;
//const int EEPROM_TELEGRAM_MESSAGES_ENABLED = 14;
//const int EEPROM_EMAIL_MESSAGES_ENABLED = 15;

const int EEPROM_PASSWORD = 16;

// Almacenar datos para la Conectividad en la EEPROM
// Define las posiciones de memoria para cada variable
const int EEPROM_WIFI_SSID = 20;
const int EEPROM_WIFI_PASSWORD = EEPROM_WIFI_SSID + 32;
const int EEPROM_BOT_TOKEN = EEPROM_WIFI_PASSWORD + 16;
const int EEPROM_CHAT_ID = EEPROM_BOT_TOKEN + 80;
const int EEPROM_AUTHOR_EMAIL = EEPROM_CHAT_ID + 16;
const int EEPROM_AUTHOR_PASSWORD = EEPROM_AUTHOR_EMAIL + 50;
const int EEPROM_RECIPIENT_EMAIL = EEPROM_AUTHOR_PASSWORD + 50;




// Manejo de scroll en display
int scrollPosition = 0;

 
// Declaraciones de funciones

// DISPLAY
void printTime(const Time& t); //Imprime la Fecha/hora en pantalla
void blinkField(int field); //Controla los campos parpadeantes el ajustar la hora
void updateAlarmDisplay(); //Actualiza la pantalla de alarmas
void blinkAlarmField(int field); //Control de los campos parpadeantes al sjustar las alarmas
void updateDisplay();  //Actualizar la pantalla estandar
void updateProgrammingDisplay(int currentOption); //Actualizar la pantalla de configuración
void updatePasswordDisplay(char* digits, int currentDigitIndex); //Actualiza la pantalla del PIN
void showRefillMessage(int day, int timeOfDay); //Muestra el mensaje de recarga
void displayAlarmMessage(int dayIndex, const char* timeOfDay); //Muestra el mensaje de Alarma
void showAdvanceMessage(int dayIndex, int timeIndex); //Muestra el mensaje de avance / retroceso
// BUZZER
void displaySoundMenu(); //Muestra el menú para activar sonidos de alarmas
void updateSoundBlinkfield(int field); //Controla los campos parpadeantes del menú sonidos de alarmas
void setSound(); //Llama al menus para ajustar el sonido de las alarmas y configura las variables
void triggerBuzzer();
// RELOJ
void setTime(); //Ajusta la hora
void setAlarms(); //Ajusta las alarmas
void checkAlarms(); //Verifica si se cumple la hora de alguna alarma
//INPUTS
void handleButtons(); //Control de los botones - pulsación larga y encedido de luz en el display
void handleProgramming(int buttonSetState, int buttonOptionState); // Controla el menú de configuración

// PASSWORD
bool inputPassword(char* password); //Verificar contraseña. Devuelve true si la contraseña es correcta
void requestPassword(); //Pedir la contraseña
void changePassword();  //Cambiar la contraseña
// MECANISMOS
bool isMotorMoving(); //Detecta si el motor se está moviendo
void attachMotor(); // Activar el motor
void unpowerMotor(); // Dejar el motor sin energía

void serveNext(); //Dispensar la dosis ingesta que corresponde
void refillCompartments(); //Recargar los compartimentos
void advance(); //Avanzar
void retreat(); //Retroceder
void goBack(int steps); //Mover el mecanismo un numero n de pasos - Se utiliza en avance y retroceso
// SENSOR DE ULTRASONIDOS HC-SR04
bool checkProximity();
void setSensor();
// EEPROM
void saveAlarmsToEEPROM(); //Guardar las alarmas en la EEPROM
void readAlarmsFromEEPROM(); //Leer las alarmas de la EEPROM
void savePasswordToEEPROM(char* password); // Guardar el PIN en la EEPROM
void readPasswordFromEEPROM(char* password); //Leer el PIN de la EEPROM
void saveConnectivityToEEPROM(); // Función para guardar los datos de conectividad en la EEPROM
void readConnectivityFromEEPROM(); // Función para leer los datos de conectividad de la EEPROM

// MENSAJERIA WEB
void connectToWiFi(); //Conecta a la red wifi - Pita 3 veces cuando conecta
void sendTelegramMessage(String message); // Envía el mensaje por Telegram
void setMessages(); //Activa o desactiva la emisión de mensajes web
void writeWebMessage(); //Composición del mensaje web
void sendEmailMessage(String message); // Envía el mensaje por Email 
void smtpCallback(SMTP_Status status); // Función de callback para obtener el estado del envío del correo
void configWeb(); //Menú para la selección de la configuración web
void configWiFi(); //Menú para la selección de la configuración WiFi
void configTelegram(); //Menú para la selección de la configuración Telegram
void configEmail(); //Menú para la selección de la configuración Email
void configNetNameKey(); //Método para cambiar el nombre de la red WiFi
void configNetPasswordKey(); //Método para cambiar la contraseña de la red WiFi
void configTelegramTokenKey(); //Método para cambiar el token API de Telegram
void configTelegramChatKey(); //Método para cambiar el chatID de Telegram
void configEmailAuthorKey(); //Método para cambiar la direccion de correo enviante
void configEmailPasswordKey(); //Método para cambiar la contraseña de aplicacion para Email
void configEmailRecipientKey(); //Método para cambiar la direccion de correo recipiente
// METODOS PARA LA CREACION DE UNA CADENA DE CARACTERES PARA INTRODUCIR DATOS 
void cleanChain(); //Método para eliminar los espacios al final de la cadena
String stringCapture(); //Método para la captura de una cadema de caracteres
void updateKeyboardDisplay(); // Despliega la cadena en la primera línea con scroll si es necesario
char obtainActualCharacter(); // Captura el caracter actual
void keyboardButtonsControl(); //Control de los botones del teclado

//***************************************************************************************
// METODOS DE DISPLAY

// Mostrar la fecha y hora en la pantalla LCD
void printTime(const Time& t) {
  
  lcd.setCursor(2, 1);
  lcd.print(dayAsString(t.day));
  lcd.setCursor(14, 1);
  lcd.print(t.date < 10 ? "0" : ""); lcd.print(t.date);
  lcd.print('/');
  lcd.print(t.mon < 10 ? "0" : ""); lcd.print(t.mon);

  lcd.setCursor(6, 2);
  lcd.print(t.hr < 10 ? "0" : ""); lcd.print(t.hr);
  lcd.print(':');
  lcd.print(t.min < 10 ? "0" : ""); lcd.print(t.min);
  lcd.print(':');
  lcd.print(t.sec < 10 ? "0" : ""); lcd.print(t.sec);

  lcd.noCursor();
}

//Controla los campos parpadeantes el ajustar la hora
void blinkField(int field) {
  if (field == 0) {  // Día de la semana
    lcd.setCursor(2, 1);
    lcd.print("         ");
    lcd.noCursor();
    delay(200);
    lcd.setCursor(2, 1);
    lcd.print(dayAsString(timeNow.day));
    lcd.noCursor();
    delay(200);
  } else if (field == 1) {  // Día del mes
    lcd.setCursor(14, 1);
    lcd.print("  ");
    lcd.noCursor();
    delay(200);
    lcd.setCursor(14, 1);
    lcd.print(timeNow.date < 10 ? "0" : ""); lcd.print(timeNow.date);
    lcd.noCursor();
    delay(200);
  } else if (field == 2) {  // Mes
    lcd.setCursor(17, 1);
    lcd.print("  ");
    lcd.noCursor();
    delay(200);
    lcd.setCursor(17, 1);
    lcd.print(timeNow.mon < 10 ? "0" : ""); lcd.print(timeNow.mon);
    lcd.noCursor();
    delay(200);
  } else if (field == 3) {  // Year
    // Scroll the year
    static int scrollPosition = 0;
    lcd.setCursor(0, 1);
    lcd.print(timeNow.date < 10 ? "0" : ""); lcd.print(timeNow.date);
    lcd.print('/');
    lcd.print(timeNow.mon < 10 ? "0" : ""); lcd.print(timeNow.mon);
    lcd.print("  "); // Space for the year
    lcd.noCursor();
    delay(200);
    lcd.setCursor(14, 1);
    lcd.print("      "); // Clear the year space
    lcd.noCursor();
    delay(200);
    lcd.setCursor(14, 1);
    lcd.print(timeNow.yr); // Print the year
    lcd.print("  ");
    lcd.noCursor();
    delay(200);
    lcd.noBlink();
  
  } else if (field == 4) {  // Hora
    lcd.setCursor(6, 2);
    lcd.print("  ");
    lcd.noCursor();
    delay(200);
    lcd.setCursor(6, 2);
    lcd.print(timeNow.hr < 10 ? "0" : ""); lcd.print(timeNow.hr);
    lcd.noCursor();
    delay(200);
  } else if (field == 5) {  // Minuto
    lcd.setCursor(9, 2);
    lcd.print("  ");
    lcd.noCursor();
    delay(200);
    lcd.setCursor(9, 2);
    lcd.print(timeNow.min < 10 ? "0" : ""); lcd.print(timeNow.min);
    lcd.noCursor();
    delay(200);
  }
}

//Actualizar la pantalla de alarmas
void updateAlarmDisplay() {
    // Mostrar la cabecera
    lcd.setCursor(0, 0);
    lcd.print("HORAS INGESTAS");

    // Mostrar la alarma de la mañana
    lcd.setCursor(0, 1);
    lcd.print("MANANA: ");
    lcd.print(morningAlarm.hour < 10 ? "0" : ""); 
    lcd.print(morningAlarm.hour);
    lcd.print(":");
    lcd.print(morningAlarm.minute < 10 ? "0" : ""); 
    lcd.print(morningAlarm.minute);

    // Mostrar la alarma de la tarde
    lcd.setCursor(0, 2);
    lcd.print("TARDE:  ");
    lcd.print(afternoonAlarm.hour < 10 ? "0" : ""); 
    lcd.print(afternoonAlarm.hour);
    lcd.print(":");
    lcd.print(afternoonAlarm.minute < 10 ? "0" : ""); 
    lcd.print(afternoonAlarm.minute);

    // Mostrar la alarma de la noche
    lcd.setCursor(0, 3);
    lcd.print("NOCHE:  ");
    lcd.print(eveningAlarm.hour < 10 ? "0" : ""); 
    lcd.print(eveningAlarm.hour);
    lcd.print(":");
    lcd.print(eveningAlarm.minute < 10 ? "0" : ""); 
    lcd.print(eveningAlarm.minute);
}

//Control de los campos parpadeantes al sjustar las alarmas
void blinkAlarmField(int field, int subfield) {
    int displayRow = -1;

    // Asignar la fila de la pantalla según el campo actual
    switch (field) {
        case 0:
            displayRow = 1; // Mañana en la segunda línea
            break;
        case 1:
            displayRow = 2; // Tarde en la tercera línea
            break;
        case 2:
            displayRow = 3; // Noche en la cuarta línea
            break;
    }

    if (displayRow != -1) {
        switch (field) {
            case 0: // Morning alarm
                if (subfield == 0) {
                    lcd.setCursor(8, displayRow); lcd.print("  "); delay(200);
                    lcd.setCursor(8, displayRow); lcd.print(morningAlarm.hour < 10 ? "0" : ""); lcd.print(morningAlarm.hour); delay(200);
                } else {
                    lcd.setCursor(11, displayRow); lcd.print("  "); delay(200);
                    lcd.setCursor(11, displayRow); lcd.print(morningAlarm.minute < 10 ? "0" : ""); lcd.print(morningAlarm.minute); delay(200);
                }
                break;
            case 1: // Afternoon alarm
                if (subfield == 0) {
                    lcd.setCursor(8, displayRow); lcd.print("  "); delay(200);
                    lcd.setCursor(8, displayRow); lcd.print(afternoonAlarm.hour < 10 ? "0" : ""); lcd.print(afternoonAlarm.hour); delay(200);
                } else {
                    lcd.setCursor(11, displayRow); lcd.print("  "); delay(200);
                    lcd.setCursor(11, displayRow); lcd.print(afternoonAlarm.minute < 10 ? "0" : ""); lcd.print(afternoonAlarm.minute); delay(200);
                }
                break;
            case 2: // Evening alarm
                if (subfield == 0) {
                    lcd.setCursor(8, displayRow); lcd.print("  "); delay(200);
                    lcd.setCursor(8, displayRow); lcd.print(eveningAlarm.hour < 10 ? "0" : ""); lcd.print(eveningAlarm.hour); delay(200);
                } else {
                    lcd.setCursor(11, displayRow); lcd.print("  "); delay(200);
                    lcd.setCursor(11, displayRow); lcd.print(eveningAlarm.minute < 10 ? "0" : ""); lcd.print(eveningAlarm.minute); delay(200);
                }
                break;
        }
        lcd.noCursor(); // Asegurarse de que el cursor esté apagado después de cada parpadeo
    }
}

//Actualizar la pantalla estandar
void updateDisplay() {
  if (currentSystemMode == NORMAL) {
    timeNow = rtc.time();
    printTime(timeNow);
  }
}

//Actualizar la pantalla de configuración

void updateProgrammingDisplay(int currentOption) {
    lcd.clear();
    int startOption;

    if (currentOption <= 4) {
        // Mostrar opciones de 1 a 4
        startOption = 1;
    } else {
        // Mostrar opciones actuales con el cursor siempre en la última fila (4)
        startOption = currentOption - 3;
    }
    
    // Mostrar las opciones en la pantalla
    for (int i = 0; i < 4; i++) {
        int option = startOption + i;
        if (option > 11) break; // Evitar opciones fuera de rango

        switch (option) {
            case 1:
                lcd.setCursor(0, i);
                lcd.print("1-RECARGAR");
                break;
            case 2:
                lcd.setCursor(0, i);
                lcd.print("2-HORAS INGESTAS");
                break;
            case 3:
                lcd.setCursor(0, i);
                lcd.print("3-AVANZAR");
                break;
            case 4:
                lcd.setCursor(0, i);
                lcd.print("4-RETROCEDER");
                break;
            case 5:
                lcd.setCursor(0, i);
                lcd.print("5-CAMBIAR HORA");
                break;
            case 6:
                lcd.setCursor(0, i);
                lcd.print("6-CAMBIAR PIN");
                break;
            case 7:
                lcd.setCursor(0, i);
                lcd.print("7-SONIDO ALARMAS");
                break;
            case 8:
                lcd.setCursor(0, i);
                lcd.print("8-SENSOR APAGADO");
                break;
            case 9:
                lcd.setCursor(0, i);
                lcd.print("9-MENSAJES WEB");
                break;
            case 10:
                lcd.setCursor(0, i);
                lcd.print("10-CONFIGURACION WEB");
                break;
            case 11:
                lcd.setCursor(0, i);
                lcd.print("11-SALIR");
                break;
        }
    }

    // Mover el cursor a la opción actual y activar el parpadeo
    int cursorPosition = currentOption <= 4 ? currentOption - 1 : 3;
    lcd.setCursor(0, cursorPosition);
    lcd.blink();
}


//Actualiza la pantalla del PIN
void updatePasswordDisplay(char* digits, int currentDigitIndex) {
    lcd.noBlink();  // Desactivar parpadeo antes de actualizar
    
    lcd.setCursor(5, 2);
    lcd.print(digits[0]);
    lcd.setCursor(7, 2);
    lcd.print(digits[1]);
    lcd.setCursor(9, 2);
    lcd.print(digits[2]);
    lcd.setCursor(11, 2);
    lcd.print(digits[3]);
    
    // Colocar el cursor en el dígito actual para parpadear
    lcd.setCursor(5 + currentDigitIndex * 2, 2);
    lcd.blink();  // Reactivar parpadeo después de actualizar
}

//Muestra el mensaje de recarga
void showRefillMessage(int day, int timeOfDay) { // Función para mostrar el mensaje de recarga actual
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(daysOfWeek[day]);
  lcd.print(" ");
  lcd.print(timesOfDay[timeOfDay]);
  delay (1000);
}

//Muestra el mensaje de Alarma
void displayAlarmMessage(int dayIndex, const char* timeOfDay, bool alarmEnabled) {
    const char* daysOfWeek[] = {"DOMINGO", "LUNES", "MARTES", "MIERCOLES", "JUEVES", "VIERNES", "SABADO"};
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("INGESTA ");
    lcd.print(daysOfWeek[dayIndex-1]);
    lcd.setCursor(0, 2);
    lcd.print(timeOfDay);

    if (alarmEnabled) {
        // Enter DISPENSING mode and reset proximity flag
        currentSystemMode = DISPENSING;
        proximityTriggered = false;

        // Trigger the buzzer and wait for proximity sensor to be triggered
        triggerBuzzer();
    }

    // Maintain the message on the screen
    unsigned long startTime = millis();
    while (millis() - startTime < 300000) {  // Adjust timing as needed
        // Keep the message on the screen without sounding the buzzer
        // Other tasks can be performed here
        if (proximityTriggered) {
            break;  // Exit loop if proximity was triggered
        }
    }

    // Return to normal mode and clear the screen
    currentSystemMode = NORMAL;
    lcd.clear();
    updateDisplay();
    lcd.noBacklight();
}


//Muestra el mensaje de avance / retroceso
void showAdvanceMessage(int dayIndex, int timeIndex) {
    // Array con los nombres de los días de la semana
    const char* daysOfWeek[] = {"DOMINGO", "LUNES", "MARTES", "MIERCOLES", "JUEVES", "VIERNES", "SABADO"};
    const char* timesOfDay[] = {"MANANA", "TARDE", "NOCHE"};
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print(daysOfWeek[dayIndex]);
    lcd.setCursor(0, 2);
    lcd.print(timesOfDay[timeIndex]);
}

// METODOS DEL BUZZER

void displaySoundMenu(int topField) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SONIDO ALARMAS");
    lcd.setCursor(0, 1);
    lcd.print("MANANA");
    lcd.setCursor(10, 1);
    lcd.print(morningAlarmEnabled ? "ACT" : "DES");

    lcd.setCursor(0, 2);
    lcd.print("TARDE");
    lcd.setCursor(10, 2);
    lcd.print(afternoonAlarmEnabled ? "ACT" : "DES");

    lcd.setCursor(0, 3); // Usar la tercera línea
    lcd.print("NOCHE");
    lcd.setCursor(10, 3);
    lcd.print(eveningAlarmEnabled ? "ACT" : "DES");

    lcd.noCursor();
    lcd.noBlink();
}



// Controla los campos parpadeantes del menú sonidos de alarmas

void updateSoundBlinkfield(int topField, int field) {
    static int blinkState = 0;
    static int lastField = 0;
    static int lastTopField = 1; // Almacenar el último topField

    int row = 1;  // Inicializar en 1 para evitar escribir en la fila 0

    // Determinar la fila correcta según el campo y el topField actual
    if (topField == 1) {
        if (field == 1) {
            row = 1;  // MANANA en la fila 1
        } else if (field == 2) {
            row = 2;  // TARDE en la fila 2
        }
    } else if (topField == 2) {
        if (field == 2) {
            row = 1;  // TARDE ahora se muestra en la fila 1
        } else if (field == 3) {
            row = 3;  // NOCHE se muestra en la fila 3
        }
    }

    // Limpiar la fila anterior solo si el campo ha cambiado y está en la misma pantalla
    if (field != lastField && topField == lastTopField) {
        int previousRow = -1;

        if (lastTopField == 1) {
            if (lastField == 1) {
                previousRow = 1;  // MANANA estaba en la fila 1
            } else if (lastField == 2) {
                previousRow = 2;  // TARDE estaba en la fila 2
            }
        } else if (lastTopField == 2) {
            if (lastField == 2) {
                previousRow = 1;  // TARDE estaba en la fila 1
            } else if (lastField == 3) {
                previousRow = 3;  // NOCHE estaba en la fila 3
            }
        }

        // Redibujar el valor del campo anterior si previousRow es válido
        if (previousRow != -1) {
            lcd.setCursor(10, previousRow);
            if ((lastField == 1 && morningAlarmEnabled) ||
                (lastField == 2 && afternoonAlarmEnabled) ||
                (lastField == 3 && eveningAlarmEnabled)) {
                lcd.print("ACT");
            } else {
                lcd.print("DES");
            }
        }
    }

    // Ahora manejar el parpadeo del campo actual
    if (blinkState == 0) {
        lcd.setCursor(10, row);
        if ((field == 1 && morningAlarmEnabled) || 
            (field == 2 && afternoonAlarmEnabled) ||
            (field == 3 && eveningAlarmEnabled)) {
            lcd.print("ACT");
        } else {
            lcd.print("DES");
        }
        blinkState = 1;
    } else {
        lcd.setCursor(10, row);
        lcd.print("   "); // Limpiar el campo para el parpadeo
        blinkState = 0;
    }

    // Actualizar los estados
    lastField = field;
    lastTopField = topField;

    lcd.noCursor();
    lcd.noBlink();
    delay(250); // Ajustar el retardo para la velocidad de parpadeo
}

void setSound() {

    //Manejo antirebote de los botones
    unsigned long lastDebounceTimeSet = 0;
    unsigned long lastDebounceTimeOption = 0;
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;
    unsigned long currentTime = millis();
    
    inProgrammingMode = true;
    int fieldToSet = 1; // Start with MANANA
    int topField = 1; // Start with MANANA and TARDE displayed
    
    displaySoundMenu(topField);
    
    while (inProgrammingMode) {
        updateSoundBlinkfield(topField, fieldToSet);
        
        //Rutina antirebote para ambos botones
    int buttonSetState = digitalRead(buttonSetPin);
    int buttonOptionState = digitalRead(buttonOptionPin);

    // Debounce buttonSet
    if (buttonSetState != lastButtonSetState) {
            lastDebounceTimeSet = currentTime;
    }
        if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
            lastButtonSetState = buttonSetState;
    }

    // Debounce buttonOption
    if (buttonOptionState != lastButtonOptionState) {
        lastDebounceTimeOption = currentTime;
    }
    if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
            lastButtonOptionState = buttonOptionState;
    }
    //Fin de la rutina antirebote
        
        if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
            // Toggle the alarm state
            switch (fieldToSet) {
                case 1:
                    morningAlarmEnabled = !morningAlarmEnabled;
                    break;
                case 2:
                    afternoonAlarmEnabled = !afternoonAlarmEnabled;
                    break;
                case 3:
                    eveningAlarmEnabled = !eveningAlarmEnabled;
                    break;
            }
            displaySoundMenu(topField); // Update the display
        }
        if (buttonSetState == LOW && lastButtonSetState == HIGH) {
            fieldToSet++;
            if (fieldToSet > 3) {
                inProgrammingMode = false;
                currentProgrammingMode = CONFIG_MODE; // Reset programming mode
                lcd.clear(); // Clear the display
            } else if (fieldToSet == 3) {
                topField = 2; // Scroll to TARDE and NOCHE
                displaySoundMenu(topField); // Update the display
            }
        }
        lastButtonOptionState = buttonOptionState;
        lastButtonSetState = buttonSetState;
        //delay(50); // Debounce delay
    }
    saveAlarmsToEEPROM();
    
}

void triggerBuzzer() {
    if (proximitySensorEnabled) {
        unsigned long startTime = millis();  // Guarda el tiempo de inicio

        while (!proximityTriggered) {
            // Activa el buzzer
            digitalWrite(buzzerPin, HIGH);
            delay(1000);  // Duración del sonido del buzzer
            digitalWrite(buzzerPin, LOW);
            delay(500);  // Retraso entre sonidos

            // Verifica el sensor de proximidad
            if (checkProximity()) {
              writeWebMessage();
              if(telegramMessagesEnabled) {
                sendTelegramMessage(message);
              }
              if(emailMessagesEnabled) {
                sendEmailMessage(message);
              }
                
              proximityTriggered = true;  // Se activa la bandera cuando se detecta un objeto
              break;  // Salir del bucle si se activa el sensor de proximidad
            }

            // Verifica si han pasado 30 minutos (1800000 milisegundos)
            if (millis() - startTime >= 1800000) {
                writeWebMessage();
                message = "MEDICAMENTO NO RETIRADO\n" + message;
                if(telegramMessagesEnabled) {
                  sendTelegramMessage(message);
                }
                if(emailMessagesEnabled) {
                  sendEmailMessage(message);
                } 
                break;  // Salir del bucle después de 30 minutos
            }
        }
    } else {
        // Buzzer suena 10 veces con un retardo entre cada sonido
        for (int i = 0; i < 10; i++) {
            digitalWrite(buzzerPin, HIGH);
            delay(500);  // Buzzer encendido durante 500 ms
            digitalWrite(buzzerPin, LOW);
            delay(500);  // Buzzer apagado durante 500 ms
        }
    }
}

//*************************************************************************
// METODOS DEL RELOJ

//Ajusta la hora
void setTime() {
  
      //Manejo antirebote de los botones
    unsigned long lastDebounceTimeSet = 0;
    unsigned long lastDebounceTimeOption = 0;
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;
    unsigned long currentTime = millis();
  
  delay(500);
  fieldToSet = 0;
  inProgrammingMode = true;
  // Limpiar la pantalla antes de entrar en el bucle de configuración
  lcd.clear();
  printTime(timeNow);
  while (inProgrammingMode) {
    blinkField(fieldToSet);
    
    //Rutina antirebote para ambos botones
    int buttonSetState = digitalRead(buttonSetPin);
    int buttonOptionState = digitalRead(buttonOptionPin);

    // Debounce buttonSet
    if (buttonSetState != lastButtonSetState) {
            lastDebounceTimeSet = currentTime;
    }
        if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
            lastButtonSetState = buttonSetState;
    }

    // Debounce buttonOption
    if (buttonOptionState != lastButtonOptionState) {
        lastDebounceTimeOption = currentTime;
    }
    if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
            lastButtonOptionState = buttonOptionState;
    }
    //Fin de la rutina antirebote

    if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
      if (fieldToSet == 0) {
        timeNow.day = static_cast<Time::Day>((timeNow.day + 1) % 7);
      } else if (fieldToSet == 1) {
        timeNow.date = (timeNow.date % 31) + 1;
      } else if (fieldToSet == 2) {
        timeNow.mon = (timeNow.mon % 12) + 1;
      } else if (fieldToSet == 3) { // Year
        timeNow.yr = (timeNow.yr + 1) % 10000; 
        rtc.time(timeNow); // Update the RTC with the new year
    }  else if (fieldToSet == 4) {
        timeNow.hr = (timeNow.hr + 1) % 24;
      } else if (fieldToSet == 5) {
        timeNow.min = (timeNow.min + 1) % 60;
      }
      rtc.time(timeNow);
    }
    if (buttonSetState == LOW && lastButtonSetState == HIGH) {
      fieldToSet++;
      if (fieldToSet > 5) {
        inProgrammingMode = false;
      }
    }
    lastButtonOptionState = buttonOptionState;
    lastButtonSetState = buttonSetState;
    // Delay to avoid bouncing
    //delay(50);
  }
  // Ensure display returns to normal after setting time
  lcd.clear();
  updateDisplay();
  // Guardar la hora en el DS1302
  rtc.time(timeNow);
}

//Ajusta las alarmas
void setAlarms() {
    
    // Manejo antirebote de los botones
    unsigned long lastDebounceTimeSet = 0;
    unsigned long lastDebounceTimeOption = 0;
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;
    unsigned long currentTime = millis();
    
    int fieldToSet = 0;
    int subfieldToSet = 0;
    inProgrammingMode = true;

    // Limpiar la pantalla y mostrar la cabecera
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("HORAS INGESTAS");
    updateAlarmDisplay();
    lcd.noBlink();
    lcd.noCursor(); // Asegurarse de que el cursor esté apagado al inicio

    while (inProgrammingMode) {
        blinkAlarmField(fieldToSet, subfieldToSet);
        
        // Rutina antirebote para ambos botones
        int buttonSetState = digitalRead(buttonSetPin);
        int buttonOptionState = digitalRead(buttonOptionPin);

        // Debounce buttonSet
        if (buttonSetState != lastButtonSetState) {
            lastDebounceTimeSet = currentTime;
        }
        if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
            lastButtonSetState = buttonSetState;
        }

        // Debounce buttonOption
        if (buttonOptionState != lastButtonOptionState) {
            lastDebounceTimeOption = currentTime;
        }
        if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
            lastButtonOptionState = buttonOptionState;
        }
        // Fin de la rutina antirebote

        // Cambiar la hora o los minutos según el campo actual
        if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
            if (fieldToSet == 0) {
                if (subfieldToSet == 0) {
                    morningAlarm.hour = (morningAlarm.hour + 1) % 24;
                } else {
                    morningAlarm.minute = (morningAlarm.minute + 1) % 60;
                }
            } else if (fieldToSet == 1) {
                if (subfieldToSet == 0) {
                    afternoonAlarm.hour = (afternoonAlarm.hour + 1) % 24;
                } else {
                    afternoonAlarm.minute = (afternoonAlarm.minute + 1) % 60;
                }
            } else if (fieldToSet == 2) {
                if (subfieldToSet == 0) {
                    eveningAlarm.hour = (eveningAlarm.hour + 1) % 24;
                } else {
                    eveningAlarm.minute = (eveningAlarm.minute + 1) % 60;
                }
            }
            
            updateAlarmDisplay();
            lcd.noCursor(); // Asegurarse de que el cursor esté apagado después de actualizar la pantalla
        }

        // Cambiar de campo (horas/minutos) o avanzar al siguiente alarm
        if (buttonSetState == LOW && lastButtonSetState == HIGH) {
            subfieldToSet++;
            if (subfieldToSet > 1) {
                subfieldToSet = 0;
                fieldToSet++;
                if (fieldToSet > 2) {
                    inProgrammingMode = false;
                    currentProgrammingMode = CONFIG_MODE;
                }
            }
            updateAlarmDisplay();
            lcd.noCursor(); // Asegurarse de que el cursor esté apagado después de actualizar la pantalla
        }

        lastButtonOptionState = buttonOptionState;
        lastButtonSetState = buttonSetState;
    }

    // Asegurarse de que la pantalla regrese al modo normal después de configurar las alarmas
    lcd.clear();
    updateDisplay();
    lcd.noCursor(); // Asegurarse de que el cursor esté apagado después de regresar al modo normal

    // Guardar las alarmas en la EEPROM después de configurarlas
    saveAlarmsToEEPROM();


    // Resetear el modo de programación
    currentProgrammingMode = CONFIG_MODE;
}

//Verifica si se cumple la hora de alguna alarma
void checkAlarms() {
    // Obtener la hora actual
    timeNow = rtc.time();
    int currentHour = timeNow.hr;
    int currentMinute = timeNow.min;
    int currentDay = static_cast<int>(timeNow.day);
    
    // Determinar el índice de tiempo (0: Mañana, 1: Tarde, 2: Noche)
    int timeIndex = 2; // 2: Noche, 1: Tarde, 0: Mañana

    if ((currentHour > eveningAlarm.hour) || (currentHour == eveningAlarm.hour && currentMinute > eveningAlarm.minute)) {
        timeIndex = 2; // Noche de hoy
    } else if ((currentHour > afternoonAlarm.hour) || (currentHour == afternoonAlarm.hour && currentMinute > afternoonAlarm.minute)) {
        timeIndex = 1; // Tarde de hoy
    } else if ((currentHour > morningAlarm.hour) || (currentHour == morningAlarm.hour && currentMinute > morningAlarm.minute)) {
        timeIndex = 0; // Mañana de hoy
    } else {
        timeIndex = 2; // Noche del día anterior
        currentDay = (currentDay - 1 + 7) % 7;
    }

    // Comparar la hora actual con las alarmas
    if (currentHour == morningAlarm.hour && currentMinute == morningAlarm.minute) {
        if (!morningAlarmTriggered) {
            serveNext();
            displayAlarmMessage(currentDay, "MANANA", morningAlarmEnabled);
            morningAlarmTriggered = true;
        }
    } else {
        morningAlarmTriggered = false; // Reset trigger for the next day
    }

    if (currentHour == afternoonAlarm.hour && currentMinute == afternoonAlarm.minute) {
        if (!afternoonAlarmTriggered) {
            serveNext();
            displayAlarmMessage(currentDay, "TARDE", afternoonAlarmEnabled);
            afternoonAlarmTriggered = true;
        }
    } else {
        afternoonAlarmTriggered = false; // Reset trigger for the next day
    }

    if (currentHour == eveningAlarm.hour && currentMinute == eveningAlarm.minute) {
        if (!eveningAlarmTriggered) {
            serveNext();
            displayAlarmMessage(currentDay, "NOCHE", eveningAlarmEnabled);
            eveningAlarmTriggered = true;
        }
    } else {
        eveningAlarmTriggered = false; // Reset trigger for the next day
    }
}

// ***************************************************************************************
// MANEJO DE LOS BOTONES

//Control de los botones - pulsación larga y encedido de luz en el display

void handleButtons() {
    static unsigned long lastDebounceTimeSet = 0;
    static unsigned long lastDebounceTimeOption = 0;
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;
    unsigned long currentTime = millis();
    int currentButtonSetState = digitalRead(buttonSetPin);
    int currentButtonOptionState = digitalRead(buttonOptionPin);

    // Enciende el backlight
    if (currentButtonSetState == LOW || currentButtonOptionState == LOW) {
        backlightConnectedTime = millis();
        lcd.backlight();
    }

    // Antirrebote para buttonSet
    if (currentButtonSetState != lastButtonSetState) {
        lastDebounceTimeSet = currentTime;
    }
    if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
        if (currentButtonSetState == LOW) {
            // Acción del botón Set
        }
    }

    // Antirrebote para buttonOption
    if (currentButtonOptionState != lastButtonOptionState) {
        lastDebounceTimeOption = currentTime;
    }
    if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
        if (currentButtonOptionState == LOW) {
            // Acción del botón Option
        }
    }

    // Comprobación si ambos botones están presionados simultáneamente
    if (currentButtonSetState == LOW && currentButtonOptionState == LOW) {
        if (currentSystemMode == NORMAL) { // Solo cambiar a modo de programación si estamos en modo normal
            requestPassword();
            // Wait until both buttons are released
            while (digitalRead(buttonSetPin) == LOW || digitalRead(buttonOptionPin) == LOW) {
                delay(10); // Small delay to prevent blocking
            }
            // Update button states after they are released
            currentButtonSetState = digitalRead(buttonSetPin);
            currentButtonOptionState = digitalRead(buttonOptionPin);
        }
    }

    lastButtonSetState = currentButtonSetState;
    lastButtonOptionState = currentButtonOptionState;

    if (currentSystemMode == PROGRAMMING) {
        handleProgramming(currentButtonSetState, currentButtonOptionState);
        // Exit the handleButtons function if still in programming mode
        if (currentSystemMode == PROGRAMMING) {
            return; 
        }
    }

    // Comprobación del tiempo para apagar el backlight
    if (millis() - backlightConnectedTime > backlightSleepTime) {
        lcd.noBacklight();
    }
}

// Controla el menú de configuración
void handleProgramming(int buttonSetState, int buttonOptionState) {
    unsigned long debounceDelay = 50;
    static unsigned long lastButtonTimeOption = 0;
    static unsigned long lastButtonTimeSet = 0;
    static int currentOption = 0; // Start with option 1
    
    // Always show the programming display initially
    if (currentOption == 0) {
        currentOption = 1; // Ensure currentOption is within valid range
        updateProgrammingDisplay(currentOption);
    }
    
    if ((millis() - lastButtonTimeOption) > debounceDelay) {
        if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
            // Cycle through options 1 to 11
            currentOption = (currentOption % 11) + 1;
            updateProgrammingDisplay(currentOption); // Solo actualizar la pantalla cuando se cambia la opción
            
            
            //lcd.noBlink();
            lastButtonTimeOption = millis();
        }
        if ((millis() - lastButtonTimeSet) > debounceDelay) {
            if (buttonSetState == LOW && lastButtonSetState == HIGH) {
                // Execute the selected option
                lcd.clear(); // Limpiar la pantalla antes de ejecutar cualquier opción
                switch (currentOption) {
                    case 1: // RECARGAR
                        currentProgrammingMode = REFILL;
                        refillCompartments();
                        break;
                    case 2: // CAMBIAR INGESTAS
                        currentProgrammingMode = SET_ALARMS;
                        
                        
                        setAlarms();
                        break;
                    case 3: // AVANZAR
                        currentProgrammingMode = ADVANCE;
                        advance();
                        break;
                    case 4: // RETROCEDER
                        currentProgrammingMode = ADVANCE;
                        retreat();
                        break;
                    case 5: // CAMBIAR HORA
                        currentProgrammingMode = SET_TIME;
                        setTime();
                        break;
                    case 6: // CAMBIAR PASSWORD
                        currentProgrammingMode = CHANGE_PASSWORD;
                        changePassword();
                        break;
                    case 7: // ALARMAS SONORAS
                        currentProgrammingMode = SET_SOUND;
                        setSound();
                        break;
                    case 8: // SENSOR PROXIMIDAD
                        currentProgrammingMode = SET_SENSOR;
                        setSensor();
                        break;
                    case 9: // ACTIVAR MENSAJES
                        currentProgrammingMode = SET_MESSAGES;
                        setMessages();
                        break;
                    case 10: // CONFIGURAR MENSAJERIA
                        currentProgrammingMode = CONFIG_WEB;
                        configWeb();
                        break;
                    
                    case 11: // SALIR
                        break;
                    
                }
                currentSystemMode = NORMAL; // Exit programming mode
                currentProgrammingMode = CONFIG_MODE; // Reset programming mode
                lastButtonTimeSet = millis();
            }
        }
        
    }
    lastButtonOptionState = buttonOptionState;
    lastButtonSetState = buttonSetState;
}        
//*********************************************************************************
// METODOS DE LA CONTRASEÑA

//Verificar contraseña. Devuelve true si la contraseña es correcta

bool inputPassword(char* password) {
    int currentDigitIndex = 0;
    char enteredDigits[5] = {'1', '2', '3', '4', '\0'};
    bool digitSelected[4] = {false, false, false, false};
    // Posiciones de los dígitos en el LCD
    int digitPositions[4] = {5, 7, 9, 11};
    lcd.setCursor(4, 2);
    lcd.print(" 1 2 3 4");
    lcd.setCursor(digitPositions[currentDigitIndex], 2);
    lcd.blink();

    // Debounce variables for buttonSet
    unsigned long lastDebounceTimeOption = 0;
    unsigned long lastDebounceTimeSet = 0;
    unsigned long debounceDelay = 50; // Adjust this value as needed

    while (currentDigitIndex < 4) {
        int buttonOptionState = digitalRead(buttonOptionPin);
        int buttonSetState = digitalRead(buttonSetPin);

        // Debounce for buttonSet
        if (buttonSetState != lastButtonSetState) {
            lastDebounceTimeSet = millis();
        }
        if ((millis() - lastDebounceTimeSet) > debounceDelay) {
            if (buttonSetState == LOW) {
                // ButtonSet is pressed and debounced
                digitSelected[currentDigitIndex] = true;
                currentDigitIndex++;
                
                if (currentDigitIndex < 4) {
                    lcd.setCursor(digitPositions[currentDigitIndex], 2);
                    lcd.blink();
                }
                delay(300); // Debounce delay
            }
        }

        if ((millis() - lastDebounceTimeOption) > debounceDelay) {
            if (buttonOptionState == LOW && lastButtonOptionState == HIGH && !digitSelected[currentDigitIndex]) {
                enteredDigits[currentDigitIndex] = (enteredDigits[currentDigitIndex] == '9') ? '0' : (enteredDigits[currentDigitIndex] + 1);
                updatePasswordDisplay(enteredDigits, currentDigitIndex);
                delay(300); // Debounce delay
            }
        }
        lastButtonOptionState = buttonOptionState;
        lastButtonSetState = buttonSetState;    
    }

    strcpy(password, enteredDigits);

    lcd.noBlink();
    return true;
}

//Pedir la contraseña
void requestPassword() {
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("Introducir PIN:");
    delay(1000);
    char enteredPassword[5];

    if (inputPassword(enteredPassword)) {
        if (strcmp(enteredPassword, currentPassword) == 0) {
            lcd.clear();
            lcd.setCursor(0,1);
            lcd.print("PIN Correcto");
            delay(1000);
            currentSystemMode = PROGRAMMING;
            currentProgrammingMode = CONFIG_MODE;
            handleProgramming(HIGH, HIGH);
        } else {
            lcd.clear();
            lcd.print("PIN Incorrecto");
            delay(2000); // Mostrar el mensaje durante 2 segundos
            currentProgrammingMode = CONFIG_MODE;
        }
    }
    lcd.noBlink();
}

//Cambiar la contraseña
void changePassword() {
    //Manejo antirebote de los botones
    unsigned long lastDebounceTimeSet = 0;
    unsigned long lastDebounceTimeOption = 0;
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;
    unsigned long currentTime = millis();

    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Nuevo PIN:");
    //lcd.setCursor(0, 1);
    delay(1000);
  
    passwordIndex = 0;
    inProgrammingMode = true;

    // Inicializar newPassword con "1234"
    strcpy(newPassword, "1234");

    // Mostrar la contraseña inicial en el display
    updatePasswordDisplay(newPassword, passwordIndex);

    while (inProgrammingMode) {
        //Rutina antirebote para ambos botones
        int buttonSetState = digitalRead(buttonSetPin);
        int buttonOptionState = digitalRead(buttonOptionPin);

        if (buttonSetState != lastButtonSetState) {
            lastDebounceTimeSet = currentTime;
        }
        if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
            lastButtonSetState = buttonSetState;
        }

        if (buttonOptionState != lastButtonOptionState) {
            lastDebounceTimeOption = currentTime;
        }
        if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
            lastButtonOptionState = buttonOptionState;
        }

        if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
            if (passwordIndex < 4) {
                newPassword[passwordIndex] = '0' + ((newPassword[passwordIndex] - '0' + 1) % 10);
                updatePasswordDisplay(newPassword, passwordIndex);
            }
        }

        if (buttonSetState == LOW && lastButtonSetState == HIGH) {
            passwordIndex++;
            if (passwordIndex > 3) {
                inProgrammingMode = false;
            } else {
                lcd.setCursor(passwordIndex * 2 + 5, 2);  // Ajusta según las nuevas posiciones
                lcd.blink();
            }
        }

        lastButtonOptionState = buttonOptionState;
        lastButtonSetState = buttonSetState;

        delay(100); // Evitar rebotes
    }

    // Confirmar contraseña
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Confirmar el PIN:");
    //lcd.setCursor(0, 1);
    delay(1000);

    passwordIndex = 0;
    inProgrammingMode = true;

    // Inicializar confirmPassword con "1234"
    strcpy(confirmPassword, "1234");

    // Mostrar la contraseña inicial en el display
    updatePasswordDisplay(confirmPassword, passwordIndex);

    while (inProgrammingMode) {
        int buttonOptionState = digitalRead(buttonOptionPin);
        int buttonSetState = digitalRead(buttonSetPin);

        if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
            if (passwordIndex < 4) {
                confirmPassword[passwordIndex] = '0' + ((confirmPassword[passwordIndex] - '0' + 1) % 10);
                updatePasswordDisplay(confirmPassword, passwordIndex);
            }
        }

        if (buttonSetState == LOW && lastButtonSetState == HIGH) {
            passwordIndex++;
            if (passwordIndex > 3) {
                inProgrammingMode = false;
            } else {
                lcd.setCursor(passwordIndex * 2 + 5, 2);  // Ajusta según las nuevas posiciones
                lcd.blink();
            }
        }

        lastButtonOptionState = buttonOptionState;
        lastButtonSetState = buttonSetState;

        delay(100); // Evitar rebotes
    }

    if (strcmp(newPassword, confirmPassword) == 0) {
        strcpy(currentPassword, newPassword);
        savePasswordToEEPROM(newPassword);
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("PIN cambiado");
        delay(2000);
    } else {
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("El PIN ");
        lcd.setCursor(0, 2);
        lcd.print("NO ES IGUAL");
        delay(2000);
    }

    lcd.clear();
    updateDisplay();
}


//******************************************************************************
// MANEJO DE MECANISMOS

// Detecta si el motor se está moviendo
bool isMotorMoving() {
    return motor.distanceToGo() != 0;
}
/*
// Activar el motor
void attachMotor() {
  AccelStepper motor(4, motorPin1, motorPin2, motorPin3, motorPin4);  
}
*/

// Dejar el motor sin energía
void unpowerMotor() {
    digitalWrite(motorPin1, LOW);
    digitalWrite(motorPin2, LOW);
    digitalWrite(motorPin3, LOW);
    digitalWrite(motorPin4, LOW);
}
//Dispensar la dosis ingesta que corresponde
void serveNext() {  
   motor.move(stepsPerPosition); // El motor se mueve lo suficiente para girar 360/22º
   
   // Mientras el motor no haya alcanzado su destino, continúa ejecutando el movimiento
   while (motor.distanceToGo() != 0) {
     motor.run();  // Ejecuta el movimiento del motor
   } 
   unpowerMotor();
}
//Recargar los compartimentos
void refillCompartments() {

    //Manejo antirebote de los botones
    unsigned long lastDebounceTimeSet = 0;
    unsigned long lastDebounceTimeOption = 0;
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;
    unsigned long currentTime = millis();
    
    lcd.clear();
    lcd.print("RECARGANDO...");
    delay(1500);  // Espera de 1.5 segundos

    inProgrammingMode = true;
    
    // Obtener la hora y el día actuales
    timeNow = rtc.time();
    int currentHour = timeNow.hr;
    int currentMinute = timeNow.min;
    int currentDay = static_cast<int>(timeNow.day);
    
    // Determinar la ingesta anterior
    int dayIndex = currentDay-1 ;
    int timeIndex = 2; // 2: Noche, 1: Tarde, 0: Mañana

    if ((currentHour > eveningAlarm.hour) || (currentHour == eveningAlarm.hour && currentMinute > eveningAlarm.minute)) {
        timeIndex = 2; // Noche de hoy
    } else if ((currentHour > afternoonAlarm.hour) || (currentHour == afternoonAlarm.hour && currentMinute > afternoonAlarm.minute)) {
        timeIndex = 1; // Tarde de hoy
    } else if ((currentHour > morningAlarm.hour) || (currentHour == morningAlarm.hour && currentMinute > morningAlarm.minute)) {
        timeIndex = 0; // Mañana de hoy
    } else {
        timeIndex = 2; // Noche del día anterior
        dayIndex = (dayIndex - 1 + 7) % 7;
    }

    int initialDayIndex = dayIndex ;
    int initialTimeIndex = timeIndex;
     
    // Mostrar el compartimento actual
    showRefillMessage(dayIndex, timeIndex);

    int refillCount = 0;

    while (refillCount < 21) {
        
        //Rutina antirebote para ambos botones
        int buttonSetState = digitalRead(buttonSetPin);
        int buttonOptionState = digitalRead(buttonOptionPin);

        // Debounce buttonSet
        if (buttonSetState != lastButtonSetState) {
            lastDebounceTimeSet = currentTime;
        }
        if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
            lastButtonSetState = buttonSetState;
        }

        // Debounce buttonOption
        if (buttonOptionState != lastButtonOptionState) {
            lastDebounceTimeOption = currentTime;
        }
        if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
            lastButtonOptionState = buttonOptionState;
        }
        //Fin de la rutina antirebote

        if (buttonSetState == LOW && lastButtonSetState == HIGH) {
            // Salir del proceso de avance
            lcd.clear();
            lcd.print("VOLVIENDO A ");
            lcd.setCursor(0, 1);
            lcd.print(daysOfWeek[initialDayIndex]);
            lcd.print(" ");
            lcd.print(timesOfDay[initialTimeIndex]);
            delay(2000);
            lcd.clear();
                            
            int steps = -refillCount * stepsPerPosition;
            goBack(steps);
            inProgrammingMode = false;
            lcd.clear();
            updateDisplay();
            currentProgrammingMode = CONFIG_MODE;;
            return;
        }

        if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
            // Avanzar al siguiente compartimento
            serveNext();
            //delay(1000);

            // Actualizar el estado del día y la hora
            timeIndex++;
            if (timeIndex > 2) {
                timeIndex = 0;
                dayIndex = (dayIndex + 1) % 7;
            }

            // Mostrar el siguiente compartimento
            showRefillMessage(dayIndex, timeIndex);

            // Incrementar el contador de avance
            refillCount++;
            if (refillCount >= 21) {
                // Salir del proceso de avance después de 21 veces
                delay(1000);
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("RECARGADO");
                lcd.setCursor(0, 1);
                lcd.print("SEMANA COMPLETA");
                delay(1500);
                lcd.clear();
                lcd.print("VOLVIENDO A ");
                lcd.print(daysOfWeek[initialDayIndex]);
                lcd.setCursor(0, 1);
                lcd.print(timesOfDay[initialTimeIndex]);
                delay(2000);
                
                int steps = -refillCount * stepsPerPosition;
                goBack(steps);
                
                lcd.clear();
                                                
                lcd.clear();
                updateDisplay();
                currentProgrammingMode = CONFIG_MODE;
                return;
            }
        }

        lastButtonOptionState = buttonOptionState;
        lastButtonSetState = buttonSetState;
        //delay(200); // Evitar rebotes
    }
}

//Avanzar
void advance() {
    
    //Manejo antirebote de los botones
    unsigned long lastDebounceTimeSet = 0;
    unsigned long lastDebounceTimeOption = 0;
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;
    unsigned long currentTime = millis();
    long initialPosition();   
    
    lcd.clear();
    lcd.print("Iniciando Avance");
    delay(1500);  // Espera de 1.5 segundos

    // Obtener la hora y el día actuales
    timeNow = rtc.time();
    int currentHour = timeNow.hr;
    int currentMinute = timeNow.min;
    int currentDay = static_cast<int>(timeNow.day);
    
// Determinar la ingesta anterior
    int dayIndex = currentDay-1 ;
    int timeIndex = 2; // 2: Noche, 1: Tarde, 0: Mañana

    if ((currentHour > eveningAlarm.hour) || (currentHour == eveningAlarm.hour && currentMinute > eveningAlarm.minute)) {
        timeIndex = 2; // Noche de hoy
    } else if ((currentHour > afternoonAlarm.hour) || (currentHour == afternoonAlarm.hour && currentMinute > afternoonAlarm.minute)) {
        timeIndex = 1; // Tarde de hoy
    } else if ((currentHour > morningAlarm.hour) || (currentHour == morningAlarm.hour && currentMinute > morningAlarm.minute)) {
        timeIndex = 0; // Mañana de hoy
    } else {
        timeIndex = 2; // Noche del día anterior
        dayIndex = (dayIndex - 1 + 7) % 7;
    }

    int initialDayIndex = dayIndex ;
    int initialTimeIndex = timeIndex;

    // Mostrar el compartimento actual
    showAdvanceMessage(dayIndex, timeIndex);

    //Obtener la posicion actual

    int advanceCount = 0;

    while (advanceCount < 21) {
        
        //Rutina antirebote para ambos botones
        int buttonSetState = digitalRead(buttonSetPin);
        int buttonOptionState = digitalRead(buttonOptionPin);

        // Debounce buttonSet
        if (buttonSetState != lastButtonSetState) {
            lastDebounceTimeSet = currentTime;
        }
        if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
            lastButtonSetState = buttonSetState;
        }

        // Debounce buttonOption
        if (buttonOptionState != lastButtonOptionState) {
            lastDebounceTimeOption = currentTime;
        }
        if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
            lastButtonOptionState = buttonOptionState;
        }
        //Fin de la rutina antirebote
        
        
        if (buttonSetState == LOW && lastButtonSetState == HIGH) {
            // Salir del proceso de avance
            lcd.clear();
            lcd.print("VOLVIENDO A ");
            lcd.setCursor(0, 1);
            lcd.print(daysOfWeek[initialDayIndex]);
            lcd.print(" ");
            lcd.print(timesOfDay[initialTimeIndex]);
            delay(2000);

            int steps = -advanceCount * stepsPerPosition;
            Serial.println("Llega hasta goback");
            
            goBack(steps);

            lcd.clear();
            updateDisplay();
            currentProgrammingMode = CONFIG_MODE;
            return;
        }

        if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
            // Avanzar al siguiente compartimento
            Serial.println("Llega hasta serveNext");
            serveNext();
            // Actualizar el estado del día y la hora
            timeIndex++;
            if (timeIndex > 2) {
                timeIndex = 0;
                dayIndex = (dayIndex + 1) % 7;
            }

            // Mostrar el compartimento actual
            showAdvanceMessage(dayIndex, timeIndex);

            // Incrementar el contador de avance
            advanceCount++;
            if (advanceCount >= 21) {
                // Salir del proceso de avance después de 21 veces
                lcd.clear();
                lcd.print("VOLVIENDO A ");
                lcd.print(daysOfWeek[initialDayIndex]);
                lcd.setCursor(0, 1);
                lcd.print(timesOfDay[initialTimeIndex]);
                delay(2000);

                int steps = -advanceCount * stepsPerPosition;
                goBack(steps);

                lcd.clear();
                updateDisplay();
                currentProgrammingMode = CONFIG_MODE;
                return;
            }
        }

        lastButtonOptionState = buttonOptionState;
        lastButtonSetState = buttonSetState;
        //delay(200); // Evitar rebotes
    }
}

//Retroceder
void retreat() {
    
    //Manejo antirebote de los botones
    unsigned long lastDebounceTimeSet = 0;
    unsigned long lastDebounceTimeOption = 0;
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;
    unsigned long currentTime = millis();
    
    lcd.clear();
    lcd.print("Retrocediendo...");
    delay(1500);  // Espera de 1.5 segundos

    // Obtener la hora y el día actuales
    timeNow = rtc.time();
    int currentHour = timeNow.hr;
    int currentMinute = timeNow.min;
    int currentDay = static_cast<int>(timeNow.day);
    
    // Determinar la ingesta anterior
    int dayIndex = currentDay-1 ;
    int timeIndex = 2; // 2: Noche, 1: Tarde, 0: Mañana

    if ((currentHour > eveningAlarm.hour) || (currentHour == eveningAlarm.hour && currentMinute > eveningAlarm.minute)) {
        timeIndex = 2; // Noche de hoy
    } else if ((currentHour > afternoonAlarm.hour) || (currentHour == afternoonAlarm.hour && currentMinute > afternoonAlarm.minute)) {
        timeIndex = 1; // Tarde de hoy
    } else if ((currentHour > morningAlarm.hour) || (currentHour == morningAlarm.hour && currentMinute > morningAlarm.minute)) {
        timeIndex = 0; // Mañana de hoy
    } else {
        timeIndex = 2; // Noche del día anterior
        dayIndex = (dayIndex - 1 + 7) % 7;
    }

    int initialDayIndex = dayIndex ;
    int initialTimeIndex = timeIndex;

    // Mostrar el primer compartimento
    showAdvanceMessage(dayIndex, timeIndex);

    int retreatCount = 0;

    while (retreatCount < 21) {
        
        //Rutina antirebote para ambos botones
        int buttonSetState = digitalRead(buttonSetPin);
        int buttonOptionState = digitalRead(buttonOptionPin);

        // Debounce buttonSet
        if (buttonSetState != lastButtonSetState) {
            lastDebounceTimeSet = currentTime;
        }
        if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
            lastButtonSetState = buttonSetState;
        }

        // Debounce buttonOption
        if (buttonOptionState != lastButtonOptionState) {
            lastDebounceTimeOption = currentTime;
        }
        if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
            lastButtonOptionState = buttonOptionState;
        }
        //Fin de la rutina antirebote

        if (buttonSetState == LOW && lastButtonSetState == HIGH) {
            // Salir del proceso de retroceso
            lcd.clear();
            lcd.print("VOLVIENDO A ");
            lcd.setCursor(0, 1);
            lcd.print(daysOfWeek[initialDayIndex]);
            lcd.print(" ");
            lcd.print(timesOfDay[initialTimeIndex]);
            delay(2000);

            int steps = retreatCount * stepsPerPosition;
            goBack(steps);

            lcd.clear();
            updateDisplay();
            currentProgrammingMode = CONFIG_MODE;
            return;
        }

        if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
            // Retroceder al compartimento anterior
            goBack(-stepsPerPosition);
            // Actualizar el estado del día y la hora
            timeIndex--;
            if (timeIndex < 0) {
                timeIndex = 2;
                dayIndex = (dayIndex - 1 + 7) % 7;
            }

            // Mostrar el siguiente compartimento
            showAdvanceMessage(dayIndex, timeIndex);

            // Incrementar el contador de retroceso
            retreatCount++;
            if (retreatCount >= 21) {
                // Salir del proceso de retroceso después de 21 veces
                lcd.clear();
                lcd.print("VOLVIENDO A ");
                lcd.print(daysOfWeek[initialDayIndex]);
                lcd.setCursor(0, 1);
                lcd.print(timesOfDay[initialTimeIndex]);
                delay(2000);

                int steps = retreatCount * stepsPerPosition;
                goBack(steps);

                lcd.clear();
                updateDisplay();
                currentProgrammingMode = CONFIG_MODE;
                return;
            }
        }

        lastButtonOptionState = buttonOptionState;
        lastButtonSetState = buttonSetState;
        //delay(200); // Evitar rebotes
    }
}

//Mover el mecanismo un numero n de pasos - Se utiliza en avance y retroceso
void goBack(int steps) {
     
   motor.move(steps);  // El motor rota el número de pasos calculados
   Serial.println("Motor inicia movimiento");
   Serial.println("steps");
   Serial.println(steps);
   Serial.println("motor.distanceToGo()");
   Serial.println(motor.distanceToGo());
   while (motor.distanceToGo() != 0) {
     motor.run();  // Ejecuta el movimiento del motor
     Serial.println("motor.run()");
     Serial.println("motor.distanceToGo()");
     Serial.println(motor.distanceToGo());
   } 
   Serial.println("Movimiento completado");
   unpowerMotor();
 }

//*********************************************************************************
// METODOS DEL SENSOR DE ULTRASONIDOS HR-SC04

bool checkProximity() {
    // Obtiene la distancia en cm
    int distance = sensor.getCM();

    if (distance <= 20) {
      return true;
    } else {
      return false;
    }
}

void setSensor() {
    bool sensorState = proximitySensorEnabled;  // Current state of the sensor (ACT or DES)
    bool settingDone = false;                   // Flag to exit the setting loop
    unsigned long lastDebounceTimeSet = 0;      // Debounce timing for the set button
    unsigned long lastDebounceTimeOption = 0;   // Debounce timing for the option button
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;           // Debounce delay
    unsigned long currentTime;
    
    while (!settingDone) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("DETECTOR PROXIMIDAD");
        lcd.setCursor(0, 1);
        lcd.print("SENSOR    ");

        // Show ACT or DES based on the current state
        if (sensorState) {
            lcd.print("ACT");
        } else {
            lcd.print("DES");
        }

        // Blinking effect for ACT/DES
        delay(500);
        lcd.setCursor(10, 1);
        lcd.print("   ");  // Clear the "ACT" or "DES" for blinking effect
        delay(500);

        // Read the current state of the buttons
        int buttonSetState = digitalRead(buttonSetPin);
        int buttonOptionState = digitalRead(buttonOptionPin);
        currentTime = millis();

        // Handle the Option Button with debouncing
        if (buttonOptionState != lastButtonOptionState) {
            if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
                lastDebounceTimeOption = currentTime;
                if (buttonOptionState == LOW) {
                    sensorState = !sensorState;  // Toggle sensor state
                }
            }
        }
        lastButtonOptionState = buttonOptionState;

        // Handle the Set Button with debouncing
        if (buttonSetState != lastButtonSetState) {
            if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
                lastDebounceTimeSet = currentTime;
                if (buttonSetState == LOW) {
                    proximitySensorEnabled = sensorState;  // Set the selected state
                    settingDone = true;  // Exit the loop
                }
            }
        }
        lastButtonSetState = buttonSetState;
    }

    // Final confirmation display
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EL SENSOR QUEDA");
    lcd.setCursor(0, 1);
    if (proximitySensorEnabled) {
        lcd.print("ACTIVADO");
    } else {
        lcd.print("DESACTIVADO");
    }
    saveAlarmsToEEPROM();
    delay(2000);  // Show confirmation for 2 seconds
    lcd.clear();
    updateDisplay();  // Return to the main display
}




//*********************************************************************************************************
// EEPROM

//Guardar las alarmas en la EEPROM
void saveAlarmsToEEPROM() {
    EEPROM.write(EEPROM_MORNING_ALARM_HOUR, morningAlarm.hour);
    EEPROM.write(EEPROM_MORNING_ALARM_MINUTE, morningAlarm.minute);
    EEPROM.write(EEPROM_AFTERNOON_ALARM_HOUR, afternoonAlarm.hour);
    EEPROM.write(EEPROM_AFTERNOON_ALARM_MINUTE, afternoonAlarm.minute);
    EEPROM.write(EEPROM_EVENING_ALARM_HOUR, eveningAlarm.hour);
    EEPROM.write(EEPROM_EVENING_ALARM_MINUTE, eveningAlarm.minute);

    // Store the alarm enable states
    EEPROM.write(EEPROM_MORNING_ALARM_ENABLED, morningAlarmEnabled);
    EEPROM.write(EEPROM_AFTERNOON_ALARM_ENABLED, afternoonAlarmEnabled);
    EEPROM.write(EEPROM_EVENING_ALARM_ENABLED, eveningAlarmEnabled);

    // Store the proximitysensor state
    EEPROM.write(EEPROM_PROXIMITY_SENSOR_ENABLED, proximitySensorEnabled);

    // Store the web notifications state
    //EEPROM.write(EEPROM_TELEGRAM_MESSAGES_ENABLED, telegramMessagesEnabled);
    //EEPROM.write(EEPROM_EMAIL_MESSAGES_ENABLED, emailMessagesEnabled);

    // Guardar los cambios 
    EEPROM.commit();
}

//Leer las alarmas de la EEPROM
void readAlarmsFromEEPROM() {
    morningAlarm.hour = EEPROM.read(EEPROM_MORNING_ALARM_HOUR);
    morningAlarm.minute = EEPROM.read(EEPROM_MORNING_ALARM_MINUTE);
    afternoonAlarm.hour = EEPROM.read(EEPROM_AFTERNOON_ALARM_HOUR);
    afternoonAlarm.minute = EEPROM.read(EEPROM_AFTERNOON_ALARM_MINUTE);
    eveningAlarm.hour = EEPROM.read(EEPROM_EVENING_ALARM_HOUR);
    eveningAlarm.minute = EEPROM.read(EEPROM_EVENING_ALARM_MINUTE);

    // Retrieve the alarm enable states
    morningAlarmEnabled = EEPROM.read(EEPROM_MORNING_ALARM_ENABLED) == 1;
    afternoonAlarmEnabled = EEPROM.read(EEPROM_AFTERNOON_ALARM_ENABLED) == 1;
    eveningAlarmEnabled = EEPROM.read(EEPROM_EVENING_ALARM_ENABLED) == 1;

    // Retrieve the proximity sensor state
    proximitySensorEnabled = EEPROM.read(EEPROM_PROXIMITY_SENSOR_ENABLED) == 1;
    
    // Retrieve the web notifications states
    //telegramMessagesEnabled = EEPROM.read(EEPROM_TELEGRAM_MESSAGES_ENABLED) == 1;
    //emailMessagesEnabled = EEPROM.read(EEPROM_EMAIL_MESSAGES_ENABLED) == 1;

}

// Guardar el PIN en la EEPROM
void savePasswordToEEPROM(char* password) {
    for (int i = 0; i < 4; i++) {
        EEPROM.write(EEPROM_PASSWORD + i, password[i]);
    }
    EEPROM.write(EEPROM_PASSWORD + 4, '\0'); // Null terminator
}

// Leer el PIN de la EEPROM
void readPasswordFromEEPROM(char* password) {
    for (int i = 0; i < 5; i++) {
        password[i] = EEPROM.read(EEPROM_PASSWORD + i);
    }
}

// Función para guardar los datos de conectividad en la EEPROM
void saveConnectivityToEEPROM() {
    // Guardar WiFi SSID
    for (int i = 0; i < 32; i++) {
        EEPROM.write(EEPROM_WIFI_SSID + i, WIFI_SSID[i]);
        if (WIFI_SSID[i] == '\0') break; // Para de escribir si llegamos al final de la cadena
    }
    
    // Guardar WiFi Password
    for (int i = 0; i < 16; i++) {
        EEPROM.write(EEPROM_WIFI_PASSWORD + i, WIFI_PASSWORD[i]);
        if (WIFI_PASSWORD[i] == '\0') break;
    }
    
    // Guardar Telegram Bot Token
    for (int i = 0; i < 80; i++) {
        EEPROM.write(EEPROM_BOT_TOKEN + i, botToken[i]);
        if (botToken[i] == '\0') break;
    }
    
    // Guardar Telegram Chat ID
    for (int i = 0; i < 16; i++) {
        EEPROM.write(EEPROM_CHAT_ID + i, chatID[i]);
        if (chatID[i] == '\0') break;
    }

    // Guardar Author Email
    for (int i = 0; i < 50; i++) {
        EEPROM.write(EEPROM_AUTHOR_EMAIL + i, AUTHOR_EMAIL[i]);
        if (AUTHOR_EMAIL[i] == '\0') break;
    }

    // Guardar Author Password
    for (int i = 0; i < 50; i++) {
        EEPROM.write(EEPROM_AUTHOR_PASSWORD + i, AUTHOR_PASSWORD[i]);
        if (AUTHOR_PASSWORD[i] == '\0') break;
    }

    // Guardar Recipient Email
    for (int i = 0; i < 50; i++) {
        EEPROM.write(EEPROM_RECIPIENT_EMAIL + i, RECIPIENT_EMAIL[i]);
        if (RECIPIENT_EMAIL[i] == '\0') break;
    }

    EEPROM.commit(); // Solo es necesario en ESP8266/ESP32
}

// Función para leer los datos de conectividad de la EEPROM
void readConnectivityFromEEPROM() {
    // Leer WiFi SSID
    for (int i = 0; i < 32; i++) {
        WIFI_SSID[i] = EEPROM.read(EEPROM_WIFI_SSID + i);
        if (WIFI_SSID[i] == '\0') break;
    }
    
    // Leer WiFi Password
    for (int i = 0; i < 16; i++) {
        WIFI_PASSWORD[i] = EEPROM.read(EEPROM_WIFI_PASSWORD + i);
        if (WIFI_PASSWORD[i] == '\0') break;
    }

    // Leer Telegram Bot Token
    for (int i = 0; i < 80; i++) {
        botToken[i] = EEPROM.read(EEPROM_BOT_TOKEN + i);
        if (botToken[i] == '\0') break;
    }
    
    // Leer Telegram Chat ID
    for (int i = 0; i < 16; i++) {
        chatID[i] = EEPROM.read(EEPROM_CHAT_ID + i);
        if (chatID[i] == '\0') break;
    }

    // Leer Author Email
    for (int i = 0; i < 50; i++) {
        AUTHOR_EMAIL[i] = EEPROM.read(EEPROM_AUTHOR_EMAIL + i);
        if (AUTHOR_EMAIL[i] == '\0') break;
    }

    // Leer Author Password
    for (int i = 0; i < 50; i++) {
        AUTHOR_PASSWORD[i] = EEPROM.read(EEPROM_AUTHOR_PASSWORD + i);
        if (AUTHOR_PASSWORD[i] == '\0') break;
    }

    // Leer Recipient Email
    for (int i = 0; i < 50; i++) {
        RECIPIENT_EMAIL[i] = EEPROM.read(EEPROM_RECIPIENT_EMAIL + i);
        if (RECIPIENT_EMAIL[i] == '\0') break;
    }
}

//**********************************************************************************************
// MENSAJERIA WEB
void connectToWiFi() {
 // Conectando a la red WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
   
  // Espera a que se conecte
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  for (int i = 0; i < 3; i++) {
    digitalWrite(buzzerPin, HIGH);
    delay(500); // Buzzer encendido durante 500 ms
    digitalWrite(buzzerPin, LOW);
    delay(500); // Buzzer apagado durante 500 ms
  } 
}
void disconnectFromWiFi() {
  // Desconectar de la red WiFi
  WiFi.disconnect();

  // Espera un momento para asegurar la desconexión
  delay(1000);

  // Confirma la desconexión con el buzzer (3 pitidos cortos)
  for (int i = 0; i < 3; i++) {
    digitalWrite(buzzerPin, HIGH);
    delay(100); // Buzzer encendido durante 100 ms
    digitalWrite(buzzerPin, LOW);
    delay(100); // Buzzer apagado durante 100 ms
  } 
}


//Composición del mensaje web
void writeWebMessage() {
   // Obtener la hora y el día actuales
    timeNow = rtc.time();
    int currentHour = timeNow.hr;
    int currentMinute = timeNow.min;
    int currentDay = static_cast<int>(timeNow.day);
    
    // Determinar la ingesta anterior
    int dayIndex = currentDay-1 ;
    int timeIndex = 2; // 2: Noche, 1: Tarde, 0: Mañana

    if ((currentHour > eveningAlarm.hour) || (currentHour == eveningAlarm.hour && currentMinute > eveningAlarm.minute)) {
        timeIndex = 2; // Noche de hoy
    } else if ((currentHour > afternoonAlarm.hour) || (currentHour == afternoonAlarm.hour && currentMinute > afternoonAlarm.minute)) {
        timeIndex = 1; // Tarde de hoy
    } else if ((currentHour > morningAlarm.hour) || (currentHour == morningAlarm.hour && currentMinute > morningAlarm.minute)) {
        timeIndex = 0; // Mañana de hoy
    } else {
        timeIndex = 2; // Noche del día anterior
        dayIndex = (dayIndex - 1 + 7) % 7;
    }

    
    String message = "INGESTA " + String(daysOfWeek[dayIndex-1]) + " " + String(timesOfDay[timeIndex]) + " " + String(currentHour) + ":" + String(currentMinute);
    Serial.println(message);
}

void setMessages() { 
    // Activa o desactiva la emisión de mensajes web y correo electrónico

    bool telegramState = telegramMessagesEnabled;  // Estado actual de las notificaciones Telegram
    bool emailState = emailMessagesEnabled;        // Estado actual de las notificaciones Email
    bool settingDone = false;                      // Bandera para salir del bucle de configuración
    bool changed = false;                          // Bandera para verificar si algún valor ha cambiado
    unsigned long lastDebounceTimeSet = 0;         // Tiempo de debounce para el botón Set
    unsigned long lastDebounceTimeOption = 0;      // Tiempo de debounce para el botón Option
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;              // Retardo de debounce
    unsigned long currentTime;
    int fieldToSet = 1;                            // Controla si se está configurando Telegram o Email

    while (!settingDone) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("NOTIFICACIONES");

        // Mostrar la opción de Telegram o Email según el campo seleccionado
        lcd.setCursor(0, 1);
        lcd.print("TELEGRAM  ");
        lcd.setCursor(10, 1);
        lcd.print(telegramState ? "ACT" : "DES");

        lcd.setCursor(0, 2);
        lcd.print("EMAIL     ");
        lcd.setCursor(10, 2);
        lcd.print(emailState ? "ACT" : "DES");

        // Efecto de parpadeo para ACT/DES en el campo seleccionado
        lcd.setCursor(10, fieldToSet == 1 ? 1 : 2); // Mueve el cursor a la posición correcta
        delay(500);
        lcd.print("   ");  // Borra "ACT" o "DES" para el efecto de parpadeo
        delay(500);

        // Leer el estado actual de los botones
        int buttonSetState = digitalRead(buttonSetPin);
        int buttonOptionState = digitalRead(buttonOptionPin);
        currentTime = millis();

        // Manejar el botón Option con debounce
        if (buttonOptionState != lastButtonOptionState) {
            if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
                lastDebounceTimeOption = currentTime;
                if (buttonOptionState == LOW) {
                    if (fieldToSet == 1) {
                        telegramState = !telegramState;  // Alternar estado de Telegram
                        changed = true;
                    } else if (fieldToSet == 2) {
                        emailState = !emailState;  // Alternar estado de Email
                        changed = true;
                    }
                }
            }
        }
        lastButtonOptionState = buttonOptionState;

        // Manejar el botón Set con debounce
        if (buttonSetState != lastButtonSetState) {
            if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
                lastDebounceTimeSet = currentTime;
                if (buttonSetState == LOW) {
                    if (fieldToSet == 1) {
                        fieldToSet = 2;  // Pasar a configurar el Email
                    } else if (fieldToSet == 2) {
                        telegramMessagesEnabled = telegramState;  // Guardar el estado de Telegram
                        emailMessagesEnabled = emailState;  // Guardar el estado de Email
                        settingDone = true;  // Salir del bucle
                    }
                }
            }
        }
        lastButtonSetState = buttonSetState;
    }

    // Mostrar mensaje de confirmación solo si se realizó un cambio
    if (changed) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CAMBIO REALIZADO");
        lcd.setCursor(0, 1);
        if (telegramState != telegramMessagesEnabled) {
            lcd.print("TELEGRAM ");
            lcd.print(telegramMessagesEnabled ? "ACTIVADO" : "DESACTIVADO");
        } else if (emailState != emailMessagesEnabled) {
            lcd.print("EMAIL ");
            lcd.print(emailMessagesEnabled ? "ACTIVADO" : "DESACTIVADO");
        }
        saveAlarmsToEEPROM();
        delay(2000);  // Mostrar confirmación por 2 segundos
    } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CAMBIO NO REALIZADO");
        delay(2000);  // Mostrar mensaje de que no se realizó ningún cambio
    }

    lcd.clear();
    updateDisplay();  // Volver a la pantalla principal
}



void sendTelegramMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {  // Comprueba si está conectado a WiFi
    HTTPClient http;
    
    // Crea el payload en formato JSON
    String payload = "{\"chat_id\":\"" + chatID + "\",\"text\":\"" + message + "\"}";

    // Configura la URL de la petición POST
    http.begin(telegramURL);
    http.addHeader("Content-Type", "application/json");

    // Envía la petición POST y captura la respuesta
    int httpResponseCode = http.POST(payload);

    // Verifica el código de respuesta del servidor
    if (httpResponseCode > 0) {
      String response = http.getString();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MENSAJE ENVIADO");
      delay(3000);
      lcd.clear();
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ERROR ENVIANDO");
      lcd.setCursor(0, 1);
      lcd.print("MENSAJE");
      delay(3000);
      lcd.clear();
      //Serial.println(httpResponseCode);
    }

    // Finaliza la conexión HTTP
    http.end();
  } else {
    lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ERROR");
      lcd.setCursor(0, 1);
      lcd.print("NO CONECTADO");
      delay(3000);
  }
}

void sendEmailMessage(String message) {
  // Configuración de la sesión de correo
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = F("127.0.0.1");

  config.time.ntp_server = F("pool.ntp.org");
  config.time.gmt_offset = 0;  // Ajusta según tu zona horaria
  config.time.day_light_offset = 0;

  smtp.debug(1);
  smtp.callback(smtpCallback);

  if (!smtp.connect(&config)) {
    char lcdBuffer[100];  // Crea un buffer para almacenar el mensaje formateado
    sprintf(lcdBuffer, "Razón: %d, %d, %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Error de conexión: ");
    lcd.setCursor(0,1);
    lcd.print("Código de estado: / Código de error: ");
    lcd.setCursor(0,2);
    lcd.print(lcdBuffer);
    delay(3000);
    lcd.clear();
    return;
  }
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Conectado al servidor");
  lcd.setCursor(0,1);
  lcd.print("de correo");
  delay(3000);
  lcd.clear();

  // Configuración del mensaje
  SMTP_Message emailMessage;
  emailMessage.sender.name = F("PASTILLERO");
  emailMessage.sender.email = AUTHOR_EMAIL;
  emailMessage.subject = message;  // Usamos el mensaje como asunto
  emailMessage.addRecipient(F("ADMINISTRADOR DEL PASTILLERO"), RECIPIENT_EMAIL);
  emailMessage.text.content = message;  // Usamos el mismo mensaje en el cuerpo del correo
  emailMessage.text.charSet = F("utf-8");
  emailMessage.text.transfer_encoding = "base64";
  emailMessage.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;

  // Enviar el correo
  if (!MailClient.sendMail(&smtp, &emailMessage)) {
    char lcdBuffer[100];  // Crea un buffer para almacenar el mensaje formateado
    sprintf(lcdBuffer, "Razón: %d, %d, %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Error al enviar el correo");
    lcd.setCursor(0,1);
    lcd.print("Código de estado: / Código de error: ");
    lcd.setCursor(0,2);
    lcd.print(lcdBuffer);
    delay(3000);
    lcd.clear();
  
  } else {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Correo electrónico");
    lcd.setCursor(0,1);
    lcd.print("enviado exitosamente");
    delay(3000);
    lcd.clear();
  
  }
}

// Función de callback para obtener el estado del envío del correo
void smtpCallback(SMTP_Status status) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("status.info()");
  delay(3000);
  lcd.clear();
  if (status.success()) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Mensaje enviado");
    delay(3000);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Mensajes enviados: %d\n");
    lcd.setCursor(0,1);
    lcd.print(status.completedCount());
    lcd.setCursor(0,2);
    lcd.print("Mensajes fallidos: %d\n");
    lcd.setCursor(0,3);
    lcd.print(status.failedCount());
    delay(3000);
    lcd.clear();
    for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
      SMTP_Result result = smtp.sendingResult.getItem(i);
      char lcdBuffer1[100];  // Crea un buffer para almacenar el mensaje formateado
      sprintf(lcdBuffer1, "Mensaje No: %d\n", i + 1);
      char lcdBuffer2[100];  // Crea un buffer para almacenar el mensaje formateado
      sprintf(lcdBuffer2, "Estado: %s\n", result.completed ? "éxito" : "fallido");
      char lcdBuffer3[100];  // Crea un buffer para almacenar el mensaje formateado
      sprintf(lcdBuffer3, "Fecha/Hora: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
    
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(lcdBuffer1);
      lcd.setCursor(0,1);
      lcd.print(lcdBuffer2);
      lcd.setCursor(0,2);
      lcd.print(lcdBuffer3);
      delay(3000);
    }
    smtp.sendingResult.clear();
  }
}

void configWeb() {
    bool settingDone = false;                      // Bandera para salir del bucle de configuración
    unsigned long lastDebounceTimeSet = 0;         // Tiempo de debounce para el botón Set
    unsigned long lastDebounceTimeOption = 0;      // Tiempo de debounce para el botón Option
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;              // Retardo de debounce
    unsigned long currentTime;
    int fieldToSet = 1;                            // Controla la línea seleccionada (2: WiFi, 3: Telegram, 4: Email)

    while (!settingDone) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CONFIGURACION WEB");

        // Mostrar las opciones de configuración
        lcd.setCursor(0, 1);
        lcd.print("WIFI");

        lcd.setCursor(0, 2);
        lcd.print("TELEGRAM");

        lcd.setCursor(0, 3);
        lcd.print("EMAIL");

        // Efecto de parpadeo para la opción seleccionada
        lcd.setCursor(0, fieldToSet);  // Mueve el cursor a la posición correcta
        delay(500);
        lcd.print("     ");  // Borra el texto para el efecto de parpadeo
        delay(500);

        // Leer el estado actual de los botones
        int buttonSetState = digitalRead(buttonSetPin);
        int buttonOptionState = digitalRead(buttonOptionPin);
        currentTime = millis();

        // Manejar el botón Option con debounce
        if (buttonOptionState != lastButtonOptionState) {
            if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
                lastDebounceTimeOption = currentTime;
                if (buttonOptionState == LOW) {
                    fieldToSet++;
                    if (fieldToSet > 4) {
                        settingDone = true;  // Salir del método cuando se pasa de la última opción
                    }
                }
            }
        }
        lastButtonOptionState = buttonOptionState;

        // Manejar el botón Set con debounce
        if (buttonSetState != lastButtonSetState) {
            if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
                lastDebounceTimeSet = currentTime;
                if (buttonSetState == LOW) {
                    switch (fieldToSet) {
                        case 2:
                            configWiFi();
                            break;
                        case 3:
                            configTelegram();
                            break;
                        case 4:
                            configEmail();
                            break;
                    }
                }
            }
        }
        lastButtonSetState = buttonSetState;
    }

    lcd.clear();
    updateDisplay();  // Volver a la pantalla principal
}

void configWiFi() {
    bool settingDone = false;                      // Bandera para salir del bucle de configuración
    unsigned long lastDebounceTimeSet = 0;         // Tiempo de debounce para el botón Set
    unsigned long lastDebounceTimeOption = 0;      // Tiempo de debounce para el botón Option
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;              // Retardo de debounce
    unsigned long currentTime;
    int fieldToSet = 1;                            // Controla la línea seleccionada (2: NOMBRE DE RED, 3: CLAVE)

    while (!settingDone) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CONFIGURACION WiFi");

        // Mostrar las opciones de configuración
        lcd.setCursor(0, 1);
        lcd.print("NOMBRE DE RED");

        lcd.setCursor(0, 2);
        lcd.print("CLAVE");

        // Efecto de parpadeo para la opción seleccionada
        lcd.setCursor(0, fieldToSet);  // Mueve el cursor a la posición correcta
        delay(500);
        lcd.print("                ");  // Borra el texto para el efecto de parpadeo
        delay(500);

        // Leer el estado actual de los botones
        int buttonSetState = digitalRead(buttonSetPin);
        int buttonOptionState = digitalRead(buttonOptionPin);
        currentTime = millis();

        // Manejar el botón Option con debounce
        if (buttonOptionState != lastButtonOptionState) {
            if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
                lastDebounceTimeOption = currentTime;
                if (buttonOptionState == LOW) {
                    fieldToSet++;
                    if (fieldToSet > 3) {
                        settingDone = true;  // Salir del método cuando se pasa de la última opción
                    }
                }
            }
        }
        lastButtonOptionState = buttonOptionState;

        // Manejar el botón Set con debounce
        if (buttonSetState != lastButtonSetState) {
            if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
                lastDebounceTimeSet = currentTime;
                if (buttonSetState == LOW) {
                    switch (fieldToSet) {
                        case 1:
                            configNetNameKey();
                            break;
                        case 2:
                            configNetPasswordKey();
                            break;
                    }
                }
            }
        }
        lastButtonSetState = buttonSetState;
    }

    lcd.clear();
    updateDisplay();  // Volver a la pantalla principal
}

void configTelegram() {
    bool settingDone = false;                      // Bandera para salir del bucle de configuración
    unsigned long lastDebounceTimeSet = 0;         // Tiempo de debounce para el botón Set
    unsigned long lastDebounceTimeOption = 0;      // Tiempo de debounce para el botón Option
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;              // Retardo de debounce
    unsigned long currentTime;
    int fieldToSet = 1;                            // Controla la línea seleccionada (2: TOKEN API, 3: CHAT ID)

    while (!settingDone) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CONFIGURAR TELEGRAM");

        // Mostrar las opciones de configuración
        lcd.setCursor(0, 1);
        lcd.print("TOKEN API");

        lcd.setCursor(0, 2);
        lcd.print("CHAT ID");

        // Efecto de parpadeo para la opción seleccionada
        lcd.setCursor(0, fieldToSet);  // Mueve el cursor a la posición correcta
        delay(500);
        lcd.print("          ");  // Borra el texto para el efecto de parpadeo
        delay(500);

        // Leer el estado actual de los botones
        int buttonSetState = digitalRead(buttonSetPin);
        int buttonOptionState = digitalRead(buttonOptionPin);
        currentTime = millis();

        // Manejar el botón Option con debounce
        if (buttonOptionState != lastButtonOptionState) {
            if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
                lastDebounceTimeOption = currentTime;
                if (buttonOptionState == LOW) {
                    fieldToSet++;
                    if (fieldToSet > 3) {
                        settingDone = true;  // Salir del método cuando se pasa de la última opción
                    }
                }
            }
        }
        lastButtonOptionState = buttonOptionState;

        // Manejar el botón Set con debounce
        if (buttonSetState != lastButtonSetState) {
            if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
                lastDebounceTimeSet = currentTime;
                if (buttonSetState == LOW) {
                    switch (fieldToSet) {
                        case 1:
                            configTelegramTokenKey();
                            break;
                        case 2:
                            configTelegramChatKey();
                            break;
                    }
                }
            }
        }
        lastButtonSetState = buttonSetState;
    }

    lcd.clear();
    updateDisplay();  // Volver a la pantalla principal
}

void configEmail() {
    bool settingDone = false;                      // Bandera para salir del bucle de configuración
    unsigned long lastDebounceTimeSet = 0;         // Tiempo de debounce para el botón Set
    unsigned long lastDebounceTimeOption = 0;      // Tiempo de debounce para el botón Option
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    unsigned long debounceDelay = 50;              // Retardo de debounce
    unsigned long currentTime;
    int fieldToSet = 1;                            // Controla la línea seleccionada (2: CORREO ENVIANTE, 3: CONTRASEÑA, 4: CORREO RECIPIENTE)

    while (!settingDone) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CONFIGURAR EMAIL");

        // Mostrar las opciones de configuración
        lcd.setCursor(0, 1);
        lcd.print("CORREO ENVIANTE");

        lcd.setCursor(0, 2);
        lcd.print("CONTRASEÑA");

        lcd.setCursor(0, 3);
        lcd.print("CORREO RECIPIENTE");

        // Efecto de parpadeo para la opción seleccionada
        lcd.setCursor(0, fieldToSet);  // Mueve el cursor a la posición correcta
        delay(500);
        lcd.print("                 ");  // Borra el texto para el efecto de parpadeo
        delay(500);

        // Leer el estado actual de los botones
        int buttonSetState = digitalRead(buttonSetPin);
        int buttonOptionState = digitalRead(buttonOptionPin);
        currentTime = millis();

        // Manejar el botón Option con debounce
        if (buttonOptionState != lastButtonOptionState) {
            if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
                lastDebounceTimeOption = currentTime;
                if (buttonOptionState == LOW) {
                    fieldToSet++;
                    if (fieldToSet > 4) {
                        settingDone = true;  // Salir del método cuando se pasa de la última opción
                    }
                }
            }
        }
        lastButtonOptionState = buttonOptionState;

        // Manejar el botón Set con debounce
        if (buttonSetState != lastButtonSetState) {
            if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
                lastDebounceTimeSet = currentTime;
                if (buttonSetState == LOW) {
                    switch (fieldToSet) {
                        case 1:
                            configEmailAuthorKey();
                            break;
                        case 2:
                            configEmailPasswordKey();
                            break;
                        case 3:
                            configEmailRecipientKey();
                            break;
                    }
                }
            }
        }
        lastButtonSetState = buttonSetState;
    }

    lcd.clear();
    updateDisplay();  // Volver a la pantalla principal
}

void configNetNameKey() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SSID: ");  // Mostrar el mensaje inicial para capturar el SSID
    lcd.setCursor(0, 1);
    lcd.print(WIFI_SSID);  // Mostrar el SSID actual

    String newSSID = stringCapture();  // Capturar la nueva cadena para el SSID

    if (newSSID.length() > 0) {
        strcpy(WIFI_SSID, newSSID.c_str());  // Asignar el nuevo SSID al valor global
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Nuevo SSID: ");
        lcd.setCursor(0, 1);
        lcd.print(WIFI_SSID);  // Mostrar el nuevo SSID capturado
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SSID no cambiado");
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    }

    lcd.clear();
    saveConnectivityToEEPROM(); //Guardar en la EEPROM el valor introducido
    updateDisplay();  // Volver a la pantalla principal
}

void configNetPasswordKey() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WIFI PASSWORD: ");  // Mostrar el mensaje inicial para capturar la contraseña wifi
    lcd.setCursor(0, 1);
    lcd.print(WIFI_PASSWORD);  // Mostrar la contraseña wifi actual

    String newNetPassword = stringCapture();  // Capturar la nueva contraseña wifi

    if (newNetPassword.length() > 0) {
        strcpy(WIFI_PASSWORD, newNetPassword.c_str());  // Asignar la nueva contraseña wifi al valor global
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Nuevo Password: ");
        lcd.setCursor(0, 1);
        lcd.print(WIFI_PASSWORD);  // Mostrar la nueva contraseña wifi capturada
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Password no cambiado");
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    }

    lcd.clear();
    saveConnectivityToEEPROM(); //Guardar en la EEPROM el valor introducido
    updateDisplay();  // Volver a la pantalla principal
}

void configTelegramTokenKey() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TELEGRAM API TOKEN: ");  // Mostrar el mensaje inicial para capturar el token API de Telegram
    lcd.setCursor(0, 1);
    lcd.print(botToken);  // Mostrar el token API de Telegram

    String NewBotToken = stringCapture();  // Capturar la nueva cadena para el token API de Telegram

    if (NewBotToken.length() > 0) {
        botToken = NewBotToken;  // Asignar el nuevo token API de Telegram al valor global
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Nuevo token API: ");
        lcd.setCursor(0, 1);
        lcd.print(botToken);  // Mostrar el nuevo token API de Telegram capturado
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("token API no cambiado");
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    }

    lcd.clear();
    saveConnectivityToEEPROM(); // Guardar en la EEPROM el valor introducido
    updateDisplay();  // Volver a la pantalla principal
}



void configTelegramChatKey() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TELEGRAM CHAT ID: ");  // Mostrar el mensaje inicial para capturar el chatID de Telegram
    lcd.setCursor(0, 1);
    lcd.print(chatID);  // Mostrar el chatID de Telegram

    String NewchatID = stringCapture();  // Capturar la nueva cadena para el chatID de Telegram

    if (NewchatID.length() > 0) {
        chatID = NewchatID;  // Asignar el nuevo chatID de Telegram al valor global
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Nuevo chatID: ");
        lcd.setCursor(0, 1);
        lcd.print(chatID);  // Mostrar el nuevo chatID de Telegram capturado
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("chatID no cambiado");
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    }

    lcd.clear();
    saveConnectivityToEEPROM(); //Guardar en la EEPROM el valor introducido
    updateDisplay();  // Volver a la pantalla principal
}


void configEmailAuthorKey() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EMAIL EMISOR: ");  // Mostrar el mensaje inicial para capturar el Email emisor
    lcd.setCursor(0, 1);
    lcd.print(AUTHOR_EMAIL);  // Mostrar el Email emisor

    String NewAuthorEmail = stringCapture();  // Capturar la nueva cadena para el Email emisor

    if (NewAuthorEmail.length() > 0) {
        strcpy(AUTHOR_EMAIL, NewAuthorEmail.c_str());  // Asignar el nuevo Email emisor al valor global
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Nuevo EMAIL emisor: ");
        lcd.setCursor(0, 1);
        lcd.print(AUTHOR_EMAIL);  // Mostrar el nuevo Email emisor capturado
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Email emisor no cambiado");
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    }

    lcd.clear();
    saveConnectivityToEEPROM(); //Guardar en la EEPROM el valor introducido
    updateDisplay();  // Volver a la pantalla principal
}

void configEmailPasswordKey() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CONTRASEÑA DE APLICACION: ");  // Mostrar el mensaje inicial para capturar la contraseña de aplicación
    lcd.setCursor(0, 1);
    lcd.print(AUTHOR_PASSWORD);  // Mostrar la contraseña de aplicación

    String NewEmailPassword = stringCapture();  // Capturar la nueva cadena para la contraseña de aplicación

    if (NewEmailPassword.length() > 0) {
        strcpy(AUTHOR_PASSWORD, NewEmailPassword.c_str());  // Asignar el nueva contraseña de aplicacion al valor global
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Nueva contraseña de aplicacion");
        lcd.setCursor(0, 1);
        lcd.print(AUTHOR_PASSWORD);  // Mostrar la nueva contraseña de aplicacion capturado
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Contraseña no cambiada");
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    }

    lcd.clear();
    saveConnectivityToEEPROM(); //Guardar en la EEPROM el valor introducido
    updateDisplay();  // Volver a la pantalla principal
}
void configEmailRecipientKey() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EMAIL RECEPTOR: ");  // Mostrar el mensaje inicial para capturar el email receptor
    lcd.setCursor(0, 1);
    lcd.print(RECIPIENT_EMAIL);  // Mostrar el email receptor

    String NewRecipientMail = stringCapture();  // Capturar la nueva cadena para el email receptor

    if (NewRecipientMail.length() > 0) {
        strcpy(RECIPIENT_EMAIL, NewRecipientMail.c_str());  // Asignar el nuevo email receptor al valor global
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Nueva email receptor");
        lcd.setCursor(0, 1);
        lcd.print(RECIPIENT_EMAIL);  // Mostrar el nuevo email receptor capturado
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("email receptor no cambiada");
        delay(2000);  // Esperar 2 segundos antes de volver a la pantalla principal
    }

    lcd.clear();
    saveConnectivityToEEPROM(); //Guardar en la EEPROM el valor introducido
    updateDisplay();  // Volver a la pantalla principal    
}

//**********************************************************************************************
// METODOS PARA LA CREACION DE UNA CADENA DE CARACTERES PARA INTRODUCIR DATOS 

void cleanChain() { //Método para eliminar los espacios al final de la cadena
    int i = cadena.length() - 1;

    // Recorrer la cadena desde el final y eliminar los espacios en blanco
    while (i >= 0 && cadena[i] == ' ') {
        i--;
    }

    // Recortar la cadena hasta el último carácter no blanco
    cadena = cadena.substring(0, i + 1);
}


String stringCapture() {
  lcd.clear();
  updateKeyboardDisplay();

  while (true) {
    keyboardButtonsControl();
    
    // Salida y guardado de la cadena si ambos botones están presionados
    if (simultaneousPressDetected) {
      lcd.noBlink();  // Desactivar parpadeo del cursor
      cleanChain();  // Limpia la cadena antes de guardarla
      return cadena;  // Terminar la captura de la cadena
    }
  }
}

void updateKeyboardDisplay() {
  // Despliega la cadena en la primera línea con scroll si es necesario
  lcd.setCursor(0, 0);
  if (cadena.length() <= 20) {
    lcd.print(cadena);
    for (int i = cadena.length(); i < 20; i++) {
      lcd.print(" ");  // Rellena el resto con espacios
    }
  } else {
    String visibleString = cadena.substring(cadena.length() - 20);
    lcd.print(visibleString);
  }

  // Seleccionar el alfabeto según el modo (mayúsculas o minúsculas)
  const char* currentAlphabet = isItLowerCase ? alfabetLower : alfabetUpper;
  int alphabetLength = strlen(currentAlphabet);

  // Despliega el teclado en las siguientes tres líneas sin scroll
  lcd.setCursor(0, 1);
  for (int i = 0; i < 20; i++) {
    lcd.print(currentAlphabet[i]);
  }

  lcd.setCursor(0, 2);
  for (int i = 20; i < 40; i++) {
    lcd.print(currentAlphabet[i]);
  }

  lcd.setCursor(0, 3);
  for (int i = 40; i < 60; i++) {
    lcd.print(currentAlphabet[i]);
  }

  // Mueve el cursor a la posición correcta en el teclado (línea 1, 2 o 3)
  int row = (charIndex / 20) + 1;
  int col = charIndex % 20;
  lcd.setCursor(col, row);
}

char obtainActualCharacter() {
  const char* currentAlphabet = isItLowerCase ? alfabetLower : alfabetUpper;
  return currentAlphabet[charIndex % strlen(currentAlphabet)];
}
void keyboardButtonsControl() {
  unsigned long currentTime = millis();

  int currentButtonOptionState = digitalRead(buttonOptionPin);
  int currentButtonSetState = digitalRead(buttonSetPin);

  // Manejo de pulsaciones simultáneas
  if (currentButtonOptionState == LOW && currentButtonSetState == LOW) {
    if (!simultaneousPressDetected) {
      if (currentTime - lastButtonOptionPress <= simultaneousPressTime && 
          currentTime - lastButtonSetPress <= simultaneousPressTime) {
        lastSimultaneousPress = currentTime;
        simultaneousPressDetected = true;
        delay(ignoreDelay);
        return; // Terminar la captura de la cadena
      }
    }
  } else {
    simultaneousPressDetected = false;
  }

  if (currentTime - lastSimultaneousPress < ignoreDelay) {
    return;
  }

  // Manejo del botón Option
  if (currentButtonOptionState == LOW && !buttonOptionHeld) {
    lastButtonOptionPress = currentTime;
    buttonOptionHeld = true;
  } else if (currentButtonOptionState == HIGH && buttonOptionHeld) {
    buttonOptionHeld = false;
    if (currentTime - lastButtonOptionPress >= longPressTime) {
      isItLowerCase = !isItLowerCase;
      updateKeyboardDisplay();
    } else if (currentTime - lastButtonOptionPress >= debounceDelay) {
      // Mueve el índice del carácter en el alfabeto
      charIndex = reverseScroll ? (charIndex - 1 + strlen(alfabetUpper)) % strlen(alfabetUpper) : (charIndex + 1) % strlen(alfabetUpper);
      updateKeyboardDisplay();
    }
  }

  // Manejo del botón Set
  if (currentButtonSetState == LOW && !buttonSetHeld) {
    lastButtonSetPress = currentTime;
    buttonSetHeld = true;
  } else if (currentButtonSetState == HIGH && buttonSetHeld) {
    buttonSetHeld = false;
    if (currentTime - lastButtonSetPress >= longPressTime) {
      reverseScroll = !reverseScroll;
    } else if (currentTime - lastButtonSetPress >= debounceDelay) {
      cadena += obtainActualCharacter();
      charIndex = (charIndex + 1) % strlen(alfabetUpper);  // Mueve el índice al siguiente carácter
      updateKeyboardDisplay();  // Actualiza el display después de añadir el carácter
    }
  }
}


//**********************************************************************************************
// SETUP

void setup() {
  Serial.begin(9600);

  // Inicializar la pantalla LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();

  // Mostrar la version de firmware
  lcd.setCursor(0,0);
  lcd.print("PASTILLERO");
  lcd.setCursor(0,1);
  lcd.print("FW: ");
  lcd.print(firmwareVersion);
  delay(1500);
  lcd.clear();

  
  // Inicializar los pines de los botones
  pinMode(buttonSetPin, INPUT_PULLUP);
  pinMode(buttonOptionPin, INPUT_PULLUP);

  // Inicializar el DS1302
  rtc.writeProtect(false);
  rtc.halt(false);

  // Leer la hora actual del DS1302
  timeNow = rtc.time();

  // Inicializar el motor
  motor.setMaxSpeed(80);
  motor.setSpeed(80);
  motor.setAcceleration(80);


  // Inicializar la EEPROM
  EEPROM.begin(EEPROM_SIZE);  
  
  // Leer las alarmas de la EEPROM
  //readAlarmsFromEEPROM();

  // Leer los datos de conectividad de la EEPROM
  //readConnectivityFromEEPROM(); 
  
  //CONTRASEÑA DESCONOCIDA
  //COMENTAR ESTA LINEA PARA REPROGRAMAR EL PIN A 1234 EN CASO DE QUE SE HAYA OLVIDADO EL PIN
  // UNA VEZ SUBIDO EL PROGRAMA; utilizar el menu de pantalla para cambiar el PIN
  // de este modo se reprograma la EEPROM
  //DESPUES DESCOMENTAR LA LINEA Y SUBIR EL PROGRAMA DE NUEVO
  // Leer el PIN de la EEPROM
  //readPasswordFromEEPROM(currentPassword);

  // Verificar si la hora del DS1302 es válida
  if (timeNow.yr == 2000) {
    // Establecer la hora inicial solo la primera vez si el año es 2000 (indica que no se ha configurado)
    timeNow = Time(2024, 1, 1, 0, 0, 0, Time::kMonday); // Configura tu hora inicial deseada
    rtc.time(timeNow);
  }


//DESCOMENTAR ESTAS LíNEAS SI ES NECESARIO RESETEAR EL RELOJ. - 
//Subir el programa,  volver a comentar estas líneas y volverlo a subir
//timeNow = Time(2024, 1, 1, 0, 0, 0, Time::kMonday); // Configura tu hora inicial deseada
//rtc.time(timeNow);


  // Mostrar la hora inicial
  updateDisplay();
}

void loop() {
  updateDisplay();
  handleButtons();
      // Verificar si es hora de erogar los medicamentos para la siguiente ingesta
  checkAlarms();
}
