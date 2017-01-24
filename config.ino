/**
  LENR logger

  Basic methods to load config settings from SD card

  Example Settings file contents is...

  ;lenr logger settings for run time
  ;sensors_enabled piped delimited list,  values are Power, Pressure, Geiger
  sensors_enabled=Power|Pressure|Geiger|IR
  send_interval_sec=5
  ;PID programs
  ;csv list of values for each program off temp
  power_off_temp=1000.5,399.5
  ;csv list of values for each program length in minutes
  run_time_mins=120,60,0
  ;repeat all programs above, 0 = off,
  repeat=0
  debug_to_serial=no
  ;hbridge_speed percentage 255=max, 0=min
  hbridge_speed=75
  ;delay at end of each cycle in phases/segments
  hbridge_delay=0
  ;thermocouple offsets
  tc_offsets=0.00,0.49
  cal_voltage=4.957
  ramp_sample_ms=15000
  ramp_max_degrees=3.00
  lowest_pressure=-13.75
  highest_pressure=29.1
  ;kP,kI,kD
  pid_tunings=1.0,0.5,0.25
  constant_offset=5.00
  power_offset=7
  ;wave form 1 positive, 2 is negitive, 0 is off, each segment is smallest unit of time 
  wave_form=1,1,0,2,2,0
  ; default is 1,0,2,0
*/

/**
  Max number of setting we can have in the config file
*/
#define MAX_SETTINGS 25

/**
  Char array to hold each line of the config file, we ignore lines starting with ';'
*/
char loggerSettings[MAX_SETTINGS][100] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};//, "",  "", "", "", ""};

/**
  Total number of settings loaded
*/
int settingsCount = 0;

/**
  Load config file from SD card
*/
void loadConfig()
{
  // re-open the file for reading:
  File myFile = SD.open("run.txt");
  if (myFile) {
    if (debugToSerial) {
      Serial.println("D^opened file to read run.txt:");
    }

    byte fileChar;
    int fileBufPos = 0;
    int fileLine = 0;
    char fileBuf[70];
    boolean ignoreUntilNewLine = false;

    // read from the file until there's nothing else in it:
    while (myFile.available()) {
      fileChar = myFile.read();
      if (fileChar == '\r') continue;
      if (fileBufPos == 0 && fileChar == ';') { //ignore line
        ignoreUntilNewLine = true;
        continue;
      }
      if (ignoreUntilNewLine) {
        if (fileChar == '\n') {
          ignoreUntilNewLine = false;
        }
        continue;
      }
      else if (fileChar != '\n') {
        loggerSettings[fileLine][fileBufPos] = fileChar;
        fileBufPos++;
      } else {
        if (fileBufPos == 0) { //ignore empty lines
          continue;
        }
        loggerSettings[fileLine][fileBufPos] = '\0';
        //increment and reset
        fileLine++;
        fileBufPos = 0;
        if (fileLine == MAX_SETTINGS) {
          break;
        }
      }
    }
    //finish off last line ending if there was nor line ending in config file
    /*if (fileBufPos!=0 && fileLine<10) {
      loggerSettings[fileLine][fileBufPos] = '\0';
      fileLine++;
      }*/

    settingsCount = fileLine;

    // close the file:
    myFile.close();
    //mapp to global settings
    loadGlobalSettings();
  } else {
    // if the file didn't open, print an error:
    Serial.println("!!^error opening RUN file on SD card, cannot run without this, see the doc");
    while (true) {
      connectionOkLight.toggle();
      delay(600);
    }
  }
}

String getConfigSetting(char *name, String defaultVal = "")
{
  for (int i = 0; i < settingsCount; i++) {
    if (getValue(loggerSettings[i], '=', 0).indexOf(name) > -1) {
      return getValue(loggerSettings[i], '=', 1);
    }
  }
  //if name not found then return defaultVal
  return defaultVal;
}

int getConfigSettingAsInt(char *name, int defaultVal = 0)
{
  if (getConfigSetting(name).length() == 0) return defaultVal;
  else return getConfigSetting(name).toInt();
}

float getConfigSettingAsFloat(char *name, int defaultVal = 0.000)
{
  if (getConfigSetting(name).length() == 0) return defaultVal;
  else return getConfigSetting(name).toFloat();
}

boolean getConfigSettingAsBool(char *name)
{
  return (getConfigSetting(name).indexOf("yes") > -1 || getConfigSetting(name).indexOf("true") > -1);
}


boolean isSensorEnabled(char *name)
{
  return (getConfigSetting("sensors_enabled").indexOf(name) > -1);
}

/**
  Populate global settings from our files config settings
*/
void loadGlobalSettings()
{
  //load up default config settings


  logToSDCard = !getConfigSettingAsBool("disable_sd_logging");

  if (DEBUG_SLAVE_SERIAL == 0) {
    debugToSerial = getConfigSettingAsBool("debug_to_serial");
  }

  thermocoupleEnabledCount = getConfigSettingAsInt("tc_enabled_count", thermocoupleEnabledCount);

  unsigned long sI = getConfigSettingAsInt("send_interval_sec");
  if (sI < 1) sI = defaultSendDataIntervalSecs;
  sendDataInterval = sI * 1000;

  if (getConfigSettingAsInt("hbridge_speed", -1) != -1) {
    hBridgeSpeed = getConfigSettingAsInt("hbridge_speed");
    if (hBridgeSpeed > 255) { //tmp force while testing to 93 - about 190 hz AC sq wave
      hBridgeSpeed = 255;
    } else if (hBridgeSpeed < 0) {
      hBridgeSpeed = 0;
    }
  }

  if (getConfigSettingAsFloat("max_temp", -999.999) != -999.999) {
    manualMaxTemp = getConfigSettingAsFloat("max_temp");
  }


  if (getConfigSettingAsFloat("cal_voltage", -999.999) != -999.999) {
    calibratedVoltage = getConfigSettingAsFloat("cal_voltage");
  }

  tempRiseSampleTime = (unsigned long) getConfigSettingAsInt("ramp_sample_ms", tempRiseSampleTime);
  tempRiseMaxDegrees = getConfigSettingAsFloat("ramp_max_degrees", tempRiseMaxDegrees);

  lowestPressure = getConfigSettingAsFloat("lowest_pressure", lowestPressure);
  highestPressure = getConfigSettingAsFloat("highest_pressure", highestPressure);

  //set pid tunings
  if (getValue(getConfigSetting("pid_tunings") + ',', ',', 0).length() > 0  &&
      getValue(getConfigSetting("pid_tunings") + ',', ',', 0).toFloat() != NAN) {
    KP = getValue(getConfigSetting("pid_tunings") + ',', ',', 0).toFloat();
    if (getValue(getConfigSetting("pid_tunings") + ',', ',', 1).length() > 0 &&
        getValue(getConfigSetting("pid_tunings") + ',', ',', 1).toFloat() != NAN) {
      KI = getValue(getConfigSetting("pid_tunings") + ',', ',', 1).toFloat();
      if (getValue(getConfigSetting("pid_tunings") + ',', ',', 2).length() > 0 &&
          getValue(getConfigSetting("pid_tunings") + ',', ',', 2).toFloat() != NAN) {
        KD = getValue(getConfigSetting("pid_tunings") + ',', ',', 2).toFloat();
      } else Serial.println("!!^PID config err");
    } else Serial.println("!!^PID config err");
  }

  hbridgeDelay = getConfigSettingAsInt("hbridge_delay", hbridgeDelay);

  sendHBridgeWaveForm();
}



void setConfigSetting(char *name, String value)
{
  for (int i = 0; i < settingsCount; i++) {
    if (getValue(loggerSettings[i], '=', 0).indexOf(name) > -1) {
      char valueChar[40];
      value.toCharArray(valueChar, 40);
      sprintf(loggerSettings[i], "%s=%s", name, valueChar);
    }
  }
}



void actionSetSettingCommand()
{
  char line[70];
  getValue(serial0Buffer, '^', 1).toCharArray(line, 70);
  if (strlen(line) > 0) {
    char name[30];
    getValue(line, '=', 0).toCharArray(name, 30);
    setConfigSetting(name, getValue(line, '=', 1));
    Serial.println("@o"); //Serial.println(line);
  } else {
    actionErrorCommand('o');
  }
}


void saveSettings()
{
  if (SD.exists("run.txt")) SD.remove("run.txt");
  File myFile = SD.open("run.txt", FILE_WRITE);
  if (myFile) {
    if (debugToSerial) {
      Serial.println("opened file to write run.txt:");
    }
    myFile.println(";LENR PID/Logger settings v2");
    for (int i = 0; i < settingsCount; i++) {
      myFile.println(loggerSettings[i]);
    }
    // close the file:
    myFile.close();
    //reload settings
    loadGlobalSettings();
    tcLoadSettings();
    setupPressure();
    loadHeaterSettings();
    Serial.println("@s");
  } else {
    actionErrorCommand('s', "Cannot save settings");
  }
}

void displaySettings()
{
  Serial.println("*dSettings");
  for (int i = 0; i < settingsCount; i++) {
    Serial.println("*d^ " + String(loggerSettings[i]));
    //Serial.println();
    delay(60);
  }
}

