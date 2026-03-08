/*
Project name:	FLT/CU1 multicart
Author: 		trwgQ26xxx
Date:			25.02.2026
Target MCUs:	Attiny 406/806
Compiler:		AVR_8_bit_GNU_Toolchain_4.0.0_52), Microchip.ATtiny_DFP.3.3.272
*/

#include <stdint.h>
#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/fuse.h>

/*
------------------ CONNECTIONS ------------------

7 segment display:
PA5 - PA7 => A, B, C; PB2 - PB5 => D, E, G, F; PA4 = DP

Flash memory address lines:
PC0 - PC2 => A16 - A18; PC3 = NC (A19 - FFU)

FLT reset:
PB0 = #FLT_RST

Push buttons:
PA3 = NEXT_SW, PA2 = BACK_SW

------------------ CONNECTIONS ------------------
*/

#define NUMBER_OF_BANKS	8

#define F_TIMER			(F_CPU / 256)					/* Hz, for TCA0 with DIV256 prescaler */
#define KEYB_DELAY		200								/* ms */
#define TIMER_VAL		((KEYB_DELAY * F_TIMER) / 1000)	/* ticks */
#define TIMER_VAL_MAX	0xFFFF							/* Max value for 16-bit timer */

#if (TIMER_VAL > TIMER_VAL_MAX)
	#error "Set keyboard delay is larger than maximum possible"
#endif

#define NEXT_KEY_IS_PRESSED				(!(PORTA.IN & (1 << 3)))
#define BACK_KEY_IS_PRESSED				(!(PORTA.IN & (1 << 2)))
#define ALL_KEYS_ARE_RELEASED			((PORTA.IN & (1 << 3)) && (PORTA.IN & (1 << 2)))

/* DIGITS: 1 - 8 */
volatile const unsigned char led_codes[NUMBER_OF_BANKS] = {0x06, 0x3B, 0x2F, 0x66, 0x6D, 0x7D, 0x07, 0x7F};

volatile bool keyboard_locked = false;
volatile uint8_t current_mem_bank = 0;

static void Run_FLT(bool run);
static void Change_mem_bank(uint8_t bank);
static void Update_LED_Display(uint8_t digit, bool dot_status);

static void Start_Timer(void);
static void Switch_Bank(void);

int main(void)
{
	/* Poke WDT */
	wdt_reset();

	/* FUSE set to 16MHz, enable 16 prescaler so F_CPU is 1MHz */
	_PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, CLKCTRL_PDIV_16X_gc | CLKCTRL_PEN_bm);

	/* Initialize timer */
	TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV256_gc;	/* Set prescaler to 256 */
	TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_NORMAL_gc;	/* Set normal mode */
	TCA0.SINGLE.CTRLC = 0;								/* No compare channels */
	TCA0.SINGLE.CTRLD = 0;								/* No split mode */
	TCA0.SINGLE.CTRLECLR = TCA_SINGLE_DIR_bm;			/* Clear direction bit (count up mode) */
	TCA0.SINGLE.EVCTRL = 0;								/* No event control */
	TCA0.SINGLE.PER = TIMER_VAL;						/* Set period for desired delay */
	TCA0.SINGLE.CNT = 0;								/* Clear counter */

	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;			/* Clear interrupt flag */
	TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;			/* Enable overflow interrupt */

	/* Initialize memory bank switch output */
	PORTC.DIRSET = 0x0F;
	PORTC.OUTCLR = 0x0F;								/* Set to first bank */

	/* Initialize FLT reset output */
	PORTB.DIRSET = (1 << 0);
	PORTB.OUTSET = (1 << 0);							/* Hold in reset */

	/* Initialize display */
	PORTA.DIRSET = 0xF0; PORTB.DIRSET = 0x3C;
	PORTA.OUTSET = 0xF0; PORTB.OUTSET = 0x3C;			/* LED display off */

	/* Initialize keyboard inputs */
	PORTA.DIRCLR = (1 << 3) | (1 << 2);
	PORTA.PIN2CTRL = PORT_PULLUPEN_bm;					/* Enable pull-up on PA2 (BACK_SW) */
	PORTA.PIN3CTRL = PORT_PULLUPEN_bm;					/* Enable pull-up on PA3 (NEXT_SW) */

	/* Configure unused pins */
	PORTA.DIRCLR = (1 << 1) | (1 << 0);					/* Configure PA0 and PA1 as inputs */
	PORTA.PIN0CTRL = PORT_PULLUPEN_bm;					/* Enable pull-up on PA0 */
	PORTA.PIN1CTRL = PORT_PULLUPEN_bm;					/* Enable pull-up on PA1 */
	PORTB.DIRCLR = (1 << 1);							/* Configure PB1 as input */
	PORTB.PIN1CTRL = PORT_PULLUPEN_bm;					/* Enable pull-up on PB1 */

	/* Init variables */
	current_mem_bank = 0;
	keyboard_locked = true;

	/* Enable interrupts globally */
	sei();

	/* Do one transaction on start */
	Switch_Bank();

	while(true)
	{
		/* If keyboard is unlocked */
		if(keyboard_locked == false)
		{
			/* Check NEXT key */
			if(NEXT_KEY_IS_PRESSED)
			{
				/* If pressed, advance bank forward */
				if(current_mem_bank >= (NUMBER_OF_BANKS - 1))
				{
					current_mem_bank = 0;
				}
				else
				{
					current_mem_bank++;
				}

				Switch_Bank();
			}
			/* Check BACK key */
			else if(BACK_KEY_IS_PRESSED)
			{
				/* If pressed, advance bank backward */
				if(current_mem_bank == 0)
				{
					current_mem_bank = NUMBER_OF_BANKS - 1;
				}
				else
				{
					current_mem_bank--;
				}

				Switch_Bank();
			}
		}

		/* Poke WDT */
		wdt_reset();
	}
}

ISR(TCA0_OVF_vect)
{
	/* Kill timer */
	TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm;

	/* Clear IRQ flag */
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;

	/* Run FLT */
	Run_FLT(true);

	/* Update display, hide dot */
	Update_LED_Display(current_mem_bank, false);

	/* Check if all keys are released */
	if(ALL_KEYS_ARE_RELEASED)
	{
		/* Unlock keyboard */
		keyboard_locked = false;
	}
	else
	{
		/* Waiting for buttons to be released, restart timer */
		Start_Timer();
	}
}

static void Start_Timer(void)
{
	/* Kill timer */
	TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm;

	/* Clear timer */
	TCA0.SINGLE.CNT = 0;

	/* Clear IRQ flag */
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;

	/* Start timer */
	TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;
}

static void Switch_Bank(void)
{
	/* Lock keyboard */
	keyboard_locked = true;

	/* Halt FLT */
	Run_FLT(false);

	/* Switch bank */
	Change_mem_bank(current_mem_bank);

	/* Update display, show dot */
	Update_LED_Display(current_mem_bank, true);

	/* Start timer */
	Start_Timer();
}

static void Run_FLT(bool run)
{
	/* Set FLT control pin */
	if(run == true)
		PORTB.OUTSET = (1 << 0);
	else
		PORTB.OUTCLR = (1 << 0);
}

static void Change_mem_bank(uint8_t bank)
{
	/* Change memory bank */
	PORTC.OUTCLR = 0x0F;
	PORTC.OUTSET = (bank & 0x0F);
}

static void Update_LED_Display(uint8_t digit, bool dot_status)
{
	/* Check if digit is within range */
	if(digit >= NUMBER_OF_BANKS)
		digit = 0;

	/* Clear display */
	PORTA.OUTSET = 0xE0;
	PORTB.OUTSET = 0x3C;

	/* Update digit */
	PORTA.OUTCLR = (led_codes[digit] & 0x07) << 5;
	PORTB.OUTCLR = (led_codes[digit] & 0x78) >> 1; // ((led_codes[digit] & 0x78) >> 3) << 2

	/* Update dot */
	if(dot_status == true)
		PORTA.OUTCLR = (1 << 4);
	else
		PORTA.OUTSET = (1 << 4);
}

/* FUSE configuration */
FUSES = {
	.WDTCFG  = 0x06,	/* Watchdog: Timeout = 256CLK (0.256s), Window mode off */
	.BODCFG  = 0xF5,	/* BOD: Level = 4.2V, Sample Frequency = 125Hz, Enabled mode in active & sleep */
	.OSCCFG  = 0x01,	/* Internal 16MHz RC Oscillator, calibration registers unlocked */
	.SYSCFG0 = 0xF2,	/* No CRC, Reset/UPDI pin configured as GPIO, EEPROM erased during chip erase */
	.SYSCFG1 = 0x04,	/* Start-Up Time = 8ms */
	.APPEND  = 0x00,	/* Application Code Section End, unused */
	.BOOTEND = 0x00		/* Boot Section End, unused */
};

/* LOCKBIT configuration */
LOCKBITS = 0xC5;		/* Memory access is unlocked */
