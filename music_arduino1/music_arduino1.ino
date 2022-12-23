/**********************************************************
 *  INCLUDES
 *********************************************************/
//#include "let_it_be_1bit.h"

/**********************************************************
 *  CONSTANTS
 *********************************************************/
// COMMENT THIS LINE TO EXECUTE WITH THE PC
//#define TEST_MODE 1

#define SAMPLE_TIME 250 
#define SOUND_PIN  11 //Speaker
#define BUTTON_PIN 10 //Mute Button
#define LED_PIN 8 //Display LED
#define BUF_SIZE 256

/**********************************************************
 *  GLOBALS
 *********************************************************/
unsigned char buffer[BUF_SIZE];
unsigned long timeOrig;
bool current_state = false;
bool previous_state = false;
bool muted = false;


/**********************************************************
 * Function: play_bit
 *********************************************************/
void play_bit() 
{
  static int bitwise = 1;
  static unsigned char data = 0;
  static int music_count = 0;

    bitwise = (bitwise * 2); //shift mask to the left
    if (bitwise > 128) { //new byte
       bitwise = 1;
       #ifdef TEST_MODE 
          data = pgm_read_byte_near(music + music_count);
          music_count = (music_count + 1) % MUSIC_LEN;
       #else
          if (Serial.available()>1) {
             data = Serial.read();
          }
       #endif
    }
    if (!muted)
      {digitalWrite(SOUND_PIN, (data & bitwise));} //play only the bit marked by the mask
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

  //Timer 2
  //interrupt period = (1/16MHz) * 32 * 125 = 2.5 ms (4000Hz) 
  TCCR2B = B00000011;     //Reset entire TCCR2B to 0 and prescaler to 32
  TIMSK2 &= B11111000;    //Disables the interrupts from timer2
  OCR2B = 125;            //Set compare register B to this value
  TCNT2 = 0;              //Set the timer2 count to 0
  TIMSK2 |= B00000100; //enable compB interrupts from timer2

}

/**********************************************************
 * Function: loop
 *********************************************************/
void loop ()
{
    mute_check();
}

ISR(TIMER2_COMPB_vect) {
  play_bit();
}
