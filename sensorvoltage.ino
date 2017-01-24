//#include <Wire.h>
//#include <Adafruit_INA219.h>
//
//Adafruit_INA219 ina219;
//float expansionBoardVoltage = 0;
//float expansionBoardCurrent = 0;
//
//
//
//void sensorVoltageSetup() {
//  ina219.begin();
//}
//
//float getExpansionBoardVoltage() 
//{
//  return expansionBoardVoltage;
//}
//
//float getExpansionBoardCurrent() 
//{
//  return expansionBoardCurrent;
//}
//
//void readExpansionBoardVoltage()
//{
//  
//    float shuntvoltage = 0;
//    float busvoltage = 0;
//
//    //  float loadvoltage = 0;
//
//    shuntvoltage = ina219.getShuntVoltage_mV();
//    busvoltage = ina219.getBusVoltage_V();
//    expansionBoardCurrent = ina219.getCurrent_mA();
//    if (expansionBoardCurrent < 0.00) expansionBoardCurrent = 0.000;
//    expansionBoardVoltage = busvoltage + (shuntvoltage / 1000);
//    if (expansionBoardVoltage < 0.00) expansionBoardVoltage = 0.000;
//}

