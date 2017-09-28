/*
Revision 1.0 - Main code by Richard Visokey AD7C - www.ad7c.com
Revision 2.0 - November 6th, 2013...  ever so slight revision by  VK8BN for AD9851 chip Feb 24 2014
Revision 3.0 - April, 2016 - AD9851 + ARDUINO PRO NANO + integrate cw decoder (by LZ1DPN) (uncontinued version)
Revision 4.0 - May 31, 2016  - deintegrate cw decoder and add button for band change (by LZ1DPN)
Revision 5.0 - July 20, 2016  - with LCD display + IF --> ready to control transceiver RFT SEG-100 (by LZ1DPN)
Revision 6.0 - August 16, 2016  - serial control buttons from computer with USB serial (by LZ1DPN) (1 up freq, 2 down freq, 3 step increment change, 4 print state)
									for no_display work with DDS generator
*/

// Include the library code
#include <LiquidCrystal.h>
#include <rotary.h>
//#include <SPI.h>
//#include <Wire.h>

//#define NUMFLAKES 10
//#define XPOS 0
//#define YPOS 1
//#define DELTAY 2

#define W_CLK 8   // Pin 8 - connect to AD9851 module word load clock pin (CLK)
#define FQ_UD 9   // Pin 9 - connect to freq update pin (FQ)
#define DATA 10   // Pin 10 - connect to serial data load pin (DATA)
#define RESET 11  // Pin 11 - connect to reset pin (RST) 
#define BTNDEC (A2)  // BAND CHANGE BUTTON
#define pulseHigh(pin) {digitalWrite(pin, HIGH); digitalWrite(pin, LOW); }

Rotary r = Rotary(2,3); // sets the pins the rotary encoder uses.  Must be interrupt pins.
LiquidCrystal lcd(12, 13, 7, 6, 5, 4); // I used an odd pin combination because I need pin 2 and 3 for the interrupts.

int_fast32_t rx=7000000; // Starting frequency of VFO
int_fast32_t rx2=1; // variable to hold the updated frequency
int_fast32_t increment = 100; // starting VFO update increment in HZ.
int buttonstate = 0;
String hertz = "100 Hz";
int  hertzPosition = 0;
byte ones,tens,hundreds,thousands,tenthousands,hundredthousands,millions ;  //Placeholders
String freq; // string to hold the frequency
int byteRead = 0;
int var_i = 0;
int BTNdecodeON = 0;
int BTNlaststate = 0;
int BTNcheck = 0;
int BTNcheck2 = 0;
int BTNinc = 3; // set default band minus 1  ==> (for 7MHz = 3)

void setup() {

// Initialize the Serial port so that we can use it for debugging
  Serial.begin(115200);
  Serial.println("Start VFO ver 6.0");
  
  pinMode(BTNDEC,INPUT);		// temporary use for band change
  digitalWrite(BTNDEC,HIGH);    //
  pinMode(A0,INPUT); // Connect to a button that goes to GND on push - rotary encoder FREQ STEP
  digitalWrite(A0,HIGH);
  lcd.begin(16, 2);

// rotary
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();

  pinMode(FQ_UD, OUTPUT);
  pinMode(W_CLK, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(RESET, OUTPUT); 
  pulseHigh(RESET);
  pulseHigh(W_CLK);
  pulseHigh(FQ_UD);  // this pulse enables serial mode on the AD9851 - see datasheet
  lcd.setCursor(hertzPosition,1);    
  lcd.print(hertz);

// for (int index = 0; index < colums; index++){
//    line1[index] = 32;
//  	line2[index] = 32;
// }
    
}


///// START LOOP - MAIN LOOP

void loop() {
	checkBTNdecode();

  if (rx != rx2){		
			showFreq();
			sendFrequency(rx);
      rx2 = rx;
      }
      
  buttonstate = digitalRead(A0);
  if(buttonstate == LOW) {
        setincrement();        
    };

/*  check if data has been sent from the computer: */
if (Serial.available()) {
    /* read the most recent byte */
    byteRead = Serial.read();
  if(byteRead == 49){     // 1 - up freq
    rx = rx + increment;
    Serial.println(rx);
    }
  if(byteRead == 50){   // 2 - down freq
    rx = rx - increment;
    Serial.println(rx);
    }
  if(byteRead == 51){   // 3 - up increment
    setincrement();
    Serial.println(increment);
    }
  if(byteRead == 52){   // 4 - print VFO state in serial console
    Serial.println("VFO_VERSION 10.0");
    Serial.println(rx);
    Serial.println(increment);
    Serial.println(hertz);
    }
  if(byteRead == 53){   // 5 - scan freq forvard 40kHz 
             var_i=0;           
             while(var_i<=4000){
                var_i++;
                rx = rx + 10;
                sendFrequency(rx);
                Serial.println(rx);
                showFreq();
                if (Serial.available()) {
          if(byteRead == 53){
              break;                       
          }
        }
      }        
   }

   if(byteRead == 54){   // 6 - scan freq back 40kHz  
             var_i=0;           
             while(var_i<=4000){
                var_i++;
                rx = rx - 10;
                sendFrequency(rx);
                Serial.println(rx);
                showFreq();
                if (Serial.available()) {
                    if(byteRead == 54){
                        break;                       
                    }
                }
             }        
   }
   
}
}	  

///END of main loop ///

/// START INTERNAL FUNCTIONS

//rotary
ISR(PCINT2_vect) {
  unsigned char result = r.process();
  if (result) {    
    if (result == DIR_CW){rx=rx+increment;}
    else {rx=rx-increment;};       
      if (rx >=55000000){rx=rx2;}; // UPPER VFO LIMIT 
      if (rx <=100000){rx=rx2;}; // LOWER VFO LIMIT (org<=1)
  }
}



// frequency calc from datasheet page 8 = <sys clock> * <frequency tuning word>/2^32
void sendFrequency(double frequency) {  
  int32_t freq = (frequency * 4294967296./180000000);  // note 180 MHz clock on 9851. also note slight adjustment of this can be made to correct for frequency error of onboard crystal
  for (int b=0; b<4; b++, freq>>=8) {
    tfr_byte(freq & 0xFF);
  }
  tfr_byte(0x001);   // Final control byte, LSB 1 to enable 6 x xtal multiplier on 9851 set to 0x000 for 9850
  pulseHigh(FQ_UD);  // Done!  Should see output
  Serial.println(frequency);
}

// transfers a byte, a bit at a time, LSB first to the 9851 via serial DATA line
void tfr_byte(byte data){
  for (int i=0; i<8; i++, data>>=1) {
    digitalWrite(DATA, data & 0x01);
    pulseHigh(W_CLK);   //after each bit sent, CLK is pulsed high
  }
}


void setincrement(){
  if(increment == 1){increment = 10; hertz = "10 Hz"; hertzPosition=0;} 
  else if(increment == 10){increment = 50; hertz = "50 Hz"; hertzPosition=0;}
  else if (increment == 50){increment = 100;  hertz = "100 Hz"; hertzPosition=0;}
  else if (increment == 100){increment = 500; hertz="500 Hz"; hertzPosition=0;}
  else if (increment == 500){increment = 1000; hertz="1 Khz"; hertzPosition=0;}
  else if (increment == 1000){increment = 2500; hertz="2.5 Khz"; hertzPosition=0;}
  else if (increment == 2500){increment = 5000; hertz="5 Khz"; hertzPosition=0;}
  else if (increment == 5000){increment = 10000; hertz="10 Khz"; hertzPosition=0;}
  else if (increment == 10000){increment = 100000; hertz="100 Khz"; hertzPosition=0;}
  else if (increment == 100000){increment = 1000000; hertz="1 Mhz"; hertzPosition=0;} 
  else{increment = 1; hertz = "1 Hz"; hertzPosition=0;};  
  showFreq();
  delay(250); // Adjust this delay to speed up/slow down the button menu scroll speed.
}


void showFreq(){
    millions = int(rx/1000000);
    hundredthousands = ((rx/100000)%10);
    tenthousands = ((rx/10000)%10);
    thousands = ((rx/1000)%10);
    hundreds = ((rx/100)%10);
    tens = ((rx/10)%10);
    ones = ((rx/1)%10);

    lcd.setCursor(0,0);
    lcd.print("                ");
    if (millions > 9){lcd.setCursor(0,0);}
    else{lcd.setCursor(1,0);}
    lcd.print(millions);
    lcd.print(".");
    lcd.print(hundredthousands);
    lcd.print(tenthousands);
    lcd.print(thousands);
    lcd.print(".");
    lcd.print(hundreds);
    lcd.print(tens);
    lcd.print(ones);
    lcd.print(" MHz ");
}

//  BAND CHANGE) !!!
void checkBTNdecode(){
BTNdecodeON = digitalRead(BTNDEC);
if(BTNdecodeON != BTNlaststate){
    if(BTNdecodeON == HIGH){
    delay(200);
    BTNcheck2 = 1;
    BTNinc = BTNinc + 1;
switch (BTNinc) {
    case 1:
      rx=1810000;
      break;
    case 2:
      rx=3500000;
      break;
    case 3:
      rx=5250000;
      break;
    case 4:
      rx=7000000;
      break;
    case 5:
      rx=10100000;
      break;
    case 6:
      rx=14000000;
      break;
    case 7:
      rx=18068000;
      break;    
    case 8:
      rx=21000000;
      break;    
    case 9:
      rx=24890000;
      break;    
    case 10:
      rx=28000000;
      break;
    case 11:
      rx=29100000;
      break;    	  
    default:
      if(BTNinc > 11){
         BTNinc = 0;
      }
    break;
  }
}

if(BTNdecodeON == LOW){
    BTNcheck2 = 0;
	}
    BTNlaststate = BTNcheck2;
  }
}

//// OK END OF PROGRAM
