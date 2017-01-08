/**
* H-bridge interface for sudoac device 
* see: https://github.com/freephases/sudoac
*/
SoftwareSerial hBridgeSerial(11, 9);
short hbPos = 0; // position in read buffer
char hbBuffer[MAX_STRING_DATA_LENGTH_SMALL + 1];
char hbInByte = 0;
float hbWatts = 0.00;
float hbAmps = 0.00;
float hbVolts = 0.00;

/**
* Set Amps and watts sent by sudoac client
*/
void setHbridgeInfo() {
  char buf[15];
  
  getValue(hbBuffer, '|', 1).toCharArray(buf, 15);
  hbAmps = atof(buf);
  
  getValue(hbBuffer, '|', 2).toCharArray(buf, 15);
  hbWatts = atof(buf);

  getValue(hbBuffer, '|', 3).toCharArray(buf, 15);
  hbVolts = atof(buf);
   
  if (debugToSerial) {
        Serial.print("H-bridge Watts: ");
        Serial.println(hbWatts);
      }

}

//void handelHbridgeOkResponse()
//{
//  char buf[4] = {0,0,0,0};
//  
//  getValue(hbBuffer, '|', 1).toCharArray(buf, 3);
//  switch (buf[0]) {
//    case '+': 
//      break;
//    case '-': 
//      break;
//    default:
//      break;
//    }
//}

/**
* Execute response
*/
void processHbridgeResponse()
{
  char recordType = hbBuffer[0];

  switch (recordType) {
    case 'R' : //got a response record/data from the our power slave
      setHbridgeInfo();
      break;
    case 'O' :
     // ok //todo make LCD show a response somehow
       //handelHbridgeOkResponse();
       break;
       
    case 'E' :
      // lcdSlaveError("OVER POWER");
       break;
  }
}

void processHbridgeSerial()
{
 // hBridgeSerial.listen();//can't get this to work on mega 1280 so limited to one software serial port
  
  while (hBridgeSerial.available() > 0)
  {
    hbInByte = hBridgeSerial.read();
 //   Serial.print(hbInByte);

    // add to our read buffer
    hbBuffer[hbPos] = hbInByte;
    hbPos++;

    if (hbInByte == '\n' || hbPos == MAX_STRING_DATA_LENGTH_SMALL) //end of max field length
    {
      hbBuffer[hbPos - 1] = 0; // delimited
      processHbridgeResponse();
      hbBuffer[0] = '\0';
      hbPos = 0;
    }
  }
}
boolean hBridgeIsOn = false;
void hBridgeTurnOn()
{
  hBridgeSerial.println("+|!");
  hBridgeIsOn = true;
}

void hBridgeTurnOff()
{
  hBridgeSerial.println("-|!");
  hBridgeIsOn = false;
  
}

boolean getIsHbridgeOn()
{
  return hBridgeIsOn;
}


/**
*
*/

float getHbridgeWatts() {
  return hbWatts;
}

float getHbridgeAmps() {
  return hbAmps;
}

float getHbridgeVolts() {
  return hbVolts;
}

float getPower()
{
  return getHbridgeWatts();
}

void setHBridgeSpeed(int hbSpeed)
{
  if (hbSpeed>99) hbSpeed = 99;
  if (hBridgeSpeed!=hbSpeed) hBridgeSpeed = hbSpeed;
  
  String s(hbSpeed);
  //if (!fromLcd) lcdSlaveMessage('H', s); 
  char buf[10];
  char speedBuf[4];
  s.toCharArray(speedBuf, 4);
  sprintf(buf, "s|%s|!", speedBuf);
  hBridgeSerial.println(buf);
}

void setHBridgeVoltage(int percentage)
{
  if (percentage>100) percentage = 100;
  else if (percentage<0) percentage = 0;
  
  String s(percentage);
  //if (!fromLcd) lcdSlaveMessage('W', s); 
  char buf[10];
  char speedBuf[4];
  s.toCharArray(speedBuf, 4);
  sprintf(buf, "v|%s|!", speedBuf);
  hBridgeSerial.println(buf);
}

void hBridgeSetup() {
 hBridgeSerial.begin(9600);
 hBridgeTurnOff();
 setHBridgeSpeed(hBridgeSpeed);
}
