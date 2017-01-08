#include <MAX31855Fix.h>

#include <PID_v1.h>

#include <RunningMedian.h>

/**
* LENR logger v2
*
*
* Uses:
*  - Arduino Mega ATmega2560
*  _ SD card compatible with SD and SPI libs
*  - MAX31855 with thermocouple x 2 (at least, can have up to 4)
*  - 5v transducer -14.5~30 PSI 0.5-4.5V linear voltage output
*  - GC-10 added as new default option
*  - OPTIONAL: Arduino Pro Mini with a OpenEnergyMonitor SMD card using analog ports 0-1 only - the power/emon slave
*    runs https://github.com/freephases/power-serial-slave.git
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
* SD card lib + SPI
*/
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>

#include <math.h>
/**
* thermocouple driver/amp lib -toredo
*/
#include <Adafruit_MAX31855.h>

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
OnOff connectionOkLight(13);

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
* Thermocouple DO and CLK ports, same for all thermocouples
*/
const int thermoDO = 3; //same as TC2
const int thermoCLK = 5; //same as TC2

/**
* Thermocouple offsets, set by sd card if seting exists, see config
*/
float manualMaxTemp = 1250.00; // temp we must ever go above - depends on thermocouple
const int thermocoupleCount = 4;
int thermocoupleEnabledCount = 4;
MAX31855Fix *thermocopulesList[thermocoupleCount];
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

float calibratedVoltage = 5.005; //4.976; //was 4.980 before 2016-01-03; now set by sd card if setting exists



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
void sendData() {
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
void readSensors() {
  thermocouplesRead();
  pressureRead();
}

void actionErrorCommand(char c){
  Serial.print("!");Serial.println(c);
}


void processMainSerialResponse() 
{
  //:[command][...]
  if (serial0Buffer[0]!=':') {
    actionErrorCommand(':');
    return;
  }
  char c = serial0Buffer[1];
  switch(c) {
    case 'h': //heater :h[action]
              if (strlen(serial0Buffer)>2) actionHeaterControlCommand(serial0Buffer[2]);
              else actionErrorCommand(c);
              break;
    case 'o': //set config/options :o^[setting name]=[value] eg :o^
              if (strlen(serial0Buffer)>3) { 
                actionSetSettingCommand();               
              }
              else actionErrorCommand(c);
              break;
    case 's': //save config :s
              saveSettings();
              
              break;
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

    // add to our read buffer
    serial0Buffer[serial0BufferPos] = serial0BufferInByte;
    serial0BufferPos++;

    if (serial0BufferInByte == '\n' || serial0BufferPos == MAX_STRING_DATA_LENGTH) //end of max field length
    {
      serial0Buffer[serial0BufferPos - 1] = 0; // delimited
      processMainSerialResponse();
      serial0Buffer[0] = '\0';
      serial0BufferPos = 0;
    }
  }
}


/**
* Process incomming slave serial data
*/
void manageSerial() {
  processGeigerSerial();
  processHbridgeSerial();
  processMainSerial();
}

/**
* Things to call when not sending data, called by other functions not just loop()
*/
void doMainLoops()
{
  manageSerial();
  readSensors();
  //powerheaterLoop();
}

/**
* Sensor and control components set up
**/
void setupDevices()
{
  sdCardSetup();
  thermocouplesSetup();
  setupPressure();
  powerheaterSetup();
  hBridgeSetup();
 
  sendDataMillis = millis();//set milli secs so we get first reading after set interval
  connectionOkLight.off(); //set light off, will turn on if logging ok, flushing means error
}

/**
* Set up
*/
void setup()
{
  Serial.begin(9600); // main serial ports used for debugging or sending raw CSV over USB for PC processing
  Serial1.begin(9600);//use by GC-10
  //Serial2 free
   //Serial3 free
  connectionOkLight.on(); //tell the user we are alive
  Serial.println("");//clear
  Serial.println("");
  Serial.print("*I^LENR logger v"); // send some info on who we are, i = info, using pipes for delimiters, cool man!
  Serial.print(getVersion());
  Serial.println(";");
  setupDevices();
  Serial.println("*I^started");
}

/**
* Main loop
*/
void loop() {
  doMainLoops();
  sendData();
}
