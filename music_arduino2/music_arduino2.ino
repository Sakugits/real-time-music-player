/**********************************************************
 *  INCLUDES
 *********************************************************/
#include "let_it_be.h"

/**********************************************************
 *  CONSTANTS
 *********************************************************/
// COMMENT THIS LINE TO EXECUTE WITH THE PC
#define TEST_MODE 1

#define SAMPLE_TIME 250 
#define SOUND_PIN  11
#define BUTTON_PIN 10
#define LED_PIN 8
#define BUF_SIZE 256

/**********************************************************
 *  GLOBALS
 *********************************************************/
unsigned char buffer[BUF_SIZE];
unsigned long timeOrig;
bool current_state = false;
bool previous_state = false;
bool muted = false;

//int cosa = 0;
/**********************************************************
 * Function: play_bit
 *********************************************************/
void play_bit() 
{
  unsigned char data = 0;
  static int music_count = 0;

  #ifdef TEST_MODE
    data = pgm_read_byte_near(music + music_count);
    music_count = (music_count + 1) % MUSIC_LEN;
  #else
    if (Serial.available()>1) {
      data = Serial.read();
    }
  #endif
  if (!muted)
    {OCR2A=data;}
    //if (cosa<10) {
      //Serial.println(data);
      //cosa++;
    //}
}

/**********************************************************
 * Function: mute_check
 *********************************************************/
void mute_check()
{
 current_state = digitalRead(BUTTON_PIN);
 if (current_state==LOW && previous_state==HIGH) {
  muted = !muted;
  digitalWrite(LED_PIN, muted);
 }
 previous_state = current_state;
}

/**********************************************************
 * Function: setup
 *********************************************************/
void setup ()
{
  // Initialize serial communications
  Serial.begin(115200);

  pinMode(SOUND_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  pinMode(BUTTON_PIN, INPUT);
  memset (buffer, 0, BUF_SIZE);

  //Timer 1
  //interrupt period = (1/16MHz) * 64 * 62 = 2.48 ms (~4032Hz)
  TCCR1A = _BV(COM1A0);
  TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);
  OCR1A = 62;
  /*TCCR1B = B00000101;     //Reset entire TCCR2B to 0 and prescaler to 32
  TIMSK1 &= B11111000;    //Disables the interrupts from timer2
  OCR1B = 125;            //Set compare register B to this value
  TCNT1 = 0;              //Set the timer2 count to 0
  // Setup Serial Monitor
  TIMSK1 |= B00000100; //enable compB interrupts from timer2*/

  //Timer 2
  //PWM configuration in pin 11

  TCCR2A = _BV(COM2A1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS20);
  TIMSK1 = _BV(OCIE1A);
  //TCCR2A |= B10000000; TCCR2A &= B10111111; //set OCR2A at bottom (non-inverting mode)
  //TCCR2A |= B00000011; TCCR2B &= B11110111; // fast PWM mode
  //TCCR2B |= B00000001; TCCR2B &= B11111001; //set prescaler to 1 (no prescaler. highest frequency)
  //timeOrig = micros();
}

/**********************************************************
 * Function: loop
 *********************************************************/
void loop ()
{
    mute_check();
    /*unsigned long timeDiff;

    mute_check();
    mute_display();
    play_bit();
    timeDiff = SAMPLE_TIME - (micros() - timeOrig);
    timeOrig = timeOrig + SAMPLE_TIME;
    delayMicroseconds(timeDiff);*/
    //play_bit();
    //delayMicroseconds(250);
}

ISR(TIMER1_COMPA_vect) {
  play_bit();
}
