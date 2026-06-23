//*******************************************************************
//
// HBW-IO-12-FM
//
// Homematic Wired Hombrew Hardware
// 12 Kanäle, jeder einzeln per CCU als INPUT (Taster/Schalter) oder
// OUTPUT (Schalter mit erweitertem Peering) konfigurierbar.
//
// Hardware: https://github.com/maxx3105/HBW-IO-12-FM
// Library:  https://github.com/maxx3105/HBWired (Fork)
// XML:      hmw_io_12_fm.xml (HMW_DEVICETYPE = 0x1B / 27 dec)
//
// Das EEPROM-Layout dieses Sketches entspricht 1:1 der XML-Beschreibung:
//   0x01           logging_time
//   0x02 - 0x05    central_address
//   0x06 bit0      direct_link_deactivate
//   0x07 - 0x08    BEHAVIOUR-Bits (12 Bits, 1=OUTPUT, 0=INPUT)
//   0x09 - 0x20    Input-Channel-Konfig (12 × 2 Byte)
//   0x21 - 0x38    Output-Channel-Konfig (12 × 2 Byte)
//   0x39 ...       Output-Peerings  (28 × 28 Byte) -> hmw_switch_ch_link
//                  (Original-HM-Format, 2-Byte-Zeiten -> HBWSwitchHM/HBWLinkSwitchHM)
//   0x349 ...      Input-Peerings   (30 × 6 Byte)  -> hmw_input_ch_link
//
//*******************************************************************

#define HARDWARE_VERSION 0x00   // muss = supported_types index1 im XML (=0)! Sonst HMW-Generic
#define FIRMWARE_VERSION 0x0300 // meldet 3.00 wie das Original -> CCU zeigt "aktuell", kein Update-Angebot
#define HMW_DEVICETYPE   0x1B   // 27 dec = supported_types index0 im XML (hmw_io_12_fm.xml)

#define NUM_CHANNELS     12

// Output-Peerings: hmw_switch_ch_link, count=28, address_step=28, address_start=0x39
#define NUM_LINKS_OUT          28
#define LINKADDRESSSTART_OUT   0x0039

// Input-Peerings: hmw_input_ch_link, count=30, address_step=6, address_start=0x349
#define NUM_LINKS_IN           30
#define LINKADDRESSSTART_IN    0x0349


// HBWired Core + benötigte Channel-/Link-Klassen
#include <HBWired.h>
#include <HBWSwitchAdvanced.h>
#include <HBWLinkKey.h>
#include <HBW_eeprom.h>
#include <EEPROM.h>

#include "HBWChanIn.h"
#include "HBWSwitchHM.h"        // Output-Channel im Original-28-Byte-HM-Peering-Format
#include "HBWLinkSwitchHM.h"    // passender 28-Byte Link-Receiver

// Pin- und Hardware-Konfiguration (anpassen, falls nötig)
#include "HBW-IO-12-FM_config.h"


// HBWired 1.2.0 definiert POWERSAVE() nur für ATmega328P/PB und RP2040.
// Für ATmega32(A)/644P/1284P liefern wir einen identischen Fallback nach
// (Sleep-Mode 0 = IDLE -- millis()/Timer laufen weiter).
#ifndef POWERSAVE
  #if defined(__AVR__)
    #include <avr/sleep.h>
    #define POWERSAVE() do { set_sleep_mode(SLEEP_MODE_IDLE); sleep_mode(); } while (0)
  #else
    #define POWERSAVE() do { } while (0)
  #endif
#endif


// ---- EEPROM-Strukturen exakt nach XML ----------------------------------------
// hbw_config_io_in (Input-Modus, 2 Byte/Kanal ab 0x09) ist in HBWChanIn.h definiert.

// 2 Byte pro Output-Kanal, address_step=2 ab 0x21:
//   byte 0 bit 0 : LOGGING (0 = OFF, 1 = ON)
//   byte 1       : reserviert
// Wir verwenden HBWSwitchAdvanced - dessen hbw_config_switch nutzt
// die ersten Bits identisch (logging:1, output_unlocked:1, n_inverted:1).
// Bits 1 und 2 sind in der XML nicht definiert und bleiben nach Reset=1
// (= unlocked, not inverted), genau das gewünschte Default-Verhalten.

struct hbw_config {
  uint8_t  logging_time;                    // 0x01
  uint32_t central_address;                 // 0x02 - 0x05
  uint8_t  direct_link_deactivate : 1;      // 0x06 bit 0
  uint8_t                         : 7;
  uint16_t behaviour;                       // 0x07 - 0x08, bit pro Kanal
  hbw_config_io_in  inCfg [NUM_CHANNELS];   // 0x09 - 0x20
  hbw_config_switch outCfg[NUM_CHANNELS];   // 0x21 - 0x38
} hbwconfig;


// ---- Channel-Array -----------------------------------------------------------

HBWChannel* channels[NUM_CHANNELS];


// ---- BEHAVIOUR-Auto-Reset ----------------------------------------------------
// Die BEHAVIOUR-Bits (INPUT/OUTPUT je Kanal) werden NUR beim Boot in die
// Kanal-Objekte (HBWChanIn vs. HBWSwitchHM) umgesetzt. Aendert die CCU sie zur
// Laufzeit, muss das Geraet neu booten, damit die Kanaele mit dem neuen Typ neu
// entstehen. -> Boot-Wert merken, Aenderung in afterReadConfig() erkennen, in
// loop() entprellt resetten (erst wenn die CCU-Config-Sequenz zur Ruhe kam).
#define BEHAVIOUR_RESET_DELAY_MS 3000
uint16_t g_bootBehaviour    = 0;
bool     g_behaviourChanged = false;
uint32_t g_lastCfgMillis    = 0;


// ---- Device-Subklasse mit afterReadConfig() ---------------------------------

class HBWIODevice : public HBWDevice {
  public:
    HBWIODevice(uint8_t _devicetype, uint8_t _hardware_version, uint16_t _firmware_version,
                Stream* _rs485, uint8_t _txen,
                uint8_t _configSize, void* _config,
                uint8_t _numChannels, HBWChannel** _channels,
                Stream* _debugstream,
                HBWLinkSender* _ls = NULL, HBWLinkReceiver* _lr = NULL)
      : HBWDevice(_devicetype, _hardware_version, _firmware_version,
                  _rs485, _txen, _configSize, _config, _numChannels, _channels,
                  _debugstream, _ls, _lr) { };

    virtual void afterReadConfig() {
      // gerätespezifische Defaults nach EEPROM-Read
      // XML-Default LOGGING_TIME = 2.0 s (factor 10 -> Geräte-Wert 20)
      if (hbwconfig.logging_time == 0xFF) hbwconfig.logging_time = 20;

      // Wird nach JEDEM Config-(Re-)Read aufgerufen. BEHAVIOUR-Aenderung der CCU
      // erkennen; 0xFFFF (frisch) zaehlt wie 0x0000 -- identisch zur setup()-Logik.
      uint16_t cur = (hbwconfig.behaviour == 0xFFFF) ? 0x0000 : hbwconfig.behaviour;
      g_behaviourChanged = (cur != g_bootBehaviour);
      g_lastCfgMillis = millis();   // Entprell-Zeitstempel (Reset erst nach Ruhe, siehe loop())
    };
};

HBWIODevice* device = NULL;


// ---- Pinout-Array (siehe HBW-IO-12-FM_config.h) -----------------------------
static const uint8_t ioPin[NUM_CHANNELS] = {
  IO1, IO2, IO3, IO4, IO5, IO6, IO7, IO8, IO9, IO10, IO11, IO12
};


void setup()
{
  // BEHAVIOUR-Bits direkt aus dem internen EEPROM lesen, BEVOR die Channels
  // erzeugt werden. Frisch geflashte Geräte (alle 0xFF) als "alle INPUT"
  // interpretieren -- entspricht dem XML-Default (option default="true" id="INPUT").
  uint16_t behaviour = (uint16_t)EEPROM.read(7) | ((uint16_t)EEPROM.read(8) << 8);
  if (behaviour == 0xFFFF) behaviour = 0x0000;
  g_bootBehaviour = behaviour;   // Referenz fuer den BEHAVIOUR-Auto-Reset

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    bool isOutput = ((behaviour >> i) & 0x01) != 0;
    if (isOutput) {
      channels[i] = new HBWSwitchHM(ioPin[i], &(hbwconfig.outCfg[i]));
    } else {
      channels[i] = new HBWChanIn(ioPin[i], &(hbwconfig.inCfg[i]));
    }
  }

  HBW_BEGIN_SERIALS();   // baud-init je MCU/Konfig (siehe Config-Header)

  device = new HBWIODevice(HMW_DEVICETYPE, HARDWARE_VERSION, FIRMWARE_VERSION,
                           &HBW_RS485, RS485_TXEN,
                           sizeof(hbwconfig), &hbwconfig,
                           NUM_CHANNELS, channels,
                           HBW_DEBUGSTREAM,
                           new HBWLinkKey<NUM_LINKS_IN, LINKADDRESSSTART_IN>(),
                           new HBWLinkSwitchHM<NUM_LINKS_OUT, LINKADDRESSSTART_OUT>());

  device->setConfigPins(BUTTON, LED);

  // hbwdebug() ist intern NULL-safe -- wenn HBW_DEBUGSTREAM == NULL ist
  // (z.B. ATmega328P/32A mit USE_HARDWARE_SERIAL), wird hier nichts ausgegeben.
  hbwdebug(F("HBW-IO-12-FM, behaviour=0x"));
  hbwdebug(behaviour, HEX);
  hbwdebug(F(", freeRam="));
  hbwdebug(freeRam());
  hbwdebug(F("\n"));
}


void loop()
{
  device->loop();

  // Auto-Reset nach BEHAVIOUR-Aenderung (INPUT<->OUTPUT). Entprellt: erst wenn
  // seit dem letzten Config-Write Ruhe ist, damit eine mehrteilige CCU-Sequenz
  // nicht mittendrin abgebrochen wird. Reset = HBWireds Watchdog (Support_WDT,
  // 1 s) nicht mehr fuettern -> sauberer Hardware-Reset, Kanaele entstehen neu.
  if (g_behaviourChanged && (uint32_t)(millis() - g_lastCfgMillis) > BEHAVIOUR_RESET_DELAY_MS) {
    hbwdebug(F("BEHAVIOUR geaendert -> Reset\n"));
    while (1) { }   // Watchdog loest in <=1 s den Reset aus (kein eigenes WDT-Re-Enable -> kein Boot-Loop)
  }

  POWERSAVE();
}
