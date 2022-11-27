/**********************************************************
 *  INCLUDES
 *********************************************************/
#include "let_it_be.h"

/**********************************************************
 *  CONSTANTS
 *********************************************************/
// COMMENT THIS LINE TO EXECUTE WITH THE PC
#define TEST_MODE 1

#define PC 2
#define SAMPLE_TIME 250 
#define SOUND_PIN  9
#define BUTTON_PIN 10
#define LED_PIN 11
#define BUF_SIZE 256

/**********************************************************
 *  GLOBALS
 *********************************************************/
unsigned char buffer[BUF_SIZE];
unsigned long timeOrig;
bool muted = false;
bool flag = false;

int SC = 0;

/**********************************************************
 * Function: play_bit
 *********************************************************/
void play_bit() 
{
  static unsigned char data = 0;
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
    {OCR1A=data;}
}

/**********************************************************
 * Function: mute_check
 *********************************************************/
void mute_check()
{
  muted = digitalRead(BUTTON_PIN);
  digitalWrite(LED_PIN, muted);
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

  //Timer 2
  //interrupt period = (1/16MHz) * 32 * 125 = 2.5 ms (4000Hz)
  TCCR2B = B00000101;     //Reset entire TCCR2B to 0 and prescaler to 32
  TIMSK2 &= B11111000;    //Disables the interrupts from timer2
  OCR2B = 125;            //Set compare register B to this value
  TCNT2 = 0;              //Set the timer2 count to 0
  // Setup Serial Monitor
  TIMSK2 |= B00000100; //enable compB interrupts from timer2

  //Timer 1
  //PWM configuration in pin 9
  TCCR1A |= B10100000; //set OCR1A at bottom (non-inverting mode)
  TCCR1B |= B00000001; //set prescaler to 1 (no prescaler. highest frequency)
  TCCR1A |= B00000001; TCCR1A &=B11111101; //set PWM to fast mode (8 bits)
  //timeOrig = micros();
}

/**********************************************************
 * Function: loop
 *********************************************************/
void loop ()
{
    /*unsigned long timeDiff;

    mute_check();
    mute_display();
    play_bit();
    timeDiff = SAMPLE_TIME - (micros() - timeOrig);
    timeOrig = timeOrig + SAMPLE_TIME;
    delayMicroseconds(timeDiff);*/
    if (flag) {
      flag = false;
      switch (SC) {
        case 0:
          mute_check();
          break;
        case 1:
          play_bit();
          break;
      }
      SC = (SC+1)%PC;
    }
}

ISR(TIMER2_COMPB_vect) {
  flag = true;
}
