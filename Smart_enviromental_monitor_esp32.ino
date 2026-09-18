




/* BREADBOARD DIAGNOSTIC EDITION
 * Overrides the historical PCB duty-cycle description below:
 * No light sleep or power cycling. GPIO25 stays HIGH as an enable signal.
 * Sensors must have their proper supply; GPIO25 is NOT a sensor supply.
 * Reads every 3 seconds; original pins, calibration and classes preserved.
 * DHT_TYPE remains DHT11: use DHT22 only if that is your actual sensor.
 * Invalid DHT samples are logged, not uploaded as fake zero measurements.
 * Low-water protection runs in BOTH Auto and Manual modes.
 * Fill the four credential placeholders before uploading.
 * Source-reviewed only; hardware and Arduino compilation not verified here.
 */
/*
=======================================================================
        SMART ENVIRONMENTAL MONITOR & CONTROLLER
                    ESP32 PORT
=======================================================================

This is a port of an Arduino Uno (ATmega328P) sketch to ESP32.

WHAT CHANGED AND WHY
=======================================================================

1) VOLTAGE LEVELS - READ THIS FIRST
   ------------------------------------------------------------
   ESP32 GPIOs are 3.3V logic and are NOT 5V tolerant.

   The original Uno design assumed 5V sensors/modules. On ESP32:

   - DHT11: use a 3.3V-compatible module, or power it from 3.3V.
     Many "DHT11 module" boards (with the onboard pull-up resistor)
     work fine at 3.3V.
   - Soil moisture sensor / water level sensor AO outputs: power
     these sensors from 3.3V, NOT 5V. If a sensor's analog output
     swings up to its supply voltage, feeding 5V into an ESP32 ADC
     pin can damage it. If you must run a sensor at 5V for
     accuracy, use a resistor-divider or logic-level shifter on
     its analog output before it reaches the ESP32.
   - ULN2003 stepper driver and relay module logic inputs: most
     ULN2003 boards and relay modules accept 3.3V logic HIGH just
     fine (check your specific module's datasheet). The STEPPER
     MOTOR itself and the RELAY COIL can still be powered from a
     separate 5V supply - only the *signal/logic* pins need to be
     3.3V-safe.
   - Pump relay: relay module logic input accepts 3.3V fine on most
     boards (check yours); the pump itself is powered/switched on
     the relay's separate COM/NO contacts, not from an ESP32 pin.

   ALL GROUNDS MUST STILL SHARE A COMMON GROUND (ESP32 GND, sensor
   GND, external 5V supply GND, ULN2003 GND, relay GND).

2) PIN MAPPING
   ------------------------------------------------------------
   Analog sensors were moved to ADC1-only pins (GPIO32-39).
   ADC2 pins are unusable for analogRead() while Wi-Fi is active
   on ESP32, so ADC1 is used here even if you're not using Wi-Fi,
   to keep the code portable.

   Pins were also chosen to avoid ESP32 "strapping" pins
   (GPIO0, 2, 5, 12, 15) which affect boot mode and can misbehave
   if driven by external circuitry during reset.

3) POWER-GATED DUTY CYCLE (replaces the old free-running loop)
   ------------------------------------------------------------
   This board is now behind an external power-management circuit
   that only keeps the rest of the electronics powered while
   POWER_ENABLE_PIN (GPIO25) is driven HIGH. Because of that, the
   old "free-running loop() + 2-second hardware timer interrupt"
   design (originally ported from the Uno's Timer2) no longer
   applies - there's no point servicing a background timer while
   the CPU is about to go to light sleep anyway.

   loop() now runs one full duty cycle sequentially, every time:

     POWER_ENABLE_PIN HIGH -> wait 2s -> read sensors / command
     actuators -> wait 2s (letting the stepper finish moving) ->
     POWER_ENABLE_PIN LOW -> light sleep 20s -> repeat.

   See the POWER-GATE / DUTY-CYCLE SETTINGS block and loop() near
   the bottom of this file for the implementation.

4) THE WATER-LEVEL "ADC CONVERSION COMPLETE INTERRUPT"
   ------------------------------------------------------------
   The Uno code manually fired off an ADC conversion and used the
   ATmega's ADC_vect interrupt to grab the result without blocking,
   because analogRead() on AVR is comparatively slow (~100us) and
   the author wanted to demonstrate ISR-based non-blocking ADC.

   ESP32's Arduino core doesn't expose an equivalent single-shot
   "conversion complete" interrupt through a simple API (doing so
   requires the lower-level esp_adc/adc_continuous driver, which
   is overkill here). ESP32's analogRead() is fast enough (tens of
   microseconds) that this port just reads it synchronously, in
   the same place in the sensor cycle as the soil and light
   sensors. The overall non-blocking loop() architecture, the
   hysteresis logic, and the Observer pattern are unchanged -
   only the "wait for an interrupt flag" plumbing was removed
   because it's no longer needed.

5) IRRIGATION ACTUATOR: PUMP RELAY, NOT A SERVO
   ------------------------------------------------------------
   The actual hardware uses a 2-terminal water pump switched by a
   relay (H7 on the schematic), not a servo-actuated valve. The
   ESP32Servo library and the ServoValve class have been removed;
   GPIO13 now drives a plain relay the same way GPIO26 drives the
   grow-light relay (see the PumpRelay class below).

6) PROGMEM
   ------------------------------------------------------------
   ESP32's toolchain supports PROGMEM and pgm_read_byte() for
   source compatibility, even though ESP32 flash is
   memory-mapped and doesn't need the AVR-style separate address
   space trick. The half-step sequence table is kept as-is.

REQUIRED LIBRARIES (Library Manager)
=======================================================================
   - DHT sensor library   (Adafruit)
   - Adafruit Unified Sensor (dependency of the DHT library)
   - Adafruit GFX Library
   - Adafruit SSD1306

=======================================================================
HARDWARE CONNECTIONS (ESP32 DevKit v1 pin numbering)
=======================================================================

DHT11
   VCC  -> 3.3V
   GND  -> GND
   DATA -> GPIO4  (use a 4.7k-10k pull-up to 3.3V if using a bare
                    sensor rather than a breakout module)

SOIL MOISTURE SENSOR
   VCC -> 3.3V   (NOT 5V - see voltage note above)
   GND -> GND
   AO  -> GPIO34 (ADC1_CH6, input-only pin, fine for analogRead)

LDR
   5V/3.3V -- LDR -- GPIO35 -- 10k resistor -- GND
   Use 3.3V as the top rail so the ADC never sees more than 3.3V.
   GPIO35 = ADC1_CH7 (input-only pin).

WATER LEVEL SENSOR
   VCC -> 3.3V   (NOT 5V)
   GND -> GND
   AO  -> GPIO32 (ADC1_CH4)

STEPPER MOTOR + ULN2003 (vent)
   GPIO16 -> ULN2003 IN1
   GPIO17 -> ULN2003 IN2
   GPIO18 -> ULN2003 IN3
   GPIO19 -> ULN2003 IN4
   ULN2003 GND -> common GND
   ULN2003 COM -> motor's positive supply rail
   Stepper motor supply: external regulated 5V supply, NOT an
   ESP32 pin. No limit switch in this design - the vent is assumed
   to start physically CLOSED at power-up, same as the original.

PUMP RELAY (irrigation, H7 on schematic)
   Relay IN  -> GPIO13
   Relay VCC -> 3.3V or 5V depending on your relay module's logic
   Relay GND -> common GND
   Pump itself is powered/switched on the relay's COM/NO contacts,
   not from an ESP32 pin. Assumes ACTIVE-LOW relay module by
   default - flip PUMP_RELAY_ACTIVE_LOW below if yours is
   active-HIGH.

RELAY / GROW LIGHT
   Relay IN  -> GPIO26
   Relay VCC -> 3.3V or 5V depending on your relay module's logic
                (most opto-isolated relay boards accept 3.3V logic
                fine even when the board itself is powered at 5V -
                check your module)
   Relay GND -> common GND
   Assumes ACTIVE-LOW relay module by default - flip
   RELAY_ACTIVE_LOW below if yours is active-HIGH.

LOW-WATER / REFILL LED
   GPIO27 -> 220-330 ohm resistor -> LED -> GND

POWER-GATE HANDSHAKE (new power-management board, H5 Master Relay)
   GPIO25 -> power-management circuit's "enable" input
   NOTE: this is GPIO25, not GPIO13. GPIO13 is the separate H7 pump
   relay above - don't confuse the two.
   HIGH = keep the rest of the circuit powered.
   LOW  = safe to cut power to the rest of the circuit.
   ESP32 itself must stay powered independently of this signal
   (it is only gating the sensors/actuators/peripherals), and it
   drives GPIO25 directly through the duty cycle in loop().

OLED SSD1306 (I2C)
   VCC -> 3.3V
   GND -> GND
   SDA -> GPIO21 (ESP32 default I2C SDA)
   SCL -> GPIO22 (ESP32 default I2C SCL)
   Address 0x3C (some modules use 0x3D)

=======================================================================
*/


// ====================================================================
// LIBRARIES
// ====================================================================
// ====================================================================
// BLYNK CONFIGURATION
// ====================================================================

#define BLYNK_TEMPLATE_ID "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "smart green house"
#define BLYNK_AUTH_TOKEN "YOUR_AUTH_TOKEN"
#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <pgmspace.h>
#include <esp_sleep.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>


// ====================================================================
// PIN DEFINITIONS
// ====================================================================

// Sensors (ADC1-only pins so they still work if Wi-Fi is used)
const byte SOIL_MOISTURE_PIN = 34;   // ADC1_CH6 - analog, matches AO output

/*
TODO: pin-mapping doc says GPIO35 (ADC1_CH7), but the LDR schematic you
shared shows it wired to GPIO33 (also ADC1, ADC1_CH5) instead. Both are
valid ADC1 pins, but only one matches your actual board. Confirm which
one is really wired and fix whichever side (doc or this line) is wrong.
*/
const byte LDR_PIN           = 35;   // ADC1_CH7 (per doc) - VERIFY vs GPIO33
const byte WATER_LEVEL_PIN   = 32;   // ADC1_CH4
const byte DHT_PIN           = 4;

// Outputs
const byte RELAY_PIN         = 26;   // Grow light relay
const byte REFILL_LED_PIN    = 27;
const byte PUMP_RELAY_PIN    = 13;   // Irrigation pump relay (H7 on schematic)

/*
Power-management handshake pin - this IS the "master relay" (H5 on the
schematic). It is GPIO25, NOT GPIO13 - GPIO13 is the separate pump relay
(H7) that switches the irrigation pump on/off. Don't mix these two up:
GPIO25 gates power to the whole board, GPIO13 only switches the pump.

HIGH = "please keep the rest of the circuit powered", LOW = "safe to cut power".
loop() below already drives this pin through exactly the sequence you
described: HIGH -> wait 2s -> read sensors / command actuators -> wait 2s
-> LOW -> sleep.
*/
const byte POWER_ENABLE_PIN  = 25;

const byte STEPPER_IN1       = 16;
const byte STEPPER_IN2       = 17;
const byte STEPPER_IN3       = 18;
const byte STEPPER_IN4       = 19;


// OLED:
// SDA = GPIO21 (default)
// SCL = GPIO22 (default)


// ====================================================================
// DHT
// ====================================================================

#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);


// ====================================================================
// OLED
// ====================================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

const byte OLED_I2C_ADDRESS = 0x3C;

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

bool oledAvailable = false;


// ====================================================================
// SENSOR CALIBRATION
// ====================================================================

/*
These are starting/example values, carried over unchanged from the
Uno version because analogReadResolution(10) below keeps the ESP32
ADC range at 0-1023, matching the original.

You MUST still calibrate them using Serial Monitor for your actual
sensors and supply voltage.
*/

// Soil sensor:
// Assumption:
// Dry = high ADC
// Wet = low ADC

const int SOIL_DRY_ADC = 850;
const int SOIL_WET_ADC = 350;


// LDR:
// With:
// 3.3V -> LDR -> A pin -> 10k -> GND

const int LDR_DARK_ADC   = 50;
const int LDR_BRIGHT_ADC = 900;


// Water-level sensor

const int WATER_EMPTY_ADC = 100;
const int WATER_FULL_ADC  = 700;


// ====================================================================
// CONTROL THRESHOLDS
// ====================================================================

// ------------------------------------------------------------
// Temperature -> Vent
// ------------------------------------------------------------

const float TEMP_OPEN_THRESHOLD  = 30.0;
const float TEMP_CLOSE_THRESHOLD = 28.0;


// ------------------------------------------------------------
// Soil -> Irrigation
// ------------------------------------------------------------

const int SOIL_START_WATERING = 30;
const int SOIL_STOP_WATERING  = 45;


// ------------------------------------------------------------
// Light -> Grow light
// ------------------------------------------------------------

const int LIGHT_ON_THRESHOLD  = 30;
const int LIGHT_OFF_THRESHOLD = 45;


// ------------------------------------------------------------
// Water safety
// ------------------------------------------------------------

const int WATER_LOW_THRESHOLD     = 20;
const int WATER_RECOVER_THRESHOLD = 30;


// ====================================================================
// STEPPER SETTINGS
// ====================================================================

/*
For a common 28BYJ-48, the exact number depends on stepping mode and
mechanical gearing.

For a vent, this is NOT necessarily one complete revolution.

Change this until the vent travels exactly from CLOSED to OPEN.
*/

const long VENT_TRAVEL_STEPS = 1024;


// Approximately one half-step every 2 ms.
const unsigned long STEPPER_INTERVAL_MS = 2;


// ====================================================================
// RELAY SETTINGS
// ====================================================================

/*
Most common relay modules are active LOW.
If your relay behaves backwards, change this to false.
*/

const bool RELAY_ACTIVE_LOW      = true;   // grow light relay
const bool PUMP_RELAY_ACTIVE_LOW = true;   // pump relay - verify against your module


// ====================================================================
// ENVIRONMENTAL DATA STRUCTURE
// ====================================================================

struct EnvironmentData
{
  float temperature;
  float humidity;

  int soilRaw;
  int soilPercent;

  int lightRaw;
  int lightPercent;

  int waterRaw;
  int waterPercent;

  bool dhtValid;
  bool waterLow;
};


// ====================================================================
// OBSERVER BASE CLASS
// ====================================================================

class Observer
{
public:

  virtual void update(const EnvironmentData& data) = 0;

  virtual ~Observer()
  {
  }
};


// ====================================================================
// NON-BLOCKING STEPPER CLASS
// ====================================================================

/*
Half-step sequence.

Pin order:

IN1
IN2
IN3
IN4
*/

const byte HALF_STEP_SEQUENCE[8][4] PROGMEM =
{
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};


class StepperVent
{
private:

  byte pins[4];

  long currentPosition;
  long targetPosition;

  byte sequenceIndex;

  unsigned long previousStepMillis;

  bool coilsOn;


  void applySequence(byte sequence)
  {
    for (byte i = 0; i < 4; i++)
    {
      byte state =
        pgm_read_byte(
          &HALF_STEP_SEQUENCE[sequence][i]
        );

      digitalWrite(pins[i], state);
    }

    coilsOn = true;
  }


  void releaseMotor()
  {
    for (byte i = 0; i < 4; i++)
    {
      digitalWrite(pins[i], LOW);
    }

    coilsOn = false;
  }


public:

  StepperVent(
    byte pin1,
    byte pin2,
    byte pin3,
    byte pin4
  )
  {
    pins[0] = pin1;
    pins[1] = pin2;
    pins[2] = pin3;
    pins[3] = pin4;

    currentPosition = 0;
    targetPosition  = 0;

    sequenceIndex = 0;

    previousStepMillis = 0;

    coilsOn = false;
  }


  void begin()
  {
    for (byte i = 0; i < 4; i++)
    {
      pinMode(pins[i], OUTPUT);

      digitalWrite(
        pins[i],
        LOW
      );
    }


    /*
    ASSUMPTION:

    The vent is physically CLOSED at startup.
    */

    currentPosition = 0;
    targetPosition  = 0;
  }


  void openVent()
  {
    /*
    Prevent repeatedly commanding the already requested position.
    */

    if (targetPosition != VENT_TRAVEL_STEPS)
    {
      targetPosition = VENT_TRAVEL_STEPS;

      Serial.println(
        F("[VENT] Opening command received")
      );
    }
  }


  void closeVent()
  {
    if (targetPosition != 0)
    {
      targetPosition = 0;

      Serial.println(
        F("[VENT] Closing command received")
      );
    }
  }


  void update()
  {
    /*
    Non-blocking motor movement.

    loop() calls this repeatedly.
    */

    if (currentPosition == targetPosition)
    {
      if (coilsOn)
      {
        releaseMotor();
      }

      return;
    }


    unsigned long now = millis();


    if (
      now - previousStepMillis <
      STEPPER_INTERVAL_MS
    )
    {
      return;
    }


    previousStepMillis = now;


    int direction;


    if (targetPosition > currentPosition)
    {
      direction = 1;
    }
    else
    {
      direction = -1;
    }


    // Move sequence forwards
    if (direction > 0)
    {
      sequenceIndex++;

      if (sequenceIndex >= 8)
      {
        sequenceIndex = 0;
      }
    }

    // Move sequence backwards
    else
    {
      if (sequenceIndex == 0)
      {
        sequenceIndex = 7;
      }
      else
      {
        sequenceIndex--;
      }
    }


    applySequence(sequenceIndex);


    currentPosition += direction;


    if (currentPosition == targetPosition)
    {
      releaseMotor();


      if (currentPosition == 0)
      {
        Serial.println(
          F("[VENT] Vent is CLOSED")
        );
      }
      else
      {
        Serial.println(
          F("[VENT] Vent is OPEN")
        );
      }
    }
  }


  bool isOpen() const
  {
    return currentPosition ==
           VENT_TRAVEL_STEPS;
  }


  bool isClosed() const
  {
    return currentPosition == 0;
  }


  bool isMoving() const
  {
    return currentPosition !=
           targetPosition;
  }


  const char* getStatus() const
  {
    if (
      currentPosition ==
      VENT_TRAVEL_STEPS
    )
    {
      return "OPEN";
    }


    if (currentPosition == 0)
    {
      return "CLOSED";
    }


    if (
      targetPosition >
      currentPosition
    )
    {
      return "OPENING";
    }


    return "CLOSING";
  }
};


// ====================================================================
// PUMP RELAY CLASS
// ====================================================================

/*
The irrigation "valve" is actually a 2-terminal water pump switched by
a relay (H7 on the schematic) - not a servo-actuated valve. This class
is deliberately identical in shape to GrowLightRelay: a plain on/off
GPIO output that only writes hardware when the commanded state
actually changes, so it plays the same role the ServoValve used to
(same open()/close()/isOpen() interface used by MoistureObserver),
just implemented as a relay instead of a servo write.
*/

class PumpRelay
{
private:

  byte relayPin;

  bool activeLow;

  bool pumpOn;


  void writeHardware(bool turnOn)
  {
    if (activeLow)
    {
      digitalWrite(
        relayPin,
        turnOn ? LOW : HIGH
      );
    }
    else
    {
      digitalWrite(
        relayPin,
        turnOn ? HIGH : LOW
      );
    }
  }


public:

  PumpRelay(
    byte pin,
    bool isActiveLow
  )
  {
    relayPin = pin;

    activeLow = isActiveLow;

    pumpOn = false;
  }


  void begin()
  {
    pinMode(
      relayPin,
      OUTPUT
    );

    pumpOn = false;

    writeHardware(false);
  }


  void open()
  {
    /*
    Prevent repeatedly writing the same state.
    */

    if (!pumpOn)
    {
      pumpOn = true;

      writeHardware(true);

      Serial.println(
        F("[IRRIGATION] Pump ON")
      );
    }
  }


  void close()
  {
    if (pumpOn)
    {
      pumpOn = false;

      writeHardware(false);

      Serial.println(
        F("[IRRIGATION] Pump OFF")
      );
    }
  }


  bool isOpen() const
  {
    return pumpOn;
  }
};


// ====================================================================
// RELAY CLASS
// ====================================================================

class GrowLightRelay
{
private:

  byte relayPin;

  bool activeLow;

  bool lightOn;


  void writeHardware(bool turnOn)
  {
    if (activeLow)
    {
      digitalWrite(
        relayPin,
        turnOn ? LOW : HIGH
      );
    }
    else
    {
      digitalWrite(
        relayPin,
        turnOn ? HIGH : LOW
      );
    }
  }


public:

  GrowLightRelay(
    byte pin,
    bool isActiveLow
  )
  {
    relayPin = pin;

    activeLow = isActiveLow;

    lightOn = false;
  }


  void begin()
  {
    pinMode(
      relayPin,
      OUTPUT
    );

    lightOn = false;

    writeHardware(false);
  }


  void turnOn()
  {
    if (!lightOn)
    {
      lightOn = true;

      writeHardware(true);

      Serial.println(
        F("[LIGHT] Grow light ON")
      );
    }
  }


  void turnOff()
  {
    if (lightOn)
    {
      lightOn = false;

      writeHardware(false);

      Serial.println(
        F("[LIGHT] Grow light OFF")
      );
    }
  }


  bool isOn() const
  {
    return lightOn;
  }
};


// ====================================================================
// ACTUATOR OBJECTS
// ====================================================================

StepperVent vent(
  STEPPER_IN1,
  STEPPER_IN2,
  STEPPER_IN3,
  STEPPER_IN4
);


PumpRelay irrigationPump(
  PUMP_RELAY_PIN,
  PUMP_RELAY_ACTIVE_LOW
);


GrowLightRelay growLight(
  RELAY_PIN,
  RELAY_ACTIVE_LOW
);


// ====================================================================
// TEMPERATURE OBSERVER
// ====================================================================

class TempObserver : public Observer
{
private:

  StepperVent& vent;

  float openThreshold;
  float closeThreshold;


public:

  TempObserver(
    StepperVent& ventReference,
    float openTemp,
    float closeTemp
  )
    : vent(ventReference)
  {
    openThreshold  = openTemp;
    closeThreshold = closeTemp;
  }


  void update(
    const EnvironmentData& data
  ) override
  {
    if (!data.dhtValid)
    {
      return;
    }


    /*
    Temperature hysteresis:

    >= 30C -> open

    <= 28C -> close

    Between 28 and 30:
    maintain the previous vent state.
    */


    if (
      data.temperature >=
      openThreshold
    )
    {
      vent.openVent();
    }


    else if (
      data.temperature <=
      closeThreshold
    )
    {
      vent.closeVent();
    }
  }
};


// ====================================================================
// MOISTURE OBSERVER
// ====================================================================

class MoistureObserver : public Observer
{
private:

  PumpRelay& valve;

  int dryThreshold;
  int wetThreshold;


public:

  MoistureObserver(
    PumpRelay& valveReference,
    int startWatering,
    int stopWatering
  )
    : valve(valveReference)
  {
    dryThreshold = startWatering;
    wetThreshold = stopWatering;
  }


  void update(
    const EnvironmentData& data
  ) override
  {
    /*
    LOW WATER HAS THE HIGHEST PRIORITY.

    If reservoir is low, irrigation must stop regardless
    of soil moisture.
    */

    if (data.waterLow)
    {
      valve.close();

      return;
    }


    /*
    Soil moisture hysteresis:

    <= 30% -> start irrigation

    >= 45% -> stop irrigation

    31-44% -> keep previous state
    */


    if (
      data.soilPercent <=
      dryThreshold
    )
    {
      valve.open();
    }


    else if (
      data.soilPercent >=
      wetThreshold
    )
    {
      valve.close();
    }
  }
};


// ====================================================================
// LIGHT OBSERVER
// ====================================================================

class LightObserver : public Observer
{
private:

  GrowLightRelay& relay;

  int lightOnThreshold;
  int lightOffThreshold;


public:

  LightObserver(
    GrowLightRelay& relayReference,
    int turnOnAt,
    int turnOffAt
  )
    : relay(relayReference)
  {
    lightOnThreshold  = turnOnAt;
    lightOffThreshold = turnOffAt;
  }


  void update(
    const EnvironmentData& data
  ) override
  {
    /*
    Light hysteresis:

    <= 30% -> grow light ON

    >= 45% -> grow light OFF
    */


    if (
      data.lightPercent <=
      lightOnThreshold
    )
    {
      relay.turnOn();
    }


    else if (
      data.lightPercent >=
      lightOffThreshold
    )
    {
      relay.turnOff();
    }
  }
};


// ====================================================================
// SUBJECT CLASS
// ====================================================================

class EnvironmentSubject
{
private:

  EnvironmentData data;


  static const byte MAX_OBSERVERS = 5;


  Observer* observers[MAX_OBSERVERS];


  byte observerCount;


public:

  EnvironmentSubject()
  {
    observerCount = 0;


    data.temperature = 0.0;
    data.humidity    = 0.0;


    data.soilRaw     = 0;
    data.soilPercent = 0;


    data.lightRaw     = 0;
    data.lightPercent = 0;


    data.waterRaw     = 0;
    data.waterPercent = 0;


    data.dhtValid = false;
    data.waterLow = false;
  }


  bool attach(
    Observer* observer
  )
  {
    if (
      observerCount >=
      MAX_OBSERVERS
    )
    {
      return false;
    }


    observers[observerCount] =
      observer;


    observerCount++;


    return true;
  }


  void setDHT(
    float temperature,
    float humidity,
    bool valid
  )
  {
    if (valid)
    {
      data.temperature =
        temperature;


      data.humidity =
        humidity;
    }


    data.dhtValid =
      valid;
  }


  void setSoil(
    int rawValue,
    int percentValue
  )
  {
    data.soilRaw =
      rawValue;


    data.soilPercent =
      percentValue;
  }


  void setLight(
    int rawValue,
    int percentValue
  )
  {
    data.lightRaw =
      rawValue;


    data.lightPercent =
      percentValue;
  }


  void setWater(
    int rawValue,
    int percentValue,
    bool low
  )
  {
    data.waterRaw =
      rawValue;


    data.waterPercent =
      percentValue;


    data.waterLow =
      low;
  }


  const EnvironmentData&
  getData() const
  {
    return data;
  }


  void notifyObservers()
  {
    /*
    POLYMORPHISM HAPPENS HERE.

    All derived objects are stored and accessed through
    Observer pointers.
    */


    for (
      byte i = 0;
      i < observerCount;
      i++
    )
    {
      if (
        observers[i] != NULL
      )
      {
        observers[i]->
          update(data);
      }
    }
  }
};


// ====================================================================
// SUBJECT AND OBSERVERS
// ====================================================================

EnvironmentSubject environment;


TempObserver temperatureObserver(
  vent,
  TEMP_OPEN_THRESHOLD,
  TEMP_CLOSE_THRESHOLD
);


MoistureObserver moistureObserver(
  irrigationPump,
  SOIL_START_WATERING,
  SOIL_STOP_WATERING
);


LightObserver lightObserver(
  growLight,
  LIGHT_ON_THRESHOLD,
  LIGHT_OFF_THRESHOLD
);


// ====================================================================
// PROGRAM STATE
// ====================================================================

bool lowWaterState =
  false;
// ====================================================================
// BLYNK / WIFI STATE
// ====================================================================

char ssid[] =  "Giza-Creativa";
char pass[] =  "Cer@@cce$$44";

bool autoMode = true;
bool sensorSampleReady = false;
unsigned long lastSampleMillis = 0;
unsigned long lastNetworkAttempt = 0;
const unsigned long SAMPLE_INTERVAL_MS = 3000;

void publishBlynkState();
void serviceNetwork();

// ====================================================================
// ADC -> PERCENT HELPER
// ====================================================================

int convertToPercent(
  int raw,
  int minimumADC,
  int maximumADC
)
{
  long denominator =
    (long)maximumADC -
    minimumADC;


  if (denominator == 0)
  {
    return 0;
  }


  long value =
    (long)(raw - minimumADC)
    * 100L
    / denominator;


  if (value < 0)
  {
    value = 0;
  }


  if (value > 100)
  {
    value = 100;
  }


  return (int)value;
}


// ====================================================================
// SENSOR CYCLE
// ====================================================================

void runSensorCycle()
{
  // ==========================================================
  // DHT
  // ==========================================================


  float humidity =
    dht.readHumidity();


  float temperature =
    dht.readTemperature();


  bool dhtValid =
    !isnan(humidity) &&
    !isnan(temperature);


  environment.setDHT(
    temperature,
    humidity,
    dhtValid
  );


  // ==========================================================
  // SOIL
  // ==========================================================


  int soilRaw =
    analogRead(
      SOIL_MOISTURE_PIN
    );


  int soilPercent =
    convertToPercent(
      soilRaw,
      SOIL_DRY_ADC,
      SOIL_WET_ADC
    );


  environment.setSoil(
    soilRaw,
    soilPercent
  );


  // ==========================================================
  // LDR
  // ==========================================================


  int lightRaw =
    analogRead(
      LDR_PIN
    );


  int lightPercent =
    convertToPercent(
      lightRaw,
      LDR_DARK_ADC,
      LDR_BRIGHT_ADC
    );


  environment.setLight(
    lightRaw,
    lightPercent
  );


  // ==========================================================
  // WATER
  // ==========================================================

  /*
  Read directly - see header note (4) on why this no longer
  needs an ADC-complete interrupt on ESP32.
  */


  int waterRaw =
    analogRead(
      WATER_LEVEL_PIN
    );


  int waterPercent =
    convertToPercent(
      waterRaw,
      WATER_EMPTY_ADC,
      WATER_FULL_ADC
    );


  bool previousState =
    lowWaterState;


  /*
  HYSTERESIS:

  <= 20%:
  water becomes LOW.

  Once low, it must rise to >= 30%
  before recovering.
  */


  if (!lowWaterState)
  {
    if (
      waterPercent <=
      WATER_LOW_THRESHOLD
    )
    {
      lowWaterState =
        true;
    }
  }


  else
  {
    if (
      waterPercent >=
      WATER_RECOVER_THRESHOLD
    )
    {
      lowWaterState =
        false;
    }
  }


  environment.setWater(
    waterRaw,
    waterPercent,
    lowWaterState
  );


  // ==========================================================
  // LOW WATER LED
  // ==========================================================


  digitalWrite(
    REFILL_LED_PIN,
    lowWaterState
      ? HIGH
      : LOW
  );


  // ==========================================================
  // WATER LEVEL EVENT
  // ==========================================================


  if (
    previousState !=
    lowWaterState
  )
  {
    if (lowWaterState)
    {
      Serial.println();

      Serial.println(
        F("*** LOW WATER ALERT ***")
      );

      Serial.println(
        F("Irrigation disabled.")
      );
    }


    else
    {
      Serial.println();

      Serial.println(
        F("*** WATER LEVEL RECOVERED ***")
      );
    }
  }


  /*
  All new sensor values are now complete.

  Notify all observers.
  */


  sensorSampleReady = true;
  lastSampleMillis = millis();
  // Safety is independent of automatic control.
  if (lowWaterState) irrigationPump.close();
  if (autoMode) environment.notifyObservers();
  if (!dhtValid) Serial.println(F("[CHECK] DHT11 read failed on GPIO4."));
  if (soilRaw == 0 && lightRaw == 0 && waterRaw == 0)
    Serial.println(F("[CHECK] All ADC inputs are zero: verify sensor supply and signals on GPIO34/35/32."));


  updateOLED();


  printSystemStatus();
}


// ====================================================================
// OLED
// ====================================================================

void updateOLED()
{
  if (!oledAvailable)
  {
    return;
  }


  const EnvironmentData& data =
    environment.getData();


  display.clearDisplay();


  display.setTextSize(1);


  display.setTextColor(
    SSD1306_WHITE
  );


  display.setCursor(
    0,
    0
  );


  // ----------------------------------------------------------
  // Temperature and humidity
  // ----------------------------------------------------------


  if (data.dhtValid)
  {
    display.print(
      F("T:")
    );


    display.print(
      data.temperature,
      1
    );


    display.print(
      F("C H:")
    );


    display.print(
      data.humidity,
      0
    );


    display.println(
      F("%")
    );
  }


  else
  {
    display.println(
      F("DHT READ ERROR")
    );
  }


  // ----------------------------------------------------------
  // Soil
  // ----------------------------------------------------------


  display.print(
    F("Soil:")
  );


  display.print(
    data.soilPercent
  );


  display.print(
    F("% ")
  );


  display.println(
    data.soilRaw
  );


  // ----------------------------------------------------------
  // Light
  // ----------------------------------------------------------


  display.print(
    F("Light:")
  );


  display.print(
    data.lightPercent
  );


  display.print(
    F("% ")
  );


  display.println(
    data.lightRaw
  );


  // ----------------------------------------------------------
  // Water
  // ----------------------------------------------------------


  display.print(
    F("Water:")
  );


  display.print(
    data.waterPercent
  );


  display.print(
    F("% ")
  );


  display.println(
    data.waterRaw
  );


  // ----------------------------------------------------------
  // Vent
  // ----------------------------------------------------------


  display.print(
    F("Vent:")
  );


  display.println(
    vent.getStatus()
  );


  // ----------------------------------------------------------
  // Irrigation + grow light
  // ----------------------------------------------------------


  display.print(
    F("Valve:")
  );


  display.print(
    irrigationPump.isOpen()
      ? F("ON")
      : F("OFF")
  );


  display.print(
    F(" Light:")
  );


  display.println(
    growLight.isOn()
      ? F("ON")
      : F("OFF")
  );


  // ----------------------------------------------------------
  // Alert
  // ----------------------------------------------------------


  if (data.waterLow)
  {
    display.println(
      F("ALERT: LOW WATER!")
    );
  }


  else
  {
    display.println(
      F("Status: NORMAL")
    );
  }


  display.display();
}


// ====================================================================
// SERIAL MONITOR OUTPUT
// ====================================================================

void printSystemStatus()
{
  const EnvironmentData& data =
    environment.getData();


  Serial.println();


  Serial.println(
    F("========================================")
  );


  Serial.println(
    F("       SMART GREENHOUSE STATUS")
  );


  Serial.println(
    F("========================================")
  );


  Serial.print(
    F("Temperature : ")
  );


  if (data.dhtValid)
  {
    Serial.print(
      data.temperature,
      1
    );


    Serial.println(
      F(" C")
    );
  }


  else
  {
    Serial.println(
      F("ERROR")
    );
  }


  Serial.print(
    F("Humidity    : ")
  );


  if (data.dhtValid)
  {
    Serial.print(
      data.humidity,
      1
    );


    Serial.println(
      F(" %")
    );
  }


  else
  {
    Serial.println(
      F("ERROR")
    );
  }


  Serial.print(
    F("Soil raw    : ")
  );


  Serial.print(
    data.soilRaw
  );


  Serial.print(
    F("  -> ")
  );


  Serial.print(
    data.soilPercent
  );


  Serial.println(
    F(" %")
  );


  Serial.print(
    F("LDR raw     : ")
  );


  Serial.print(
    data.lightRaw
  );


  Serial.print(
    F("  -> ")
  );


  Serial.print(
    data.lightPercent
  );


  Serial.println(
    F(" %")
  );


  Serial.print(
    F("Water raw   : ")
  );


  Serial.print(
    data.waterRaw
  );


  Serial.print(
    F("  -> ")
  );


  Serial.print(
    data.waterPercent
  );


  Serial.println(
    F(" %")
  );


  Serial.print(
    F("Water state : ")
  );


  Serial.println(
    data.waterLow
      ? F("LOW")
      : F("OK")
  );


  Serial.print(
    F("Vent        : ")
  );


  Serial.println(
    vent.getStatus()
  );


  Serial.print(
    F("Valve       : ")
  );


  Serial.println(
    irrigationPump.isOpen()
      ? F("OPEN")
      : F("CLOSED")
  );


  Serial.print(
    F("Grow light  : ")
  );


  Serial.println(
    growLight.isOn()
      ? F("ON")
      : F("OFF")
  );


  Serial.println(
    F("========================================")
  );
}

// ====================================================================
// BLYNK CONTROL
// ====================================================================

BLYNK_CONNECTED()
{
  // Send the current Auto Mode state to Blynk
  Blynk.virtualWrite(V8, autoMode);
}


// Grow Light - V5
BLYNK_WRITE(V5)
{
  if (!autoMode)
  {
    int value = param.asInt();

    if (value == 1)
    {
      growLight.turnOn();
    }
    else
    {
      growLight.turnOff();
    }
  }
}


// Water Pump - V6
BLYNK_WRITE(V6)
{
  if (!autoMode)
  {
    int value = param.asInt();

    if (value == 1)
    {
      // Safety: do not run the pump if water level is low
      if (sensorSampleReady && millis() - lastSampleMillis < 6000UL && !environment.getData().waterLow)
      {
        irrigationPump.open();
      }
      else
      {
        irrigationPump.close();
        Blynk.virtualWrite(V6, 0);
      }
    }
    else
    {
      irrigationPump.close();
    }
  }
}


// Vent - V7
BLYNK_WRITE(V7)
{
  if (!autoMode)
  {
    int value = param.asInt();

    if (value == 1)
    {
      vent.openVent();
    }
    else
    {
      vent.closeVent();
    }
  }
}


// Auto / Manual Mode - V8
BLYNK_WRITE(V8)
{
  autoMode = (param.asInt() != 0);
  if (autoMode && sensorSampleReady) environment.notifyObservers();

  Serial.print(F("[BLYNK] Auto Mode: "));

  if (autoMode)
  {
    Serial.println(F("ON"));
  }
  else
  {
    Serial.println(F("OFF"));
  }
}
// ====================================================================
// SETUP
// ====================================================================

void setup()
{
  Serial.begin(
    115200
  );


  Serial.println();


  Serial.println(
    F("Smart Environmental Monitor & Controller (ESP32)")
  );


  Serial.println(
    F("System starting...")
  );
// ==========================================================
// BLYNK / WIFI CONNECTION
// ==========================================================

// Network starts after hardware initialization below.

  // ADC CONFIGURATION
  // ==========================================================

  /*
  Keep the ADC range at 0-1023 (10-bit) so the existing
  calibration constants (SOIL_DRY_ADC, LDR_DARK_ADC, etc.)
  stay valid without rescaling.

  11dB attenuation gives close to the full 0-3.3V input range.
  */

  analogReadResolution(10);

  analogSetAttenuation(ADC_11db);


  // ==========================================================
  // LED
  // ==========================================================


  pinMode(
    REFILL_LED_PIN,
    OUTPUT
  );


  digitalWrite(
    REFILL_LED_PIN,
    LOW
  );


  // ==========================================================
  // POWER-GATE HANDSHAKE PIN
  // ==========================================================

  pinMode(
    POWER_ENABLE_PIN,
    OUTPUT
  );


  /*
  Breadboard edition: keep the external enable signal HIGH continuously.
  */

  digitalWrite(
    POWER_ENABLE_PIN,
    HIGH
  );


  // ==========================================================
  // DHT
  // ==========================================================


  delay(3000); // Initial sensor power-up settling time.
  dht.begin();


  // ==========================================================
  // ACTUATORS
  // ==========================================================


  vent.begin();


  irrigationPump.begin();


  growLight.begin();


  // ==========================================================
  // OBSERVER REGISTRATION
  // ==========================================================


  environment.attach(
    &temperatureObserver
  );


  environment.attach(
    &moistureObserver
  );


  environment.attach(
    &lightObserver
  );


  // ==========================================================
  // OLED
  // ==========================================================


  Wire.begin();
  // Wire.begin(21, 22); // uncomment and use if you need to
                          // override the default I2C pins


  oledAvailable =
    display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_I2C_ADDRESS
    );


  if (oledAvailable)
  {
    display.clearDisplay();


    display.setTextSize(1);


    display.setTextColor(
      SSD1306_WHITE
    );


    display.setCursor(
      0,
      0
    );


    display.println(
      F("Smart Greenhouse")
    );


    display.println(
      F("Starting...")
    );


    display.display();


    Serial.println(
      F("OLED initialized.")
    );
  }


  else
  {
    Serial.println(
      F("OLED NOT FOUND.")
    );


    Serial.println(
      F("Check SDA/SCL wiring and OLED address.")
    );
  }


  Serial.println(
    F("System initialization complete.")
  );


  Serial.println(
    F("IMPORTANT: Vent must start physically CLOSED.")
  );
  // Read sensors and establish pump safety before accepting cloud commands.
  runSensorCycle();
  Blynk.config(BLYNK_AUTH_TOKEN);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  lastNetworkAttempt = millis();
  Serial.println(F("[MODE] Breadboard: continuous power, no light sleep, sample every 3s."));}


// ====================================================================
// BREADBOARD CONTINUOUS LOOP
// ====================================================================

void publishBlynkState()
{
  if (!Blynk.connected() || !sensorSampleReady) return;
  const EnvironmentData& data = environment.getData();
  if (data.dhtValid) {
    Blynk.virtualWrite(V0, data.temperature);
    Blynk.virtualWrite(V1, data.humidity);
  }
  Blynk.virtualWrite(V2, data.soilPercent);
  Blynk.virtualWrite(V3, data.lightPercent);
  Blynk.virtualWrite(V4, data.waterPercent);
  Blynk.virtualWrite(V5, growLight.isOn());
  Blynk.virtualWrite(V6, irrigationPump.isOpen());
  Blynk.virtualWrite(V7, vent.isOpen());
  Blynk.virtualWrite(V8, autoMode);
}

void serviceNetwork()
{
  if (WiFi.status() == WL_CONNECTED && Blynk.connected()) {
    Blynk.run();
    return;
  }
  if (millis() - lastNetworkAttempt < 10000UL) return;
  lastNetworkAttempt = millis();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[WiFi] Reconnecting; local sensor control continues."));
    WiFi.reconnect();
  } else {
    // Bound the connection attempt so local control is not stalled for seconds.
    if (Blynk.connect(250)) publishBlynkState();
  }
}

void loop()
{
  vent.update();
  if (millis() - lastSampleMillis >= SAMPLE_INTERVAL_MS) {
    runSensorCycle();
    publishBlynkState();
  }
  if (!sensorSampleReady || environment.getData().waterLow ||
      millis() - lastSampleMillis >= 6000UL) {
    irrigationPump.close();
  }
  serviceNetwork();
  vent.update();
  delay(1);
}