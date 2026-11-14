#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#define adc_disable() (ADCSRA &= ~(1 << ADEN))  // disable ADC (before power-off)
#define adc_enable() (ADCSRA |= (1 << ADEN))    // re-enable ADC

// How it works:
// We use the standard air spray device’s “9 Min” mode.
// When this mode is selected, power is supplied to our Digispark board, pin P0 is connected to GND, and the standard air spray device’s delay is activated (9 min).
// Every 8 minutes (resetTime), we open the connection between P0 and GND and close it again to reset the air spray device’s delay timer.
// Every 2.5 hours (pauseTime), we run doSpray() for a duration of (workTime) to activate the air spray device’s mechanism.
// After (workTime), we reset the timer, and the cycle repeats.
// This logic allows the air spray device’s manual activation button to be used as usual.
// P0 is connected to the air spray device’s controller “9 Min” mode pin (the chip’s pin is disconnected from the switch).
// P1 is connected to the air spray device’s controller “Off” mode pin.

const uint32_t pauseTime = 9000;  // Air freshener activation time (sec)
const uint32_t workTime = 10;     // Delay time before the air freshener mechanism activates (sec)
const uint32_t resetTime = 480;   // Air freshener timer reset time (sec)

uint32_t mainTimer = 0;
bool state = false;  // true - Spray mode has been activated

/*
  Disconnect P1 from GND
  Connect P0 to GND
  Glade enters “9 Minutes” mode
  Initial operating mode
*/
void enableDefaultTimer() {
  pinMode(1, INPUT);
  digitalWrite(1, LOW);
  delay(250);
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW);
  state = false;
}

/*
  Glade sprays when switching from “Off” mode to any other mode (in our case, “9 Minutes”)
  Disconnect P0 from GND—turns “9 Minutes” mode off
  Connect P1 to GND—turns on “Off” mode
  Disconnect P1 from GND—turns off “Off” mode
  Connect P0 to GND—turns on “9 Minutes” mode
  A spray is released
*/
void doSpray() {
  state = true;
  pinMode(0, INPUT);
  digitalWrite(0, LOW);
  delay(250);
  pinMode(1, OUTPUT);
  digitalWrite(1, LOW);
  delay(750);
  pinMode(1, INPUT);
  digitalWrite(1, LOW);
  delay(250);
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW);
}

/*
  Disconnect P0 from GND
  Connect P0 to GND
  Glade enters “9 Minutes” mode; the chip's standard timer delay is reset
  Do not perform this step if spray mode is active
*/
void resetDefaultTimer() {
  if (state) {
    return;
  }

  pinMode(0, INPUT);
  digitalWrite(0, LOW);
  delay(250);
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW);
}

void setup() {
  // Power Saving setup
  for (byte i = 0; i < 6; i++) {
    pinMode(i, INPUT);     // Set all ports as INPUT to save energy
    digitalWrite(i, LOW);  //
  }
  adc_disable();                        // Disable Analog-to-Digital Converter
  wdt_reset();                          // Watchdog reset
  wdt_enable(WDTO_8S);                  // Watchdog enable Options: 15MS, 30MS, 60MS, 120MS, 250MS, 500MS, 1S, 2S, 4S, 8S
  WDTCR |= _BV(WDIE);                   // Interrupts watchdog enable
  sei();                                // enable interrupts
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // Sleep Mode: max

  enableDefaultTimer();
}

void loop() {
  mainTimer += 8;  // Each step takes 8 seconds

  if (mainTimer >= pauseTime) {
    if (state) {
      if (mainTimer > (pauseTime + workTime)) {
        mainTimer = 0;
        state = false;
      }
    } else {
      doSpray();
    }
  }

  if (mainTimer >= resetTime) {
    resetDefaultTimer();
  }

  sleep_enable();
  sleep_cpu();
}

ISR(WDT_vect) {
  WDTCR |= _BV(WDIE);
}
