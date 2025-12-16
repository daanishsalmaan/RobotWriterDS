/*
  Example of processing incoming serial data without blocking.

  Author:   Nick Gammon
  Date:     13 November 2011.
  Modified: 31 August 2013.

  Adjusted by J.Bonnyman for Mechatronics Lab testing
  Further modified by I A Jones to add initial message including $ and LED blink
  
  Released for public use.
*/

// How much serial data we expect before a newline
const unsigned int MAX_INPUT = 100;   // Expanded to allow for any 'long' G-Code lines

boolean ledState = false;

long waitPeriod = 500;

void setup ()
{
  Serial.begin (115200);
  Serial.println("Test sketch to emulate writing robot $");
  pinMode(LED_BUILTIN, OUTPUT);
} // end of setup

void loop()
{
  
  // if serial data available, process it
  if (Serial.available() > 0)
    processIncomingByte(Serial.read ());

}  // end of loop

// Send back on the serial line once we have received a line of data (always 'OK' in this example)
void process_data (const char * data)
{
  Serial.println (data);
}  // end of process_data

void processIncomingByte (const byte inByte)
{
  // Static used so that input_line can continue to have characters appended, likewise the count is held on return
  // This approach avoids the use of global variables :-)
  
  static char input_line[MAX_INPUT];
  static unsigned int input_pos = 0;

  switch (inByte)
  {

    case '\n':   // end of text
      input_line[input_pos] = 0;  // terminating null byte

      // terminator reached - we simply need to send back 'ok' (rather than echoing the text sent as per the original example)
      process_data("ok");
      ledState = !ledState;  // Toggle the LED state 
      digitalWrite(LED_BUILTIN, ledState);
      delay(waitPeriod);


      // reset buffer for next time
      input_pos = 0;
      break;

    case '\r':   // discard carriage return
      break;

    default:
      // keep adding if not full ... allow for terminating null byte
      // Not really needed for the task but seemed good to keep it (in case we perhaps add a LCD 
      // to use when testing in future).
      
      if (input_pos < (MAX_INPUT - 1))
        input_line[input_pos++] = inByte;
      break;

  }  // end of switch

} // end of processIncomingByte
