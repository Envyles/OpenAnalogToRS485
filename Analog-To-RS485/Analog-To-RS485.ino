#include <Wire.h>
#include <ADS1115.h> // Manual for library: http://lygte-info.dk/project/ADS1115Library%20UK.html By HKJ from lygte-info.dk

//Settings
#define ADDRESS     1     //The Adress the system reacts to
#define RS485BAUD   9600  //RS485 Baud
#define BUFFERD     1     //Sets if the read Data is Bufferd (1 Yes / 0 No)
#define BUFFERPERC  5    //Sets the "Buffer" Percentage (Value between 1 and 99, 1 fast, 99 slow reaction)
#define DEBUG       0     //Debug output enabled (0 No Debug / 1 ADC Debug / 2 RS485 Debug infos)    

//General hardware related settings. There shoud be no reason to touch this Part
#define RS485Tx     HIGH
#define RS485Rx     LOW
#define RS485TxRx   PA1
#define CALVALUE    8000  //Value representing 1.5 Volts, for calibrating purposes
#define LIN_A       0.04  //Value representing a in ax+b (Liniarity)
#define LIN_B       0.17  //Value representing b in ax+b (Zero Point drift)
#define RESISTOR    120   //OHM for analog input  

#define ADS_SPS     ADS1115_SPEED_860SPS //Sampels per Second Settings for ADC

String addrHex;                //Communication Addr
String inputRS485Buffer = "";  //Buffer for incomming chars, till a complete Message is read
char byteReceived;             //The last byte recived
String data[8];                //The Data Fields (8, Since it shoud output the mA and Voltage)
float rawData[4];              //The Data Fields for the raw Input
long timerStart;               //Startpoint of the Timer
int adSpsTime;                 //SPS Time


ADS1115 adc;
/*
   Setup the Device
*/
void setup() {
  delay(100);
  //Init Serial
  Serial.begin(RS485BAUD * 4); //DEBUG
  Serial2.begin(RS485BAUD);

  pinMode(RS485TxRx, OUTPUT);
  digitalWrite(RS485TxRx, RS485Rx);

  //Transform the Address to HEX
  addrHex = String(ADDRESS, HEX);

  //Set the Address to two digits
  if (ADDRESS < 16) {
    addrHex = "0" + addrHex;
  }
  addrHex.toUpperCase();

  //Print it once if DEBUG == 1
  if (DEBUG == 1) {
    delay(1000);
    Serial.print(addrHex);
  }

  //Initialise the output array. Im Using a ASCII encoded Format, basicly a Float in its String representation.
  for (int i = 0; i < 8; i++) {
    data[i] = "+000.00";
  }
  for (int i = 0; i < 4; i++) {
    rawData[i] = 0;
  }

  Wire.begin();
  //8 Samples per second since it has the lowest noise
  adc.setSpeed(ADS_SPS);

  //Not perfect since the timer derivates at higher speeds, but shoud be okay!
  int fakt;
  fakt = 8;
  for (int i = 0; i < ADS_SPS; i++) {
    fakt = fakt * 2;
  }
  adSpsTime = 1000 / fakt * 8;

  //Set the Timer to the current Runtime
  timerStart = millis();
}

/*
   Main programm loop
*/
void loop()
{
  //Check ADC
  if ( millis() - timerStart > adSpsTime ) {
    readValuesIntoBuffer();
  }

  //check Communications
  if (Serial2.available())
  {
    byteReceived = Serial2.read();    // Read received byte
    //Keep reading till you recive a \r
    if (byteReceived != '\r') {
      inputRS485Buffer = inputRS485Buffer + byteReceived;
    } else {
      if (DEBUG == 2) {
        Serial.print("IN:");
        Serial.println(inputRS485Buffer);
      }

      //The software i used to test this piece of code requierd a awnser to a "%" Call to recognise it
      if (inputRS485Buffer.charAt(0) == '%') {
        if (inputRS485Buffer.charAt(1) == addrHex.charAt(0) && inputRS485Buffer.charAt(2) == addrHex.charAt(1)) {
          digitalWrite(RS485TxRx, RS485Tx);  // Enable RS485 Transmit
          Serial2.print("!" + addrHex);
          Serial2.write('\r');
          delay((int)((4 * 1100) / (RS485BAUD / 8) + 10));                   // Wait for the complete transmit of 4 symbols | (RS485BAUD/8) 1 baud is 1 bit, so a factor of 8 is used to convert it to byte |1100 instead of 1000 to give it a aditional 10% buffertime
          digitalWrite(RS485TxRx, RS485Rx);  // Disable RS485 Transmit
          if (DEBUG == 2) {
            Serial.print("OUT:");
            Serial.println("!" + addrHex);
          }
        }
      } else if (inputRS485Buffer.charAt(0) == '#') {
        //Data has been requestet. Check if the requested Addr is equal to the Module address and awnser.
        if (inputRS485Buffer.charAt(1) == addrHex.charAt(0) && inputRS485Buffer.charAt(2) == addrHex.charAt(1)) {
          digitalWrite(RS485TxRx, RS485Tx);  // Enable RS485 Transmit
          String tmpOut;
          tmpOut = ">";
          for (int i = 0; i < 8; i++) {
            tmpOut = tmpOut + data[i];
          }
          Serial2.print(tmpOut);
          Serial2.write('\r');
          delay((int)((tmpOut.length() * 1100) / (RS485BAUD / 8) + 10));                   // Wait for the complete transmit of the symbols | (RS485BAUD/8) 1 baud is 1 bit, so a factor of 8 is used to convert it to byte |1100 instead of 1000 to give it a aditional 10% buffertime
          digitalWrite(RS485TxRx, RS485Rx);  // Disable RS485 Transmit
          if (DEBUG == 2) {
            Serial.print("OUT:");
            Serial.println(tmpOut);
          }
        }
      }
      inputRS485Buffer = "";
    }
  }
}

void readValuesIntoBuffer() {
  if (BUFFERD == 1) {
    rawData[0] = (rawData[0] * (100 - BUFFERPERC) +  (float)adc.convert(ADS1115_CHANNEL0, ADS1115_RANGE_6144) * BUFFERPERC) / 100;
    rawData[1] = (rawData[1] * (100 - BUFFERPERC) +  (float)adc.convert(ADS1115_CHANNEL1, ADS1115_RANGE_6144) * BUFFERPERC) / 100;
    rawData[2] = (rawData[2] * (100 - BUFFERPERC) +  (float)adc.convert(ADS1115_CHANNEL2, ADS1115_RANGE_6144) * BUFFERPERC) / 100;
    rawData[3] = (rawData[3] * (100 - BUFFERPERC) +  (float)adc.convert(ADS1115_CHANNEL3, ADS1115_RANGE_6144) * BUFFERPERC) / 100;
  } else {
    rawData[0] = (float)adc.convert(ADS1115_CHANNEL0, ADS1115_RANGE_6144);
    rawData[1] = (float)adc.convert(ADS1115_CHANNEL1, ADS1115_RANGE_6144);
    rawData[2] = (float)adc.convert(ADS1115_CHANNEL2, ADS1115_RANGE_6144);
    rawData[3] = (float)adc.convert(ADS1115_CHANNEL3, ADS1115_RANGE_6144);
  }

  for (int i = 0; i < 4; i++) {
    float Voltage = rawData[i] / CALVALUE * 1.5; //Multiply by 1.5 since this is the Value used as test reference
    float AMP = rawData[i] / (RESISTOR * (CALVALUE / 1000)) * 1.5;
    //Calibration:
    float AMP2 = AMP + LIN_B + (LIN_A * (AMP-4)); //calibrating AMP
    Voltage = Voltage * (AMP2/AMP); //Correcting the Voltage
    AMP = AMP2;
    
    data[i] = buildSignalValue(String(AMP));
    data[i + 4] = buildSignalValue(String(Voltage));
  }

  if (DEBUG == 1) {
    //DEBUG OUTPUT
    Serial.print(String(data[0]));
    Serial.print(';');
    Serial.print(String(data[1]));
    Serial.print(';');
    Serial.print(String(data[2]));
    Serial.print(';');
    Serial.print(String(data[3]));
    Serial.print(';');
    Serial.print(String(data[4]));
    Serial.print(';');
    Serial.print(String(data[5]));
    Serial.print(';');
    Serial.print(String(data[6]));
    Serial.print(';');
    Serial.print(String(data[7]));
    Serial.println(';');
  }
}


String buildSignalValue(String Value) {
  bool negative;
  negative = false;
  Value.trim();

  if (Value.indexOf("?") > -1) {
    return "-999.00";
  }

  if (Value.indexOf("-") > -1) {
    negative = true;

    //Entferne das -
    Value.replace("-", " ");
    Value.trim();
  }

  if (!(Value.indexOf(".") > -1)) {
    if (Value.length() < 5) {
      Value = Value + ".0";
    }
  }

  while (Value.length() < 6) {
    Value = "0" + Value;
  }

  if (negative) {
    Value = "-" + Value;
  } else {
    Value = "+" + Value;
  }
  return Value;
}
