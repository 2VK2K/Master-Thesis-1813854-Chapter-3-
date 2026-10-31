// Includes:
#include <Wire.h>;

// Pin alias:
const uint Latch1_4 = 3;
const uint Latch5_8 = 2;
const uint LEDPin = LED_BUILTIN;
bool LedVal = true;
bool newData = false;
const int MotorCount = 8;
// Serial data processing:
const byte numChars = 32;
char receivedChars[numChars];
char TempChars[numChars];        // temporary array for use when parsing  
// variables to hold the parsed data
//char MessageFromPC[numChars] = {0};
char chCMD = 0x00;
char PCValC = 0x00;
int PCValI = 0;
unsigned int PCValM = 0;
float PCValF = 0;
const int MotorMaxAmp = 1023; // Max amplitude for motor speed.
const float SampleRate = 1000; // Current set up on 1kHz output
const float pix2 = 6.283185307; // Save time by not working this out each time. It will be accurate enough.

const unsigned long MaxTimer = 4294967295;

// Helper stuff:
//Helpers Helpers(LEDPin);

// Timing variables:
bool Flagms = false;
bool Flagcs = false;
bool Flagds = false;
unsigned int Thous;
unsigned int Hunds;
unsigned int Tiens;
unsigned long Currentms = 0;
unsigned long Currentus = 0;

// Structure for motor values:
struct MOTOR{
  // Mapping:
  int TacPos;     // Tacho pin location
  bool TacVal;    // Last state of tacho pin, used for edge detection.
  int DirPos;     // Which pin is the direction switch on?
  int PowPos;     // Which pin is the power switch on?
  uint8_t DACPos; // Which DAC register for the motor?
	// Speed variables:
	int UseSpeed;	  // The user inputs to this value
	int SetSpeed;	  // The system pushes this value to the DAC
	int GetSpeed;	  // This is the speed read from the tacho
	unsigned long TachoInterval; // This allows us to get the speed from the tacho.
  unsigned long LastTacho; // The last Tacho value - use with micros() to catch the value for up a level
	// Stuff to auto-vary the speed (and direction) of the motor
	bool RunSine;	  // Do we run the sine function or not? Use this value to check if i need to UseSpeed >> SetSpeed
	float SetFreq;	// The frequency the of the sine function.
	float MStep;	  // Step point used for the sine wave
  float MInc;     // Increment size
	// Control variables:
	bool DirCW;		  // Clockwise direction, true or false; false for "negative" speeds
	bool OnOff;		  // Is the motor powered up or not?
};

// Declare motor structures:
MOTOR mot[MotorCount];

char * strtokIndx; // this is used by strtok() as an index

// IO Expander:
//Adafruit_MCP23X17 mcpIOX;
//uint8_t IntGPIOA = 0x00;
//uint8_t IntGPIOB = 0x00;
const uint8_t IOX_BASE_ADDR = 0x20;
const uint8_t IOX_VAL_ADDR = 0x14;
const uint8_t IOX_DIR_ADDR = 0x00;
int GPIOVal = 0x0000;
bool UDIOX = true;

// DAC:
// Left in for now - need a better library
// Adafruit_MCP4728 mcpDAC;
const uint8_t DAC_BASE_ADDR = 0x60;
const uint8_t DAC_CMD_WR = 0x00;
// Command is AAAAA 00 X
const uint8_t DAC_REG_0 = 0x00;
const uint8_t DAC_REG_1 = 0x08;
const uint8_t DAC_REG_2 = 0x10;
const uint8_t DAC_REG_3 = 0x18;
const uint8_t DAC_REG_4 = 0x20;
const uint8_t DAC_REG_5 = 0x28;
const uint8_t DAC_REG_6 = 0x30;
const uint8_t DAC_REG_7 = 0x38;
const uint8_t DAC_REG_VREF = 0x40;
const uint8_t DAC_REG_PD = 0x48;
const uint8_t DAG_REG_GAIN = 0x50;
const int DAC_VREF = 0x0000; // Use VDD, unbuffered.
const int DAC_PD_4Count = 0xFF00; // Power down motors 5-8
const int DAC_PD_6Count = 0xF000; // Power down motors 7-8
const int DAC_PD_8Count = 0x0000; // Power down no motors
const int DAC_GAIN = 0x0000; // Set gain to 1x.
unsigned int MotorIndex = 0;
bool UDDAC = true;

//
void setup() {
  //Helpers.begin();
  pinMode(LEDPin, OUTPUT);
  digitalWrite(LEDPin, HIGH); // Active LOW signal
  // Motors.begin();
  // Set up serial bus:
  Serial.begin(115200);
  //Serial.println("Bus Active");
  // Set up I2C bus
  Wire.begin();
  Wire.setClock(400000);

// The mapping here is wrong - should be other way round within byte I think!!!!!!!!!!!!!
  mot[0] = {0, false, 9, 8, DAC_REG_0, 0, 0, 0, 0, 0, false, 0, 0, false, false}; // Motor defaults to all off
  mot[1] = {1, false, 11, 10, DAC_REG_2, 0, 0, 0, 0, 0, false, 0, 0, false, false}; // Motor defaults to all off
  mot[2] = {4, false, 13, 12, DAC_REG_4, 0, 0, 0, 0, 0, false, 0, 0, false, false}; // Motor defaults to all off
  mot[3] = {5, false, 15, 14, DAC_REG_6, 0, 0, 0, 0, 0, false, 0, 0, false, false}; // Motor defaults to all off
  mot[4] = {7, false, 1, 0, DAC_REG_7, 0, 0, 0, 0, 0, false, 0, 0, false, false}; // Motor defaults to all off
  mot[5] = {6, false, 3, 2, DAC_REG_5, 0, 0, 0, 0, 0, false, 0, 0, false, false}; // Motor defaults to all off
  mot[6] = {9, false, 5, 4, DAC_REG_3, 0, 0, 0, 0, 0, false, 0, 0, false, false}; // Motor defaults to all off
  mot[7] = {8, false, 7, 6, DAC_REG_1, 0, 0, 0, 0, 0, false, 0, 0, false, false}; // Motor defaults to all off

  // put your setup code here, to run once:
  pinMode(Latch1_4, OUTPUT);
  digitalWrite(Latch1_4, HIGH); // Active LOW signal
  pinMode(Latch5_8, OUTPUT);
  digitalWrite(Latch5_8, HIGH); // Active LOW signal
  // Tacho inputs:
  /*
  for (int i=0; i< MotorCount; i++){
    pinMode(mot[i].TacPos, INPUT);
  }*/
  pinMode(mot[0].TacPos, INPUT);

  // Test if IOX is available:
  // Should test if DAC is available:

  // Initialise:
  // IOX:
  IOXBegin();
  DACBegin();
}

void loop() {
  // Timer:
  TickTock();
  // Serial receiver:
  recvWithStartEndMarkers();
  CRealReceiver();
  // Check Tacho pins:
  SpeedCheck();
  // "RT"OS bits...
  if (Flagms == true) {
    // Latch the DACs
    DACLatch();
    Flagms = false;
    // 1 ms tick
    // Check for sine calcs:
    FuncSine();
    // Update DAC values as required:
    if (UDIOX == true){
      IOXUpdate();
      UDIOX = false;
    }
  }

  if (Flagcs == true){
    Flagcs = false;
  }
  if (Flagds == true){
    Flagds = false;
     if (LedVal == true){
      LedVal = false;
      digitalWrite(LEDPin, LOW);
    }else if (LedVal == false){
      LedVal = true;
      digitalWrite(LEDPin, HIGH);
    }
  }
}

void IOXBegin(){
  Wire.beginTransmission(IOX_BASE_ADDR);
  Wire.write(IOX_DIR_ADDR);
  Wire.write(0x00); // Set GPIOA as output
  Wire.write(0x00); // Set GPIOB as output
  Wire.endTransmission();
}

void IOXUpdate(){
  Wire.beginTransmission(IOX_BASE_ADDR);
  Wire.write(IOX_VAL_ADDR);
  Wire.write(lowByte(GPIOVal)); // A before B
  Wire.write(highByte(GPIOVal));
  Wire.endTransmission();
}

void DACUpdate(int i){
  Wire.beginTransmission(DAC_BASE_ADDR);
    Wire.write(mot[i].DACPos);
    Wire.write(highByte(mot[i].SetSpeed));
    Wire.write(lowByte(mot[i].SetSpeed));
//  }
  Wire.endTransmission();
}

void DACBegin(){
  Wire.beginTransmission(DAC_BASE_ADDR);
  Wire.write(DAC_REG_PD);
  Wire.write(DAC_PD_4Count);
  Wire.endTransmission();
  for (int i=0; i < MotorCount; i++){
    DACUpdate(i);
    
  }
}

void DACLatch(){
  digitalWrite(Latch1_4, LOW); // Active LOW signal
  digitalWrite(Latch1_4, HIGH); // Active LOW signal
  digitalWrite(Latch5_8, LOW); // Active LOW signal
  digitalWrite(Latch5_8, HIGH); // Active LOW signal
}

void FuncSine(){
  for (int i = 0; i < MotorCount; i++){
    if (mot[i].RunSine == true){
      mot[i].UseSpeed = ((sin(mot[i].MStep) * (MotorMaxAmp + 1)) + MotorMaxAmp);
      mot[i].MStep += mot[i].MInc;
      if (mot[i].MStep > pix2){
        mot[i].MStep -= pix2;
      }
      if (mot[i].UseSpeed < 0){
        mot[i].SetSpeed = mot[i].UseSpeed * -1;
        bitWrite(GPIOVal, mot[MotorIndex].DirPos, 0);
      } else if (mot[i].UseSpeed > 0){
        mot[i].SetSpeed = mot[i].UseSpeed;
        bitWrite(GPIOVal, mot[MotorIndex].DirPos, 1);
      }
    UDIOX = true;
    DACUpdate(i);
    }
  }
}

void SpeedCheck(){
  for (int i = 0; i < MotorCount; i++){
    if ((mot[i].TacVal == false) && (digitalRead(mot[i].TacPos) == HIGH)){
      Currentus = micros();
      mot[i].TacVal = true;
      // Rollover occurred
      if (mot[i].LastTacho > Currentus){
        mot[i].TachoInterval = Currentus + (MaxTimer - mot[i].LastTacho);
      } else if (mot[i].LastTacho < Currentus){
        mot[i].TachoInterval = Currentus - mot[i].LastTacho;
      }
      mot[i].LastTacho = Currentus;
      mot[i].GetSpeed = 60000000/(6*mot[i].TachoInterval);
      //Serial.println(mot[i].TachoInterval); 
    } else if ((mot[i].TacVal == true) && (digitalRead(mot[i].TacPos) == LOW)){
      mot[i].TacVal = false;
    }
  }
}

// Serial functions:
void CRealReceiver()
{  
  if (newData == true) {
      strcpy(TempChars, receivedChars); // Make temp copy, just easier for now
      parseData();
      newData = false;
  }
}

// Functions from https://forum.arduino.cc/t/serial-input-basics-updated/382007/3
void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;

    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                newData = true;
                // Debug
                //Serial.println("Got");
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

void parseData() {      // split the data into its parts
  int Numdex = 0;
  int Stringdex = 0;
  int StringErr = 0;
  bool ErrFlg = false;
  char MessageFromPC[5] = {0};

  // First character of the string should be the CMD char, check if only one char is sent:
  if (TempChars[(Stringdex+1)] != ','){
    // Report command error - CMD not recognised, too long
    StringErr = '1';
    ErrFlg = true;
    SerialErrors(1);
  }else{
    chCMD = TempChars[Stringdex];
  }
  Stringdex += 2; // Jump the comma
  // Should now encounter a single digit integer between 1-8
  // First test if single digit:
  if (TempChars[(Stringdex+1)] != ','){
    // Report command error - CMD not recognised, too long
    SerialErrors(1); // Make better to imply typo in motor number
  } else{
    //PCValM = unsigned(TempChars[Stringdex]); // gives -49???
    PCValM = TempChars[Stringdex] - '0'; // Quick and dirty way to convert char to int
    // Ensure value is within bounds:
    if (0 < PCValM < 9) {
      MotorIndex = PCValM - 1;
      // Debug
      //Serial.print("->");
      //Serial.println(PCValM);
    } else {
      StringErr = 3;
      ErrFlg = true;
    }
  }
  Stringdex += 2; // Jump the comma
  // Here comes the hard part, which is very dependent on what came before:
  // First - I break out if an error has been encountered:
  if (ErrFlg != false){
    SerialErrors(StringErr);
    ErrFlg = false;
  } else {
    // Begin the processing:
    // Check CMD byte:
    if ((chCMD == 'D') || (chCMD == 'P') || (chCMD == '?')) {
      if (TempChars[(Stringdex+1)] != '\0'){// The terminator character appears to not be right here.
      // Report command error - variable out of bounds
      StringErr = 4;
      ErrFlg = true;
      } else {
        PCValC = TempChars[Stringdex];
      }
    } else if (chCMD == 'F') {
      do {
        MessageFromPC[Numdex] = TempChars[Stringdex];
        Numdex++;
        Stringdex++;
      } while (TempChars[Stringdex] != '\0');
      // Check if in bounds
      if (0 < atof(MessageFromPC) < 500){
        // Report command error - variable out of bounds
        StringErr = 4;
        ErrFlg = true;
      } else {
        PCValF = atof(MessageFromPC);     // convert this part to a float
      }
    } else if (chCMD = 'S') {
      do {
        MessageFromPC[Numdex] = TempChars[Stringdex];
        Numdex++;
        Stringdex++;
      } while (TempChars[Stringdex] != '\0');
      PCValI = atoi(MessageFromPC);     // convert this part to a int
      // Check if in bounds
      if ((-6000 > PCValI) || (PCValI > 6000)){
        // Report command error - variable out of bounds
        // Debug
        //Serial.println("oeps");
        StringErr = 4;
        ErrFlg = true;
      }
    } else {
      // Report command error - command not recognised
      StringErr = 4;
      ErrFlg = true;
    }
  }
  // Got all values now, let's get to processing:
  if (ErrFlg != false){
    SerialErrors(StringErr);
    ErrFlg = false;
  } else{
    ProcessMSG();
  }
}

void ProcessMSG() {
  switch(chCMD){
    case 'D': // Change direction of motor n, expect char 
      switch (PCValC){
        case 'a': // anticlockwise
          // Set outbit low
          bitWrite(GPIOVal, mot[MotorIndex].DirPos, 0);
          mot[MotorIndex].DirCW = false;
          // set IOX change flag:
          UDIOX = true;
          break;
        case 'c': // clockwise
          // Set outbit high
          bitWrite(GPIOVal, mot[MotorIndex].DirPos, 1);
          mot[MotorIndex].DirCW = true;
          // set IOX change flag:
          UDIOX = true;
          break;
        case 't': // toggle
          // Toggle outbit
          bitWrite(GPIOVal, mot[MotorIndex].DirPos, !bitRead(GPIOVal, mot[MotorIndex].DirPos));
          mot[MotorIndex].DirCW = !mot[MotorIndex].DirCW;
          // set IOX change flag:
          UDIOX = true;
          break;
        default:
          // Report error, value out of bounds:
          SerialErrors(4);
          break;
      }
      // Debug:
      Serial.print("Got:");
      Serial.print(chCMD);
      Serial.print("->");
      Serial.print(PCValM);
      Serial.print("->");
      Serial.println(PCValC);
      break;
    case 'F': // Change frequency of motor n, expect float
      // Load value:
      mot[MotorIndex].SetFreq = PCValF;
      // Set run sine flag:
      // 0 turns it off!!!
      if (PCValF = 0.0){
        mot[MotorIndex].RunSine = false;
      }
      else{
        mot[MotorIndex].RunSine = true;
        mot[MotorIndex].MInc = (pix2*mot[MotorIndex].SetFreq)/SampleRate;
      }
      Serial.print("Got:");
      Serial.print(chCMD);
      Serial.print("->");
      Serial.print(PCValM);
      Serial.print("->");
      Serial.println(PCValF);
      break;
    case 'P': // Change power state of motor n, expect char
      // Load value:
      switch (PCValC){
        case 'a': // power aan
          // Set outbit low
          bitWrite(GPIOVal, mot[MotorIndex].PowPos, 1);
          // set IOX change flag:
          UDIOX = true;
          break;
        case 'u': // power uit
          // Set outbit high
          bitWrite(GPIOVal, mot[MotorIndex].PowPos, 0);
          // set IOX change flag:
          UDIOX = true;
          break;
        case 't': // toggle
          // Toggle outbit
          bitWrite(GPIOVal, mot[MotorIndex].PowPos, !bitRead(GPIOVal, mot[MotorIndex].PowPos));
          // set IOX change flag:
          UDIOX = true;
          break;
        default:
          // Report error, value out of bounds:
          SerialErrors(4);
          break;
      }
      Serial.print("Got:");
      Serial.print(chCMD);
      Serial.print("->");
      Serial.print(PCValM);
      Serial.print("->");
      Serial.println(PCValC);
      break;
    case 'S': // Change speed of motor n, expect int 
        // Load value:
      mot[MotorIndex].UseSpeed = (PCValI/6); // Conversion factor = 6 - 6000 rpm is max, 1000 values until saturation (995 or so actually)
      if (mot[MotorIndex].UseSpeed == 0){
        mot[MotorIndex].SetSpeed = 0;
        bitWrite(GPIOVal, mot[MotorIndex].PowPos, 0); // Turn motor off if speed is zero
      } else if (mot[MotorIndex].UseSpeed < 0){
        mot[MotorIndex].SetSpeed = (mot[MotorIndex].UseSpeed * -1);
        bitWrite(GPIOVal, mot[MotorIndex].DirPos, 0);
        mot[MotorIndex].DirCW = false;
        bitWrite(GPIOVal, mot[MotorIndex].PowPos, 1); // Turn motor on if speed is non-zero
      } else if (mot[MotorIndex].UseSpeed > 0){
        mot[MotorIndex].SetSpeed = mot[MotorIndex].UseSpeed;
        bitWrite(GPIOVal, mot[MotorIndex].DirPos, 1);
        mot[MotorIndex].DirCW = true;
        bitWrite(GPIOVal, mot[MotorIndex].PowPos, 1); // Turn motor on if speed is non-zero
      }
      UDIOX = true;
      DACUpdate(MotorIndex);
      Serial.print("Got:");
      Serial.print(chCMD);
      Serial.print("->");
      Serial.print(PCValM);
      Serial.print("->");
      Serial.println(PCValI);
      break;
    case '?':
      // Check bounds & Load value:
      switch (PCValC){
        case 'd': // current direction setting for motor n
            Serial.print("Motor ");
            Serial.print(PCValM);
            Serial.print(" runs ");
          if (bitRead(GPIOVal, mot[MotorIndex].DirPos) == 1){
            Serial.println("clockwise");
          } else {
            Serial.println("anticlockwise");
          }
          break;
        case 'f': // current frequency setting for motor n
            Serial.print("Motor ");
            Serial.print(PCValM);
            Serial.print(" frequency is ");
          Serial.println(mot[MotorIndex].SetFreq);
          break;
        case 'p': // current power setting for motor n
            Serial.print("Motor ");
            Serial.print(PCValM);
            Serial.print(" is ");
          if (bitRead(GPIOVal, mot[MotorIndex].PowPos) == 1){
            Serial.println("powered");
          } else {
            Serial.println("unpowered");
          }
          break;
        case 's': // current speed setting for motor n
          Serial.print("Motor ");
          Serial.print(PCValM);
          Serial.print("'s set speed (rpm) is ");
          if (mot[MotorIndex].DirCW != true){
            Serial.print("-");
          }
          Serial.println(6*mot[MotorIndex].SetSpeed);
          break;
        case 't': // current read speed for motor n
            Serial.print("Motor ");
            Serial.print(PCValM);
            Serial.print("'s tacho speed (rpm) is ");
          if (mot[MotorIndex].DirCW != true){
            Serial.print("-");
          }
          Serial.println(mot[MotorIndex].GetSpeed);
          break;
        default:
          // Report error, value out of bounds:
          SerialErrors(4);
          break;
      }
      break;
    default:
      break;
  }
}

// Serial Error Reporting
void SerialErrors(int ErrVal){
  switch(ErrVal){
    case 1:
      Serial.println("Command too long!");
      break;
    case 2:
      Serial.println("Command invalid!");
      break;
    case 3:
      Serial.println("Motor index not recognised!");
      break;
    case 4:
      Serial.println("Variable invalid!");
      break;
    default:
      Serial.println("Unknown problem");
      break;
  }
}

// Tick generator
void TickTock(){
  if (Currentms != millis()){
    // Update current:
    Currentms = millis();
    Flagms = true;
    if (Thous == 9){
      Thous = 0;
      Flagcs = true;
      if (Hunds == 9){
        Hunds = 0;
        Flagds = true;
        /* Not using lower speed tickers:
        if (Tiens = 9){
          Tiens = 0;
        }
        else{
          Tiens += 1;
        }*/
      }
      else{
        Hunds += 1;
      }
    }
    else{
      Thous += 1;
    }
  }
}