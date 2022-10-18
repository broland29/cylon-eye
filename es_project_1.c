/* Embedded Software - Project 1 - Bálint Roland
 *	- unfortunately i did not follow the proposed skeleton
 *		- this solution is purely interrupt-based, which is nice as the objective was to familiarize with them
 *		- this makes some part of the code uninterruptible (there are "too many" interrupts)
 *	- the cylon eye continues traveling even if unseen (hidden by the thermometer)
 *	- there is a little trick at the beginning, which makes the first "movement" slower	
 */

#include <avr/io.h>				// for standard library iom4809.h
#include <avr/interrupt.h>		// for ISR
#include <avr/cpufunc.h>		// for ccp_write_io

#define GOING_RIGHT 0	// cyclon eye going right
#define GOING_LEFT  1	// cyclon eye going left

#define FULL_SCALE 1023		//10 bit resolution
// first level is 0, it's minimum value is 0V; important to have a float in the paranthesis
#define LEVEL_1_MIN FULL_SCALE * (1/10.0)
#define LEVEL_2_MIN FULL_SCALE * (2/10.0)
#define LEVEL_3_MIN FULL_SCALE * (3/10.0)
#define LEVEL_4_MIN FULL_SCALE * (4/10.0)
#define LEVEL_5_MIN FULL_SCALE * (5/10.0)
#define LEVEL_6_MIN FULL_SCALE * (6/10.0)
#define LEVEL_7_MIN FULL_SCALE * (7/10.0)
#define LEVEL_8_MIN FULL_SCALE * (8/10.0)
#define LEVEL_9_MIN FULL_SCALE * (9/10.0)

#define THREE_VOLTS FULL_SCALE * (3/5.0)	// since VDD is the reference, full scale is 5 volts

#define NORMAL 0		// traveling 1
#define FLIPPED 1		// traveling 0
uint8_t mode = NORMAL; 

/* Time math
 *	- CPU frequency is 20*10^6 => period is 0.05 us
 *	- prescaler is 1024 => divide f. by 1024 => actual period is 51.2 us
 *	- time = count * period, max count is set by PER register
 *		- for 1 second:		count = 1,000,000 / 51.2 = 19,531.25
 *		- for 0.25 second:	count = 250,000 / 51.2 = 4,882.8125
 */
#define PER_FOR_1PS	19531
#define PER_FOR_4PS 4882


// association between PORTs and bits connected to the led array
struct led_bits_t
{
	PORT_t *port;	// led port
	uint8_t bm;		// bit mapping
};


/* for details, check "ATmega4809 mapping to Arduino Nano and UNO"
 *	- values are from the two data sheets, reflecting connections
 *	- my approach is to make an array of structs to store mapping
 *		- it looks less error prone than two separate arrays
 *		- makes code more visible
 *		- i can write loops, manipulate based on index where suitable
 */
struct led_bits_t LED_ARRAY[10] =
{
	{&PORTC, PIN5_bm},
	{&PORTC, PIN4_bm},
	{&PORTA, PIN0_bm},
	{&PORTF, PIN5_bm},
	{&PORTC, PIN6_bm},
	{&PORTB, PIN2_bm},
	{&PORTF, PIN4_bm},
	{&PORTA, PIN1_bm},
	{&PORTA, PIN2_bm},
	{&PORTA, PIN3_bm}
};


void clock_init (void);
void leds_init(void);
void buttons_init(void);

void clear_leds(void);
void set_leds(void);
void toggle_leds(void);

void tca0_init(void);
void rtc_init(void);
void adc_init(void);


int main(void)
{	
	clock_init();
	leds_init();
	buttons_init();
	
	tca0_init();
	rtc_init();
	adc_init();
	
	// i cheat a little and start by first led being on; since my 
	//  approach works with toggling, this workaround is necessary
	clear_leds();
	LED_ARRAY[0].port->OUTSET = LED_ARRAY[0].bm;	
	
	sei();
	
	while(1)
	{
	}
	
	return 1;	
}


// disables clock prescaler, so we will have 20MHz - this is an assembly macro
void clock_init (void)
{
	ccp_write_io( (void *) &CLKCTRL.MCLKCTRLB , (0 << CLKCTRL_PEN_bp));
}


// sets the used bits (by LEDs) to outputs 
void leds_init()
{
	PORTC.DIR = PIN6_bm | PIN5_bm | PIN4_bm;
	PORTA.DIR = PIN3_bm | PIN2_bm | PIN1_bm | PIN0_bm;
	PORTB.DIR = PIN2_bm;
	PORTF.DIR = PIN5_bm | PIN4_bm;
}


// turns off every LED
void clear_leds()
{	
	for(uint8_t i = 0; i < 10; i++)
	{
		LED_ARRAY[i].port->OUTCLR = LED_ARRAY[i].bm;
	}	
}


// turns on every LED
void set_leds()
{	
	for(uint8_t i = 0; i < 10; i++)
	{
		LED_ARRAY[i].port->OUTSET = LED_ARRAY[i].bm;
	}
}


// toggles every LED
void toggle_leds()
{	
	for(uint8_t i = 0; i < 10; i++)
	{
		LED_ARRAY[i].port->OUTTGL = LED_ARRAY[i].bm;
	}
}


/* Timer Counter A0
 *	- used in 16 bit mode => we control it through TCA0.SINGLE
 *	- for time math, see  #define section
 */
void tca0_init(void)
{
	TCA0.SINGLE.CTRLA = 0b00001111;		// prescaler DIV1024 and enable (start count)
	TCA0.SINGLE.CTRLB = 0b00000000;		// normal mode (top value in PER register)
	
	TCA0.SINGLE.EVCTRL = 0b00000000;		// disable count on event input
	TCA0.SINGLE.INTCTRL = 0b00000001;		// enable counter overflow/underflow
	TCA0.SINGLE.PER = PER_FOR_1PS;			//will overflow when this value occurs
}


/* interrupt service routine of TCA0
 *	- happens when TCA0 overflows
 *	- moves the led, creating the cylon eye effect
 */
ISR(TCA0_OVF_vect)
{	
	static uint8_t position;						// default static local variable value is 0
	static uint8_t direction = GOING_RIGHT;			// we start by turning on second led and turning off first
	
	// start with new "canvas"; necessary since "thermometer" may (and will) ruin our pattern
	if(mode == NORMAL)
	{
		clear_leds();
	}
	else if (mode == FLIPPED)
	{
		set_leds();
	}
	
	// logic for traveling led	
	if(direction == GOING_RIGHT)
	{
		if(position == 9)				// reached last led to the right - change direction
		{
			direction = GOING_LEFT;
			position--;
			LED_ARRAY[position].port->OUTTGL = LED_ARRAY[position].bm;
		}
		else							// regular "shift"
		{
			position++;
			LED_ARRAY[position].port->OUTTGL = LED_ARRAY[position].bm;
		}
	}
	else if (direction == GOING_LEFT)
	{
		if(position == 0)				// reached last led to the left - change direction
		{
			direction = GOING_RIGHT;
			position++;
			LED_ARRAY[position].port->OUTTGL = LED_ARRAY[position].bm;
		}
		else							// regular "shift"
		{
			position--;
			LED_ARRAY[position].port->OUTTGL = LED_ARRAY[position].bm;
		}
	}
	
	// clear interrupt flag
	TCA0.SINGLE.INTFLAGS = 0b00000001;	
}


/* Real Time Counter 
 *	- offers two timing functions: RTC and PIT
 *	- we use PIT (Periodic Interrupt Timer)
 *	- the clock is 1024 Hz => period is 0.0009765625
 *	- time = count * clock_period
 *	- we want the time 16 sec => count is 16,384
 */
void rtc_init()
{
	while (RTC.STATUS > 0)
	{
		// wait for all registers to be synchronized 
	}
	RTC.CLKSEL = RTC_CLKSEL_INT1K_gc;							// select the 1024 Hz internal clock
	RTC.PITCTRLA = RTC_PERIOD_CYC16384_gc | RTC_PITEN_bm;		// number of clock cycles before int. is 16484, PIT enabled
	RTC.PITINTCTRL = RTC_PI_bm;									// periodic interrupt enabled
}


/* Interrupt Service Routine for RTC
 *	- controls variable "mode"
 *	- switches it periodically
 */
ISR(RTC_PIT_vect)
{
	if (mode == NORMAL)
	{
		mode = FLIPPED;
	}
	else if (mode == FLIPPED)
	{
		mode = NORMAL;
	}
	
	//clear interrupt flag
	RTC.PITINTFLAGS = RTC_PI_bm;	
}


/* Analog to Digital converter 0
 *	- set up to read potentiometer's value
 */
void adc_init(void)
{
	ADC0.CTRLA = 0b00000010;		// 10-bit resolution, free running mode, not enabled yet
	ADC0.CTRLB = 0b00000000;		// no sample accumulation
	ADC0.CTRLC = 0b01010110;		// sample capacitance 1; reference is VDD; prescaler DIV128
	ADC0.CTRLD = 0b00100000;		// initial delay 16 cycles
	ADC0.MUXPOS = 0b00000011;		// AIN3 is used (due to shield's arch.)
	ADC0.INTCTRL = 0b00000001;		// enable result ready interrupt
	
	ADC0.CTRLA |= 0b00000001;		// enable
	ADC0.COMMAND = 0b00000001;		// start the first conversion (free running mode => later ones start automatically)
	
	// reading ADC0.RES will set interrupt flag 1 automatically
}


// helper method to "print" thermometer pattern on LEDs
void thermometer(int level)
{
	clear_leds();
	for(uint8_t i = 0; i <= level; i++)
		LED_ARRAY[i].port->OUTSET = LED_ARRAY[i].bm;
}


/* Interrupt Service Routine for ADC0
 *	- reads value constantly
 *	- changes TCA0's period register accordingly
 *	- if button is pressed, displays thermometer pattern
*/
ISR(ADC0_RESRDY_vect)
{
	uint16_t adc_reading;		// 10 bit resolution => 8-bit variable not enough
	adc_reading = ADC0.RES;		// reading necessary even if not used, to reset int. flag

	// reason of using PERBUF is this avoids a bug: when switching from 1PS to 4PS, the period is decreased;
	// if the count was greater than the new limit, the count will overflow (takes time) before reaching the new limit
	if (adc_reading <= THREE_VOLTS)
		TCA0.SINGLE.PERBUF = PER_FOR_1PS;		
	else 
		TCA0.SINGLE.PERBUF = PER_FOR_4PS;
	
	if (PORTE.IN & PIN1_bm)
		return;

	if (adc_reading < LEVEL_1_MIN)
		thermometer(0);
	else if (adc_reading < LEVEL_2_MIN)
		thermometer(1);
	else if (adc_reading < LEVEL_3_MIN)
		thermometer(2);
	else if (adc_reading < LEVEL_4_MIN)
		thermometer(3);
	else if (adc_reading < LEVEL_5_MIN)
		thermometer(4);
	else if (adc_reading < LEVEL_6_MIN)
		thermometer(5);
	else if (adc_reading < LEVEL_7_MIN)
		thermometer(6);
	else if (adc_reading < LEVEL_8_MIN)
		thermometer(7);
	else if (adc_reading < LEVEL_9_MIN)
		thermometer(8);
	else
		thermometer(9);
}


// initialize button - the other button is unused because it is also connected to an LED
void buttons_init(void)
{
	PORTE.DIRCLR = PIN1_bm;					// make pin 1 output
	PORTE.PIN1CTRL |= PORT_PULLUPEN_bm;		// enable internal pull-up
}
