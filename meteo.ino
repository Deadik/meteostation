// #include <Wire.h>
#include <Adafruit_BMP085.h>
#include <LiquidCrystal.h>
#include "DHT.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/interrupt.h>
//#include <Ticker.h>
#include <timer.h>

#include "big_chars.h"

#define DHTPIN 6
#define DHTTYPE DHT22
#define ONE_WIRE_BUS 7 
#define BUTTON 9
#define BACKGROIND_PIN 10
#define STEPS_BEFORE_OFF 4
#define STEPS_DELAY 200
#define INT_DELAY 3

/*************************************************** 
  This is an example for the BMP085 Barometric Pressure & Temp Sensor

  Designed specifically to work with the Adafruit BMP085 Breakout 
  ----> https://www.adafruit.com/products/391

  These displays use I2C to communicate, 2 pins are required to  
  interface
  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/

// Connect VCC of the BMP085 sensor to 3.3V (NOT 5.0V!)
// Connect GND to Ground
// Connect SCL to i2c clock - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 5
// Connect SDA to i2c data - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 4
// EOC is not used, it signifies an end of conversion
// XCLR is a reset pin, also not used here

Adafruit_BMP085 bmp;
LiquidCrystal lcd(12, 11, 2, 3, 4, 5);
DHT dht(DHTPIN, DHTTYPE);
//OneWire  ds(7);
//Ticker updatChecker;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

auto timer = timer_create_default();

/**
 * 0 - Off
 * 1 - Rise
 * 2 - Down
 * 3 - On
 */
int displayState = 0;
int displayBrightness = 0;

int stepsBeforeOff = 0;
int stepsDelay = 0;
int intDelayCounter = 0;
bool intBlocked = false;

int x = 0;



int step = 0;

void pciSetup(byte pin)
{
    *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // enable pin
    PCIFR  |= bit (digitalPinToPCICRbit(pin)); // clear any outstanding interrupt
    PCICR  |= bit (digitalPinToPCICRbit(pin)); // enable interrupt for the group
}

void updateInformation(char first, char second, float value){
  lcd.setCursor(0,0);
  lcd.write(first);
  lcd.setCursor(0,1);
  lcd.write(second);

  int full = floor(value);
  int dec = (value-full)*100;
  int length;
  
  String dCel = String(full);
  dCel+=String('.');
  dCel+=String(dec);
  
  length = dCel.length();
  for(int i=0;i<length;i++){
    x = i*3+1;
    char ch = dCel.charAt(i);
    customChar(ch);
  }
}

void rerender(){
  x = 0;
  
  switch(step){
    case 0:
      updateInformation('t', 'i', dht.readTemperature());
    break;
    case 1:
      sensors.requestTemperatures();
      updateInformation('t', 'o', sensors.getTempCByIndex(0));
    break;
    case 2:
      updateInformation('h', 'i', dht.readHumidity());
    break;
    case 3:
      float presure = bmp.readPressure();
      Serial.println("presure");
      Serial.println(presure);
      float presureInmmHg = presure*0.0075;
      updateInformation('p', 'i', presureInmmHg);
    break;
  }
}

void switchStep(){
  stepsDelay = 0;
  step++;
  if(step>3){
    step=0;
  }
  rerender();
  stepsBeforeOff++;
  if(stepsBeforeOff > STEPS_BEFORE_OFF){
    switchOffDisplay();
  }
}

bool displayBrightnessInc(){
  displayBrightness++;
  if(displayBrightness>255){
    displayBrightness = 255;
    displayState = 3;
    rerender();
  }
  analogWrite(BACKGROIND_PIN, displayBrightness);
}

bool displayBrightnessDec(){
  displayBrightness--;
  if(displayBrightness<0){
    displayBrightness = 0;
    displayState = 0;
  }
  analogWrite(BACKGROIND_PIN, displayBrightness);
}

bool riseTick(void *argument){
  if(intBlocked){
    intDelayCounter++;
    if(intDelayCounter > INT_DELAY){
      intDelayCounter = 0;
      intBlocked = false;
    }
  }
  switch(displayState){
    case 1:
      displayBrightnessInc();
    break;
    case 2:
      displayBrightnessDec();
    break;
    case 3:
      if(stepsDelay < STEPS_DELAY){
        stepsDelay++;
      }else{
        switchStep();
      }
    break;
  }
  return true;
}

void setup() {
  cli();
  // assignes each segment a write number
  lcd.createChar(8,LT);
  lcd.createChar(1,UB);
  lcd.createChar(2,RT);
  lcd.createChar(3,LL);
  lcd.createChar(4,LB);
  lcd.createChar(5,LR);
  lcd.createChar(6,UMB);
  lcd.createChar(7,LMB);
  
  lcd.begin(16, 2);

  sei();
  Serial.begin(9600);
  dht.begin();
  if (!bmp.begin()) {
  Serial.println("Could not find a valid BMP085 sensor, check wiring!");
  //while (1) {}
  }

  pinMode(BACKGROIND_PIN, OUTPUT);           // set pin to input
  digitalWrite(BACKGROIND_PIN, LOW);

  pinMode(BUTTON, INPUT);
  pciSetup(BUTTON);

  //timer.every(5000, switchModeByTimer);
  timer.every(10, riseTick);
}

void switchOnDisplay(){
  displayState = 1;
}

void switchOffDisplay(){
  displayState = 2;
}

ISR(PCINT0_vect) 
{
  Serial.println("Push");
  if(digitalRead(BUTTON) == HIGH){
    Serial.println("Push HIGH");
    stepsBeforeOff = 0;
    if(!intBlocked){
      switch(displayState){
        case 0:
        case 1:
        case 2:
          switchOnDisplay();
        break;
        case 3:
          stepsDelay = STEPS_DELAY;
        break;
      }
      intBlocked = true;
    }
  }
}

void custom0()
{ // uses segments to build the number 0
  lcd.setCursor(x, 0); 
  lcd.write(8);  
  lcd.write(1); 
  lcd.write(2);
  lcd.setCursor(x, 1); 
  lcd.write(3);  
  lcd.write(4);  
  lcd.write(5);
}

void custom1()
{
  lcd.setCursor(x,0);
  lcd.write(1);
  lcd.write(2);
  lcd.write(32);
  lcd.setCursor(x,1);
  lcd.write(32);
  lcd.write(255);
  lcd.write(32);
}

void custom2()
{
  lcd.setCursor(x,0);
  lcd.write(6);
  lcd.write(6);
  lcd.write(2);
  lcd.setCursor(x, 1);
  lcd.write(3);
  lcd.write(7);
  lcd.write(7);
}

void custom3()
{
  lcd.setCursor(x,0);
  lcd.write(6);
  lcd.write(6);
  lcd.write(2);
  lcd.setCursor(x, 1);
  lcd.write(7);
  lcd.write(7);
  lcd.write(5); 
}

void custom4()
{
  lcd.setCursor(x,0);
  lcd.write(3);
  lcd.write(4);
  lcd.write(2);
  lcd.setCursor(x, 1);
  lcd.write(32);
  lcd.write(32);
  lcd.write(255);
}

void custom5()
{
  lcd.setCursor(x,0);
  lcd.write(255);
  lcd.write(6);
  lcd.write(6);
  lcd.setCursor(x, 1);
  lcd.write(7);
  lcd.write(7);
  lcd.write(5);
}

void custom6()
{
  lcd.setCursor(x,0);
  lcd.write(8);
  lcd.write(6);
  lcd.write(6);
  lcd.setCursor(x, 1);
  lcd.write(3);
  lcd.write(7);
  lcd.write(5);
}

void custom7()
{
  lcd.setCursor(x,0);
  lcd.write(1);
  lcd.write(1);
  lcd.write(2);
  lcd.setCursor(x, 1);
  lcd.write(32);
  lcd.write(8);
  lcd.write(32);
}

void custom8()
{
  lcd.setCursor(x,0);
  lcd.write(8);
  lcd.write(6);
  lcd.write(2);
  lcd.setCursor(x, 1);
  lcd.write(3);
  lcd.write(7);
  lcd.write(5);
}

void custom9()
{
  lcd.setCursor(x,0);
  lcd.write(8);
  lcd.write(6);
  lcd.write(2);
  lcd.setCursor(x, 1);
  lcd.write(32);
  lcd.write(32);
  lcd.write(255);
}

void customDot()
{
  lcd.setCursor(x,0);
  lcd.write(32);
  lcd.write(32);
  lcd.write(32);
  lcd.setCursor(x,1);
  lcd.write(32);
  lcd.write(255);
  lcd.write(32);
}

void customMinus()
{
  lcd.setCursor(x,0);
  lcd.write(32);
  lcd.write(32);
  lcd.write(32);
  lcd.setCursor(x, 1);
  lcd.write(1);
  lcd.write(1);
  lcd.write(1);
}

void customChar(char ch){
  switch(ch){
        case '0':
          custom0();
        break;
        case '1':
          custom1();
        break;
        case '2':
          custom2();
        break;
        case '3':
          custom3();
        break;
        case '4':
          custom4();
        break;
        case '5':
          custom5();
        break;
        case '6':
          custom6();
        break;
        case '7':
          custom7();
        break;
        case '8':
          custom8();
        break;
        case '9':
          custom9();
        break;
        case '.':
          customDot();
        break;
        case '-':
          customMinus();
        break;
      }
}

void loop() {
  timer.tick();
}
