// https://www.alexallmont.com/avr-cheap-bass-drum-synthesis/
// Attiny25/45/85, no device to test
// fuse L = 0xE2, H = 0xDF, E = 0xFF 

// compiler : Arduino IDE 1.8.5, AVR-GCC 4.9.2
// changed to ATtiny13
// fuse L = 0x3A, H = 0xFB
// 2019-FEB-26, xiaolaba, copy and include support ATtiny13, pin assignment is same as Attiy45





#include <avr/io.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>








// Workaround for PROGMEM issues under g++: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=34734 
#ifdef PROGMEM 
#undef PROGMEM 
#define PROGMEM __attribute__((section(".progmem.data"))) 
#endif

//--------------------------------------------------------------------------------
// Lookup tables
//--------------------------------------------------------------------------------
// Sine lookup table 0-2*PI => 0-255, -1-1 => 0x00-0xff
const uint8_t sine_tbl[256] PROGMEM = {
  0x80, 0x83, 0x86, 0x89, 0x8c, 0x90, 0x93, 0x96,
  0x99, 0x9c, 0x9f, 0xa2, 0xa5, 0xa8, 0xab, 0xae,
  0xb1, 0xb3, 0xb6, 0xb9, 0xbc, 0xbf, 0xc1, 0xc4,
  0xc7, 0xc9, 0xcc, 0xce, 0xd1, 0xd3, 0xd5, 0xd8,
  0xda, 0xdc, 0xde, 0xe0, 0xe2, 0xe4, 0xe6, 0xe8,
  0xea, 0xeb, 0xed, 0xef, 0xf0, 0xf1, 0xf3, 0xf4,
  0xf5, 0xf6, 0xf8, 0xf9, 0xfa, 0xfa, 0xfb, 0xfc,
  0xfd, 0xfd, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xfd,
  0xfd, 0xfc, 0xfb, 0xfa, 0xfa, 0xf9, 0xf8, 0xf6,
  0xf5, 0xf4, 0xf3, 0xf1, 0xf0, 0xef, 0xed, 0xeb,
  0xea, 0xe8, 0xe6, 0xe4, 0xe2, 0xe0, 0xde, 0xdc,
  0xda, 0xd8, 0xd5, 0xd3, 0xd1, 0xce, 0xcc, 0xc9,
  0xc7, 0xc4, 0xc1, 0xbf, 0xbc, 0xb9, 0xb6, 0xb3,
  0xb1, 0xae, 0xab, 0xa8, 0xa5, 0xa2, 0x9f, 0x9c,
  0x99, 0x96, 0x93, 0x90, 0x8c, 0x89, 0x86, 0x83,
  0x80, 0x7d, 0x7a, 0x77, 0x74, 0x70, 0x6d, 0x6a,
  0x67, 0x64, 0x61, 0x5e, 0x5b, 0x58, 0x55, 0x52,
  0x4f, 0x4d, 0x4a, 0x47, 0x44, 0x41, 0x3f, 0x3c,
  0x39, 0x37, 0x34, 0x32, 0x2f, 0x2d, 0x2b, 0x28,
  0x26, 0x24, 0x22, 0x20, 0x1e, 0x1c, 0x1a, 0x18,
  0x16, 0x15, 0x13, 0x11, 0x10, 0x0f, 0x0d, 0x0c,
  0x0b, 0x0a, 0x08, 0x07, 0x06, 0x06, 0x05, 0x04,
  0x03, 0x03, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x03,
  0x03, 0x04, 0x05, 0x06, 0x06, 0x07, 0x08, 0x0a,
  0x0b, 0x0c, 0x0d, 0x0f, 0x10, 0x11, 0x13, 0x15,
  0x16, 0x18, 0x1a, 0x1c, 0x1e, 0x20, 0x22, 0x24,
  0x26, 0x28, 0x2b, 0x2d, 0x2f, 0x32, 0x34, 0x37,
  0x39, 0x3c, 0x3f, 0x41, 0x44, 0x47, 0x4a, 0x4d,
  0x4f, 0x52, 0x55, 0x58, 0x5b, 0x5e, 0x61, 0x64,
  0x67, 0x6a, 0x6d, 0x70, 0x74, 0x77, 0x7a, 0x7d
};

// Impact pulse table, quasi logarithmic dropoff for modulation
const uint8_t impact_tbl[256] PROGMEM = {
  255, 253, 250, 248, 245, 243, 240, 238,
  235, 233, 230, 228, 226, 223, 221, 219,
  216, 214, 212, 210, 208, 205, 203, 201,
  199, 197, 195, 193, 191, 189, 187, 185,
  183, 181, 179, 178, 176, 174, 172, 170,
  168, 167, 165, 163, 161, 160, 158, 156,
  155, 153, 152, 150, 148, 147, 145, 144,
  142, 141, 139, 138, 136, 135, 133, 132,
  130, 129, 128, 126, 125, 123, 122, 121,
  119, 118, 117, 116, 114, 113, 112, 111,
  109, 108, 107, 106, 104, 103, 102, 101,
  100, 99, 98, 96, 95, 94, 93, 92,
  91, 90, 89, 88, 87, 86, 85, 84,
  83, 82, 81, 80, 79, 78, 77, 76,
  75, 74, 73, 72, 71, 71, 70, 69,
  68, 67, 66, 65, 64, 64, 63, 62,
  61, 60, 60, 59, 58, 57, 57, 56,
  55, 54, 54, 53, 52, 51, 51, 50,
  49, 49, 48, 47, 46, 46, 45, 45,
  44, 43, 43, 42, 41, 41, 40, 40,
  39, 38, 38, 37, 37, 36, 35, 35,
  34, 34, 33, 33, 32, 32, 31, 31,
  30, 30, 29, 29, 28, 28, 27, 27,
  26, 26, 26, 25, 25, 24, 24, 23,
  23, 23, 22, 22, 21, 21, 21, 20,
  20, 20, 19, 19, 19, 18, 18, 18,
  17, 17, 17, 16, 16, 16, 15, 15,
  15, 15, 14, 14, 14, 13, 13, 13,
  13, 13, 12, 12, 12, 12, 11, 11,
  11, 11, 11, 10, 10, 10, 10, 10,
  10, 9, 9, 9, 9, 9, 9, 9,
  9, 8, 8, 8, 8, 8, 8, 8
};

//--------------------------------------------------------------------------------
// Analogue port read
//--------------------------------------------------------------------------------
void analogRead()
{
  ADCSRA |= _BV(ADEN);            // Analog-Digital enable bit
  ADCSRA |= _BV(ADSC);            // Discarte first conversion
  while (ADCSRA & _BV(ADSC)) {};  // Wait until conversion is done
  ADCSRA |= _BV(ADSC);            // Start single conversion
  while (ADCSRA & _BV(ADSC)) {};  // Wait until conversion is done
  ADCSRA &= ~_BV(ADEN);           // Shut down ADC
}

//--------------------------------------------------------------------------------
// 8 bit drum implementation
//--------------------------------------------------------------------------------
class Drum
{
public:
  Drum() :
    m_playing(false),
    m_startPos(0),
    m_speed(0),
    m_impactPos(0),
    m_sinePos(0),
    m_sample(0)
  {
  }

  void updatePot1(uint8_t pot1)
  {
    m_startPos = pot1;
  }

  void updatePot2(uint8_t pot2)
  {
    m_speed = pot2;
  }

  void hitDrum()
  {
    m_playing = false;
    m_impactPos = m_startPos << 8;
    m_sinePos = 0;
    m_playing = true;
  }

  void update()
  {
    if (m_playing)
    {
      // Increment the impact sample pos by the speed and use it's value
      // to update the sine pos.
      m_impactPos += m_speed;
      m_sinePos += pgm_read_byte(&impact_tbl[(m_impactPos >> 8) & 0xff]);
      m_sample = pgm_read_byte(&sine_tbl[(m_sinePos >> 4) & 0xff]);

      // Stop the drum at the end of the sample.
      if (m_impactPos > 0xff00)
      {
        m_playing = false;
      }
    }
    else if (m_sample)
    {
      // Fade out sample if nothing playing
      m_sample--;
    }
  }
  
  uint8_t getSample()
  {
    return m_sample;
  }

private:
  bool      m_playing;
  uint8_t   m_startPos;  // 
  uint8_t   m_speed;     // 
  uint16_t  m_impactPos; // Impact sample offset
  uint16_t  m_sinePos;   // Sine sample offset
  uint8_t   m_sample;    // Next sample to play.
};

Drum g_drum;

//--------------------------------------------------------------------------------
// General use utility timer
//--------------------------------------------------------------------------------
uint16_t g_timer = 0;

//--------------------------------------------------------------------------------
// Fast PWM interrupt
//--------------------------------------------------------------------------------
ISR(TIM0_COMPA_vect)
{
  // Update sample at 31.25kHz
  OCR0A = g_drum.getSample();

  // Update timer if it's running.
  if (g_timer)
  {
    g_timer++;
  }

  // Process next sample.
  g_drum.update();
}

//--------------------------------------------------------------------------------
#define PLAYBACK_RATE  31250L
#define DEBOUNCE_MS    200L
#define DEBOUNCE_TICKS (DEBOUNCE_MS * PLAYBACK_RATE / 1000L)

//--------------------------------------------------------------------------------
// Main loop
//--------------------------------------------------------------------------------
int main(void)
{
  // PWM output on PORTB0 = pin 5.
  DDRB = _BV(0);

  // PWM init, 8Mhz / 256 gives 31.25kHz
  TCCR0A =
    _BV(COM0A1) |             // Clear OC0A/OC0B on Compare Match.
    _BV(WGM00) |_BV(WGM01);   // Fast PWM, top 0xff, update OCR at bottom.
  TCCR0B = _BV(CS00);         // No prescaling, full 8MHz operation.

//  TIMSK  = _BV(OCIE0A);       // Timer/Counter0 Output Compare Match A Interrupt Enable

/* add support ATtiny13
 * xiaolaba 2019-FEB-26
*/
#if defined (__AVR_ATtiny25__) | (__AVR_ATtiny45__) | (__AVR_ATtiny85__)
  TIMSK = _BV(OCIE0A);  // Timer/Counter0 Output Compare Match A Interrupt Enable
  //nop;
#endif

#if defined (__AVR_ATtiny13__)
  TIMSK0 = _BV(OCIE0A);  // Timer/Counter0 Output Compare Match A Interrupt Enable
  //nop;
#endif




  // Analogue init.
  ADCSRA |=
    _BV(ADEN) |               //  ADC Enable
    _BV(ADPS1) | _BV(ADPS0);  // Div 8 prescaler

  // Enable interrupts.
  sei();

  // Main loop.
  for (;;)
  {
    //------------------------------------------------
    // Pot 1 determines type of drum and param 1.
    // ADC2 (PINB 4, chip pin 3).
    ADMUX = _BV(ADLAR) | _BV(MUX1);
    analogRead();
    g_drum.updatePot1(ADCH);
  
    //------------------------------------------------
    // Pot 2 determines trigger mode and param 2.
    // ADC3 (PINB 3, chip pin 2).
    ADMUX = _BV(ADLAR) | _BV(MUX0) | _BV(MUX1);
    analogRead();
    g_drum.updatePot2(ADCH);
  
    //------------------------------------------------
    // Read PINB 2 (chip pin 7) to trigger drum
    if (PINB & _BV(2))
    {
      if (!g_timer)
      {
        g_drum.hitDrum();
        g_timer++;
      }
      else if (g_timer > DEBOUNCE_TICKS)
      {
        g_timer = 0;
      }
    }
  }
}
