#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DS1302.h>
#include <PaunaStepper.h>
#include <EEPROM.h>

// Definir los pines para los motores
const int motorPin1 = 8;
const int motorPin2 = 9;
const int motorPin3 = 10;
const int motorPin4 = 11;

// Declaración de motores y otros objetos
PaunaStepper motor;

// Definir los pasos por revolución para los motores
const int stepsPerPosition = 465; // Motor 28BYJ-48 - 2048 steps por vuelta - Engranajes dispensador 1/5 - 22 compartimentos por vuelta - 2048*5/22 

// Definir la dirección de la pantalla LCD I2C y el tamaño (20x4)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Definir los pines del DS1302
const int kCePin = 6;  // Chip Enable
const int kIoPin = 5;  // Input/Output
const int kSclkPin = 4;  // Serial Clock

// Crear un objeto DS1302
DS1302 rtc(kCePin, kIoPin, kSclkPin);

// Definir los pines de los botones
const int buttonSetPin = 2;
const int buttonOptionPin = 3;

//Variables para el control de la luz y el sleep-mode
unsigned long backlightSleepTime = 30000;
unsigned long backlightConnectedTime = 0;

// Variables para manejar el estado de los botones

bool buttonSetPressed = false; //*******
bool buttonOptionPressed = false;
int buttonOptionPressTime = 0;

unsigned long lastDebounceTime = 0;//*******
unsigned long debounceDelay = 50;
unsigned long longPressTime = 2000;//*******

unsigned long buttonPressStart = 0;//*******


bool inProgrammingMode = false;
int lastButtonSetState = HIGH;
int lastButtonOptionState = HIGH;

int fieldToSet = 0; // Campo que estamos configurando
//int programmingMode = 0; // 0: Set Time, 1: Set Alarms
int subfieldToSet = 0;

struct AlarmTime {
  int hour;
  int minute;
};

AlarmTime morningAlarm = {9, 0};
AlarmTime afternoonAlarm = {15, 0};
AlarmTime eveningAlarm = {23, 0};

// Variables para manejo de contraseña
char currentPassword[5] = "1234";
char enteredPassword[5];
int passwordIndex = 0;
bool passwordCorrect = false;
bool newPasswordSet = false;
char newPassword[5];
char confirmPassword[5];
int currentDigitIndex = 0;
char enteredDigits[5] = {'1', '2', '3', '4', '\0'};
bool digitSelected[4] = {false, false, false, false};
// Posiciones de los dígitos en el LCD
int digitPositions[4] = {1, 3, 5, 7};
int passwordStep = 0;

// Estados del sistema
enum SystemMode { NORMAL, PROGRAMMING };
SystemMode currentSystemMode = NORMAL;

// Modos de programación
enum ProgrammingMode { SET_TIME, REFILL, SET_ALARMS, CHANGE_PASSWORD, ENTER_PASSWORD, ADVANCE, RETREAT };
ProgrammingMode currentProgrammingMode = SET_TIME;

// Valores iniciales para el reloj
Time timeNow(2024, 11, 6, 23, 28, 0, Time::kTuesday);

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

// Direcciones EEPROM
const int EEPROM_CURRENT_HOUR = 0;
const int EEPROM_CURRENT_MINUTE = 1;
const int EEPROM_CURRENT_SECOND = 2;
const int EEPROM_CURRENT_DAY = 3;

const int EEPROM_MORNING_ALARM_HOUR = 10;
const int EEPROM_MORNING_ALARM_MINUTE = 11;
const int EEPROM_AFTERNOON_ALARM_HOUR = 12;
const int EEPROM_AFTERNOON_ALARM_MINUTE = 13;
const int EEPROM_EVENING_ALARM_HOUR = 14;
const int EEPROM_EVENING_ALARM_MINUTE = 15;
const int EEPROM_PASSWORD = 16;


// Manejo de scroll en display
int scrollPosition = 0;

//Variables para la determinación de la siguiente ingesta
int currentHour;
int currentMinute;
int currentDay;
int dayIndex;
int timeIndex;

//Variables para la recarga
int initialDayIndex;
int initialTimeIndex;

//Variables para avance y retroceso
int advanceCount;
int retreatCount;
 
// Declaraciones de funciones
void pauseForDebugging();
// DISPLAY
void printTime(const Time& t);
void blinkField(int field);
void blinkAlarmField(int field);
void updateAlarmDisplay();
void updateDisplay();
void updateProgrammingDisplay(int currentOption);
//void updatePasswordDisplay(char enteredDigits[], int currentDigitIndex, bool blink = false);
void updatePasswordDisplay(char* digits, int currentDigitIndex);
void showRefillMessage(int day, int timeOfDay);
void displayAlarmMessage(int dayIndex, const char* timeOfDay);
void showAdvanceMessage(int dayIndex, int timeIndex);
// RELOJ
void setTime();
void setAlarms();
void checkAlarms();
//INPUTS
void handleButtons();
void handleProgramming(int buttonSetState, int buttonOptionState);
// PASSWORD
bool inputPassword(char* password);
void requestPassword();
void changePassword();
// MECANISMOS
void serveNext();
void refillCompartments();
void advance();
void goBack(int steps);
void retreat();
// EEPROM
// void saveTimeToEEPROM(const Time& time);
// Time readTimeFromEEPROM();
void saveAlarmsToEEPROM();
void readAlarmsFromEEPROM();



void pauseForDebugging() {
    Serial.println("Press any key to continue...");
    while (Serial.available() == 0) {
        // Wait for user input
    }
    Serial.read(); // Clear the input buffer
    Serial.println("Continuing execution...");
}


//*************************************************************************
// METODOS DE DISPLAY

void printTime(const Time& t) {
  // Mostrar la fecha y hora en la pantalla LCD
  
  lcd.setCursor(0, 0);
  lcd.print(dayAsString(t.day));
  lcd.setCursor(10, 0);
  lcd.print(t.date < 10 ? "0" : ""); lcd.print(t.date);
  lcd.print('/');
  lcd.print(t.mon < 10 ? "0" : ""); lcd.print(t.mon);

  lcd.setCursor(4, 1);
  lcd.print(t.hr < 10 ? "0" : ""); lcd.print(t.hr);
  lcd.print(':');
  lcd.print(t.min < 10 ? "0" : ""); lcd.print(t.min);
  lcd.print(':');
  lcd.print(t.sec < 10 ? "0" : ""); lcd.print(t.sec);

  lcd.noCursor();
}
void updateTimeDisplay() {
    Time t = rtc.time(); // Obtener la hora actual del RTC
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Fecha: ");
    lcd.print(t.day);
    lcd.print("/");
    lcd.print(t.mon);
    lcd.print("/");
    lcd.print(t.yr);

    lcd.setCursor(0, 1);
    lcd.print("Hora: ");
    lcd.print(t.hr);
    lcd.print(":");
    lcd.print(t.min);

    blinkField(fieldToSet); // Hacer parpadear el campo que se está configurando
}

void blinkField(int field) {
    lcd.noBlink();
    switch (field) {
        case 0:
            lcd.setCursor(6, 0); // Parpadear día
            lcd.blink();
            break;
        case 1:
            lcd.setCursor(9, 0); // Parpadear mes
            lcd.blink();
            break;
        case 2:
            lcd.setCursor(12, 0); // Parpadear año
            lcd.blink();
            break;
        case 3:
            lcd.setCursor(6, 1); // Parpadear hora
            lcd.blink();
            break;
        case 4:
            lcd.setCursor(9, 1); // Parpadear minuto
            lcd.blink();
            break;
    }
}

void updateAlarmDisplay() {
    lcd.clear();
    if (fieldToSet == 0) {
        lcd.print("Mañana: ");
        lcd.setCursor(0, 1);
        if (subfieldToSet == 0) {
            lcd.print(morningAlarm.hour);
            lcd.print(":");
            if (morningAlarm.minute < 10) lcd.print("0");
            lcd.print(morningAlarm.minute);
        } else {
            lcd.print(morningAlarm.hour);
            lcd.print(":");
            if (morningAlarm.minute < 10) lcd.print("0");
            lcd.print(morningAlarm.minute);
        }
    } else if (fieldToSet == 1) {
        lcd.print("Tarde: ");
        lcd.setCursor(0, 1);
        if (subfieldToSet == 0) {
            lcd.print(afternoonAlarm.hour);
            lcd.print(":");
            if (afternoonAlarm.minute < 10) lcd.print("0");
            lcd.print(afternoonAlarm.minute);
        } else {
            lcd.print(afternoonAlarm.hour);
            lcd.print(":");
            if (afternoonAlarm.minute < 10) lcd.print("0");
            lcd.print(afternoonAlarm.minute);
        }
    } else if (fieldToSet == 2) {
        lcd.print("Noche: ");
        lcd.setCursor(0, 1);
        if (subfieldToSet == 0) {
            lcd.print(eveningAlarm.hour);
            lcd.print(":");
            if (eveningAlarm.minute < 10) lcd.print("0");
            lcd.print(eveningAlarm.minute);
        } else {
            lcd.print(eveningAlarm.hour);
            lcd.print(":");
            if (eveningAlarm.minute < 10) lcd.print("0");
            lcd.print(eveningAlarm.minute);
        }
    }

    // Ajustar la posición del cursor
    lcd.setCursor(subfieldToSet == 0 ? 7 : 10, 1);
    lcd.cursor();
}

void blinkAlarmField(int field, int subfield) {
  int displayRow = -1;
  if (field == scrollPosition) {
    displayRow = 0;
  } else if (field == scrollPosition + 1) {
    displayRow = 1;
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
    lcd.noCursor(); // Ensure the cursor is turned off after each blink
  }
}



void updateDisplay() {
  if (currentSystemMode == NORMAL) {
    timeNow = rtc.time();
    printTime(timeNow);
  }
}

void updateProgrammingDisplay(int currentOption) {
    lcd.clear();

    int startOption = currentOption > 2 ? currentOption - 1 : 1;

    // Mostrar las opciones según la opción actual
    for (int i = 0; i < 2; i++) {
        int option = startOption + i;
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
        }
    }

    // Mover el cursor a la opción actual y activar el parpadeo
    int cursorPosition = currentOption > 2 ? 1 : currentOption - 1;
    lcd.setCursor(0, cursorPosition);
    lcd.blink();
}




void showRefillMessage(int day, int timeOfDay) { // Función para mostrar el mensaje de recarga actual
  Serial.println("REFILL MESSAGE");
  lcd.clear();
  lcd.print(daysOfWeek[day]);
  lcd.print(" ");
  lcd.print(timesOfDay[timeOfDay]);
  delay (1000);
}

// Función para mostrar el mensaje de medicamentos erogados en la pantalla
void displayAlarmMessage(int dayIndex, const char* timeOfDay) {
    // Array con los nombres de los días de la semana
    const char* daysOfWeek[] = {"DOMINGO", "LUNES", "MARTES", "MIERCOLES", "JUEVES", "VIERNES", "SABADO"};

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("INGESTA ");
    lcd.print(daysOfWeek[dayIndex]);
    lcd.setCursor(0, 1);
    lcd.print(timeOfDay);
    // Mostrar el mensaje durante 900 segundos (15 minutos)
    unsigned long startTime = millis();
    while (millis() - startTime < 900000) {
        // Mantener el mensaje en la pantalla
    }

    // Volver a la pantalla normal
    lcd.clear();
    updateDisplay();
}

void showAdvanceMessage(int dayIndex, int timeIndex) {
    // Array con los nombres de los días de la semana
    const char* daysOfWeek[] = {"DOMINGO", "LUNES", "MARTES", "MIERCOLES", "JUEVES", "VIERNES", "SABADO"};
    const char* timesOfDay[] = {"MANANA", "TARDE", "NOCHE"};

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(daysOfWeek[dayIndex]);
    lcd.setCursor(0, 1);
    lcd.print(timesOfDay[timeIndex]);
}

//*************************************************************************
// METODOS DEL RELOJ

void setTime() {
    currentProgrammingMode = SET_TIME;
    currentSystemMode = PROGRAMMING;
    inProgrammingMode = true;
    fieldToSet = 0;

    lcd.clear();
    lcd.print("Configurar hora");

    while (inProgrammingMode) {
        updateTimeDisplay();

        // Espera a que se presione el botón para ajustar el tiempo
        // La lógica del manejo de botones en handleButtons()
    }

    currentSystemMode = NORMAL;
    lcd.clear();
    updateDisplay();
}

void setAlarms() {
    currentProgrammingMode = SET_ALARMS;
    currentSystemMode = PROGRAMMING;
    inProgrammingMode = true;
    fieldToSet = 0;
    subfieldToSet = 0;
    scrollPosition = 0;

    lcd.clear();
    lcd.print("Configurar alarmas");

    while (inProgrammingMode) {
        updateAlarmDisplay();

        // Espera a que se presione el botón para ajustar las alarmas
        // La lógica del manejo de botones se ha movido a handleButtons()
    }

    currentSystemMode = NORMAL;
    lcd.clear();
    updateDisplay();

    // Guardar las alarmas en la EEPROM después de configurarlas
    saveAlarmsToEEPROM();
}


void checkAlarms() {
  // Obtener la hora actual
  timeNow = rtc.time();
  int currentHour = timeNow.hr;
  int currentMinute = timeNow.min;
  int currentDay = static_cast<int>(timeNow.day);

  // Comparar la hora actual con las alarmas
  if (currentHour == morningAlarm.hour && currentMinute == morningAlarm.minute) {
    serveNext();
    delay (3000);
    Serial.println("displayAlarmMessage - checkAlarms");
    displayAlarmMessage(currentDay, "MANANA");
  } else if (currentHour == afternoonAlarm.hour && currentMinute == afternoonAlarm.minute) {
    serveNext();
    displayAlarmMessage(currentDay, "TARDE");
  } else if (currentHour == eveningAlarm.hour && currentMinute == eveningAlarm.minute) {
    serveNext();
    displayAlarmMessage(currentDay, "NOCHE");
  }
}

// ***************************************************************************************
// MANEJO DE LOS BOTONES
void handleButtons() {
  delay(3000);
  //Serial.println("handleButtons");
    static unsigned long lastDebounceTimeSet = 0;
    static unsigned long lastDebounceTimeOption = 0;
    static int lastButtonSetState = HIGH;
    static int lastButtonOptionState = HIGH;
    static bool buttonOptionPressed = false;
    static unsigned long buttonOptionPressTime = 0;
    unsigned long currentTime = millis();

    int buttonSetState = digitalRead(buttonSetPin);
    int buttonOptionState = digitalRead(buttonOptionPin);

    //int fieldToSet = 0;
    //int subfieldToSet = 0;
    
    // Enciende el backlight
    //Serial.println("backlight on");
    if (buttonSetState == LOW || buttonOptionState == LOW) {
        backlightConnectedTime = millis();
        lcd.backlight();  
    }
/*Serial.println("buttonSetState");
Serial.println(buttonSetState);
Serial.println("buttonOptionState");
Serial.println(buttonOptionState);
*/
    
    // Antirrebote para buttonSet
    if (buttonSetState != lastButtonSetState) {
        lastDebounceTimeSet = currentTime;
    }
    if ((currentTime - lastDebounceTimeSet) > debounceDelay) {
        if (buttonSetState == LOW && lastButtonSetState == HIGH) {
            // Lógica para diferentes modos de programación
            if (currentProgrammingMode == REFILL) {
                Serial.println("REFILL - SET");
                // Lógica de manejo de botones para REFILL
                //if (buttonSetState == LOW && lastButtonSetState == HIGH) {
                if (buttonSetState == LOW && lastButtonSetState == HIGH) {
                    // Salir del proceso de avance
                    lcd.clear();
                    lcd.print("VOLVIENDO A ");
                    lcd.setCursor(0, 1);
                    lcd.print(daysOfWeek[initialDayIndex]);
                    lcd.print(" ");
                    lcd.print(timesOfDay[initialTimeIndex]);
                    delay(2000);

                    int steps = -refillCount * stepsPerPosition;
                    goBack(steps);

                    currentSystemMode = NORMAL;
                    lcd.clear();
                    updateDisplay();
                    return;
                }
            } 
/*
            else if (currentProgrammingMode == SET_ALARMS) {
                // Cambiar entre campos y subcampos
                subfieldToSet = (subfieldToSet + 1) % 2;
                if (subfieldToSet == 0) {
                    fieldToSet = (fieldToSet + 1) % 3;
                }
                updateAlarmDisplay();
*/                
            else if (currentProgrammingMode == SET_ALARMS) {
                Serial.println("SET_ALARMS - SET");
                // Lógica de manejo de botones para SET_ALARMS
                if (buttonSetState == LOW && lastButtonSetState == HIGH) {
                    subfieldToSet++;
                    if (subfieldToSet > 1) {
                        subfieldToSet = 0;
                        fieldToSet++;
                        if (fieldToSet > 2) {
                            inProgrammingMode = false;
                            currentSystemMode = NORMAL;
                            currentProgrammingMode = SET_TIME;
                        }
                    }
                    // Ajustar la posición del scroll
                    if (fieldToSet > 1 && scrollPosition < 2) {
                        scrollPosition++;
                    } else if (fieldToSet < 2 && scrollPosition > 0) {
                        scrollPosition--;
                    }
                    updateAlarmDisplay();
                    lcd.noCursor(); // Ensure the cursor is turned off after updating display
                }
            } 
            
    
            
            else if (currentProgrammingMode == SET_TIME) {
                Serial.println("SET_TIME - SET");
                // Lógica de manejo de botones para SET_TIME
                if (buttonSetState == LOW && lastButtonSetState == HIGH) {
                    fieldToSet++;
                    if (fieldToSet > 5) {
                        inProgrammingMode = false;
                        currentSystemMode = NORMAL;
                    }
                }
            }
 /*             
             else if (currentProgrammingMode == SET_TIME) {
                // Cambiar entre campos de tiempo
                fieldToSet = (fieldToSet + 1) % 6;
            }
*/            
            else if (currentProgrammingMode == ADVANCE) {
              
                Serial.println("ADVANCE - SET");
                // Lógica de manejo de botones para ADVANCE
                lcd.clear();
                lcd.print("VOLVIENDO A ");
                lcd.setCursor(0, 1);
                lcd.print(daysOfWeek[initialDayIndex]);
                lcd.print(" ");
                lcd.print(timesOfDay[initialTimeIndex]);
                delay(2000);

                int steps = -advanceCount * stepsPerPosition;
                goBack(steps);

                currentSystemMode = NORMAL;
                lcd.clear();
                updateDisplay();
                return;
            } 
            
            else if (currentProgrammingMode == RETREAT) {
              
                Serial.println("RETREAT - SET");
                // Lógica de manejo de botones para RETREAT
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

                    currentSystemMode = NORMAL;
                    lcd.clear();
                    updateDisplay();
                    return;
                }
            }
           
            else if (currentProgrammingMode == CHANGE_PASSWORD) {
              
                Serial.println("CHANGE_PASSWORD - SET");
                passwordIndex++;
                if (passwordIndex > 3) {
                    inProgrammingMode = false;
                } else {
                    lcd.setCursor(passwordIndex * 2, 1); // Mover el cursor al siguiente dígito
                    lcd.blink();
                }
            } else if (currentProgrammingMode == ENTER_PASSWORD) {
              
                Serial.println("ENTER_PASSWORD - SET");
                
/*                digitSelected[currentDigitIndex] = true;
                currentDigitIndex++;
            
                if (currentDigitIndex < 4) {
                    lcd.setCursor(digitPositions[currentDigitIndex], 1);
                    lcd.blink();
                }
*/
                enteredDigits[currentDigitIndex]++;
                if (enteredDigits[currentDigitIndex] > '9') {
                    enteredDigits[currentDigitIndex] = '0';
                }
                lcd.setCursor(digitPositions[currentDigitIndex], 1);
                lcd.print(enteredDigits[currentDigitIndex]);
            }
                
                
                
                
                
                
                
                
                
                
                
                
                passwordIndex++;
                if (passwordIndex > 3) {
                    inProgrammingMode = false;
                } else {
                    lcd.setCursor(passwordIndex * 2, 1);
                    lcd.blink();
                }
            }
        }
    // Delay to avoid bouncing
    delay(50);
    lastButtonSetState = buttonSetState;
    
    // Antirrebote para buttonOption
    if (buttonOptionState != lastButtonOptionState) {
        lastDebounceTimeOption = currentTime;
    }
    if ((currentTime - lastDebounceTimeOption) > debounceDelay) {
        if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
            // Lógica para diferentes modos de programación
            if (currentProgrammingMode == REFILL) {
              
                Serial.println("REFILL - OPTION");
                // Lógica de manejo de botones para REFILL
                if (lastButtonOptionState == HIGH) {
                    refillCount = 0;
                    while (refillCount < 21) {
                        if (digitalRead(buttonOptionPin) == LOW) {
                            // Avanzar al siguiente compartimento
                            serveNext();
                            delay(1000);

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
                                lcd.clear();
                                lcd.print("VOLVIENDO A ");
                                lcd.print(daysOfWeek[initialDayIndex]);
                                lcd.setCursor(0, 1);
                                lcd.print(timesOfDay[initialTimeIndex]);
                                delay(2000);

                                int steps = -refillCount * stepsPerPosition;
                                goBack(steps);

                                currentSystemMode = NORMAL;
                                lcd.clear();
                                updateDisplay();
                                return;
                            }
                        }
                    }
                }
            } 
            
            else if (currentProgrammingMode == SET_ALARMS) {
              
                Serial.println("SET_ALARMS - OPTION");
                // Lógica de manejo de botones para SET_ALARMS
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
                lcd.noCursor(); // Ensure the cursor is turned off after updating display
            } 
            
            
            else if (currentProgrammingMode == SET_TIME) {
              
                Serial.println("SET_TIME - OPTION");
                // Lógica de manejo de botones para SET_TIME
                blinkField(fieldToSet);
                if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
                    if (fieldToSet == 0) {
                        timeNow.day = static_cast<Time::Day>((timeNow.day + 1) % 7);
                    } else if (fieldToSet == 1) {
                        timeNow.date = (timeNow.date % 31) + 1;
                    } else if (fieldToSet == 2) {
                        timeNow.mon = (timeNow.mon % 12) + 1;
                    } else if (fieldToSet == 3) {
                        timeNow.yr++;
                    } else if (fieldToSet == 4) {
                        timeNow.hr = (timeNow.hr + 1) % 24;
                    } else if (fieldToSet == 5) {
                        timeNow.min = (timeNow.min + 1) % 60;
                    }
                    rtc.time(timeNow);
                }
            }
            /* 
            else if (currentProgrammingMode == SET_TIME) {
                // Lógica de manejo de botones para SET_TIME
                if (fieldToSet == 0) {
                    timeNow.day = static_cast<Time::Day>((timeNow.day + 1) % 7);
                } else if (fieldToSet == 1) {
                    timeNow.date = (timeNow.date % 31) + 1;
                } else if (fieldToSet == 2) {
                    timeNow.mon = (timeNow.mon % 12) + 1;
                } else if (fieldToSet == 3) {
                    timeNow.yr++;
                } else if (fieldToSet == 4) {
                    timeNow.hr = (timeNow.hr + 1) % 24;
                } else if (fieldToSet == 5) {
                    timeNow.min = (timeNow.min + 1) % 60;
                }
                rtc.time(timeNow);
                blinkField(fieldToSet);
            }*/
            else if (currentProgrammingMode == ADVANCE) {
              
                Serial.println("ADVANCE - OPTION");
                // Lógica de manejo de botones para ADVANCE
                advanceCount = 0;
                while (advanceCount < 21) {
                    serveNext();
                    delay(1000);

                    // Actualizar el estado del día y la hora
                    timeIndex++;
                    if (timeIndex > 2) {
                        timeIndex = 0;
                        dayIndex = (dayIndex + 1) % 7;
                    }

                    // Mostrar el siguiente compartimento
                    showAdvanceMessage(dayIndex, timeIndex);

                    // Incrementar el contador de avance
                    advanceCount++;
                    if (advanceCount >= 21) {
                        lcd.clear();
                        lcd.print("VOLVIENDO A ");
                        lcd.print(daysOfWeek[initialDayIndex]);
                        lcd.setCursor(0, 1);
                        lcd.print(timesOfDay[initialTimeIndex]);
                        delay(2000);

                        int steps = -advanceCount * stepsPerPosition;
                        goBack(steps);

                        currentSystemMode = NORMAL;
                        lcd.clear();
                        updateDisplay();
                        return;
                    }
                }
            }  
            
            else if (currentProgrammingMode == RETREAT) {
              
                Serial.println("RETREAT - OPTION");
                // Lógica de manejo de botones para RETREAT
                if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
                    // Retroceder al compartimento anterior
                    goBack(-stepsPerPosition);
                    delay(1000);

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

                        currentSystemMode = NORMAL;
                        lcd.clear();
                        updateDisplay();
                        return;
                    }
                }
            }

            else if (currentProgrammingMode == CHANGE_PASSWORD) {
              
                Serial.println("CHANGE_PASSWORD - OPTION");
                
                
                
                
                
                if (passwordStep == 0) {
                    newPassword[passwordIndex] = '0' + ((newPassword[passwordIndex] - '0' + 1) % 10); // Incrementar dígito
                    //updatePasswordDisplay(newPassword, passwordIndex,false);
                    updatePasswordDisplay(newPassword, passwordIndex);
                } else if (passwordStep == 1) {
                    confirmPassword[passwordIndex] = '0' + ((confirmPassword[passwordIndex] - '0' + 1) % 10); // Incrementar dígito
                    //updatePasswordDisplay(confirmPassword, passwordIndex,false);
                    updatePasswordDisplay(confirmPassword, passwordIndex);
                }
            } else if (currentProgrammingMode == ENTER_PASSWORD) {
              
                Serial.println("ENTER_PASSWORD - OPTION");
                
                enteredDigits[currentDigitIndex]++;
                if (enteredDigits[currentDigitIndex] > '9') {
                    enteredDigits[currentDigitIndex] = '0';
                }
                lcd.setCursor(digitPositions[currentDigitIndex], 1);
                lcd.print(enteredDigits[currentDigitIndex]);               
                
                
                
                
/*                
                enteredDigits[currentDigitIndex] = (enteredDigits[currentDigitIndex] == '9') ? '0' : (enteredDigits[currentDigitIndex] + 1);
                updatePasswordDisplay(enteredDigits, currentDigitIndex);
                delay(300); // Debounce delay
                
              
                if (passwordIndex < 4) {
                    enteredDigits[passwordIndex] = '0' + ((enteredDigits[passwordIndex] - '0' + 1) % 10); // Incrementar dígito
                    //updatePasswordDisplay(enteredDigits, passwordIndex, false);
                    updatePasswordDisplay(enteredDigits, passwordIndex);
                }
            }
*/
            
            // Delay to avoid bouncing
            // delay(50);
        }
    }
    lastButtonOptionState = buttonOptionState;

    // Lógica para detección de pulsación larga en buttonOption
    if (buttonOptionState == LOW && !buttonOptionPressed) {
        buttonOptionPressed = true;
        buttonOptionPressTime = currentTime;
    } else if (buttonOptionState == HIGH && buttonOptionPressed) {
        buttonOptionPressed = false;
    }

    if (buttonOptionPressed && (currentTime - buttonOptionPressTime >= 3000)) {
        Serial.println("DETECTADA PULSACION LARGA");
        Serial.println("currentSystemMode");
        Serial.println(currentSystemMode);
        
        if (currentSystemMode == NORMAL) { // Solo cambiar a modo de programación si estamos en modo normal
            inProgrammingMode = true;
            Serial.println("PIDIENDO EL PSSWD");
            requestPassword();
            
        }
    }


    // Comprobación del tiempo para apagar el backlight
    if (millis() - backlightConnectedTime > backlightSleepTime) {
        lcd.noBacklight();
    }
}
}


void handleProgramming(int buttonSetState, int buttonOptionState) {
    
    static unsigned long lastButtonTime = 0;
    static int currentOption = 0; // Start with option 1

    if ((millis() - lastButtonTime) > debounceDelay) {
        if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
            // Cycle through options 1 to 6
            currentOption = (currentOption % 6) + 1;
            updateProgrammingDisplay(currentOption); // Solo actualizar la pantalla cuando se cambia la opción
            lastButtonTime = millis();
        }

        if (buttonSetState == LOW && lastButtonSetState == HIGH) {
            // Execute the selected option
            lcd.clear(); // Limpiar la pantalla antes de ejecutar cualquier opción
            switch (currentOption) {
                case 1: // RECARGAR
                    currentProgrammingMode = REFILL;
                    Serial.println("REFILL - handleProgramming");
                    refillCompartments();
                    break;
                case 2: // CAMBIAR INGESTAS
                    currentProgrammingMode = SET_ALARMS;
                    Serial.println("SET_ALARMS - handleProgramming");
                    setAlarms();
                    break;
                case 3: // AVANZAR
                    currentProgrammingMode = ADVANCE;
                    Serial.println("ADVANCE - handleProgramming");
                    advance();
                    break;
                case 4: // RETROCEDER
                    currentProgrammingMode = RETREAT;
                    Serial.println("RETREAT - handleProgramming");
                    retreat();
                    break;
                case 5: // CAMBIAR HORA
                    currentProgrammingMode = SET_TIME;
                    Serial.println("SET_TIME - handleProgramming");
                    setTime();
                    break;
                case 6: // CAMBIAR PASSWORD
                    currentProgrammingMode = CHANGE_PASSWORD;
                    Serial.println("CHANGE_PASSWORD - handleProgramming");
                    changePassword();
                    break;
            }
            currentSystemMode = NORMAL; // Exit programming mode
            currentProgrammingMode = SET_TIME; // Reset programming mode
            lastButtonTime = millis();
        }

        lastButtonOptionState = buttonOptionState;
        lastButtonSetState = buttonSetState;
    }
}



//*********************************************************************************
// METODOS DE LA CONTRASEÑA

bool inputPassword(char* password) {
    int digitPositions[4] = {1, 3, 5, 7};
    lcd.setCursor(0, 1);
    lcd.print(" 1 2 3 4");
    currentDigitIndex = 0; // Reiniciar el índice del dígito actual
    lcd.setCursor(digitPositions[currentDigitIndex], 1);
    lcd.blink();

    inProgrammingMode = true; // Asegurarse de que inProgrammingMode esté habilitado

    while (inProgrammingMode) {
        handleButtons();
        delay(200); // Evitar rebotes
    }

    strcpy(password, enteredDigits);
    lcd.noBlink();
    return true;
}

void requestPassword() {
    lcd.clear();
    lcd.print("Introducir PIN:");
    delay(1000);
    
    currentProgrammingMode = ENTER_PASSWORD;
    
    if (inputPassword(enteredPassword)) {
        if (strcmp(enteredPassword, currentPassword) == 0) {
            lcd.clear();
            lcd.print("PIN Correcto");
            delay(1000);
            currentSystemMode = PROGRAMMING;
            currentProgrammingMode = SET_TIME;
            handleProgramming(HIGH, HIGH);
        } else {
            lcd.clear();
            lcd.print("PIN Incorrecto");
            delay(2000); // Mostrar el mensaje durante 2 segundos
            currentSystemMode = NORMAL;
        }
    }
    lcd.noBlink();
}

void changePassword() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nuevo PIN:");
  lcd.setCursor(0, 1);
  delay(1000);
  
  passwordIndex = 0;
  inProgrammingMode = true;

  // Inicializar newPassword con "1234"
  strcpy(newPassword, "1234");

  // Mostrar la contraseña inicial en el display
  updatePasswordDisplay(newPassword, passwordIndex);

  while (inProgrammingMode) {
    int buttonOptionState = digitalRead(buttonOptionPin);
    int buttonSetState = digitalRead(buttonSetPin);

    if (buttonOptionState == LOW && lastButtonOptionState == HIGH) {
      if (passwordIndex < 4) {
        newPassword[passwordIndex] = '0' + ((newPassword[passwordIndex] - '0' + 1) % 10); // Incrementar dígito
        updatePasswordDisplay(newPassword, passwordIndex);
      }
    }

    if (buttonSetState == LOW && lastButtonSetState == HIGH) {
      passwordIndex++;
      if (passwordIndex > 3) {
        inProgrammingMode = false;
      } else {
        lcd.setCursor(passwordIndex * 2, 1); // Mover el cursor al siguiente dígito
        lcd.blink();
      }
    }

    lastButtonOptionState = buttonOptionState;
    lastButtonSetState = buttonSetState;

    delay(200); // Evitar rebotes
  }

  // Confirmar contraseña
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Confirmar el PIN:");
  lcd.setCursor(0, 1);
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
        confirmPassword[passwordIndex] = '0' + ((confirmPassword[passwordIndex] - '0' + 1) % 10); // Incrementar dígito
        updatePasswordDisplay(confirmPassword, passwordIndex);
      }
    }

    if (buttonSetState == LOW && lastButtonSetState == HIGH) {
      passwordIndex++;
      if (passwordIndex > 3) {
        inProgrammingMode = false;
      } else {
        lcd.setCursor(passwordIndex * 2, 1); // Mover el cursor al siguiente dígito
        lcd.blink();
      }
    }

    lastButtonOptionState = buttonOptionState;
    lastButtonSetState = buttonSetState;

    delay(200); // Evitar rebotes
  }

  if (strcmp(newPassword, confirmPassword) == 0) {
    strcpy(currentPassword, newPassword);
    savePasswordToEEPROM(newPassword);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PIN cambiado");
    delay(2000);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("El PIN no es igual");
    delay(2000);
  }

  currentSystemMode = NORMAL;
  lcd.clear();
  updateDisplay();
  
}

void updatePasswordDisplay(char* digits, int currentDigitIndex) {
  lcd.setCursor(0, 1);
  lcd.print(digits[0]);
  lcd.setCursor(2, 1);
  lcd.print(digits[1]);
  lcd.setCursor(4, 1);
  lcd.print(digits[2]);
  lcd.setCursor(6, 1);
  lcd.print(digits[3]);

  // Colocar el cursor en el dígito actual para parpadear
  lcd.setCursor(currentDigitIndex * 2, 1);
  lcd.blink();
}









































/*


    bool inputPassword(char* password) {
    int currentDigitIndex = 0;
    char enteredDigits[5] = {'1', '2', '3', '4', '\0'};
    bool digitSelected[4] = {false, false, false, false};

    // Posiciones de los dígitos en el LCD
    int digitPositions[4] = {1, 3, 5, 7};

    lcd.setCursor(0, 1);
    lcd.print(" 1 2 3 4");
    lcd.setCursor(digitPositions[currentDigitIndex], 1);
    lcd.blink();

    while (currentDigitIndex < 4) {
        int buttonOptionState = digitalRead(buttonOptionPin);
        int buttonSetState = digitalRead(buttonSetPin);

        if (buttonOptionState == LOW && !digitSelected[currentDigitIndex]) {
            enteredDigits[currentDigitIndex] = (enteredDigits[currentDigitIndex] == '9') ? '0' : (enteredDigits[currentDigitIndex] + 1);
            updatePasswordDisplay(enteredDigits, currentDigitIndex);
            delay(300); // Debounce delay
        }

        if (buttonSetState == LOW) {
            digitSelected[currentDigitIndex] = true;
            currentDigitIndex++;
            
            if (currentDigitIndex < 4) {
                lcd.setCursor(digitPositions[currentDigitIndex], 1);
                lcd.blink();
            }
            delay(300); // Debounce delay
        }
    }

    strcpy(password, enteredDigits);

    lcd.noBlink();
    return true;
}







bool inputPassword(char* password) {
    currentDigitIndex = 0;
    char enteredDigits[5] = {'1', '2', '3', '4', '\0'};
    bool digitSelected[4] = {false, false, false, false};

    // Posiciones de los dígitos en el LCD
    int digitPositions[4] = {1, 3, 5, 7};

    lcd.setCursor(0, 1);
    lcd.print(" 1 2 3 4");
    lcd.setCursor(digitPositions[currentDigitIndex], 1);
    lcd.blink();

    while (currentDigitIndex < 4) {
        int buttonOptionState = digitalRead(buttonOptionPin);
        int buttonSetState = digitalRead(buttonSetPin);

        if (buttonOptionState == LOW && !digitSelected[currentDigitIndex]) {
            enteredDigits[currentDigitIndex] = (enteredDigits[currentDigitIndex] == '9') ? '0' : (enteredDigits[currentDigitIndex] + 1);
            updatePasswordDisplay(enteredDigits, currentDigitIndex);
            delay(300); // Debounce delay
        }

        if (buttonSetState == LOW) {
            digitSelected[currentDigitIndex] = true;
            currentDigitIndex++;
            
            if (currentDigitIndex < 4) {
                lcd.setCursor(digitPositions[currentDigitIndex], 1);
                lcd.blink();
            }
            delay(300); // Debounce delay
        }
    }

    strcpy(password, enteredDigits);

    lcd.noBlink();
    return true;
}














bool inputPassword(char* password) {
    passwordIndex = 0;
    inProgrammingMode = true;
    currentProgrammingMode = ENTER_PASSWORD;

    // Inicializar enteredDigits con "1234"
    strcpy(enteredDigits, "1234");
    Serial.println("enteredDigits");
    Serial.println(enteredDigits);

    // Mostrar la contraseña inicial en el display
    updatePasswordDisplay(enteredDigits, passwordIndex, true);

    while (inProgrammingMode) {
        handleButtons();
        delay(200); // Evitar rebotes
    }

    strcpy(password, enteredDigits);
    lcd.noBlink();
    return true;
}










void updatePasswordDisplay(char* digits, int currentDigitIndex) {
  lcd.setCursor(0, 1);
  lcd.print(digits[0]);
  lcd.setCursor(2, 1);
  lcd.print(digits[1]);
  lcd.setCursor(4, 1);
  lcd.print(digits[2]);
  lcd.setCursor(6, 1);
  lcd.print(digits[3]);

  // Colocar el cursor en el dígito actual para parpadear
  lcd.setCursor(currentDigitIndex * 2, 1);
  lcd.blink();
}






void updatePasswordDisplay(char enteredDigits[], int currentDigitIndex, bool blink) {
    Serial.println("updatePasswordDisplay");
    lcd.setCursor(0, 1);
    for (int i = 0; i < 4; i++) {
        if (i == currentDigitIndex && blink) {
            lcd.blink();
        } else {
            lcd.noBlink();
        }
        lcd.print(enteredDigits[i] == 0 ? '_' : enteredDigits[i]);
        lcd.print(" ");
    }
}









void requestPassword() {
    lcd.clear();
    lcd.print("Introducir PIN:");
    delay(1000);
Serial.println("requestPassword()");
    enteredDigits[0] = '1';
    enteredDigits[1] = '2';
    enteredDigits[2] = '3';
    enteredDigits[3] = '4';
    currentDigitIndex = 0;
    inProgrammingMode = true;
    currentProgrammingMode = ENTER_PASSWORD;
    updatePasswordDisplay(enteredDigits, currentDigitIndex, true);

    while (inProgrammingMode) {
        handleButtons();
        delay(200);
    }

    enteredDigits[4] = '\0';
    lcd.noBlink();

    if (strcmp(enteredDigits, currentPassword) == 0) {
        lcd.clear();
        lcd.print("PIN Correcto");
        delay(1000);
        currentSystemMode = PROGRAMMING;
        currentProgrammingMode = SET_TIME;
        handleProgramming(HIGH, HIGH);
    } else {
        lcd.clear();
        lcd.print("PIN Incorrecto");
        delay(2000);
        currentSystemMode = NORMAL;
    }
}

void changePassword() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Nuevo PIN:");
    delay(1000);

    passwordIndex = 0;
    passwordStep = 0;
    inProgrammingMode = true;
    currentProgrammingMode = CHANGE_PASSWORD;

    // Inicializar newPassword con "1234"
    strcpy(newPassword, "1234");

    // Mostrar la contraseña inicial en el display
    updatePasswordDisplay(newPassword, passwordIndex, false);

    while (inProgrammingMode) {
        handleButtons();
        delay(200); // Evitar rebotes
    }

    // Confirmar contraseña
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Confirmar el PIN:");
    delay(1000);

    passwordIndex = 0;
    passwordStep = 1;
    inProgrammingMode = true;

    // Inicializar confirmPassword con "1234"
    strcpy(confirmPassword, "1234");

    // Mostrar la contraseña inicial en el display
    updatePasswordDisplay(confirmPassword, passwordIndex, false);

    while (inProgrammingMode) {
        handleButtons();
        delay(200); // Evitar rebotes
    }

    if (strcmp(newPassword, confirmPassword) == 0) {
        strcpy(currentPassword, newPassword);
        savePasswordToEEPROM(newPassword);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("PIN cambiado");
        delay(2000);
    } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("El PIN no es igual");
        delay(2000);
    }

    currentSystemMode = NORMAL;
    lcd.clear();
    updateDisplay();
}
*/


//******************************************************************************
// MANEJO DE MECANISMOS


void serveNext() {
  motor.rotateStep(stepsPerPosition); // El motor lo justo para que el tambor gire 360/22º
}

void goBack(int steps) {
    motor.rotateStep(steps); // El motor rota el número de pasos calculados
}


void refillCompartments() {
    currentProgrammingMode = REFILL;
    currentSystemMode = PROGRAMMING;
    refillCount = 0;
    // Obtener la hora y el día actuales
    timeNow = rtc.time();
    currentHour = timeNow.hr;
    currentMinute = timeNow.min;
    currentDay = static_cast<int>(timeNow.day);

    // Determinar la siguiente ingesta
    dayIndex = currentDay-1;
    timeIndex = 0; // 0: Mañana, 1: Tarde, 2: Noche
    initialDayIndex = dayIndex;
    initialTimeIndex = timeIndex;
   
    
    Serial.println (dayIndex);
    Serial.println (timeIndex);
    lcd.clear();
    lcd.print("RECARGANDO");

    while (refillCount < 21) {
        // Espera a que se presione el botón para avanzar al siguiente compartimento
        // Lógica de avance movida a handleButtons()
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
            lcd.clear();
            lcd.print("VOLVIENDO A ");
            lcd.setCursor(0, 1);
            lcd.print(daysOfWeek[initialDayIndex]);
            lcd.print(" ");
            lcd.print(timesOfDay[initialTimeIndex]);
            delay(2000);

            int steps = -refillCount * stepsPerPosition;
            goBack(steps);

            currentSystemMode = NORMAL;
            lcd.clear();
            updateDisplay();
            return;
        }
    }
}

void advance() {
    currentProgrammingMode = ADVANCE;
    currentSystemMode = PROGRAMMING;
    advanceCount = 0;

    currentHour = timeNow.hr;
    currentMinute = timeNow.min;
    currentDay = static_cast<int>(timeNow.day);

    // Determinar la siguiente ingesta
    dayIndex = currentDay -1;
    timeIndex = 0; // 0: Mañana, 1: Tarde, 2: Noche
    
    initialDayIndex = dayIndex-1;
    initialTimeIndex = timeIndex ;

    lcd.clear();
    lcd.print("Avanzar compart.");

    while (advanceCount < 21) {
        // Espera a que se presione el botón para avanzar al siguiente compartimento
        // La lógica del manejo de botones se ha movido a handleButtons()

        // Actualizar el estado del día y la hora
        timeIndex++;
        if (timeIndex > 2) {
            timeIndex = 0;
            dayIndex = (dayIndex + 1) % 7;
        }

        // Mostrar el siguiente compartimento
        showAdvanceMessage(dayIndex, timeIndex);

        // Incrementar el contador de avance
        advanceCount++;
        if (advanceCount >= 21) {
            lcd.clear();
            lcd.print("VOLVIENDO A ");
            lcd.setCursor(0, 1);
            lcd.print(daysOfWeek[initialDayIndex]);
            lcd.print(" ");
            lcd.print(timesOfDay[initialTimeIndex]);
            delay(2000);

            int steps = -advanceCount * stepsPerPosition;
            goBack(steps);

            currentSystemMode = NORMAL;
            lcd.clear();
            updateDisplay();
            return;
        }
    }
}

void retreat() {
    currentProgrammingMode = RETREAT;
    currentSystemMode = PROGRAMMING;
    retreatCount = 0;
    
    timeNow = rtc.time();
    currentHour = timeNow.hr;
    currentMinute = timeNow.min;
    currentDay = static_cast<int>(timeNow.day);

    // Determinar la siguiente ingesta
    dayIndex = currentDay -1;
    timeIndex = 0; // 0: Mañana, 1: Tarde, 2: Noche
    
    initialDayIndex = dayIndex-1;
    initialTimeIndex = timeIndex ;


    lcd.clear();
    lcd.print("Retroceder compart.");

    while (retreatCount < 21) {
        // Espera a que se presione el botón para retroceder al compartimento anterior
        // La lógica del manejo de botones se ha movido a handleButtons()

        // Actualizar el estado del día y la hora
        timeIndex--;
        if (timeIndex < 0) {
            timeIndex = 2;
            dayIndex = (dayIndex - 1 + 7) % 7;
        }

        // Mostrar el compartimento anterior
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

            currentSystemMode = NORMAL;
            lcd.clear();
            updateDisplay();
            return;
        }
    }
}


//*********************************************************************************************************
// EEPROM



// Guardar y leer desde la EEPROM

void saveAlarmsToEEPROM() {
    EEPROM.write(EEPROM_MORNING_ALARM_HOUR, morningAlarm.hour);
    EEPROM.write(EEPROM_MORNING_ALARM_MINUTE, morningAlarm.minute);
    EEPROM.write(EEPROM_AFTERNOON_ALARM_HOUR, afternoonAlarm.hour);
    EEPROM.write(EEPROM_AFTERNOON_ALARM_MINUTE, afternoonAlarm.minute);
    EEPROM.write(EEPROM_EVENING_ALARM_HOUR, eveningAlarm.hour);
    EEPROM.write(EEPROM_EVENING_ALARM_MINUTE, eveningAlarm.minute);
}

void readAlarmsFromEEPROM() {
    morningAlarm.hour = EEPROM.read(EEPROM_MORNING_ALARM_HOUR);
    morningAlarm.minute = EEPROM.read(EEPROM_MORNING_ALARM_MINUTE);
    afternoonAlarm.hour = EEPROM.read(EEPROM_AFTERNOON_ALARM_HOUR);
    afternoonAlarm.minute = EEPROM.read(EEPROM_AFTERNOON_ALARM_MINUTE);
    eveningAlarm.hour = EEPROM.read(EEPROM_EVENING_ALARM_HOUR);
    eveningAlarm.minute = EEPROM.read(EEPROM_EVENING_ALARM_MINUTE);
}

void savePasswordToEEPROM(char* password) {
    for (int i = 0; i < 4; i++) {
        EEPROM.write(EEPROM_PASSWORD + i, password[i]);
    }
    EEPROM.write(EEPROM_PASSWORD + 4, '\0'); // Null terminator
}

void readPasswordFromEEPROM(char* password) {
    for (int i = 0; i < 5; i++) {
        password[i] = EEPROM.read(EEPROM_PASSWORD + i);
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

  // Inicializar los pines de los botones
  pinMode(buttonSetPin, INPUT_PULLUP);
  pinMode(buttonOptionPin, INPUT_PULLUP);

  // Inicializar el DS1302
  rtc.writeProtect(false);
  rtc.halt(false);

  // Leer la hora actual del DS1302
  //timeNow = rtc.time();
  
  // Leer las alarmas de la EEPROM
  readAlarmsFromEEPROM();
  
  //COMENTAR ESTA LINEA PARA REPROGRAMAR EL PIN A 1234 EN CASO DE QUE SE HAYA OLVIDADO EL PIN
  // UNA VEZ SUBIDO EL PROGRAMA; DESCOMENTAR LA LINEA Y SUBIRLO DE NUEVO
  // Leer el PIN de la EEPROM
  readPasswordFromEEPROM(currentPassword);

  // Verificar si la hora del DS1302 es válida
  if (timeNow.yr == 2000) {
    // Establecer la hora inicial solo la primera vez si el año es 2000 (indica que no se ha configurado)
    timeNow = Time(2024, 18, 6, 1, 2, 0, Time::kMonday); // Configura tu hora inicial deseada
    rtc.time(timeNow);
  }

  // Inicializar los motores
  motor.attach(8, 9, 10, 11, 60, 4096, P_FULLSTEP);

  // Mostrar la hora inicial
  updateDisplay();
}

void loop() {
  updateDisplay();
  handleButtons();
      // Verificar si es hora de erogar los medicamentos para la siguiente ingesta
  checkAlarms();
}
