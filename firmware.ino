#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

/*
  How it works
  ------------
  The stock Glade controller sprays immediately when the mode changes from
  "Off" to any other mode (9/18/36 min). The whole mod exploits this:
    P0 -> stock "9 Min" input (chip pin cut from the physical switch)
    P1 -> stock "Off" input
  Pins emulate open-drain: OUTPUT+LOW = assert (pull to GND), INPUT = release.
  The physical switch stays at "9 Min" (this position powers the Digispark).

  - Normally P0 is asserted ("9 Min"), so the stock 9-min timer runs.
  - Every resetTime we briefly release P0 to reset the stock timer before it
    reaches 9 min, so the device never sprays on its own.
  - Every pauseTime we pulse "Off" (P1) then re-assert "9 Min" (P0): the
    Off->9Min transition makes the device spray once.
  - The manual boost button keeps working (handled by the stock controller).
  - Between ticks the ATtiny85 sleeps; the watchdog wakes it every 8 s.
*/

// ---- Pins (open-drain emulation) ----
const uint8_t PIN_9MIN = 0;  // P0
const uint8_t PIN_OFF = 1;   // P1

// ---- Timing, seconds (multiples of the 8 s watchdog tick) ----
const uint32_t pauseTime = 9000;  // spray interval: 2.5 h
const uint32_t resetTime = 360;   // stock-timer reset period: 6 min (keep < 540 s with margin!)
const uint32_t workTime = 16;     // hold after a spray before restarting the cycle
const uint8_t WDT_TICK = 8;       // watchdog wake-up period, seconds

// ---- Mode-switch settle times, ms (empirically tuned) ----
const uint16_t SETTLE_MS = 250;
const uint16_t OFF_MS = 750;

static_assert(resetTime < 540UL, "resetTime must stay below the stock 9-min timer");
static_assert(resetTime < pauseTime, "resetTime must be shorter than pauseTime");

uint32_t mainTimer = 0;
uint32_t lastReset = 0;
bool spraying = false;

// open-drain helpers: never drive HIGH (stock inputs have pull-ups)
inline void release(uint8_t pin) {
  pinMode(pin, INPUT);
  digitalWrite(pin, LOW);
}
inline void assertLow(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void enableDefaultTimer() {
  release(PIN_OFF);
  delay(SETTLE_MS);
  assertLow(PIN_9MIN);
  spraying = false;
}

void doSpray() {
  spraying = true;
  release(PIN_9MIN);  // leave "9 Min"
  delay(SETTLE_MS);
  assertLow(PIN_OFF);  // enter "Off"
  delay(OFF_MS);
  release(PIN_OFF);  // leave "Off"
  delay(SETTLE_MS);
  assertLow(PIN_9MIN);  // Off -> 9 Min: the device sprays
}

void resetDefaultTimer() {
  release(PIN_9MIN);  // briefly leave "9 Min" to reset the stock timer
  delay(SETTLE_MS);
  assertLow(PIN_9MIN);
}

void setup() {
  // Set all ports as INPUT to save energy
  for (uint8_t i = 0; i < 6; i++) {
    pinMode(i, INPUT);
    digitalWrite(i, LOW);
  }


  ADCSRA &= ~_BV(ADEN);                          // ADC off
  ACSR |= _BV(ACD);                              // analog comparator off
  PRR |= _BV(PRADC) | _BV(PRUSI) | _BV(PRTIM1);  // stop unused clocks (Timer0 kept for delay)

  wdt_reset();                          // Watchdog reset
  wdt_enable(WDTO_8S);                  // Watchdog enable Options: 15MS, 30MS, 60MS, 120MS, 250MS, 500MS, 1S, 2S, 4S, 8S
  WDTCR |= _BV(WDIE);                   // Interrupts watchdog enable
  sei();                                // enable interrupts
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // Sleep Mode: max

  enableDefaultTimer();
}

void loop() {
  mainTimer += WDT_TICK;  // WDT free-runs: one tick ~= 8 s regardless of body time

  if (mainTimer >= pauseTime) {
    if (spraying) {
      if (mainTimer >= (pauseTime + workTime)) {
        mainTimer = 0;
        lastReset = 0;
        spraying = false;
      }
    } else {
      doSpray();
    }
  }

  // reset the stock timer once per resetTime (not on every tick)
  if (!spraying && (mainTimer - lastReset >= resetTime)) {
    lastReset = mainTimer;
    resetDefaultTimer();
  }

  sleep_enable();
  sleep_cpu();
  sleep_disable();
}

ISR(WDT_vect) {
  WDTCR |= _BV(WDIE);  // re-arm the watchdog interrupt
}
