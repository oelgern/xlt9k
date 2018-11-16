// XLT9K - eXtreme Light Timer 9K
// For more information see https://github.com/oelgern/xlt9k
//
// Note: You'll need Adafruit's RTClib: https://github.com/adafruit/RTClib
//
// The author or authors make no claim to this data or the use of this data.
// Anyone is free to do anything with this data.

// Thresholds to start and stop timer
//  These will need to be customized by the user.
#define START_LEVEL 40
#define STOP_LEVEL 60

// Date and time functions using a DS1307 RTC connected via I2C and Wire lib
#include <Wire.h>
#include "RTClib.h"

// Using LiquidCrystal library
#include <LiquidCrystal.h>

// Pin for the light meter
#define LIGHT_SENSOR A2

// Pin for the RTC squarewave
#define SQW A3

enum timerStates {
  READY,  // After timer initialization, and before the timer starts
  TIMING, // While the time is elapsing
  DONE    // After the timer is stopped
};

// The state machine starts in the ready mode.
enum timerStates timerState = READY;

// Select the pins used on the LCD panel.
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Declare realtime clock.
RTC_DS1307 rtc;

// Everything is global because ... laziness.
uint8_t day = 0;
uint8_t hour = 0;
uint8_t min = 0;
uint8_t sec = 0;
uint16_t lastFrac = 0;
uint16_t fracCounter = 0;
uint16_t fracSec = 0;
uint16_t currentLevel = 0;

// Add a second to the base time.
// This is done when the fractional timer overflows.
void incrementSec() {

  // Check if the minute should increment.
  if (sec == 59) {
    sec = 0;

    // Check if the hour should increment.
    if (min == 59) {
      min = 0;

      // Check if the day should increment.
      if (hour == 23) {
        hour = 0;
        day ++;
      }
      else {
        hour ++;
      }
    }
    else {
      min ++;
    }
  }
  else {
    sec ++;
  }
}

// Display all time text except fractions of a second.
void displayTimeBase() {

  // Move to the begining of the second line.
  lcd.setCursor(0,1);

  if (day < 10) {

    // Print leading 0 so field is always same number of characters and the
    //  subsequent characters remain in their same columns.
    lcd.print('0');
  }
  lcd.print(day, DEC);
  lcd.print('d');
  if (hour < 10) {
    lcd.print('0');
  }
  lcd.print(hour, DEC);
  lcd.print('h');
  if (min < 10) {
    lcd.print('0');
  }
  lcd.print(min, DEC);
  lcd.print('m');
  if (sec < 10) {
    lcd.print('0');
  }
  lcd.print(sec, DEC);
  lcd.print('.');
}

// Display the fraction of a second.
void displayTimeFrac () {

  // The fractional portion's display columns must stay consistent.
  lcd.setCursor(12,1);
  if (fracSec < 10) {
    lcd.print("000");
  }
  else if (fracSec < 100) {
    lcd.print("00");
  }
  else if (fracSec < 1000) {
    lcd.print("0");
  }
  lcd.print(fracSec, DEC);

  // Remember this time for the next pass.
  lastFrac = fracSec;
}

// Display the light level.
void displayLevel () {

  // Always display the level in the same columns and row.
  lcd.setCursor(12,0);
  if (currentLevel < 10) {
    lcd.print("   ");
  }
  else if (currentLevel < 100) {
    lcd.print("  ");
  }
  else if (currentLevel < 1000) {
    lcd.print(" ");
  }
  lcd.print(currentLevel, DEC);
}

// Calculate the fractional portion of the time.
void calcFrac() {

  // Multiplier is (2^(bit-width of counter))/(10^(number of decimal digits)
  //               2^16/10^4 = 65536/10000 = 0.152587890625
  fracSec = fracCounter * 0.152587890625;
}

// Interrupt service routine for the square wave pin change
ISR (PCINT1_vect) {

  // 32kHz square wave changes at 64kHz (actually 65536)
  //  When this rolls over to 0, another second has passed.
  fracCounter++;
}

// Do this first.
void setup () {

  // Start the lcd library.
  lcd.begin(16, 2);

  // Set the SQW (squarewave) pin to digital input mode.
  pinMode(SQW, INPUT);

  // Enable the pull-up resistor for SQW pin.
  digitalWrite(SQW, HIGH);

  // Enable the square wave output.
  rtc.writeSqwPinMode(SquareWave32kHz);

  // Set the LIGHT_SENSOR pin to analog input mode.
  pinMode(LIGHT_SENSOR, INPUT);

  // Enable the pin change interrupt for the bank.
  PCICR  |= bit (digitalPinToPCICRbit(SQW));

  // Move to the begining of the first line.
  lcd.setCursor(0,0);
  lcd.print("Ready,  Lvl:");
 
  // Update the displayed elapsed time.
  displayTimeBase();
  displayTimeFrac();
}

// This method loops continuously after setup() completes.
void loop () {

  // Measure the current light level.
  currentLevel = analogRead(LIGHT_SENSOR);

  // What is the current state?
  switch (timerState) {

    // After timer initialization, and before the timer starts
    case READY:

      // The light level is too dark to start the timer.
      if (currentLevel >= START_LEVEL) {

        // Display the light level.
        displayLevel();
      }

      // The light level is bright enough to start the timer.
      else {
 
        // Clear any outstanding pin change interrupts.
        PCIFR  |= bit (digitalPinToPCICRbit(SQW));
 
        // Enable the square wave pin interrupt to start the timer.
        *digitalPinToPCMSK(SQW) |= bit (digitalPinToPCMSKbit(SQW));
 
        // Move to the begining of the first line.
        lcd.setCursor(0,0);
        lcd.print("Timing,");

        // Transition the state machine.
        timerState = TIMING;
      }
      break;

    // While the time is elapsing
    case TIMING:

      // The light level is too bright to stop the timer.
      if (currentLevel < STOP_LEVEL) {

        // Calculate the fractional portion of the time.
        calcFrac();

        // A new second has started.
        if (fracSec < lastFrac) {

          // Display the light level.
          displayLevel();

          // Increment to the new second.
          incrementSec();

          // Update the displayed elapsed time.
          displayTimeBase();
        }

        // Update the fractional second portion of the displayed elapsed time.
        displayTimeFrac();
      }

      // The light level is dim enough to stop the timer.
      else {

        // Disable the square wave pin interrupt to stop the timer.
        *digitalPinToPCMSK(SQW) &= ~(bit (digitalPinToPCMSKbit(SQW)));
 
        // Calculate the fractional portion of the time.
        calcFrac();
 
        // Update the displayed elapsed time.
        displayTimeBase();
        displayTimeFrac();

        // Move to the begining of the first line.
        lcd.setCursor(0,0);
        lcd.print("Done,   Lvl:");

        // Transition the state machine.
        timerState = DONE;
      }
      break;

    // After the timer is stopped
    case DONE: // This is a dead-end state. Device must be reset to take another reading.

      // The following is a hack to display the light level only once per second.

      // Enable the square wave pin interrupt to start the timer.
      //  Note that this won't change the display.
      *digitalPinToPCMSK(SQW) |= bit (digitalPinToPCMSKbit(SQW));
 
      // Calculate the fractional portion of the time.
      calcFrac();

      // A new second has started.
      if (fracSec < lastFrac) {

        // Display the light level.
        displayLevel();
      }

      // Remember this time for the next pass.
      lastFrac = fracSec;
      break;
  }
}
