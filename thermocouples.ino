/**
  LENR logger - array of thermocopules, fixed at build time not run time
  see: https://learn.adafruit.com/downloads/pdf/thermocouple.pdf
       https://learn.adafruit.com/calibrating-sensors/why-calibrate
       https://learn.adafruit.com/calibrating-sensors/maxim-31855-linearization
*/

const long readThermocouplesInterval = LL_TC_READ_INTERVAL;
int thermocopuleCSPorts[thermocoupleCount] = {4, 2, 6, 7};
float tcOffsets[thermocoupleCount] = {0.00,0.00,0.00,0.00};
int tcControlIndex = 0;//TC to use to control pid

unsigned long readThermocouplesMillis = 0;
//int thermocouplesCurrentSensor = 0;

void thermocouplesSetup()
{
  tcLoadSettings();
  for (int i = 0; i < getThermocouplesCount(); i++) {
    //lcdSlaveMessage('m', " thermocouple "+String(i)+"  ");
    thermocopulesList[i] = new MAX31855Fix(thermocopuleCSPorts[i], tcOffsets[i]);         
  }
  
  for (int i = 0; i < getThermocouplesCount(); i++) {
    thermocopulesList[i]->readSensor();
    delay(500);
    thermocopulesList[i]->readSensor();
    delay(350);
  }
}


void thermocouplesRead()
{ 
  if (readThermocouplesMillis==0 ||  millis() - readThermocouplesMillis >= readThermocouplesInterval) {
    readThermocouplesMillis = millis();
    for (int i = 0; i < getThermocouplesCount(); i++) {
      thermocopulesList[i]->readSensor();
      //Serial.println(thermocopulesList[i]->getTemp(), DEC);
    }  
    heaterRegulator();
  }
}

int getThermocouplesCount()
{
  return thermocoupleEnabledCount;
}

/*void thermocouplesStepRead()
{
  if (millis() - readThermocouplesMillis >= readThermocouplesInterval) {
    readThermocouplesMillis = millis();
    thermocopulesList[thermocouplesCurrentSensor]->read();
    thermocouplesCurrentSensor++;
    if (thermocouplesCurrentSensor == thermocoupleCount) {
      thermocouplesCurrentSensor = 0;
    }
  }
}*/

float getControlTcTemp()
{
  return thermocopulesList[tcControlIndex]->getTemp(); 
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

