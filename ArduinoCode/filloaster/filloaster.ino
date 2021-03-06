#include <SPI.h>
#include <PN532_SPI.h>
#include "PN532.h"

//NFC things
PN532_SPI pn532spi(SPI, 10);
PN532 nfc(pn532spi);

// LED vars 
const int ledPin = 13;

// LED read vars
String inputString = "";         // a string to hold incoming data
boolean toggleComplete = false;  // whether the string is complete
char inChar;

// Potmeter vars
const int analogInPin = A1;
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
/*
If 0, then nothing is on it.
If 1, then glass is on it, get glass weight.
If 2, drink is being poured. If value stablizes, go to 3
If 3, Measure the total amount (glass + drink). If less than x go to 4
If 4, Then send signal to refill.
*/

//FSM for NFCs
int NFC_State = 0;
/*
If 0, then waiting for phone number
If 1, then halting for diff NFC
If 2, then waiting for type of drink
If 3, then not waiting.
*/


//Constants to measure liquid.
float noWeights = 15;
float glassWeight;
int glassConstant = 3; //magic number
float calibWeight = 0;
int calibCount = 0;
float sumOfCalibLoads = 0;
float fullGlass;
float currentGlass = 0;
int difference;
int magicNum = 60;
int numIterations = 50;
boolean noGlass = false;

//other constants
float amountDrunk = 0;
float totalAmountDrunk = 0;
float refillThreshold = 0.5;

//time Constants
unsigned long StartTime = 0;
unsigned long CurrentTime = 0;
unsigned long ElapsedTime = 0;
int leaveTime = 10;
int userLeft = 0;



void setup() {
  // initialize serial:
  Serial.begin(9600);
  // init LEDS
  pinMode(ledPin,OUTPUT);
  digitalWrite(ledPin,0);
  // init NFC modules
  nfc.begin();
  nfc.setPassiveActivationRetries(0xFF);
  nfc.SAMConfig();    
}


void loop() 
{
  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID 
  //Currently using a tag to mimick phone
  //tagData is 467F0A433D80
  if (NFC_State == 0)
  {
    getPhoneNumber(success, uid, uidLength);
  }
  //Have a delay period
  else if (NFC_State == 1)
  {
    delay(5000);
    NFC_State = 2;
  }
  //Get the type of drink
  else if (NFC_State == 2)
  {
    getGlassType(success, uid, uidLength);
    Filloaster_State = 1;
  }
  else if (NFC_State == 3)
  {
    senseorMeasurements();
  }
  delay(30); // give the Arduino some breathing room.

}

void getPhoneNumber(boolean success, uint8_t uid[], uint8_t uidLength){
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
  if (success) 
  {
    Serial.print("P"); // begin character 
    for (uint8_t i=0; i < uidLength; i++) 
    {
      Serial.print(uid[i], HEX); 
    }
    Serial.print("H"); // end character
    NFC_State = 1;
    // Wait 1 second before continuing
  }
  else
  {
    // PN532 probably timed out waiting for a card
    //Serial.println("404");
  }
}

void getGlassType(boolean success, uint8_t uid[], uint8_t uidLength)
{
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
  if (success) 
  {
    Serial.print("N"); // begin character 
    for (uint8_t i=0; i < uidLength; i++) 
    {
      Serial.print(uid[i], HEX); 
    }
    Serial.print("C"); // end character
    NFC_State = 3;
    // Wait 1 second before continuing
  }
  else
  {
    //Serial.println("404");
  }
}

void senseorMeasurements()
{
  //Deals with incoming messages
  receivedSignalFromNode();
  //Convert analog value to load (in mL)
  int analogValue = analogRead(0);
  analogValueAverage = 0.99*analogValueAverage + 0.01*analogValue;
  //float load = analogToLoad(analogValue) * 1000;
  float load = analogValue;
//

  sendValues();
  //Load is in mL

  // get the weight of the glass
  if (Filloaster_State == 1)
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
    difference = 25;
    if (calibWeight - difference < load < calibWeight + difference) //magic number
    {
      calibCount++;
      sumOfCalibLoads = sumOfCalibLoads + load;
    }
    else
    {
      calibCount = 0;
      sumOfCalibLoads = 0;
    }
    //The value has been constant for 10 iterations
    if (calibCount == numIterations)
    {
      fullGlass = sumOfCalibLoads / numIterations;
      currentGlass = fullGlass;
      Filloaster_State = 3;
      calibCount = 0;
      sumOfCalibLoads = 0;
    }
    calibWeight = load;
  }
  //Glass is full now, now we measure amount in cup
  else if (Filloaster_State == 3)
  {
    if (load > currentGlass + magicNum) //magic number
    {
      totalAmountDrunk = totalAmountDrunk + amountDrunk;
      amountDrunk = 0;
      Filloaster_State = 2;
    }
    // make sure that we don't take the case when person is drinking
    if (!(load < glassWeight)) //means not currently drinking: cup is not in air
    {
      //////////////////////////////////////////////////////////////////////////////Check HERE
      currentGlass = load;
      amountDrunk = fullGlass - currentGlass;
      if (amountDrunk < 0) amountDrunk = 0;

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
    if (load > currentGlass + magicNum) //magic number
    {
      totalAmountDrunk = totalAmountDrunk + amountDrunk;
      amountDrunk = 0;
      Filloaster_State = 2;
    }    
    if (!(load < glassWeight)) //means not currently drinking: cup is not in air
    {
      currentGlass = load;
      amountDrunk = fullGlass - currentGlass;
      if (amountDrunk < 0) amountDrunk = 0;    
    }
    //Code to receive signal from webserver and to modify refillThreshold
    //Then jump to State 3
  }

  //In a separate section, check if the customer left by checking the value
  //of load and comparing it to if filloaster empty or not. Use 10 seconds.
  //Then send a message to the phone it was assigned to.
  sensorValue = analogRead(analogInPin);  
  checkDone(load);
}


void checkDone(float load){
  /*filloaster state must be >=2
  have a bool so that for the first time its 0, startTime.
  Otherwize look at CurrentTime and Elapsed.
  If not 0, then turn bool off. and Reset
unsigned long StartTime = millis();

unsigned long CurrentTime = millis();
unsigned long ElapsedTime = CurrentTime - StartTime;
  */
  if (Filloaster_State == 1) return;
  
  //first time there no glass
  if (!(noGlass)){
    if (load < (glassConstant * noWeights)){
      StartTime = millis();
      noGlass = true;
    }
  }
  //not first time
  else{
    CurrentTime = millis();
    ElapsedTime = CurrentTime - StartTime;
    if (ElapsedTime > leaveTime*1000)
    {
      userLeft = 1;
      sendValues();
    }
    if (load >= (glassConstant * noWeights)){
      StartTime = 0;
      noGlass = false;
      CurrentTime = 0;
      ElapsedTime = 0;
    }
  }
}

//Converts AnalogValue to an actual weight.
float analogToLoad(float analogval){
  float load = mapfloat(analogval, analogvalA, analogvalB, loadA, loadB);
  return load;
}
//Map function for float
float mapfloat(float x, float i_min, float in_max, float out_min, float out_max)
{
  return (x - i_min) * (out_max - out_min) / (in_max - i_min) + out_min;
}

//CHIJKMNOPQVWXYZ
void sendValues(){
  //state of filloaster
  Serial.print("S");
  Serial.print(Filloaster_State);
  Serial.print("D");
  //amount in current glass
  Serial.print("G");      
  Serial.print(currentGlass); 
  Serial.print("L");
  //amount in a "full glass"
  Serial.print("F");      
  Serial.print(fullGlass); 
  Serial.print("U");
  //Total amount user drank
  Serial.print("T");      
  Serial.print(totalAmountDrunk); 
  Serial.print("A");
  //User left or not
  Serial.print("U");
  Serial.print(userLeft);
  Serial.print("R");

  Serial.print("C");
  Serial.print(ElapsedTime/1000); // in terms of seconds
  Serial.print("H");

  Serial.print("\n");
// Value of Potmeter
  Serial.print("B"); // begin character 
  //Serial.print((int)load); Not fully accurate 
  Serial.print(sensorValue); 
  Serial.print("E"); // end character
}

void receivedSignalFromNode()
{
  // Recieve data from Node and write it to a String
  while (Serial.available() && toggleComplete == false) 
  {
    inChar = (char)Serial.read();
    if(inChar == 'E')
    { // end character for led
      toggleComplete = true;
    }
    else
    {
      inputString += inChar; 
    }
  }
  // Toggle LED 13
  if(!Serial.available() && toggleComplete == true)
  {
    toggling();
  }
}

void toggling()
{
  // convert String to int. 
  int recievedVal = stringToInt();
  
  if(recievedVal == 0)
  {
    digitalWrite(ledPin,recievedVal);
  }
  else if(recievedVal == 1)
  {
    digitalWrite(ledPin,recievedVal);
  }    
  toggleComplete = false;
}

int stringToInt()
{
    char charHolder[inputString.length()+1];
    inputString.toCharArray(charHolder,inputString.length()+1);
    inputString = "";
    int _recievedVal = atoi(charHolder);
    return _recievedVal;
}
