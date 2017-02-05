/**
  LENR logger pressure related monitoring functions
*/

float pressurePsi = 0.000;
//float pressurePsiTotal = 0.000;
int pressureReadCounter = 0;
const int pressureReadCount = 20;
//unsigned long pressureRawV = 0;
const int pressurePort = A0;//LL_PIN_PRESSURE;
const int readPressureInterval = 110;
unsigned long readPressureMillis = 0;

float calFactor = 0.00;

RunningMedian psiSamples = RunningMedian(pressureReadCount);

void setupPressure()
{
  pinMode(pressurePort, INPUT);
  calFactor =  1023.00 / calibratedVoltage;
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

//void readPressureOLD()
//{
//  /*
//    Pressure Range:  -14.5~30 PSI
//    Input :4.980 (last mesured, using own varible VR's so can set voltage
//    Output: 0.5-4.5V linear voltage output .
//    4.980 is mega when using wall socket
//  */
//  if (millis() - readPressureMillis >= readPressureInterval) {
//    readPressureMillis = millis();
//    pressureRawV += analogRead(pressurePort);
//    //pressurePsi = mapfloat(pressureRawV, (int) calFactor*0.5, (int) calFactor*4.5, -14, 30);
//    //pressurePsiTotal += mapfloat((float)pressureRawV, calFactor*0.5, calFactor*4.5, -14.5, 30.0);
//    pressureReadCounter++;
//    if (pressureReadCounter == pressureReadCount) {
//      int p  = pressureRawV / pressureReadCount;
//      pressurePsi = mapfloat((float)p, calFactor * 0.5, calFactor * 4.5, -14.5, 30.0);
//      //make rounded double with one digit precision
//      if (pressurePsi > 0.10 || pressurePsi < -0.10)
//        pressurePsi = round(pressurePsi * 10) / 10;
//      //String s = String(pressurePsi, 1);
//      //pressurePsi = s.toFloat();
//      pressureReadCounter = 0;
//      pressureRawV = 0;
//      if (debugToSerial) {
//        // Serial.print("Pressure raw: ");
//        // Serial.print(pressureRawV);
//        Serial.print(", mapped PSI: ");
//        Serial.println(pressurePsi, DEC);
//      }
//    }
//  }
//}
int count = 0;

//todo > tidy up!!!

void pressureRead()
{
  if (millis() - readPressureMillis >= readPressureInterval) {
    readPressureMillis = millis();

    count++;
    //calFactor =  1023.00 / calibratedVoltage;
    int x = analogRead(pressurePort);
    
    psiSamples.add(x);//mapfloat((float) x, calFactor * 0.5, calFactor * 4.5, -14.5, 30.0));

    if (count == pressureReadCount) {
     // calibratedVoltage = readVcc();
      count = 0;
       
      // calFactor =  1023.00 / calibratedVoltage;//+0.0078);//(readVcc()-0.6); //calibratedVoltage;
       //String s(mapfloat(psiSamples.getMedian(), calFactor * 0.5, calFactor * 4.5, -14.5, 30.0);
     // int psis = ((int) psiSamples.getMedian())*100;
      pressurePsi = ((int)(mapfloat(psiSamples.getMedian(), calFactor * 0.5, calFactor * 4.5, -14.5, 30.0)*10))/10.00;//
        //psiSamples.clear();
      //if (pressurePsi>-0.10&&pressurePsi<0.10) pressurePsi  = 0.00;
      //else
      
      //pressurePsi = round(pressurePsi * 10) / 10; 
      //http://forum.arduino.cc/index.php?topic=86779.0

       
      if (pressurePsi == -0.00) pressurePsi = 0.00;
     // if (debugToSerial) {
        Serial.print("D^mapped PSI: ");
        Serial.println(pressurePsi, DEC);
     // }
    }
  }
}

float getPressurePsi()
{
  return pressurePsi;
}

boolean isPressureHigh()
{
  return (pressurePsi >= highestPressure || pressurePsi <= lowestPressure);
}

