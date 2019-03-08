#include <dht.h>
#include <LiquidCrystal.h>
#include <Time.h>
#include <TimeLib.h>
#include <DS1302RTC.h>
#include <Servo.h>
#include <EEPROM.h>

#include "pitches.h" //add Equivalent frequency for musical note
#include "themes.h" //add Note vale and duration

// #define IDEAL_TEMP 71.6 // Para Testing Only. Set to current temp, then blow on sensor.

#define ARR_SIZE(arr) (sizeof(arr) / sizeof(*arr))

/***** Globals *******/
dht DHT;
Servo servo;
int slr; // servo_last_read

bool fan = 0;
bool temp = 0;
bool error = 0;
unsigned long startup = 0;
unsigned long day18 = (86400 * 18);
unsigned long hatchday = 0;


/* 
 * At day 18 we stop turning the eggs. 
 * It's time to to move them to a hatching basket.
 */
 
/* Servos */

#define HATCH_SERVO 10

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Set pins:  CE, IO,CLK
DS1302RTC RTC(27, 29, 31);

// Optional connection for RTC module
#define DS1302_GND_PIN 33
#define DS1302_VCC_PIN 35


/********* PINS *************/
#define BUZZER 8
#define FAN 9
#define DHT11_PIN 7

#define LED A8
/****************************/
#ifndef IDEAL_TEMP
  #define IDEAL_TEMP 99.5
#endif

#define IDEAL_HUMIDITY 45
#define IDEAL_HUMIDITY_MIN (IDEAL_HUMIDITY-5)
#define IDEAL_HUMIDITY_MAX (IDEAL_HUMIDITY+5)

#define IDEAL_HUMIDITY_HATCHING 70
#define IDEAL_HUMIDITY_HATCHING_MIN (IDEAL_HUMIDITY_HATCHING-5)
#define IDEAL_HUMIDITY_HATCHING_MAX (IDEAL_HUMIDITY_HATCHING+5)

#define MIN_TURNS_PER_DAY 3
#define MAX_TURNS_PER_DAY 9 // Should be an odd number of days.
#define MAX_TURNING_DAYS 18

// Humidity: 40 to 50 percent humidity must be maintained for 
// the first 18 days; 65 to 75 percent humidity is needed for 
// the final days before hatching

#define C2F(T) ((9.0 / 5.0) * (T) + 32.0)

#define XXX {print_rtc(0),Serial.print(__FUNCTION__),  \
  Serial.print("(); Humidity "), \
  Serial.print(DHT.humidity), Serial.print(" % TempF "), \
  Serial.print(C2F(DHT.temperature)), \
  Serial.println(); \
  lcd.setCursor(0, 1), \
  lcd.print("H:"), \ 
  lcd.print(DHT.humidity), \
  lcd.print("% F:"),lcd.print(C2F(DHT.temperature)); \
}

void mario() {
    lcd.clear(); lcd.print("Mario Theme");
    for (int thisNote = 0; thisNote < (sizeof(MarioUW_note) / sizeof(int)); thisNote++) {
      int pauseBetweenNotes;
      int noteDuration = 1000 / MarioUW_duration[thisNote]; //convert duration to time delay
      analogWrite(LED, MarioUW_note[thisNote] % 255);
      tone(BUZZER, MarioUW_note[thisNote], noteDuration);
      pauseBetweenNotes = noteDuration * 1.80;
      delay(pauseBetweenNotes);
      noTone(8); //stop music on pin 8
    }
    analogWrite(LED, 0);

}

void alarm(const char *str) {
  int i;
  error = 1;
  lcd.setCursor(0,0);
  lcd.print(" !ERROR! ");
  lcd.print(str);
  for (i = 0 ; i < 10 ; i++) {
    tone(BUZZER, 2000, 200);
    delay(200);
  }
}

void fan_control() {
    int chk = DHT.read11(DHT11_PIN);
    int f = 0;
    
    if (DHT.temperature < 0) { // Fuck it. They are all dead or we are getting a sensor misreading.
        Serial.print("DHT Sensor Error: ");
        Serial.println(chk);
        alarm(__FUNCTION__);
        return;
    }
    
    f = C2F(DHT.temperature);

    if ((f > IDEAL_TEMP) && (fan == 0)) {
        servo_pos(180); // Open hatch first.
        delay(2000); // Wait 2 seconds. We want to avoid amp spikes.
        Serial.println("Turning Fan ON");
        digitalWrite(FAN, HIGH);
        fan = 1;
        XXX;
    }
    if ((f < IDEAL_TEMP) && (fan == 1)) {
        Serial.println("Turning Fan OFF");
        digitalWrite(FAN, LOW); // Fan off then close the hatch
        servo_pos(0);
        delay(2000);
        fan = 0;
        XXX;
      }
}


void humidity_control() {
  int day = ((RTC.get() - startup) / 86400);
  int min, max;
  if (day >= 18) {
    min = IDEAL_HUMIDITY_HATCHING_MIN;
    max = IDEAL_HUMIDITY_HATCHING_MAX;
  } else {
    min = IDEAL_HUMIDITY_MIN;
    max = IDEAL_HUMIDITY_MAX;
  }
  // If humidity is above or blelow where we need it. 
  // Open or Close the hatch.
  if (DHT.humidity >= max)
    servo_pos(179);

  if (DHT.humidity <= min)
    if (slr != 0)
      servo_pos(1);

}
void rtc_setup() {

  // Activate RTC module
  digitalWrite(DS1302_GND_PIN, LOW);
  pinMode(DS1302_GND_PIN, OUTPUT);

  digitalWrite(DS1302_VCC_PIN, HIGH);
  pinMode(DS1302_VCC_PIN, OUTPUT);

  Serial.println("Activating RTC module");
  delay(5000);

  EEPROM.get(0, startup);
  if ((startup == 0) || (startup == 255)) {
    Serial.print("Getting startup time from RTC: ");
    startup = RTC.get();
  } else {
    Serial.print("Got startup time from EEPROM: ");
  }
  Serial.println(startup);
  hatchday = startup + day18 + (86400.0 * 3.0);  
  Serial.print((hatchday - startup)/86400.0);
  Serial.println(" days to go till chicks start hatching.");
}

void print2digits(int number) {
  if (number >= 0 && number < 10)
    Serial.write('0');
  Serial.print(number);
}

void print_rtc(int t) {

  tmElements_t tm;
  
  Serial.print("UNIX Time: ");
  Serial.print(RTC.get());

  if (! RTC.read(tm)) {
    Serial.print("  Time = ");
    print2digits(tm.Hour);
    Serial.write(':');
    print2digits(tm.Minute);
    Serial.write(':');
    print2digits(tm.Second);
    Serial.print(", Date (D/M/Y) = ");
    Serial.print(tm.Day);
    Serial.write('/');
    Serial.print(tm.Month);
    Serial.write('/');
    Serial.print(tmYearToCalendar(tm.Year));
    Serial.print(", DoW = ");
    Serial.print(tm.Wday);
    Serial.println();
  } else {
    Serial.println("ERROR: DS1302 read!  Please check the circuitry.");
    alarm(__FUNCTION__);
    delay(9000);
  }
  
  // Wait one second before repeating :)
  delay (t);
}


/* Servo Control */
void servo_pos(int pos)
{           
  Serial.print("Moving Servo to POS: ");
  servo.attach(HATCH_SERVO);
  servo.write(pos);
  slr = servo.read(); // servo_last_read
  Serial.println(slr);
  delay(3000);
  servo.detach();
}

void setup()
{
    Serial.begin(9600);
    // pinMode(BUZZER, OUTPUT);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    Serial.println("LCD Setup");
    lcd.begin(16, 2);
    lcd.clear();
    lcd.setCursor(0, 0);
    // mario();
    Serial.print("FAN Setup: Mode [Fahrenheit] ");
    Serial.println(IDEAL_TEMP);
    pinMode(FAN, OUTPUT);
    lcd.clear(),lcd.print("RTC Setup");
    rtc_setup();

    Serial.println("Servo Setup");
    lcd.clear(); lcd.print("Servo Setup");
    servo_pos(0);
    servo_pos(180);
    servo_pos(0);
    delay(5000);
    Serial.println("Enter number 1 for time and temp readings");
    Serial.println("Setup Complete"); lcd.clear(); lcd.print("Setup Complete");
    delay(1000);
    lcd.clear(); lcd.print("Eggubator: 1.0");
    delay(2000);
    lcd.clear();
}

void loop()
{
    unsigned long eestartup = 0UL;
    delay(3000);
    fan_control();
        
    if (!error) {
      lcd.setCursor(0, 0);
      lcd.print("!Huevos! ");
    }
    lcd.setCursor(9, 0);
    // Countdown number of days to go.
    // lcd.print((1814400.0 - (millis() / 1000.0))/86400.0);
    lcd.print((RTC.get() - startup) / 86400.0);
    
    lcd.setCursor(0, 1);
    lcd.print("H:");
    lcd.print(DHT.humidity);
     ((millis() % 2) == 0 ? 
     lcd.print("% F:"),lcd.print(C2F(DHT.temperature)) : \
     lcd.print("% C:"),lcd.print(DHT.temperature));

    humidity_control();
    
    if (Serial.available() >= 1) {
      int key = Serial.parseInt();
      switch(key) { 
        case 255:
          lcd.clear();
          alarm(__FUNCTION__);
          break;
        case 1:
          XXX;
          break;
        case 1000:
          mario();
          break;
        case 1024:
          EEPROM.put(0, RTC.get());
          Serial.println(eestartup);
          XXX;
          break;  
        case 1025:
          EEPROM.get(0, eestartup);
          Serial.println(eestartup);
          break;
        default:
          Serial.print("key=");
          Serial.println(key);
          servo_pos(key);
          break;
      }
    }
}
