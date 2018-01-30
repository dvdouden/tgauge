// Required libraries:
// Clickencoder https://github.com/0xPIT/encoder/tree/arduino
// LiquidCrystal_I2C https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library

#include <ClickEncoder.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

char buffer0[16] = "Spd xxx Tgt xxx";
char buffer1[16] = "Ac xx.x Dc xx.x";

int visSpd = 13000;
int visTgt = 13000;

ClickEncoder speedKnob(A1, A2, A3);
ClickEncoder accKnob(5,6,7);

void setup() {
  // put your setup code here, to run once:
pinMode( 9, OUTPUT);
pinMode( 10, OUTPUT);
digitalWrite( 9, LOW);
digitalWrite( 10, LOW);

pinMode(13, OUTPUT);

writeAcc();
writeDec();

	lcd.begin();

	// Turn on the blacklight and print a message.
	lcd.backlight();


  Serial.begin(9600);
 ICR1 = 10000; // sets freq to 100Hz
  TCCR1A = _BV(COM1A1) | _BV(COM1A0) /*| _BV(COM1B1) | _BV(COM1B0) */ /*| _BV(WGM10)*/;
  OCR1A = 3200; // sets duty cycle to 32.00%
  OCR1B = 0; // sets duty cycle to 16.00%
  TCCR1B = _BV(WGM13) | _BV(CS11);
  TIMSK1 |= _BV(TOIE1);
  
  TCCR2A = _BV(WGM21); // CTC
  TCCR2B = _BV(CS22); // prescaler /64
  OCR2A = 250; // overflow every millisecond (16,000,000 / 64 = 250,000)
  TIMSK2 = _BV( OCIE2A );
}

bool dirTgt = false;
int spdTgt = 0;
long tgt = 0;
long spd = 0;
int acc = 22;
int dec = 43;

#define DUTY 3200

volatile bool tenMillis = false;

void loop() {
  Serial.println(spd );
  writeSpd();
  writeTgt();
  
      lcd.setCursor (0 ,0);
    lcd.print( buffer0 );
    lcd.setCursor (0 ,1);
    lcd.print( buffer1 );

  
  delay(100);
}

void updateSpeed() {
  
    long s = spd > 0 ? spd : -spd;
    long t = tgt > 0 ? tgt : -tgt;
    bool ds = spd > 0;
    bool dt = tgt > 0;
    
    bool accelerate = false;
    
    if ( spd != tgt ) {
      if ( ds != dt ) {
        accelerate = false;
        s -= dec;
        if ( s <= 0 ) {
          ds = !ds;
          s = -s;
        }
      } else if ( s > t ) {
        accelerate = false;
        s -= dec;
        if ( s <= 0 ) {
          ds = !ds;
        }
      } else {
        accelerate = true;
        s += acc;
        if ( s > t ) {
          s = t;
        }
      }
      if ( !ds ) {
        spd = -s;
      } else {
        spd = s;
      }
    }
    
    buffer1[2] = accelerate ? '>' : ' ';
    buffer1[10] = accelerate ? ' ' : '>';  
}


ISR(TIMER1_OVF_vect)
{
  // interrupt triggered every 10 milliseconds
  //updateSpeed();
  
  spdTgt -= speedKnob.getValue();
  if ( spdTgt > 130 ) {
    spdTgt = 130;
  } else if ( spdTgt < 0 ) {
    spdTgt = 0;
  }
  switch ( speedKnob.getButton() ) {
    case ClickEncoder::Closed:
    case ClickEncoder::Pressed:
    case ClickEncoder::Held:
      dirTgt = false;
      break;
   default:
     dirTgt = true;
     break;
  }
  tgt = dirTgt ? spdTgt : -spdTgt;
  tgt *= 1000;
  
  updateSpeed();
  visSpd = spd / 1000;
  visTgt = tgt / 1000;
  
  
  OCR1A = 10000 - (spd / 40);
}
ISR(TIMER2_COMPA_vect)
{
  // deal with it
  speedKnob.service();
}

void writeSpd() {
  writeInt( visSpd, buffer0 + 3 );
}


void writeTgt() {
  writeInt( visTgt, buffer0 + 11 );
}

void writeInt( int i, char* buffer ) {
  int j = (i > 0 ? i : -i );
  
  buffer[0] = i == 0 ? ' ' : ( i > 0 ? '>' : '<' );
  buffer[1] = '0' + ( j / 100 );
  buffer[2] = '0' + ( ( j % 100 ) / 10 );
  buffer[3] = '0' + ( j % 10 );
}


void writeAcc()
{
  writeDecimal( acc, buffer1 + 3);
}

void writeDec() {
  writeDecimal( dec, buffer1 + 11 );
}

void writeDecimal( int d, char* buffer ) {
  buffer[0] = '0' + (d / 100);
  buffer[1] = '0' + ((d % 100 ) / 10);
  buffer[3] = '0'+ (d % 10);
}
