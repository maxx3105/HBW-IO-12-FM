/* Pin- und Hardware-Konfiguration für HBW-IO-12-FM
 *
 * Unterstützte Mikrocontroller (jeweils im Standard-Pinout der gängigen Cores):
 *   - ATmega328P     (Arduino IDE / "Arduino Nano/Uno")
 *   - ATmega32(A)    (MightyCore "Standard" pinout)        <-- HW-Ziel HBW-IO-12-FM
 *   - ATmega644P(A)  (MightyCore "Standard" pinout)
 *   - ATmega1284P    (MightyCore "Standard" pinout)
 *
 * MightyCore-Standard-Pinout (32/644/1284):
 *   PB0..PB7 = 0..7    (LED_BUILTIN = 7)
 *   PD0..PD7 = 8..15   (8=RX0, 9=TX0, 10=RX1, 11=TX1 -- nur 644P/1284P)
 *   PC0..PC7 = 16..23
 *   PA0..PA7 = 24..31  (= A0..A7)
 *
 * WICHTIG: Die Pin-Zuordnung der 12 IOs muss gegen den Schaltplan der
 * konkreten Platine geprüft werden!
 *   https://github.com/maxx3105/HBW-IO-12-FM/blob/main/HBW-IO-12-FM.pdf
 */

#ifndef HBW_IO_12_FM_CONFIG_H
#define HBW_IO_12_FM_CONFIG_H

#include <Arduino.h>
#include <EEPROM.h>
#include "FreeRam.h"

EEPROMClass* EepromPtr = &EEPROM;   // internes EEPROM

// ============================================================================
// ATmega328P  (Arduino Nano / Pro Mini / Uno -- ein UART)
// ============================================================================
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__)

  // RS485 standardmäßig per SoftwareSerial, UART0 für USB-Debug.
  // Definiere USE_HARDWARE_SERIAL (z.B. via -DUSE_HARDWARE_SERIAL), um
  // RS485 auf den Hardware-UART zu legen (dann kein Debug verfügbar).

  #define IO1   A3
  #define IO2   10
  #define IO3   11
  #define IO4   A0
  #define IO5   A1
  #define IO6   A2
  #define IO7   A4
  #define IO8   A5
  #define IO9    9
  #define IO10   7
  #define IO11   6
  #define IO12   5

  #define LED   31

  #ifdef USE_HARDWARE_SERIAL
    #define RS485_TXEN  2
    #define BUTTON      30
    #define HBW_RS485       (Serial)
    #define HBW_DEBUGSTREAM (NULL)
    #define HBW_BEGIN_SERIALS() do { Serial.begin(19200, SERIAL_8E1); } while (0)
  #else
    #define RS485_RXD   4
    #define RS485_TXD   2
    #define RS485_TXEN  3
    #define BUTTON      8

    #include <HBWSoftwareSerial.h>
    HBWSoftwareSerial rs485(RS485_RXD, RS485_TXD);
    #define HBW_RS485       (rs485)
    #define HBW_DEBUGSTREAM (&Serial)
    #define HBW_BEGIN_SERIALS() do { Serial.begin(115200); rs485.begin(19200); } while (0)
  #endif

// ============================================================================
// ATmega32 / ATmega32A  (MightyCore -- ein UART, kein Serial1)
// ============================================================================
#elif defined(__AVR_ATmega32__) || defined(__AVR_ATmega32A__)

  // Der 32A hat nur EINEN Hardware-UART und KEINE Pin-Change-Interrupts
  // (HBWSoftwareSerial = INT_ONLY; RX braeuchte INT0/1/2). SoftwareSerial-RS485
  // funktioniert hier nicht zuverlaessig -> RS485 MUSS auf den Hardware-UART.
  // Folge: kein Geraete-Debug-Serial (der UART IST der Bus) -> mitlesen ueber
  // das Gateway-BUS-RX-Log bzw. den CCU-Posteingang.

  // -- TMSTAMP onboard (fix) --
  #define LED          31       // PA7  (onboard LED)
  #define BUTTON       30       // PA6  (onboard Config-Taster)

  // -- RS485 auf Hardware-UART:  RO -> Pin 8 (PD0/RX0),  DI -> Pin 9 (PD1/TX0) --
  #define RS485_TXEN   2        // PB2  -> Transceiver DE (+ /RE zusammen)
  #define HBW_RS485       (Serial)
  #define HBW_DEBUGSTREAM (NULL)
  #define HBW_BEGIN_SERIALS() do { Serial.begin(19200, SERIAL_8E1); } while (0)

  // 12 IOs (Arduino/MightyCore-Pin-Nummern) -- bewusst weg von UART(8/9),
  // TXEN(2), LED/Taster(30/31), JTAG(18-21) und SPI(4-7):
  #define IO1   24    // PA0
  #define IO2   25    // PA1
  #define IO3   26    // PA2
  #define IO4   27    // PA3
  #define IO5   28    // PA4
  #define IO6   29    // PA5
  #define IO7   10    // PD2
  #define IO8   11    // PD3
  #define IO9   12    // PD4
  #define IO10  13    // PD5
  #define IO11  16    // PC0
  #define IO12  17    // PC1

// ============================================================================
// ATmega644P(A) / ATmega1284P  (MightyCore -- zwei UARTs)
// ============================================================================
#elif defined(__AVR_ATmega644P__)  || defined(__AVR_ATmega644PA__) \
   || defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega1284__)

  // TMSTAMP-1284: LED=PA7 und Config-Taster=PA6 sind ONBOARD belegt,
  // daher duerfen die 12 IOs PA6/PA7 NICHT nutzen -> IO7/IO8 auf PC4/PC5 verlegt.
  // !! Pin<->Kanal-Zuordnung gegen den TMSTAMP-Carrier-Schaltplan pruefen !!
  // 12 IOs: PA0..PA5 (24..29) + PC0..PC5 (16..21)
  #define IO1   24    // PA0
  #define IO2   25    // PA1
  #define IO3   26    // PA2
  #define IO4   27    // PA3
  #define IO5   28    // PA4
  #define IO6   29    // PA5
  #define IO7   20    // PC4   (war PA6, jetzt Config-Taster)
  #define IO8   21    // PC5   (war PA7, jetzt LED)
  #define IO9   16    // PC0
  #define IO10  17    // PC1
  #define IO11  18    // PC2
  #define IO12  19    // PC3

  #define LED   31              // PA7  (TMSTAMP onboard LED)
  #define BUTTON       30       // PA6  (TMSTAMP onboard Config-Taster)

  // RS485 fest auf zweitem UART (Serial1: PD2=RX1, PD3=TX1), Debug auf Serial.
  #define RS485_TXEN   2        // PB2
  #define HBW_RS485       (Serial1)
  #define HBW_DEBUGSTREAM (&Serial)
  #define HBW_BEGIN_SERIALS() do { Serial.begin(115200); Serial1.begin(19200, SERIAL_8E1); } while (0)

#else
  #error "HBW-IO-12-FM: Nicht unterstuetzter Mikrocontroller. Erlaubt: ATmega328P, ATmega32(A), ATmega644P(A), ATmega1284P."
#endif

#endif  // HBW_IO_12_FM_CONFIG_H
