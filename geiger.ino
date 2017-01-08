short serial1BufferPos = 0; // position in read buffer
char serial1Buffer[MAX_STRING_DATA_LENGTH_SMALL + 1];
char serial1BufferInByte = 0;
int geigerCpm = 0;
/**
* LENR logger Geiger counter related functions
* GC-10 using it's tx port only
*/
int geigerGetCpm() 
{
  return geigerCpm;
}

/**
* Do something with response from geiger counter
*/
void processSerial1Response()
{
  String s(serial1Buffer);
  geigerCpm = s.toInt();
}

/**
 * Process serial data sent via serial 1 uses end of line as delimiter
 */
void processGeigerSerial()
{
  while (Serial1.available() > 0)
  {
    serial1BufferInByte = Serial1.read();

    // add to our read buffer
    serial1Buffer[serial1BufferPos] = serial1BufferInByte;
    serial1BufferPos++;

    if (serial1BufferInByte == '\n' || serial1BufferPos == MAX_STRING_DATA_LENGTH) //end of max field length
    {
      serial1Buffer[serial1BufferPos - 1] = 0; // delimited
      processSerial1Response();
      serial1Buffer[0] = '\0';
      serial1BufferPos = 0;
    }
  }
}
