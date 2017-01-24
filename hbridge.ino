/**
  H-bridge interface for sudoac device
  see: https://github.com/freephases/sudoac
*/
/*
   Not all pins on the Mega and Mega 2560 support change interrupts, so only the following can be used for
   RX: 10, 11, 12, 13, 14, 15, 50, 51, 52, 53,
   A8 (62), A9 (63), A10 (64), A11 (65), A12 (66), A13 (67), A14 (68), A15 (69).
*/
//SoftwareSerial hBridgeSerial(10, 9);
short hbPos = 0; // position in read buffer
char hbBuffer[MAX_STRING_DATA_LENGTH_SMALL + 1];
char hbInByte = 0;
float hbWatts = 0.00;
float hbAmps = 0.00;
float hbVolts = 0.00;
boolean hBridgeActive = false;
boolean hBridgeIsOn = false; // gets set via serial if hb confirms it is ok
int hBridgeVoltagePwmValue = 0;
boolean hBridgeIsActive()
{
  return hBridgeActive;
}
/**
  Set Amps and watts sent by sudoac client
*/
void setHbridgeInfo() {
  char buf[15];

  getValue(hbBuffer, '|', 1).toCharArray(buf, 15);
  hbAmps = atof(buf);

  getValue(hbBuffer, '|', 2).toCharArray(buf, 15);
  hbWatts = atof(buf);

  getValue(hbBuffer, '|', 3).toCharArray(buf, 15);
  hbVolts = atof(buf);

  //  if (debugToSerial) {
  //        Serial.print("D^H-bridge Watts: ");
  //        Serial.println(hbWatts);
  //      }

}

void actionHbridgeOkResponse()
{
  char buf[4] = {0, 0, 0, 0};
  hBridgeActive = true;
  //example response: OK|+|!
  getValue(hbBuffer, '|', 1).toCharArray(buf, 4);
  switch (buf[0]) {
    case '+': hBridgeIsOn = true; 
              Serial.println("@h+");
              break;
    case '-': hBridgeIsOn = false; 
               Serial.println("@h-");
              break;
    case 'g': //it just started up, so reset
              if (buf[1] == 'o') {
                hBridgeIsOn = false;                
                Serial.println("@h^SudoAc Connected");
              }
              break;
    case 's': //speed set ok
              Serial.println("@hf");
              break;
    case 'd': //delay in cycles set ok
              Serial.println("@hd");
              break;
    case 'v': //voltage set ok
              Serial.println("@hv");
              break;
    default:
      break;
  }
}

/**
  Execute response
*/
void processHbridgeResponse()
{
  char recordType = hbBuffer[0];

  switch (recordType) {
    case 'R' : //got a response record/data packet
      setHbridgeInfo();
      break;
    case 'O' : //OK
      if (hbBuffer[1] == 'K') actionHbridgeOkResponse();
      break;
    case 'E' : //Error
      Serial.println("!!^HBHA");//high amps error, hbridge turned it's self off
      hBridgeIsOn = false;
      break;
  }
}

void processHbridgeSerial()
{
  // hBridgeSerial.listen();//can't get this to work on mega 1280 so limited to one software serial port

  while (Serial2.available() > 0)
  {
    hbInByte = Serial2.read();
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

void hBridgeTurnOn()
{
  Serial2.println("+|!");
}

void hBridgeTurnOff()
{
  Serial2.println("-|!");
}

boolean getIsHbridgeOn()
{
  return hBridgeIsOn;
}


/**

*/

float getHbridgeWatts()
{
  return hbWatts;
}

float getHbridgeAmps()
{
  return hbAmps;
}

float getHbridgeVolts()
{
  return hbVolts;
}

float getPower()
{
  return getHbridgeWatts();
}

void setHBridgeSpeed(int hbSpeed)
{
  if (hbSpeed > 255)
    hbSpeed = 255;
  else if (hbSpeed < 0) 
    hbSpeed = 0;
  
  if (hBridgeSpeed != hbSpeed) {
    hBridgeSpeed = hbSpeed;

  String s(hbSpeed);
  char buf[10];
  char speedBuf[4];
  s.toCharArray(speedBuf, 4);
  sprintf(buf, "s|%s|!", speedBuf);
  Serial2.println(buf);
  }
}

void setHBridgeVoltage(int pwmValue)
{
  if (pwmValue > 255) pwmValue = 255;
  else if (pwmValue < 0) pwmValue = 0;

  if (hBridgeVoltagePwmValue != pwmValue) {
      hBridgeVoltagePwmValue = pwmValue;
    String s(pwmValue);
    char buf[10];
    char speedBuf[4];
    s.toCharArray(speedBuf, 4);
    sprintf(buf, "v|%s|!", speedBuf);
    Serial2.println(buf);
  }
}

void setHBridgeDelay(int delayMillis)
{  
  if (delayMillis<0) delayMillis = 0;
  
  String s(delayMillis);
  char buf[17];
  char speedBuf[12];
  s.toCharArray(speedBuf, 12);
  sprintf(buf, "d|%s|!", speedBuf);
  Serial2.println(buf);
}

void sendHBridgeWaveForm()
{
  String wf = getConfigSetting("wave_form", "1,0,2,0");
  Serial2.println("w|"+wf+"|!");
}


void hbWake()
{
  if (!hBridgeActive) {
    Serial2.println("?|!");
  }
}

void hBridgeSetup()
{
  Serial2.begin(11000);
  hBridgeTurnOff();
  delay(60);
  setHBridgeVoltage(0);
  setHBridgeSpeed(hBridgeSpeed);
  //delay(60);
  //setHBridgeDelay(hbridgeDelay);
  //delay(60);
}
