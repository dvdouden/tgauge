// Required libraries:
// Clickencoder https://github.com/0xPIT/encoder/tree/arduino
// LiquidCrystal_I2C https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library

#include <ClickEncoder.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

// one rotary encoder for setting speed, one for accelleration
ClickEncoder speedKnob(A1, A2, A3);
ClickEncoder accKnob(5,6,7);

// Buffers for the LCD display, makes writing to the display a lot faster
char buffer0[16] = "Spd xxx Tgt xxx";
char buffer1[16] = "Ac xx.x Dc xx.x";

// visual speed and target speed as used by the display buffer
int visSpd = 0;
int visTgt = 0;

void setup() {
	// pin 9 and 10 are connected to the H bridge
	pinMode( 9, OUTPUT);
	pinMode( 10, OUTPUT);
	digitalWrite( 9, LOW);
	digitalWrite( 10, LOW);
	
	// update acceleration and deceleration values in display buffer
	writeAcc();
	writeDec();

	lcd.begin();
	lcd.backlight();
	
	// set up timer 1 for PWM
	// using Phase and Frequency correct PWM with TOP = ICR1
	// in this PWM mode, the timer counts from 0 to TOP and back to 0
	// OC1A is set high when the timer >= OCR1A, and set low when timer < OCR1A (same for OC1B)
	// when the timer hits 0 again, the overflow interrupt is triggered
	// Prescaler is /8, timer clock = 2MHz
	ICR1 = 10000; // sets freq to 100Hz
	TCCR1A = _BV(COM1A1) | _BV(COM1A0) | _BV(COM1B1) | _BV(COM1B0); // enable output on compare match for OC1A and OC1B
	OCR1A = 10000; // sets duty cycle to 0%
	OCR1B = 10000; // sets duty cycle to 0%
	TCCR1B = _BV(WGM13) | _BV(CS11); // select PWM mode, set prescaler to /8
	TIMSK1 |= _BV(TOIE1); // Enable overflow interrupt

	// Set up timer 2 for input updates (reading rotary encoders)
	// this generates an interrupt every millisecond
	TCCR2A = _BV(WGM21); // CTC (clear timer on compare match)
	TCCR2B = _BV(CS22); // prescaler /64
	OCR2A = 250; // overflow every millisecond (16,000,000 / 64 = 250,000)
	TIMSK2 = _BV( OCIE2A ); // Enable output compare interrupt on OCR2A
}

void loop() {
	// update speed and target speed in display buffer
	writeSpd();
	writeTgt();

	// write display buffer to display
	lcd.setCursor( 0, 0 );
	lcd.print( buffer0 );
	lcd.setCursor ( 0, 1 );
	lcd.print( buffer1 );
	
	// wait for 100 milliseconds (refresh display at 10fps)
	delay( 100 );
}

// variables used while calculating new speed and direction
bool dirTgt = false; // direction as set by direction switch
int spdTgt = 0; // speed as set by speed knob in km/h
long tgt = 0; // target speed in km/h * 1000, may be negative 
long spd = 0; // speed in km/h * 1000, may be negative
int acc = 22; // acceleration in km/h/s (* 10). So 22 == 2.2km/h/s
int dec = 43; // deceleration in km/h/s (* 10). So 43 == 4.3km/h/s


void updateSpeed() {
	// calculate new speed based on:
	// current speed and direction
	// target speed and direction
	// acceleration and deceleration values
	
	// get absolute speed and target speed and their directions
	long s = spd > 0 ? spd : -spd;
	long t = tgt > 0 ? tgt : -tgt;
	bool ds = spd > 0;
	bool dt = tgt > 0;

	// keep track of whether we need to accelerate or decelerate
	bool accelerate = false;

	if ( spd != tgt ) {
		if ( ds != dt ) {
			// different direction, decelerate to 0 first
			accelerate = false;
			s -= dec;
			if ( s <= 0 ) {
				// reached or overshot 0; reverse direction
				ds = !ds;
				s = -s;
			}
		} else if ( s > t ) {
			// current speed exceeds target speed; decelerate
			accelerate = false;
			s -= dec;
			if ( s < t ) {
				// overshot target speed
				s = t;
			}
		} else {
			// current speed below target speed; accelerate
			accelerate = true;
			s += acc;
			if ( s > t ) {
				// overshot target speed
				s = t;
			}
		}
		// update actual speed with correct direction
		if ( !ds ) {
			spd = -s;
		} else {
			spd = s;
		}
	}

	// update acceleration/deceleration indicator in display buffer
	buffer1[2] = accelerate ? '>' : ' ';
	buffer1[10] = accelerate ? ' ' : '>';  
}


ISR( TIMER1_OVF_vect )
{
	// interrupt triggered every 10 milliseconds by timer 1

	// read input from knobs and switches and update target speed and direction
	spdTgt -= speedKnob.getValue();
	
	// cap speed at 130 km/h
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
	
	// TODO: handle acceleration inputs
	
	// update actual target speed
	tgt = dirTgt ? spdTgt : -spdTgt;
	tgt *= 1000;

	// update actual speed based on target speed and acceleration/deceleration values
	updateSpeed();
	
	// update visual representation of speeds
	visSpd = spd / 1000;
	visTgt = tgt / 1000;
	
	// set PWM value
	OCR1A = 10000 - (spd / 40);
	// TODO: handle reverse direction
	// TODO: add lookup table for correct speed
}

ISR(TIMER2_COMPA_vect)
{
  speedKnob.service();
}

// Methods for writing values to the display buffer
void writeSpd() {
	writeInt( visSpd, buffer0 + 3 );
}


void writeTgt() {
	writeInt( visTgt, buffer0 + 11 );
}

void writeInt( int i, char* buffer ) {
	// negative value means backward, positive means forward
	
	// get absolute value
	int j = (i > 0 ? i : -i );

	// direction indicator
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
