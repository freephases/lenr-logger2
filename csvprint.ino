/**
* LENR logger
*
* Prints out results to CSV strings and Serial 
*/

/**
* get CSV string of headers for all enabled sensors
*/
String getCsvStringHeaders()
{
   String ret = String("\"Time\"");
     
   if (isSensorEnabled("Power")) {
     ret = ret +",\"Power\"";
     ret = ret +",\"Amps\"";
     ret = ret +",\"Volts\"";
   }
   if (isSensorEnabled("Pressure")) {
     ret = ret +",\"PSI\"";
   }
   if (isSensorEnabled("Geiger")) {
     ret = ret +",\"CPM\"";
   }

   for(int i=0; i<getThermocouplesCount(); i++) {
     ret = ret + ",\"Thermocouple"+(i+1)+"\"";
   }

 

  
   
   return ret;
}


/**
* Get CSV string of all enabled sensor readings
*/
String getCsvString(char delimiter = ',', boolean addMillis = true, int maxTCTemps = 0)
{
  String ret = String("");
  if (addMillis) {
    ret = String(millis());        
  } else {
    ret = String("*D^");   
  }
  
    char s_[15];
    String tmp;
   /*if (isSensorEnabled("TC1")) {
     dtostrf(getThermocoupleAvgCelsius1(),2,3,s_);
     String tmp = String(s_);
     if (ret.length()>0) ret = ret + delimiter;
     ret = ret + tmp;
     
   }
   if (isSensorEnabled("TC2")) {     
     dtostrf(getThermocoupleAvgCelsius2(),2,3,s_);
     String tmp = String(s_);
    if (ret.length()>0) ret = ret + delimiter;
     ret = ret + tmp;
   }*/
   
   
   if (isSensorEnabled("Power")) {    
     dtostrf(getPower(),2,3,s_);
     tmp = String(s_);
    if (ret.length()>0) ret = ret + delimiter;
     ret = ret + tmp;
     dtostrf(getHbridgeAmps(),2,3,s_);
     tmp = String(s_);
    if (ret.length()>0) ret = ret + delimiter;
     ret = ret + tmp;

    dtostrf(getHbridgeVolts(),2,3,s_);
     tmp = String(s_);
    if (ret.length()>0) ret = ret + delimiter;
     ret = ret + tmp;
     
   } 

   if (isSensorEnabled("Pressure")) {
     dtostrf(getPressurePsi(),2,3,s_);
     tmp = String(s_);
     if (ret.length()>0) ret = ret + delimiter;
     ret = ret + tmp;
   }
   
   if (isSensorEnabled("Geiger")) {
     //dtostrf(geigerGetCpm(),2,3,s_);
     tmp = String(geigerGetCpm());
     if (ret.length()>0) ret = ret + delimiter;
     ret = ret + tmp;
   }

   int maxTcToSend = getThermocouplesCount();
   if (maxTCTemps!=0) maxTcToSend = maxTCTemps;
   for(int i=0; i< maxTcToSend; i++) {
       dtostrf(thermocopulesList[i]->getTemp(),2,3,s_);
       tmp = String(s_);
      if (ret.length()>0) ret = ret + delimiter;
       ret = ret + tmp;            
   }

  
    
   
   return ret;
}

/**
* Print headers in to file if writing a line for the first time since opening
*/
boolean sdCardFirstLineLogged = false;

/**
* Save all enabled sensors values to SD card CSV file DATALOG
*/
void saveCsvData()
{
  if (!sdCardFirstLineLogged) {
    saveLineToDatalog(getCsvStringHeaders());
 }
 sdCardFirstLineLogged = true;
 saveLineToDatalog(getCsvString()); 
 
  if (debugToSerial) {
      Serial.println("csv data saved to datalog file");
    }
}

/**
* Print result to serial 0  for interfacing with a program on a PC over USB
*/
void printRawCsv() {    
  Serial.println(getCsvString(',', false));
}


