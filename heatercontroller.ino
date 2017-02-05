/**
  Heater Control, controls SSR or/and Hbridge to heater power supply
  using PID lib that uses up to 5 specific run programs taken from
  "power_off_temp" config file setting (see: config tab/file)
*/
const unsigned long powerheaterCheckInterval = 1200;
unsigned long powerheaterMillis = 0;
//OnOff heaterPowerControl(36);
//OnOff heaterPowerControl2(37);
boolean heaterTimedOut = true; // true if reached maxRunTimeAllowedMillis
#define PID_MAX_PROGRAMS 10
float powerOffTemps[PID_MAX_PROGRAMS];
//float powerTempsError[PID_MAX_PROGRAMS];
unsigned long maxRunTimeAllowedMillis[PID_MAX_PROGRAMS];
int powerTempsCount = 0;
int currentProgram = 0;
int repeatProgramsBy = 0;
int repeatProgramsByCount = 0;
unsigned long currentProgramMillis = 0;
unsigned long runStartedMillis = 0;
const unsigned long intervalBeforePower = 60000;
//unsigned long dataStreamStartedMillis = 0;
boolean powerHeaterAutoMode = true;
unsigned long totalRunTimeMillis = 0;
unsigned long programsRunStartMillis = 0;
int currentRunNum = 1;//used for display of currentProgram due to repating programs
//Define Variables we'll be connecting to for PID maths
double Setpoint, Input, Output, lastOutput;
unsigned long lastTurnedOffMillis = 0;
unsigned long lastTurnedOnMillis = 0;
boolean isManualRun = false;
const unsigned long WindowSize = 255;
unsigned long windowStartTime = 0;
unsigned long computeTempRisesStart = 0;
double lastTemp = 0.000;
int powerVoltageValue = 0;// 0% = 10v on step down, 255 = ~49.8v on step down
int lowestPower = 0;
int highestPower = 255;

unsigned long hBridgeIsOnStartMillis = 0;
float hBridgeIsOnStartTemp = 0.00;


PID myPID(&Input, &Output, &Setpoint, KP, KI, KD, DIRECT);


void heaterOn()
{
  if (!thermocouplesAreActive()) {
    Serial.println("!h^NOTC");//no tc error
    thermocouplesWake();
  } else if (!hBridgeIsActive()) {
    Serial.println("!h^NOHB");//no h-bridge error
    hbWake();
  } else if (!getIsHbridgeOn()) {
    lastTurnedOnMillis = millis();
    lastTurnedOffMillis = 0;
    computeTempRisesStart = 0;//reset computeTempRisesStart
    hBridgeTurnOn();
  }
}

/**
  heaterOff

  @param doNotCheckOnStatus boolean - if true do not check if heater is on before turning off, default is to check (false)
*/
void heaterOff(boolean doNotCheckOnStatus = false)
{
  if (doNotCheckOnStatus || getIsHbridgeOn()) {
    lastTurnedOffMillis = millis();
    lastTurnedOnMillis = 0;
    hBridgeTurnOff();
  }
}


void loadHeaterSettings()
{
  repeatProgramsBy = getConfigSettingAsInt("repeat", 0);
  /**
    Load the programs to run
  */
  for (int i = 0; i < PID_MAX_PROGRAMS; i++) {

    if (getValue(getConfigSetting("power_off_temp") + ',', ',', i).length() > 0) {

      if (getValue(getConfigSetting("power_off_temp") + ',', ',', i).toFloat() == NAN) {
        if (debugToSerial) {
          Serial.print ("D^Error reading power_off_temp value: ");
          Serial.println(getValue(getConfigSetting("power_off_temp") + ',', ',', i));
        }
        break; // bad value, stop processing power_on_temp settings
      }

      //powerOverSteps[i] = getValue(getConfigSetting("power_oversteps") + ',', ',', i).toFloat();
      powerOffTemps[i] = getValue(getConfigSetting("power_off_temp") + ',', ',', i).toFloat();
      if (powerOffTemps[i] > manualMaxTemp) {
        powerOffTemps[i] = manualMaxTemp;
      }
      //powerTempsError[i] = 0.00;//set to zero to start with

      if (getValue(getConfigSetting("run_time_mins") + ',', ',', i).toInt() == 0) maxRunTimeAllowedMillis[i] = 0;
      else maxRunTimeAllowedMillis[i] = (unsigned long) (60000 * (unsigned long) getValue(getConfigSetting("run_time_mins") + ',', ',', i).toInt());

      totalRunTimeMillis += maxRunTimeAllowedMillis[i];
      powerTempsCount++;

      if (debugToSerial) {
        Serial.print("D^Power temp ");
        Serial.print(powerTempsCount);
        //Serial.print(" added - on: ");
        //Serial.print(powerOnTemps[i], DEC);
        Serial.print(" off: ");
        Serial.print(powerOffTemps[i], DEC);
        //Serial.print(" step: ");
        // Serial.print(powerOverSteps[i], DEC);
        Serial.print(" time: ");
        Serial.println(maxRunTimeAllowedMillis[i]);
      }
    } else {
      break; // stop processing power/program settings
    }
  }

}
/**
  Load power/PID settings
*/
void powerheaterSetup()
{
  heaterOff(true);//makes sure it's off incase of reboot without a turn off heater command before hand

  loadHeaterSettings();
  Input = getControlTcTemp();
  Setpoint = powerOffTemps[0];
  myPID.SetOutputLimits(0, WindowSize);

  myPID.SetSampleTime(powerheaterCheckInterval);//200mS is default
  //powerheaterMillis = millis() - powerheaterCheckInterval;
}

/**
  `Set PID to use specific PID program
*/
void heaterProgramStart()
{
  Input = getControlTcTemp();
  Setpoint = powerOffTemps[currentProgram];
  int i = map((int)Setpoint, -200, 950, 0, WindowSize);
  setHBridgeVoltage(i);
  powerVoltageValue = i;
  Output = powerVoltageValue;
  delay(60);
}

/**
  Check and switch program to run or end run
*/
void checkProgramToRun()
{
  if (currentProgramMillis != 0 && millis() - currentProgramMillis >= maxRunTimeAllowedMillis[currentProgram])  {
    currentProgram++;
    currentRunNum++;
    if (currentProgram == powerTempsCount) {
      currentProgram = 0;//reset to first program
      repeatProgramsByCount++;//repeat count
      if (repeatProgramsBy > 0 && repeatProgramsByCount < repeatProgramsBy) {
        repeatProgramsByCount++;
        currentProgramMillis = 0;//reset current program timer
        myPID.SetMode(MANUAL);//stop PID
      } else {
        myPID.SetMode(MANUAL);//stop PID
        currentRunNum = 0;
        repeatProgramsByCount = 0;
        heaterTimedOut = true;
        heaterOff();
        Serial.print("@hs");
        return;
      }
    } else {
      currentProgramMillis = 0;//reset current program timer
      myPID.SetMode(MANUAL);//stop PID
    }
    //Set specific PID vars for this program
    heaterProgramStart();
  }
}

/**
  set timer watchers
*/
void startProgramTimeTrackers()
{
  if (programsRunStartMillis == 0)
  {
    programsRunStartMillis = millis();
  }
  if (currentProgramMillis == 0) {
    currentProgramMillis = millis();
    lowestPower = powerVoltageValue - 7; //we make lowest 6% lower then current power
    if (lowestPower < 0) lowestPower - 0;
    highestPower = powerVoltageValue;//we know this power can get to given temp
    if (highestPower < 0) highestPower = 6;
    myPID.SetOutputLimits(lowestPower, highestPower);
    myPID.SetMode(AUTOMATIC);//start PID
  }
}

/**
  Check temp if in manual run
*/
void monitorManualRun() {
  if (getIsHbridgeOn()) {
    if (getControlTcTemp() > manualMaxTemp) {
      Serial.println("!!^MAXT");//hit max temp
      heaterOff();
    } else if (isPressureHigh()) {
      Serial.println("!!^HIPR");//high pressure
      heaterOff();
    }
  } else if (isManualRun && !isPressureHigh() && getControlTcTemp() < manualMaxTemp) {
    heaterOn();
  }
}


void reducePower(int byAmount, int lowestPower = 0)
{
  int powerLevelTmp = powerVoltageValue - byAmount;
  if (powerLevelTmp < lowestPower) powerLevelTmp = lowestPower;

  if (powerLevelTmp != powerVoltageValue) {
    powerVoltageValue = powerLevelTmp;
    setHBridgeVoltage(powerVoltageValue);
    Output = powerVoltageValue;
  }
}

void incresePower(int byAmount, int highestPower = 255)
{
  int powerLevelTmp = powerVoltageValue + byAmount;
  if (powerLevelTmp > highestPower) powerLevelTmp = highestPower;

  if (powerLevelTmp != powerVoltageValue) {
    powerVoltageValue = powerLevelTmp;
    setHBridgeVoltage(powerVoltageValue);
    Output = powerVoltageValue;
  }
}

void adjustPowerLevelsForRamping()
{
  unsigned long now = millis();
  if (computeTempRisesStart == 0) {
    computeTempRisesStart = now;
    lastTemp = Input;
  }
  else if (now - computeTempRisesStart > tempRiseSampleTime) {
    computeTempRisesStart = now;
    double tempRiseInCycle = Input - lastTemp;
    lastTemp = Input;
    if (lastTemp >= Setpoint - 5.0) {
      reducePower(3);//3% is about 0.6v
      tempRiseSampleTime = 3000;
    }
    else if (tempRiseInCycle > tempRiseMaxDegrees) { 
      reducePower(6);//6% is about 1.1v
    } else if (tempRiseInCycle < tempRiseMaxDegrees) {
      incresePower(6);
    }
  }
}

void adjustPowerLevelsForConstantTemp()
{
  unsigned long now = millis();
  if (computeTempRisesStart == 0 || now - computeTempRisesStart > 1500) {
    computeTempRisesStart = now;
    double diffWithTarget = Input - Setpoint;

    // if (tempRiseInCycle>5.00) tempRiseInCycle = 5.00;
    //  else if (tempRiseInCycle< -5.00) tempRiseInCycle = -5.00;

    lastTemp = Input;
    if (diffWithTarget > 0.25) { // 60000 / 5000 = 12, 1 ºC / 12 = 0.083 ºC
      reducePower(3, lowestPower); //3% is about 0.6v
    } else if (diffWithTarget < 0.25) {
      incresePower(3, highestPower);
    }
  }
}


/**
  Turn heater on or off for this specific PID program
*/
void manageRun()
{
  if (heaterTimedOut) {
    monitorManualRun();
    return;// not running
  }

  Input = getControlTcTemp();

  if (getIsHbridgeOn()) {
    if (Input > manualMaxTemp) {
      Serial.println("!!^MAXT");//hit max temp
      heaterOff();
      return;
    } else if (isPressureHigh()) {
      Serial.println("!!^HIPR");//high pressure
      heaterOff();
      return;
    }
    else if (currentProgramMillis == 0) {
      adjustPowerLevelsForRamping();
    }
    else {

      //       double gap = abs(Setpoint-Input); //distance away from setpoint
      //        if(gap<10)
      //        {  //we're close to setpoint, use conservative tuning parameters
      //          myPID.SetTunings(conKp, conKi, conKd);
      //        }
      //        else
      //        {
      //           //we're far from setpoint, use aggressive tuning parameters
      //           myPID.SetTunings(KP, KI, KD);
      //        }

      myPID.Compute();
      //adjustPowerLevelsForConstantTemp();
      //      adjustPowerLevelsForConstantTemp();
      if (Output != lastOutput) {

        //if (Output>0.0)
        setHBridgeVoltage((int)Output);
        lastOutput = Output;
        //else heaterOff();
      }

    }

    if (Input >= Setpoint) {
      //reached target temp now set timers for current program
      startProgramTimeTrackers();
      if (maxRunTimeAllowedMillis[currentProgram] == 0) {
        heaterOff();
      }
    }

  } else {
    if (Input < Setpoint + 5.00 && !isPressureHigh()) {
      heaterOn();
    }
  }
}


void regulateHeater()
{
  unsigned long now = millis();
  if (powerheaterMillis == 0 || now - powerheaterMillis >= powerheaterCheckInterval) {
    powerheaterMillis = now;

    if (powerHeaterAutoMode) {
      checkProgramToRun();
    }
    manageRun();
  }
}


/**
  Set the run of all programs or run in manual - always on mode
*/
void startRun()
{
  if (!getIsHbridgeOn()) {
    setHBridgeSpeed(hBridgeSpeed);//reset to config setting in case user has been messing around with values
    heaterTimedOut = false;
    if (powerHeaterAutoMode) {
      windowStartTime = millis();
      runStartedMillis = millis();
      programsRunStartMillis = 0;
      currentProgramMillis = 0;
      currentRunNum = 1;
      currentProgram = 0;
      isManualRun = false;
      heaterProgramStart();
      // currentProgramMillis = millis();
      reducePower(0);//start low
      computeTempRisesStart = 0;
      //turn the PID on
      myPID.SetTunings(KP, KI, KD);
      myPID.SetMode(MANUAL);//make sure it;s off
    } else {
      //manual mode
      heaterOn();
      heaterTimedOut = true;//no timmer
      isManualRun = true;
    }
  }
}

void stopRun()
{
  heaterOff();
  //turn the PID off
  myPID.SetMode(MANUAL);
  heaterTimedOut = true;
  isManualRun = false;
  //heaterMode = -1;
  currentProgramMillis = 0;
  repeatProgramsByCount = 0;

  runStartedMillis = 0;
  programsRunStartMillis = 0;
}

void setPowerHeaterAutoMode(boolean autoMode)
{
  powerHeaterAutoMode = autoMode;
}

boolean getPowerHeaterAutoMode()
{
  return powerHeaterAutoMode;
}

boolean hasRunStarted()
{
  return (programsRunStartMillis != 0);
}

int getTotalProgramsToRun()
{
  return  repeatProgramsBy > 1 ? powerTempsCount * repeatProgramsBy : powerTempsCount;
}

int getCurrentProgramNum()
{
  return currentRunNum;
}

int getMinsToEndOfRun()
{
  unsigned long totalTimeLeft = 0;
  unsigned long millisNow = millis();

  int repeatTotal = repeatProgramsBy > 1 ? repeatProgramsBy : 1;
  unsigned long totalRunTime = programsRunStartMillis > 0 ? millisNow - programsRunStartMillis : 0;
  totalTimeLeft = (totalRunTimeMillis * (unsigned long) repeatTotal) - totalRunTime;

  return (int) (totalTimeLeft / 60000);
}

unsigned long getTotalRunningTimeMillis()
{
  return runStartedMillis > 0 ? millis() - runStartedMillis : 0;
}

int getTotalRunningTimeMins()
{
  return runStartedMillis > 0 ? getTotalRunningTimeMillis() / 60000 : 0;
}

void heaterWatchDogStart()
{
  hBridgeIsOnStartMillis = millis(); 
  hBridgeIsOnStartTemp = getControlTcTemp(); 
}

void heaterWatchDogStop()
{
  hBridgeIsOnStartMillis = 0; 
}

void actionHeaterControlCommand(char action)
{
  switch (action) {
    case '+': heaterOn(); heaterWatchDogStart(); break;
    case '-': heaterOff(); heaterWatchDogStop() break;
    case 'r': startRun(); Serial.println("@hr"); heaterWatchDogStart(); break;
    case 'a': setPowerHeaterAutoMode(true); Serial.println("@ha"); break;
    case 's': stopRun(); Serial.println("@hs"); heaterWatchDogStop(); break;
    case 'm': setPowerHeaterAutoMode(false); Serial.println("@hm"); break;
    case 'e': Serial.println("*he^"+String(getMinsToEndOfRun())); break;
    case 'c': Serial.println("*hc^"+String(getTotalProgramsToRun()));  break;
    case 'p': Serial.println("*hp^"+String(getCurrentProgramNum()));  break;
    case 't': Serial.println("*ht^"+String(getTotalRunningTimeMillis())); break;
    case 'v': //:hv^255
      setHBridgeVoltage(getValue(serial0Buffer, '^',  1).toInt());  
      Serial.println("@hv");    
      break;
    case 'f': //:hf^255
      setHBridgeSpeed(getValue(serial0Buffer, '^',  1).toInt());
      setConfigSetting("hbridge_speed", getValue(serial0Buffer, '^',  1));
      Serial.println("@hf"); 
      break;
    case 'd': //:hd^99  value should be 0-99
      setHBridgeDelay(getValue(serial0Buffer, '^',  1).toInt());
      setConfigSetting("hbridge_delay", getValue(serial0Buffer, '^',  1));
      Serial.println("@hd"); 
      break;    
  }
}

/**
 * Check we are going up in temp, if not stop heater and run
 */
void heaterWatchDog()
{
  if (hBridgeIsOnStartMillis> 0) {
    if (millis()-hBridgeIsOnStartMillis>60000) { //1 min
      if (getControlTcTemp()-hBridgeIsOnStartTemp > 2.00) {
        hBridgeIsOnStartMillis = 0;//going up in temp good, no need to check now 
      } else {
        hBridgeIsOnStartMillis = 0;
        stopRun(); // no temp raise, stop heater and run if running
        Serial.println("!h^No temp increase detected"); 
      }
    }
  }

}


