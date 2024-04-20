/* 
Project name:	FLT/CU1 multicart
Author: 		trwgQ26xxx
Date:			18.04.2024
Target MCUs:	AT89C1051/AT89C2051/AT89C4051 or AT89S2051/AT89S4051
Compiler:		SDCC version 4.4.0 (32-bit)
*/

#include <at89x051.h>

/*
------------------ CONNECTIONS ------------------

7 segment display:
P1.0 - P1.6 => A - G; P1.7 = DP

Flash memory address lines:
P3.0 - P3.2 => A16 - A18; P3.3 = NC (A19 - FFU)

FLT reset:
P3.4 = #FLT_RST

Push buttons:
P3.5 = NEXT_SW, P3.7 = RST_SW (BACK_SW)

------------------ CONNECTIONS ------------------
*/

#define CHANGE_MEM_BANK(bank)			do{ P3 &= 0xF0;	P3 |= (bank & 0x0F);		}while(0)

#define HALT_FLT						do{ P3_4 = 0;								}while(0)
#define RUN_FLT							do{ P3_4 = 1;								}while(0)

#define INIT_KEYS						do{ P3_5 = 1; P3_7 = 1;						}while(0)
#define NEXT_KEY_IS_PRESSED				(!(P3_5))
#define BACK_KEY_IS_PRESSED				(!(P3_7))
#define ALL_KEYS_ARE_RELEASED			((P3_5) && (P3_7))

enum DotStatus {DOT_ON = 1, DOT_OFF = 0};
#define DISABLE_LED_DISPLAY				do{ P1 = 0xFF;								}while(0)
#define UPDATE_LED_DISPLAY(digit, dot)	do{ P1 = (dot) ? (digit) : (digit & 0x80);	}while(0)

#define NUMBER_OF_BANKS	8

//#define F_CPU			3600000						/* Hz*/
//#define F_TIMER		(F_CPU / 12)				/* Hz */
//#define KEYB_DELAY	(65536 / F_TIMER) * 1000	/* ms */

enum KeyboardStatus { KEYBOARD_LOCKED = 2, KEYBOARD_WAITING_FOR_BUTTONS_TO_BE_RELEASED = 1, KEYBOARD_UNLOCKED = 0 };

volatile unsigned char keyboard_lock_status = KEYBOARD_LOCKED;
volatile unsigned char current_bank = 0;

void Start_Timer1(void)
{
	/* Kill timer */
	TR1 = 0;
	
	/* Clear timer */
	/* With 3.6MHz CPU clock it would give 218ms delay */
	TH1 = 0;
	TL1 = 0;	
		
	/* Clear flag */
	TF1 = 0;
	
	/* Start timer */
	TR1 = 1;
}

void Timer1_ISR(void) __interrupt(TF1_VECTOR)
{
	/* Kill timer */
	TR1 = 0;
	
	/* Clear IRQ flag */
	TF1 = 0;
	
	/* Run FLT */
	UPDATE_LED_DISPLAY(current_bank, DOT_OFF);
	RUN_FLT;
	
	/* Check if all keys are released */
	if(ALL_KEYS_ARE_RELEASED)
	{
		/* Unlock keyboard */
		keyboard_lock_status = KEYBOARD_UNLOCKED;
	}
	else
	{
		/* Waiting for buttons to be released */
		keyboard_lock_status = KEYBOARD_WAITING_FOR_BUTTONS_TO_BE_RELEASED;
		
		/* Restart timer */
		Start_Timer1();
	}
}

void Switch_Bank(void)
{
	/* Lock keyboard */
	keyboard_lock_status = KEYBOARD_LOCKED;
	
	/* Switch bank */
	HALT_FLT;
	UPDATE_LED_DISPLAY(current_bank, DOT_ON);
	CHANGE_MEM_BANK(current_bank);
	
	/* Start timer */
	Start_Timer1();
}

void main(void)
{
	/* DIGITS: 1 - 8 */
	unsigned char led_codes[NUMBER_OF_BANKS] = {0x79, 0x24, 0x30, 0x19, 0x12, 0x02, 0x78, 0x00};

	/* Set registers */
	TCON = 0;							/* Clear flags */
	TMOD = (0x1 << 4);					/* Timer 1 mode 1 (16-bit mode) */
	TL0 = 0; TH0 = 0; 					/* Clear timers */
	TL1 = 0; TH1 = 0;
	
	SCON = 0;							/* Disable UART */
	PCON = 0;							/* Disable power down modes */
	
	IP = (1 << 3);						/* Timer 1 high priority IRQ */
	TF1 = 0;							/* Clear flag */
	IE = ((1 << 7) | (1 << 3)) & 0xFF;	/* Enable IRQs & Timer 1 IRQ */
	
	P3_6 = 1;							/* Configure comparator internal output as input */
	
	/* Init variables */
	current_bank = 0;
	keyboard_lock_status = KEYBOARD_LOCKED;

	/* Set inputs */
	INIT_KEYS;
	
	/* Do one transaction on start */
	Switch_Bank();	
	
	while(1)
	{
		/* If keyboard is unlocked */
		if(keyboard_lock_status == KEYBOARD_UNLOCKED)
		{
			/* Check NEXT key */
			if(NEXT_KEY_IS_PRESSED)
			{
				/* If pressed, advance bank forward */
				if(current_bank >= (NUMBER_OF_BANKS - 1))
				{
					current_bank = 0;
				}
				else
				{
					current_bank++;
				}
				
				Switch_Bank();
			}
			/* Check BACK key */
			else if(BACK_KEY_IS_PRESSED)
			{
				/* If pressed, advance bank backward */
				if(current_bank == 0)
				{
					current_bank = NUMBER_OF_BANKS - 1;
				}
				else
				{
					current_bank--;
				}
				
				Switch_Bank();
			}	
		}
	}
}