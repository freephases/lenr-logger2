/**
* Heater Control, controls SSR or/and Hbridge to heater power supply
* using PID lib that uses up to 5 specific run programs taken from
* "power_off_temp" config file setting (see: config tab/file)
*/
const unsigned long powerheaterCheckInterval = 1200;
unsigned long powerheaterMillis = 0;
//OnOff heaterPowerControl(36);
//OnOff heaterPowerControl2(37);
boolean heaterTimedOut = true; // true if reached maxRunTimeAllowedMillis
#define PID_MAX_PROGRAMS 5
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
const unsigned long WindowSize = 101;
unsigned long windowStartTime = 0;
unsigned long computeTempRisesStart = 0;
double lastTemp = 0.000;
int powerVoltagePercentage = 0;// 0% = 10v on step down, 100 = ~49.8v on step down
int lowestPower = 0;
int highestPower = 100;

////                                  ,Kp,Ki,Kd,
PID myPID(&Input, &Output, &Setpoint, KP, KI, KD, DIRECT);
//double consKp=1, consKi=0.05, consKd=0.25;

void heaterOn()
{
  if (!getIsHbridgeOn()) {
    lastTurnedOnMillis = millis();
    lastTurnedOffMillis = 0;
    computeTempRisesStart = 0;//reset computeTempRisesStart
    hBridgeTurnOn();
    if (debugToSerial) {
      Serial.println("Heater on");
    }
  }
}

/**
* heaterOff
*
* @param doNotCheckOnStatus boolean - if true do not check if heater is on before turning off, default is to check (false)
*/
void heaterOff(boolean doNotCheckOnStatus = false)
{
  if (doNotCheckOnStatus || getIsHbridgeOn()) {
    lastTurnedOffMillis = millis();
    lastTurnedOnMillis = 0;
    hBridgeTurnOff();

    if (debugToSerial) {
      Serial.println("Heater off");
    }
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
          Serial.print ("Error reading power_off_temp value: ");
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
        Serial.println("Power temp ");
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
* Load power/PID settings
*/
void powerheaterSetup()
{
  heaterOff(true);//makes sure it's off incase of reboot without a turn off heater command before hand

  loadHeaterSettings();
  Input = 0;
  Setpoint = powerOffTemps[0];
  myPID.SetOutputLimits(0, WindowSize);
  
  myPID.SetSampleTime(LL_TC_READ_INTERVAL);//200mS is default
  //powerheaterMillis = millis() - powerheaterCheckInterval;
}

/**
* `Set PID to use specific PID program
*/
void heaterProgramStart()
{
  Input = getControlTcTemp();
  Setpoint = powerOffTemps[currentProgram];
  int i = map((int)Setpoint, -200, 950, 0,100);
  setHBridgeVoltage(i);
  powerVoltagePercentage =i; 
  Output = powerVoltagePercentage;
  delay(60);
}

/**
* Check and switch program to run or end run
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
* set timer watchers
*/
void startProgramTimeTrackers()
{
  if (programsRunStartMillis == 0)
  {
    programsRunStartMillis = millis();
  }
  if (currentProgramMillis == 0) {
      currentProgramMillis = millis();      
      lowestPower = powerVoltagePercentage-7;//we make lowest 6% lower then current power
      if (lowestPower<0) lowestPower - 0;
      highestPower = powerVoltagePercentage;//we know this power can get to given temp
      if (highestPower<0) highestPower = 6;
      myPID.SetOutputLimits(lowestPower, highestPower);
      myPID.SetMode(AUTOMATIC);//start PID      
  }
}

/**
* Check temp if in manual run
*/
void monitorManualRun() {  
  if (getIsHbridgeOn()) {
    if (getControlTcTemp() > manualMaxTemp || isPressureHigh()) {
      heaterOff();
    }
  } else if (isManualRun && !isPressureHigh() && getControlTcTemp() < manualMaxTemp) { 
      heaterOn();
  }
}


void powerDebug() {
//  if (debugToSerial) {
//    Serial.print("vp: "); Serial.print(powerVoltagePercentage); Serial.print(" ");
//    Serial.print("setpoint: "); Serial.print(Setpoint); Serial.print(" ");
//    Serial.print("input: "); Serial.print(Input); Serial.print(" ");
//    Serial.print("output: "); Serial.print(Output); Serial.print(" ");
//    //
//    Serial.print("kp: "); Serial.print(myPID.GetKp()); Serial.print(" ");
//    Serial.print("ki: "); Serial.print(myPID.GetKi()); Serial.print(" ");
//    Serial.print("kd: "); Serial.print(myPID.GetKd()); Serial.println();
//  }
}

void reducePower(int byPercent, int lowestPower=0)
{
  int powerLevelTmp = powerVoltagePercentage-byPercent;
  if (powerLevelTmp<lowestPower) powerLevelTmp = lowestPower;
  
  if (powerLevelTmp!=powerVoltagePercentage) {
    powerVoltagePercentage = powerLevelTmp;
    setHBridgeVoltage(powerVoltagePercentage);
    Output = powerVoltagePercentage;
  }
}

void incresePower(int byPercent, int highestPower=100)
{
  int powerLevelTmp = powerVoltagePercentage+byPercent;
  if (powerLevelTmp>highestPower) powerLevelTmp = highestPower;
  
  if (powerLevelTmp!=powerVoltagePercentage) {
    powerVoltagePercentage = powerLevelTmp;
    setHBridgeVoltage(powerVoltagePercentage);
    Output = powerVoltagePercentage;
  }
}

void adjustPowerLevelsForRamping()
{  
  unsigned long now = millis();
  if (computeTempRisesStart==0) {
    computeTempRisesStart = now;
    lastTemp = Input;
  }
  else if (now-computeTempRisesStart>tempRiseSampleTime) {
    computeTempRisesStart = now;
    double tempRiseInCycle = Input-lastTemp;   
    lastTemp = Input;
    if (lastTemp>=Setpoint-5.0) {
      reducePower(1);//3% is about 1.1v
      tempRiseSampleTime = 3000;
    }
    else if (tempRiseInCycle>tempRiseMaxDegrees) { // 60000 / 5000 = 12, 1 ºC / 12 = 0.083 ºC
     // int byPercent = 100 / ddd; //tempRiseInCycle/100; 1
      reducePower(3);//3% is about 1.1v
    } else if (tempRiseInCycle<tempRiseMaxDegrees) {
      incresePower(3);
    }
  } 
}

void adjustPowerLevelsForConstantTemp()
{  
  unsigned long now = millis();
  if (computeTempRisesStart==0 || now-computeTempRisesStart>1500) {
    computeTempRisesStart = now;
    double diffWithTarget = Input-Setpoint;
    
   // if (tempRiseInCycle>5.00) tempRiseInCycle = 5.00;
  //  else if (tempRiseInCycle< -5.00) tempRiseInCycle = -5.00;
       
    lastTemp = Input;
    if (diffWithTarget>0.25) { // 60000 / 5000 = 12, 1 ºC / 12 = 0.083 ºC
      reducePower(1,lowestPower);//3% is about 1.1v
    } else if (diffWithTarget< 0.25) {
      incresePower(1, highestPower);
    }
  } 
}


/**
* Turn heater on or off for this specific PID program
*/
void manageRun()
{
  if (heaterTimedOut) {
    monitorManualRun();
    return;// not running
  }

  Input = getControlTcTemp();

  if (getIsHbridgeOn()) {
    if (currentProgramMillis==0) {
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
      if (Output!=lastOutput) {
        
        //if (Output>0.0) 
        setHBridgeVoltage((int)Output);
        lastOutput = Output;
        //else heaterOff();
      }
      
    }
  
    if (Input >= Setpoint) {
      //reached target temp now set timers for current program
      startProgramTimeTrackers();
      if (maxRunTimeAllowedMillis[currentProgram]==0) {
        heaterOff();
      } //else if (Input > Setpoint+5.00) {
        //heaterOff();
      //}
    } 
    if (isPressureHigh()) {
      heaterOff();
    }
  } else {
     if (Input < Setpoint+5.00 && !isPressureHigh()) {
      //reached target temp now set timers for current program
      //startProgramTimeTrackers();
      heaterOn();
    }
  }
  //Serial.println(millis() - windowStartTime);
 /* if (Output < millis() - windowStartTime) {
    heaterOff();
  }
  else {
    heaterOn();
  }*/
//  if (maxRunTimeAllowedMillis[currentProgram]!=0) {
//    unsigned long now = millis();
//    if(now - windowStartTime>WindowSize)
//    { //time to shift the Relay Window
//      windowStartTime += WindowSize;
//    }
//    
//    if(Output > now - windowStartTime) heaterOn();
//    else heaterOff();
//  }  
  //powerDebug();
}


void powerheaterLoop()
{
//  if (!heaterTimedOut && powerHeaterAutoMode && millis() - powerheaterMillis >= powerheaterCheckInterval) {
//    powerheaterMillis = millis();
//    checkProgramToRun();
//    manageRun();
//  }
}

void heaterRegulator()
{
   if (powerHeaterAutoMode)
      checkProgramToRun();
    manageRun(); 
}


/**
* Set the run of all programs or run in manual - always on mode
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


void actionHeaterControlCommand(char command)
{
  //@[command][action]=ok
  //*[command][action]=responde to request for info
  switch(command) {
    case '+': heaterOn(); Serial.print("@h+"); break;
    case '-': heaterOff(); Serial.print("@h-"); break;
    case 'r': startRun(); Serial.print("@hr"); break;
    case 'a': setPowerHeaterAutoMode(true); Serial.print("@ha"); break;
    case 's': stopRun(); Serial.print("@hs"); break;
    case 'm': setPowerHeaterAutoMode(false); Serial.print("@hm"); break;
    case 'e': Serial.print("*he");  Serial.println(getMinsToEndOfRun()); break;
    case 'p': Serial.print("*hp");  Serial.println(getCurrentProgramNum());  break;
    case 't': Serial.print("*ht");  Serial.println(getTotalRunningTimeMillis()); break;
    case 'c': Serial.print("*hc");  Serial.println(getTotalProgramsToRun());  break;
  }
}


