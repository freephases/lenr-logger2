float tcTemps[thermocoupleCount] = { -0.00, -0.00, -0.00, -0.00};
uint8_t tcErrors[thermocoupleCount] = {0,0,0,0}; 
/*
 * tcErrors masks
    if(0x01 & tcErrors[X]){Serial.print("OPEN  ");}
    if(0x02 & tcErrors[X]){Serial.print("Overvolt/Undervolt  ");}
    if(0x04 & tcErrors[X]){Serial.print("TC Low  ");}
    if(0x08 & tcErrors[X]){Serial.print("TC High  ");}
    if(0x10 & tcErrors[X]){Serial.print("CJ Low  ");}
    if(0x20 & tcErrors[X]){Serial.print("CJ High  ");}
    if(0x40 & tcErrors[X]){Serial.print("TC Range  ");}
    if(0x80 & tcErrors[X]){Serial.print("CJ Range  ");}
 */
float tcOffsets[thermocoupleCount] = {0.00, 0.00, 0.00, 0.00};
int tcControlIndex = 0;//TC to use to control pid
/*
   Main serial buffer vars
*/
short serial3BufferPos = 0; // position in read buffer
char serial3Buffer[MAX_STRING_DATA_LENGTH_SMALL + 1];
char serial3BufferInByte = 0;
boolean thermocouplesActive = false;

#define NaN -9999.99


boolean thermocouplesAreActive()
{
  return thermocouplesActive;
}

void thermocouplesSetTemps()
{
  char buf[65];
  getValue(serial3Buffer, '^', 1).toCharArray(buf, 65);

  for (int i = 0; i < getThermocouplesCount(); i++) {
    tcTemps[i] = getValue(buf, ',', i).toFloat();
    if (tcTemps[i]!=NaN) tcTemps[i] += tcOffsets[i];
  }

}

void thermocouplesSetErrors()
{
  char buf[65];
  getValue(serial3Buffer, '^', 1).toCharArray(buf, 30);

  for (int i = 0; i < getThermocouplesCount(); i++) {
    tcErrors[i] = (uint8_t) getValue(buf, ',', i).toInt();
  }

}

String getTcErrorMessage(uint8_t errorCode)
{
  String error("");
  if(0x01 & errorCode){error.concat("OPEN  ");}
  if(0x02 & errorCode){error.concat("Overvolt/Undervolt  ");}
  if(0x04 & errorCode){error.concat("TC Low  ");}
  if(0x08 & errorCode){error.concat("TC High  ");}
  if(0x10 & errorCode){error.concat("CJ Low  ");}
  if(0x20 & errorCode){error.concat("CJ High  ");}
  if(0x40 & errorCode){error.concat("TC Range  ");}
  if(0x80 & errorCode){error.concat("CJ Range  ");}

  return error;
}

void thermocouplesWake()
{
  if (!thermocouplesActive) {
    Serial3.println(":?");
  }
}

void thermocouplesProcessError(char command)
{
   switch (command) {
    case '^': //we have a error messgae
      Serial.println("!T^"+getValue(serial3Buffer, '^', 1));
      break;
    case 'C': //thermocouple count error
      Serial.println("!T^Error setting TC count");
      break;
   }
}

void thermocouplesActionCommand(char command)
{
  switch (command) {
    case '?': thermocouplesActive = true; //confirmed startup and a least 1 tc connected;
      break;
    case 'C': Serial3.println(":C^" + String(getThermocouplesCount())); break;
    case 'R':
      if (serial3Buffer[2] == '^' && strlen(serial3Buffer) > 3) {
        thermocouplesSetTemps();
        break;
      }
    case 'E':
      if (serial3Buffer[2] == '^' && strlen(serial3Buffer) > 3) {
        thermocouplesSetErrors();
        break;
      }
    case '@':
       //ok serial3Buffer[2] == comannd that is ok
       //processOk(serial3Buffer[2]);
       break;
    case '!':
       //error       
       if (serial3Buffer[2]!=0) 
        thermocouplesProcessError(serial3Buffer[2]);
        break;
    default: 
        //do nothing fro now not Serial3.println("!!^bad"); 
      break;
  }
}


void processThermocouplesSerial()
{
  //callheaterRegulator();
  while (Serial3.available() > 0)
  {
    serial3BufferInByte = Serial3.read();
    // add to our read buffer
    serial3Buffer[serial3BufferPos] = serial3BufferInByte;
    serial3BufferPos++;

    if (serial3BufferInByte == '\n' || serial3BufferPos == MAX_STRING_DATA_LENGTH_SMALL) //end of max field length
    {
      serial3Buffer[serial3BufferPos - 1] = 0; // delimited
      if (serial3Buffer[0] == ':') thermocouplesActionCommand(serial3Buffer[1]);
      else Serial3.println(":!^ubad");
      serial3Buffer[0] = '\0';
      serial3Buffer[1] = '\0';
      serial3Buffer[2] = '\0';
      serial3BufferPos = 0;
    }
  }
}

int getThermocouplesCount()
{
  return thermocoupleEnabledCount;
}

float getControlTcTemp()
{
  return tcTemps[tcControlIndex];
}

float getTCTemp(int index)
{
  return tcTemps[index];
}

void tcLoadSettings()
{
  for (int i = 0; i < getThermocouplesCount(); i++) {

    if (getValue(getConfigSetting("tc_offsets") + ',', ',', i).length() > 0) {

      if (getValue(getConfigSetting("tc_offsets") + ',', ',', i).toFloat() == NAN) {
        if (debugToSerial) {
          Serial.print ("Error reading tc_offsets value: ");
          Serial.println(getValue(getConfigSetting("tc_offsets") + ',', ',', i));
        }
        break; // bad value, stop processing power_on_temp settings
      }

      tcOffsets[i] = getValue(getConfigSetting("tc_offsets") + ',', ',', i).toFloat();
    }
  }
}

void thermocouplesSetup()
{
  tcLoadSettings();
  delay(1000);
  Serial3.begin(115200);
  Serial3.println(":?");
}

