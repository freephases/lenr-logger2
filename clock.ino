///**
// * ds3231 module
// */
//#include <Wire.h>
////#define CONFIG_UNIXTIME
//#include "ds3231.h"
//
//void clockSetup()
//{    
//    Wire.begin();
//    DS3231_init(DS3231_INTCN);
//    //memset(recv, 0, BUFF_MAX);
//    //Serial.println("GET time");
//}
//
//String clockGetISOdate()
//{
//  struct ts t;
//  DS3231_get(&t);
//  char buff[21];
//   snprintf(buff, 21, "%d-%02d-%02dT%02d:%02d:%02dZ", t.year,
//             t.mon, t.mday, t.hour, t.min, t.sec);
//
//  return String(buff);
//}
//
//void clockSetDate(struct ts t)
//{
//  //TssmmhhWDDMMYYYY 
//   DS3231_set(t);
//}

