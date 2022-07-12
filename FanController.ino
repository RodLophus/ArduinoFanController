/*
 * Copyright 2022 Rodolfo B. Manin
 * This code is licensed under MIT license (see the LICENSE file for details)
 */

#include <EEPROM.h>
#include <Thermistor.h>
#include <NTC_Thermistor.h>
#include <SmoothThermistor.h>

#define WaitSerial while(Serial.available() < 1)
#define Print(a) Serial.print(a)
#define PrintLn(a) Serial.println(a)

#define NTC_PIN       A1      // Temperature sensor
#define OC1B_PIN      10      // PWM output - channel 2
#define OC1A_PIN      9       // PWM output - channel 1
#define PCINT23_PIN   7       // Fan 1 tachometer input
#define PCINT22_PIN   6       // Fan 2 tachometer input
#define PCINT21_PIN   5       // Fan 3 tachometer input
#define PCINT20_PIN   4       // Fan 4 tachometer input

// Temperature sensor parameters
#define REFERENCE_RESISTANCE  21500
#define NOMINAL_TEMPERATURE   25
#define SMOOTHING_FACTOR      5

// PWM frequency can be fine-tuned for the 16-bit Output Compare Unit (OC1) only,
// as it allows ORC1A or ICR1 to be used as the TOP limiter for TCNT1
// (for the 8-bit units, TOP = 0xFF)
#define PWM_FREQ_HZ 25000L
#define TCNT1_TOP   F_CPU / (2 * PWM_FREQ_HZ)        // formula at [Section 15.9.4]; N = 1 (no prescaling)
#define TACH_UPDATE_FREQ_HZ 1                        // how many times per second the tachometer value will be updated

struct t_ctrlChannel {
  byte lowTemp;
  byte highTemp;
  byte minPwm;
  byte maxPwm;
};

struct t_config {
  unsigned int ntcRes25;
  unsigned int ntcB;
  struct t_ctrlChannel ctrl[2];
};

struct t_config config;
Thermistor* thermistor;
byte monitorCounter = 0;
volatile byte refreshFlag = 0;
volatile word subTimer = 0;
volatile byte previousTachoInputState;
volatile word tachoCounter[] = { 0, 0, 0, 0 };
volatile word tachoRpm[] = { 0, 0, 0, 0 };


/*
 * Just print a string, a number and another string
 */
void ptnt(char *text1, int number, char *text2) {
  char buf[80];
  sprintf(buf, "%s%u%s", text1, number, text2);
  PrintLn(buf);
}


/*
 * Ask for a new value for a configuration parameter, validate the answer and save
 */
unsigned int updateConfigParam(word currentValue, word min, word max) {
  unsigned long num = 0;
  byte ch;
  while(1) {
    Print("New value (");
    Print(min); Print(" to ");  Print(max);
    Print(") ["); Print(currentValue); Print("] : ");
    while(1) {
      WaitSerial;
      ch = (byte)Serial.read();
      if((ch == 0x0D) || (ch == 0x0A)) {
        if(num == 0) {
          return currentValue;
        }
        if((num < min) || (num > max)) {
          PrintLn("  ** Out of range **");
          num = 0;
          break;
        }
        return num;
      }
      ch -= '0';
      if(ch < 11) {
        Print(ch);
        num = 10 * num + ch;
      }
    }
  }
}


/*
 * Configuration menu
 */
void updateConfig() {
  byte i;
  byte err;
  while(1) {
    PrintLn("\r\nConfiguration\n");
    PrintLn("NTC temperature sensor:");
    ptnt("(a) Resistance at 25 C..........: ", config.ntcRes25, " Ohm");
    ptnt("(b) B factor....................: ", config.ntcB, "\n");
    PrintLn("PWM channel 1 (fans 1 and 2):");
    ptnt("(c) Low temperature threshold...: ", config.ctrl[0].lowTemp, " C");
    ptnt("(d) High temperature threshold..: ", config.ctrl[0].highTemp, " C");
    ptnt("(e) Minimum PWM duty cycle......: ", config.ctrl[0].minPwm, " %");
    ptnt("(f) Maximum PWM duty cycle......: ", config.ctrl[0].maxPwm, " %\n");
    PrintLn("PWM channel 2 (fans 3 and 4):");
    ptnt("(g) Low temperature threshold...: ", config.ctrl[1].lowTemp, " C");
    ptnt("(h) High temperature threshold..: ", config.ctrl[1].highTemp, " C");
    ptnt("(i) Minimum PWM duty cycle......: ", config.ctrl[1].minPwm, " %");
    ptnt("(j) Maximum PWM duty cycle......: ", config.ctrl[1].maxPwm, " %\n");
    PrintLn("(z) Save and exit\n");
    WaitSerial;
    switch(Serial.read()) {
      case 'a': config.ntcRes25 = updateConfigParam(config.ntcRes25, 1000, 65535); break;
      case 'b': config.ntcB = (unsigned int)updateConfigParam(config.ntcB, 1000, 65535); break;
      case 'c': config.ctrl[0].lowTemp = (byte)updateConfigParam(config.ctrl[0].lowTemp, 10, 100); break;
      case 'd': config.ctrl[0].highTemp = (byte)updateConfigParam(config.ctrl[0].highTemp, 10, 100); break;
      case 'e': config.ctrl[0].minPwm = (byte)updateConfigParam(config.ctrl[0].minPwm, 0, 100); break;
      case 'f': config.ctrl[0].maxPwm = (byte)updateConfigParam(config.ctrl[0].maxPwm, 0, 100); break;
      case 'g': config.ctrl[1].lowTemp = (byte)updateConfigParam(config.ctrl[1].lowTemp, 10, 100); break;
      case 'h': config.ctrl[1].highTemp = (byte)updateConfigParam(config.ctrl[1].highTemp, 10, 100); break;
      case 'i': config.ctrl[1].minPwm = (byte)updateConfigParam(config.ctrl[1].minPwm, 0, 100); break;
      case 'j': config.ctrl[1].maxPwm = (byte)updateConfigParam(config.ctrl[1].maxPwm, 0, 100); break;
      case 'z':
        err = 0;
        for(i  = 0; i < 2; i++) {
          if(config.ctrl[i].lowTemp > config.ctrl[i].highTemp) {
            ptnt("- Low temperature threshold > high temperature threshold on channel ", i + 1, ".");
            err = 1;
          } 
          if(config.ctrl[i].minPwm > config.ctrl[i].maxPwm) {
            ptnt("- Minimum PWM duty cycle > maximum PWM duty cycle on channel ", i + 1, ".");
            err = 1;
          }
        }
        if(err) {
          PrintLn("\nConfiguration not saved. Fix the parameters above and try again.\n");
          PrintLn("Press any key...");
          WaitSerial;
          Serial.read();
        } else {
          EEPROM.put(0, config);
          PrintLn("Configuration updated.\n");
          return;
        }
    }
  }
}


/*
 * Sets the PWM duty cycle for a channel
 */
void setPwm(byte module, byte duty_cycle) {
  word n = (word) (duty_cycle * TCNT1_TOP) / 100;
  if(module == 1) OCR1A = n;
  if(module == 0) OCR1B = n;
}


/*
 * Output compare (PWM generator) initialization
 */
void pwmOutputInit() {
  pinMode(OC1A_PIN, OUTPUT);
  pinMode(OC1B_PIN, OUTPUT);
  // CS[12..10]   = 0b001 : Clock source = I/Oclk; no prescaling
  // COM1x1[1..0] = 0b10  : non-inverting compare output mode [Section 15.9.4] and [Table 15.4]
  // WGM[13..10]  = 15    : Phase correct PWM; TOP = ICR1
  TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
  TCCR1B = (1 << WGM13) | (1 << CS10);
  // Enable Timer1 interrupt on overflow
  TIMSK1 = (1 << TOIE1);
  // Set Timer1 frequency
  ICR1 = TCNT1_TOP;
}


/*
 * Tachometers inputs (with interrupt on pin change) initialization
 */
void tachometerInputInit(void) {
  // Use lower external pull-ups for better noise imunity
  pinMode(PCINT23_PIN, INPUT_PULLUP);
  pinMode(PCINT22_PIN, INPUT_PULLUP);
  pinMode(PCINT21_PIN, INPUT_PULLUP);
  pinMode(PCINT20_PIN, INPUT_PULLUP);
  PCICR |= (1 << PCIF2);        // Enables pin change interrupts on PCINT[23..16] [Section 12.2.5]
  PCMSK2 |= 0b11110000;         // Enables pin change interrupts on PCINT[23..20] [Section 12.2.6]
  previousTachoInputState = PIND & 0xF0;
}


/*
 * ISR for Timer1: calculate fan speed and triggers a refresh for the real time monitor
 */
ISR(TIMER1_OVF_vect) {
  byte i;
  if(subTimer++ > (PWM_FREQ_HZ / TACH_UPDATE_FREQ_HZ)) {
    // Runs TACH_UPDATE_FREQ_HZ times / sec.
    // Calculates fan's speeds (in RPM) and resets the tacho toggle counters
    subTimer = 0;
    for(i = 0; i < 4; i++) {
      // A standard PC fan "tachometer" pin toggles its state each 1/4 revolution
      // "N" toggles per second = (N/4)*60 RPM
      tachoRpm[i] = tachoCounter[i] * (60 / 4) * TACH_UPDATE_FREQ_HZ;
      tachoCounter[i] = 0;
    }
    refreshFlag = 1;
  }
}


/*
 * ISR for pin changes: increments the change counters used for RPM calculation
 */
ISR(PCINT2_vect) {
  byte tachoInputState;
  byte changedPins;
  tachoInputState = PIND & 0xF0;
  changedPins = tachoInputState ^ previousTachoInputState;
  // Increments the tacho signal toggle counter on fans where it changed
  if(changedPins & 0b10000000) tachoCounter[0]++;
  if(changedPins & 0b01000000) tachoCounter[1]++;
  if(changedPins & 0b00100000) tachoCounter[2]++;
  if(changedPins & 0b00010000) tachoCounter[3]++;
  previousTachoInputState = tachoInputState;
}


/*
 * Arduino setup
 */
void setup() {
  EEPROM.get(0, config);
  pwmOutputInit();
  tachometerInputInit();
  Thermistor* originThermistor = new NTC_Thermistor(NTC_PIN, REFERENCE_RESISTANCE,
    (double)config.ntcRes25, NOMINAL_TEMPERATURE, (double)config.ntcB);
  thermistor = new SmoothThermistor(originThermistor, SMOOTHING_FACTOR);
  Serial.begin(9600);
}


/*
 * Arduino main loop
 */
void loop() {
  byte lineCounter = 0;
  char monitorText[50];
  double temperature;
  byte pwm[2];
  byte channel;
  if(refreshFlag) {
    temperature = thermistor->readCelsius();
    for(channel = 0; channel < 2; channel++) {
      if(temperature < config.ctrl[channel].lowTemp) {
        pwm[channel] = config.ctrl[channel].minPwm;
      } else {
        if(temperature > config.ctrl[channel].highTemp) {
          pwm[channel] = config.ctrl[channel].maxPwm;
        } else {
          pwm[channel] = ((config.ctrl[channel].maxPwm - config.ctrl[channel].minPwm) / (config.ctrl[channel].highTemp - config.ctrl[channel].lowTemp)) * 
            (temperature - config.ctrl[channel].lowTemp) + config.ctrl[channel].minPwm;
        }
      }
      setPwm(channel, pwm[channel]);
      if(monitorCounter > 0) {
        if(++monitorCounter > 10) {
          monitorCounter = 1;
          Serial.println("Temp(C)\tPWM1(%) PWM2(%)  RPM1  RPM2  RPM3  RPM4");  
        }
        sprintf(monitorText, "\t %3d     %3d     %4d  %4d  %4d  %4d", pwm[0], pwm[1], tachoRpm[0], tachoRpm[1], tachoRpm[2], tachoRpm[3]);
        Serial.print(temperature);
        Serial.println(monitorText);
      }
    }
    refreshFlag = 0;
  }
  if(Serial.available() > 0) {
    if(monitorCounter > 0) {
      Serial.read();
      PrintLn("\nReal-time monitor terminated.\n");
      monitorCounter = 0;
    } else {
      switch(Serial.read()) {
        case 'm':
          PrintLn("\nReal-time monitor.  Press any key to exit.\n");
          monitorCounter = 10;
          break;
        case 'c':
          updateConfig();
          break;
        default:
          PrintLn("Press \"c\" to change the configuration or \"m\" to monitor");
      }
    }
  }
}


