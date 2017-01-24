/**
*  _     ___  _  _  ___   _                              __      _     _ 
* | |   | __|| \| || _ \ | | ___  __ _  __ _  ___  _ _  / /_ __ (_) __| |
* | |__ | _| | .` ||   / | |/ _ \/ _` |/ _` |/ -_)| '_|/ /| '_ \| |/ _` |
* |____||___||_|\_||_|_\ |_|\___/\__, |\__, |\___||_| /_/ | .__/|_|\__,_|
*                                |___/ |___/              |_|            
*
* version 2
* 
* Logger and PID for use with LENR experiments
* 
* Uses:
*  - Arduino Mega ATmega2560
*  _ SD card compatible with SD and SPI libs
*  - TC quad module (4 TC's connented to unit with MC and therefore a single serial)
*    see: https://github.com/freephases/ThermocoupleUnit
*  - 5v transducer -14.5~30 PSI 0.5-4.5V linear voltage output
*  - GC-10 Geiger counter uit
*  - PSU+Stepdown+H-Bridge module connected by serial
*    see: https://github.com/freephases/sudoac
*
*  Copyright (c) 2015-2017 free phases research
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
*  THE SOFTWARE.
*/

/**
* libs
*/
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <PID_v1.h>
#include <RunningMedian.h>
#include <math.h>



/**
* OnOff is simple class to manage digital ports see: https://github.com/freephases/arduino-onoff-lib
*/
#include <OnOff.h>

/**
* Include main defines/settings for logger
*/
#include "logger.h"


/**
* Led to signal we are working, using digital pin 30 on Mega connected to 5v green LED
*/
OnOff connectionOkLight(LL_PIN_OK_LIGHT);

/**
* global vars for run time...
*/
unsigned long sendDataMillis = 0; // last millis since sending data to whereever

/**
* Log data to 'DATALOG' file on SD card - can be disable via RUN file's disable_sd_logging setting
* will just append data to end of file so you need to clean it up before running out of space
*/
boolean logToSDCard = true;

/**
* Send debug messages to serial 0 not good if using USB connection to PC
*/
boolean debugToSerial = (DEBUG_SLAVE_SERIAL == 1);

/**
* Data send interval
* how long do we want the interval between sending data, set via send_interval_sec in config file
*/
unsigned long sendDataInterval = 5000;//send data every XX millisecs, default is 5 secs

/*
 * Main serial buffer vars
 */
short serial0BufferPos = 0; // position in read buffer
char serial0Buffer[MAX_STRING_DATA_LENGTH_SMALL + 1];
char serial0BufferInByte = 0;

/**
* Thermocouple offsets, set by sd card if seting exists, see config
*/
float manualMaxTemp = 800.00; // temp we must ever go above - depends on thermocouple
const int thermocoupleCount = 4;
int thermocoupleEnabledCount = 4;

/**
 * parameters for controlling ramp up to temp
 */
unsigned long tempRiseSampleTime = 15000;
double tempRiseMaxDegrees = 3.00;

float lowestPressure = -13.50;
float highestPressure = 29.50;

/**
* hBridgeSpeed speed, set by sd card if seting exists, see config or
* by lcd/keypad via lcdslave
*/
int hBridgeSpeed = 10;//~3600Hz which is about 500 pluses with AC at 1000Hz; more work todo
int hbridgeDelay = 0;//delay at end of each cycle in phases/segments

float calibratedVoltage = 5.0038; //4.976; //was 4.980 before 2016-01-03; now set by sd card if setting exists

/*
where K_p, K_i, and K_d, all non-negative, denote the coefficients for the proportional, integral, and derivative terms, respectively (sometimes denoted P, I, and D). In this model,

P accounts for present values of the error. For example, if the error is large and positive, the control output will also be large and positive.
I accounts for past values of the error. For example, if the current output is not sufficiently strong, error will accumulate over time, and the controller will respond by applying a stronger action.
D accounts for possible future values of the error, based on its current rate of change.[1]
*/
// PID tuning aggressive
double KP = 0.5;  // 1 degree error will increase the power by KP percentage points
double KI = 0.65; // KI percentage points per degree error per second,
double KD = 0.25;   // Not yet used



/**
* Send the data to slave or serial, depending on config settings
*/
void sendData() 
{
  if (sendDataMillis == 0 || millis() - sendDataMillis >= sendDataInterval) {
    sendDataMillis = millis();

  connectionOkLight.on();
  if (logToSDCard) {
      saveCsvData();
  }
  printRawCsv();
  connectionOkLight.off();
  }
}

/**
* Read all sensors at given intervals
*/
void readSensors() 
{
  pressureRead();
}

void actionErrorCommand(char c, String message = "bad command"){
  //Serial.print("!");
  Serial.println("!"+String(c)+"^"+message);
}

  //our responses
  //@[command][action] - means ok
  //*[command][action]^(value) - response to request for info, [action] is optional
  //![command]^(message) - error (command = ! is misc. error)
  //D^(message) - debug
  //R^(csv string) - data results

void processMainSerialResponse() 
{
  //incoming is . :[command][...]
  if (serial0Buffer[0]!=':') {
    actionErrorCommand(':');
    return;
  }
  char c = serial0Buffer[1];
  switch(c) {
    case 'h': //heater :h[action]([^value])
              if (strlen(serial0Buffer)>2) actionHeaterControlCommand(serial0Buffer[2]);
              else actionErrorCommand(c);
              break;
    case 'o': //set config/options :o^[setting name]=[value] eg :o^
              if (strlen(serial0Buffer)>3&&serial0Buffer[2]=='^') { 
                actionSetSettingCommand();               
              }
              else actionErrorCommand(c);
              break;
    case 's': //save config :s
              saveSettings();              
              break;
    case 'd': //display settings :d
              displaySettings();              
              break;
    case 'v': //display board voltage :v             
              Serial.println("*v^" +String(readVcc()));
              break;
//    case 't': //set date and time :t^2017-01-08 12.30pm GMT
//              //todo with clock module
//              break;
//              
//    case '?': //help :?
//              actionHelpCommand();
//              break;
    default: actionErrorCommand(c);
              break;
  }
}

void processMainSerial()
{
   while (Serial.available() > 0)
  {
    serial0BufferInByte = Serial.read();
    //if (serial0BufferInByte == '\r') {
     // continue; //ignore line feed
    //}
    // add to our read buffer
    serial0Buffer[serial0BufferPos] = serial0BufferInByte;
    serial0BufferPos++;

    if (serial0BufferInByte == '\n' || serial0BufferPos == MAX_STRING_DATA_LENGTH) //end of max field length
    {
      serial0Buffer[serial0BufferPos - 1] = 0; // delimited
      processMainSerialResponse();
      serial0Buffer[0] = '\0';
      serial0Buffer[1] = '\0';
      serial0Buffer[2] = '\0';
      serial0BufferPos = 0;
    }
  }
}

/**
* Process serial connections, do one at a time per main loop to stop one serial cloging up the system
*/
int serialTick = 0;
void manageSerial() 
{
  serialTick++;
  if (serialTick>4) serialTick = 1;
  
  switch(serialTick) {
    case 1: processMainSerial(); break;
    case 2: processGeigerSerial(); break;
    case 3: processThermocouplesSerial();break;
    case 4: processHbridgeSerial(); break;
  }
}


/**
* Sensor and control components set up
**/
void setupDevices()
{
  sdCardSetup();//must load first as it load the settngs/config
  thermocouplesSetup();
  hBridgeSetup();
  powerheaterSetup();
  geigerSetup();
  setupPressure();  
  sendDataMillis = millis();//set milli secs so we get first reading after set interval
  connectionOkLight.off(); //set light off, will turn on if logging ok, flushing means error
}

/**
* Set up
*/
void setup()
{
  Serial.begin(115200); // main serial ports used for debugging or sending raw CSV over USB for PC processing, see DEBUG_SLAVE_SERIAL to logger.h
  //using also hardware serial ports:
  // - Serial1: GC-10
  // - Serial2: h bridge
  // - Serial3: TC unit
  connectionOkLight.on(); //tell the user we are alive
  Serial.println("**");//clear
  Serial.println("***");
  Serial.print("*I^LENR logger v"); // send some info on who we are, i = info, using pipes for delimiters, cool man!
  Serial.println(getVersion());
  setupDevices();
  Serial.println("*I^started");
}

/**
* Main loop
*/
void loop() 
{  
  readSensors();
  manageSerial();  
  regulateHeater();
  sendData();
}
