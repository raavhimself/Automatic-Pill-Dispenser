// INCLUYE control manual desde el teclado
#include <Arduino.h>
#include <Wire.h>
#include <ESP_FlexyStepper.h>
#include <Keypad.h>

#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

//LCD

hd44780_I2Cexp lcd;

//NUMPAD
#define NUMPAD_ROW_NUM     4 // four rows
#define NUMPAD_COLUMN_NUM  4 // four columns
char keys[NUMPAD_ROW_NUM][NUMPAD_COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pin_rows[NUMPAD_ROW_NUM]      = {19, 18, 5, 17}; // GIOP19, GIOP18, GIOP5, GIOP17 connect to the row pins
byte pin_column[NUMPAD_COLUMN_NUM] = {16, 4, 0, 2};   // GIOP16, GIOP4, GIOP0, GIOP2 connect to the column pins

Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, NUMPAD_ROW_NUM, NUMPAD_COLUMN_NUM );

// Variable global para almacenar la entrada de dígitos en el teclado
String inputDigits = "";

// IO pin assignments
const int MOTOR_AVANCE_STEP_PIN = 33;
const int MOTOR_AVANCE_DIRECTION_PIN = 25;
const int MOTOR_AVANCE_ENABLE_PIN = 23;

const int BUTTON_PIN = 13; //define the IO pin the emergency stop switch is connected to
const int LIMIT_SWITCH_PIN = 32;   //define the IO pin where the limit switches are connected to (switches in series in normally closed setup against ground)

// Speed settings
const int MAX_DISTANCE_TO_TRAVEL_IN_STEPS = 3000;
const int SPEED_IN_STEPS_PER_SECOND = 300;
const int ACCELERATION_IN_STEPS_PER_SECOND = 800;
const int DECELERATION_IN_STEPS_PER_SECOND = 800;


long valorAvance;

// create the stepper motor object

ESP_FlexyStepper stepperAvance;

int previousDirectionAvance = 1;

//variables for software debouncing of the limit switches
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 200; //the minimum delay in milliseconds to check for bouncing of the switch. Increase this slighlty if you switches tend to bounce a lot
bool buttonStateChangeDetected = false;
byte limitSwitchState = 0;
byte oldConfirmedLimitSwitchState = 0;

void ICACHE_RAM_ATTR emergencySwitchHandler()
{
  // we do not realy need to debounce here, since we only want to trigger a stop, no matter what.
  // So even triggering mutliple times does not realy matter at the end
  if (digitalRead(BUTTON_PIN) == LOW) // Switch is configured in active low configuration
  {
    // the boolean true in the following command tells the stepper to hold the emergency stop until reaseEmergencyStop() is called explicitly.
    // If ommitted or "false" is given, the function call would only stop the current motion and then instanlty would allow for new motion commands to be accepted
    stepperAvance.emergencyStop(true);
  }
  else
  {
    // release a previously enganed emergency stop when the emergency stop button is released
    stepperAvance.releaseEmergencyStop();
  }
}

void limitSwitchHandler()
{
  limitSwitchState = digitalRead(LIMIT_SWITCH_PIN);
  lastDebounceTime = millis();
}

int obtenerValorVariable() {  //Obtener el codigo que identifica el producto introducido en el teclado
  long valorVariable;
  Serial.println ("int obtenerValorVariable()");
  inputDigits = "";
  lcd.setCursor(0, 3);
  lcd.blink();
  while (true) {
      char key = keypad.getKey();
      if (key) {
        Serial.println(key);
      }
      if (key == '*') {
        // El operador ha presionado #
        // Finaliza la captura del codigo del producto
        break;
      } else if (key >= '0' && key <= '9') {
        // El operador ha presionado un dígito
        // Almacena el dígito en la variable inputDigits
        inputDigits += key;
        lcd.setCursor(0, 0);
        lcd.blink();
        lcd.print(inputDigits);
        valorVariable = atof(inputDigits.c_str());
        }
    }
  return valorVariable;
   Serial.println("ValorVariable");
   Serial.println(valorVariable);
  }


bool incrementoAutomaticoAvance = false;

void controlManual() {
  Serial.print("controlManual()");
  char manualControlKey = keypad.getKey();
  while (manualControlKey != '#') {
    manualControlKey = keypad.getKey();
    if (manualControlKey == '2') {
      // Si se presiona la tecla '2', activar o desactivar el modo de incremento automático
      incrementoAutomaticoAvance = !incrementoAutomaticoAvance;
      Serial.print("Modo de incremento automático activo: ");
      Serial.println(incrementoAutomaticoAvance ? "SI" : "NO");
      if (incrementoAutomaticoAvance) {
        // Si el incremento automático se activa, incrementar pasos automáticamente hasta que se presione '2' de nuevo
        while (keypad.getKey() != '2') {
          stepperAvance.moveRelativeInSteps(1);
          delay(100); // Ajusta este valor según la velocidad deseada
        }
        Serial.println("Incremento automático detenido");
      }
    } else if (manualControlKey == '3') {
      // Si se presiona la tecla '3', mover el stepper un paso en la dirección correspondiente
      stepperAvance.moveRelativeInSteps(1);
    } else if (manualControlKey == '1') {
      // Si se presiona la tecla '1', mover el stepper un paso en la dirección opuesta
      stepperAvance.moveRelativeInSteps(-1);
    }
  }
  valorAvance =stepperAvance.getCurrentPositionInSteps();
  Serial.println("valorAvance - read position");
  Serial.println(valorAvance);
}
void readKeypad() {
  char key = keypad.getKey();

  if (key) {
    Serial.println(key);
  }

  if (key == 'A') {
    // El operador ha presionado la tecla A
    // Realiza las acciones correspondientes
    valorAvance = obtenerValorVariable();
    Serial.println("valorAvance - read keaypad");
    Serial.println(valorAvance);
  } else if (key == 'D') {
    // Control manual
    Serial.println("Control manual");
    controlManual();
  } else if (key == '#') {
    // Salida y memorización de datos
    Serial.println("Salida y memorización de datos");
  } else if (key >= '0' && key <= '9') {
    // El operador ha presionado un dígito
    // Almacena el dígito en la variable inputDigits
    inputDigits += key;
  }
}



void posicionInicial() {
  Serial.println("void posicionInicial()");
  
  while (digitalRead(LIMIT_SWITCH_PIN) == LOW) {
    stepperAvance.moveRelativeInSteps(-1);
    delay(100);
  }
  
  // Almacenar la posición del fin de carrera
  long posicionFinCarreraAvance = stepperAvance.getCurrentPositionInSteps();
  Serial.print("Posición del fin de carrera 1: ");
  Serial.println(posicionFinCarreraAvance);
  stepperAvance.moveRelativeInSteps(1750);
  stepperAvance.setCurrentPositionAsHomeAndStop(); 
  Serial.println("POSICION:");
  Serial.println(stepperAvance.getCurrentPositionInSteps());
}

void setup()
{
  delay(1000);
  Serial.begin(9600);
  Serial.println("void setup()");
  // set the LCD address to 0x27 for a 20 chars and 4 line display
  lcd.begin(20, 4);  // Inicializar la pantalla LCD
  

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("COMENZANDO:");
  Serial.println("COMENZANDO:");
  delay(500);
  
  pinMode(MOTOR_AVANCE_STEP_PIN,    OUTPUT);
  pinMode(MOTOR_AVANCE_STEP_PIN,   OUTPUT);
  pinMode(MOTOR_AVANCE_ENABLE_PIN, OUTPUT);
  digitalWrite(MOTOR_AVANCE_ENABLE_PIN, LOW);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LIMIT_SWITCH_PIN, INPUT_PULLUP);

  //attach an interrupt to the IO pin of the ermegency stop switch and specify the handler function
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), emergencySwitchHandler, RISING);

  //attach an interrupt to the IO pin of the limit switch and specify the handler function
  attachInterrupt(digitalPinToInterrupt(LIMIT_SWITCH_PIN), limitSwitchHandler, CHANGE);

  // tell the ESP_flexystepper in which direction to move when homing is required (only works with a homing / limit switch connected)
  stepperAvance.setDirectionToHome(-1);

  // connect and configure the stepper motor to its IO pins
  stepperAvance.connectToPins(MOTOR_AVANCE_STEP_PIN, MOTOR_AVANCE_DIRECTION_PIN);

  // set the speed and acceleration rates for the stepper motor
  stepperAvance.setSpeedInStepsPerSecond(SPEED_IN_STEPS_PER_SECOND);
  stepperAvance.setAccelerationInStepsPerSecondPerSecond(ACCELERATION_IN_STEPS_PER_SECOND);
  stepperAvance.setDecelerationInStepsPerSecondPerSecond(DECELERATION_IN_STEPS_PER_SECOND);

  
  Serial.print("POSICION INICIAL");
  posicionInicial();
  delay(2000);
}
void correrSecuencia() {
  stepperAvance.moveRelativeInSteps(1000);
  lcdPrintPosition();
  
  stepperAvance.moveRelativeInSteps(-500);
  lcdPrintPosition();

  stepperAvance.moveToPositionInSteps(0);
  lcdPrintPosition();
}

void leerBoton() {
  Serial.println ("void leerBoton()");
  if (digitalRead(BUTTON_PIN) == LOW) { 
    correrSecuencia(); 
  }
}


void lcdPrintPosition() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("POSICION:");
  Serial.println("POSICION:");
  lcd.setCursor(0, 1);
  lcd.print(stepperAvance.getCurrentPositionInSteps());
  Serial.println(stepperAvance.getCurrentPositionInSteps());
  delay (1000);
}


void loop()
{
  // Leer estado del botón
  leerBoton();
  Serial.println ("leerBoton()");
  // Leer entrada del teclado numérico
  readKeypad();
  
}
