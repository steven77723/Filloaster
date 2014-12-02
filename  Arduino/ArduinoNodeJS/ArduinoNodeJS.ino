#include "SPI.h"
#include "PN532_SPI.h"
#include "snep.h"
#include "NdefMessage.h"

//NFC Constants
PN532_SPI pn532spi(SPI, 10);
SNEP nfc(pn532spi);
uint8_t ndefBuf[128];
//NFC Vars
String drinkType = "";

// LED vars 
const int ledPin = 13;

// LED read vars
String inputString = "";         // a string to hold incoming data
boolean toggleComplete = false;  // whether the string is complete
char inChar;

// Potmeter vars
const int analogInPin = A0;
int sensorValue = 0;        // value read from the potmeter
int prevValue = 0;          // previous value from the potmeter

//load cell calibration
float loadA = 0.355; 
int analogvalA = 130; 
float loadB = 0.440; 
int analogvalB = 220; 
float analogValueAverage = 0;

//FSM for the system
int Filloaster_State = 0;

//Constants to measure liquid.
float noWeights;
float glassWeight;
int glassConstant = 3; //magic number
float calibWeight = 0;
int calibCount = 0;
float sumOfCalibLoads = 0;
float fullGlass;
float currentGlass = 0;

//other constants
float amountDrunk = 0;
float totalAmountDrunk = 0;
float refillThreshold = 0.5;



void setup() {
  // initialize serial:
  Serial.begin(9600);
  // init LEDS
  pinMode(ledPin,OUTPUT);
  digitalWrite(ledPin,0);
}


void loop() 
{
  int analogValue = analogRead(0);
  analogValueAverage = 0.99*analogValueAverage + 0.01*analogValue;
  float load = analogToLoad(analogValue) * 1000;
  //Load is in mL
  Serial.print("S");
  Serial.print(Filloaster_State);
  Serial.print("D");

  // get the weight of the glass
  else if (Filloaster_State == 1)
  {
    if (load > (glassConstant * noWeights))
    {
      glassWeight = load;
      Filloaster_State = 2;
    }
  }
  //get weight of water and glass
  else if (Filloaster_State == 2)
  {
    if (calibWeight - 10  <load < calibWeight +10) //magic number
    {
      calibCount++;
      sumOfCalibLoads = sumOfCalibLoads + load;
    }
    else
    {
      calibCount = 0;
      sumOfCalibLoads = 0;
    }
    //The value has been constant for 20 iterations
    if (calibCount == 20)
    {
      fullGlass = sumOfCalibLoads / 10;
      currentGlass = sumOfCalibLoads / 10;
      Filloaster_State = 3;
    }
    calibWeight = load;
  }
  //Glass is full now, now we measure amount in cup
  else if (Filloaster_State == 3)
  {
    if (load > currentGlass + 10) //magic number
    {
      totalAmountDrunk = totalAmountDrunk + amountDrunk;
      amountDrunk = 0;
      Filloaster_State = 2;
    }
    // make sure that we don't take the case when person is drinking
    if (!(load < glassWeight)) //means not currently drinking: cup is not in air
    {
      currentGlass = load;
      amountDrunk = fullGlass - currentGlass;
      if (currentGlass < refillThreshold * fullGlass)
      {
        //Send signal to webserver to refill.
        Filloaster_State = 4;
      }
    }
  }
  // sent signal to waiter and waiting for response
  else if (Filloaster_State == 4)
  {
    //Code when the waiter refills it.
    //I should look over here. What if waiter picks glass up.
    if (load > currentGlass + 10) //magic number
    {
      totalAmountDrunk = totalAmountDrunk + amountDrunk;
      amountDrunk = 0;
      Filloaster_State = 2;
    }    
    if (!(load < glassWeight)) //means not currently drinking: cup is not in air
    {
      currentGlass = load;
      amountDrunk = fullGlass - currentGlass;
    }
    //Code to receive signal from webserver and to modify refillThreshold
    //Then jump to State 3
  } 


















  // Potmeter
   sensorValue = analogRead(analogInPin);  

  // read the analog in value:
  if(prevValue != sensorValue)
  {
    Serial.print("B"); // begin character 
    //Serial.print(Filloaster_State);
    //Serial.print(".....");
    //Serial.print(load); Not fully accurate 
    Serial.print(sensorValue); 
    Serial.print("E\n"); // end character
    prevValue = sensorValue;
  } 
  delay(50); // give the Arduino some breathing room.
}












int stringToInt()
{
    char charHolder[inputString.length()+1];
    inputString.toCharArray(charHolder,inputString.length()+1);
    inputString = "";
    int _recievedVal = atoi(charHolder);
    return _recievedVal;
}

//Converts AnalogValue to an actual weight.
float analogToLoad(float analogval){
  float load = mapfloat(analogval, analogvalA, analogvalB, loadA, loadB);
  return load;
}
//Map function for float
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}