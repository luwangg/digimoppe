uint8_t uarttest = 0;

void setup() {
  DDRB |= (1<<4) | (1<<3); // PB4 = digital pin 12, PB3 = digital pin 11
  PORTB = 0;
  Serial.begin(1000000);
  pinMode(2, INPUT_PULLUP);
  if(!digitalRead(2)) { // UART test mode
    uarttest = 1;
  } else {
    /* ADC prescaler is 32 for 500 kHz clock.
       Timer1 prescaler is 8, so counting to 4*14 results in 14 ADC clocks
       for a sample rate of 16 MHz / 32 / 14 = 35714 Hz. */
    TCCR1A = 0; // WGM10=0, WGM11=0
    TCCR1B = 1<<WGM12; // WGM12=1: clear timer on compare match A
    OCR1A = 4 * 14 -1;
    OCR1B = 0;
    TIMSK1 = 1<<OCIE1B; // interrupt on match B (it also triggers ADC)
    TCNT0 = 0;


    // ADC settings
    ADMUX = (3<<REFS0) | (0<<ADLAR) | (0<<MUX0); // right aligned
    ADCSRB = (5<<ADTS0); // trigger on timer1 compare match B
    DIDR0 = (1<<ADC0D);
    ADCSRA = (1<<ADATE) | (5<<ADPS0); // no interrupt, auto trigger, prescaler 32

    sei();
    // enable ADC and start conversion last
    ADCSRA |= (1<<ADEN);
    TCCR1B |= (2<<CS10); // start timer1: prescale by 8
  }
}

volatile uint8_t txbyte0=0, txbyte1=0, txfull=0;

void loop() {
  if(uarttest) {
    while ( !( UCSR0A & (1<<UDRE0)) );
    UDR0 = txbyte0;
    txbyte0++;
  } else
  if(txfull) {
    while ( !( UCSR0A & (1<<UDRE0)) );
    UDR0 = txbyte0;
    while ( !( UCSR0A & (1<<UDRE0)) );
    UDR0 = txbyte1;
    txfull=0;
  }
}


/* Result is read in TIMER1 ISR instead of ADC ISR.
   This is done because ADC is triggered by the timer interrupt flag
   and it's not triggered again before the interrupt flag is cleared.
   It can be cleared manually in ADC ISR but then it's cleared too late
   resulting in every second trigger to be missed.
   Using the TIMER1 ISR clears it automatically and we can use it to also
   read the ADC result to avoid the need for separate ADC interrupt,
   saving some time. */
ISR(TIMER1_COMPB_vect) {
  uint16_t ad;

  ad = ADCW;
  PORTB ^= 1<<4; // PB4 = digital pin 12
  
  if(!txfull) {
    PORTB &= ~(1<<3);
    // send lowest 7 bits in first byte, highest 3 bits in second byte
    // highest bit of first byte is 1 to allow simple synchronization
    txbyte0 = 0x80 | (ad & 0x7F);
    txbyte1 = 0x07 & (ad >> 7);
    txfull = 1;
  } else {
    PORTB |= 1<<3; // UART too slow, put PB3 high as "error message"
  }
}

