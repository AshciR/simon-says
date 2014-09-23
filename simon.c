/* --COPYRIGHT--,BSD
 * Copyright (c) 2012, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/

/**************************************************************************************/
/* This project will implement a Simon­like game where
 * the 5 LEDs on the capacitive touch pads are lit one at a time in a
 * pseudo­random sequence.
 *
 * When the player
 * correctly repeats the sequence by touching the corresponding touch pads then a
 * longer sequence is played.  If the player makes an error then the game should
 * indicate it by flashing LEDs and writing to the LCD.
 *
 * Please note that this project is based of a template provided by Susan Jarvis
 * for the course ECE 2049 at WPI.
 *
 * Inputs: None
 * Outputs None
 *
 * Written by : Richard Walker
 * 				28 Jun 2014
 */

#include <msp430.h>
#include <stdint.h>
#include <stdlib.h>
#include "inc\hw_memmap.h"
#include "driverlibHeaders.h"
#include "CTS_Layer.h"

#include "grlib.h"
#include "LcdDriver/Dogs102x64_UC1701.h"

/* LED definitions */
#define NUM_KEYS	5
#define LED4		BIT5
#define LED5		BIT4
#define LED6		BIT3
#define LED7		BIT2
#define LED8		BIT1

/* Simon game state definition */
#define WELCOME		0
#define PLAYSEQ		1
#define CHECSEQ		2
#define ERROR		3
#define CONGRATS	4

/* Integers used to represent buttons */
#define NULL_BUTTON	-1
#define X			0
#define Square		1
#define Octogon		2
#define Triangle	3
#define Circle		4

/* Maximum Sequence Length */
#define MAX_SEQ_LENGTH	64

struct Element* keypressed;  // user defined struct Element is defined in file structure.c

/*
 * Elements of this list contain information about the hardware
 * for each capacitive button on the board.
 */
const struct Element* address_list[NUM_KEYS] =
{
	&PAD1,	// X
	&PAD2,	// Square
	&PAD3,  // Octagon
	&PAD4,	// Triangle
	&PAD5	// Circle
};

/*
 * Create array of masks for setting the LEDs
 */
const uint8_t ledMask[NUM_KEYS] =
{
    LED8,
    LED7,
    LED6,
    LED5,
    LED4
};

// Define global variables
tContext g_sContext;	// user defined type used by graphics library
long stringWidth = 0;

// Function prototypes
void configDisplay(void);
void configTouchPadLEDs(void);
void swDelay(char);
void initSys(void);
void dispWelcome(void);
int getButton(void);
void countDown(int *state);
void playSequence(int *sequence, int *diff, int *state);
int getSpeed(int difficulty);
void checkSequence(int *sequence, int *diff, int *state);
void displayButton(int button);
void errorMsg(int *state);
void congratsMsg(int *state);

void main(void) {

	int state = WELCOME; 				//The default state plays the welcome message
	volatile int button = NULL_BUTTON;	// Stores the last button pressed
	int givenSeq[MAX_SEQ_LENGTH] = {0}; // Holds the sequence given by the game
	int diff = 0; 				 		// Holds the current difficulty of the game, start at 1 by default

	/* Initialize the system.*/
	initSys();

	/* Start the infinite loop */
	while (1) {

		/* Determines which state the game is currently in */
		switch (state) {
		case WELCOME:

			/* Display welcome message */
			dispWelcome();

			/* Keep checking until a button is pressed */
			do {
				button = getButton();
			} while ((button == NULL_BUTTON) || (button != X));

			/* If X is pressed, start the countdown, then go to play sequence */
			countDown(&state);
			break;

		case PLAYSEQ:

			/* Play the sequence to be mimicked*/
			playSequence(givenSeq, &diff, &state);
			break;

		case CHECSEQ:

			/* Check the sequence provided by the user*/
			checkSequence(givenSeq, &diff, &state);
			break;

		case ERROR:
			/* Display's Error Message */
			errorMsg(&state);
			break;

		case CONGRATS:
			/* Display's congratulation Message */
			congratsMsg(&state);
			break;

		} // End of Switch

	} // End of Infinite Loop

}	// End of Main


void initSys(void) {
	// Stop WDT
	WDTCTL = WDTPW | WDTHOLD;		// Stop watchdog timer

	//Perform initializations
	configTouchPadLEDs();
	configDisplay();

	/* establish baseline for cap touch monitoring */
	TI_CAPT_Init_Baseline(&keypad);
	TI_CAPT_Update_Baseline(&keypad, 5);
}

void configDisplay(void) {
	// Set up LCD -- These function calls are part on a TI supplied library
	Dogs102x64_UC1701Init();
	GrContextInit(&g_sContext, &g_sDogs102x64_UC1701);
	GrContextForegroundSet(&g_sContext, ClrBlack);
	GrContextBackgroundSet(&g_sContext, ClrWhite);
	GrContextFontSet(&g_sContext, &g_sFontFixed6x8);
	GrClearDisplay(&g_sContext);
	GrFlush(&g_sContext);
}

void configTouchPadLEDs(void) {
	/* This function initializes the digital IO Port 1 pins used by LEDs 4-8
	 *         LED4--------R-----<P1.5
	 *         LED5--------R-----<P1.4
	 *         LED6--------R-----<P1.3
	 *         LED7--------R-----<P1.2
	 *         LED8--------R-----<P1.1
	 *
	 * Inputs: None
	 * Outputs: None
	 *
	 * Susan Jarvis, ECE2049, 28 Aug 2013
	 */

	P1SEL = P1SEL & ~(BIT5 | BIT4 | BIT3 | BIT2 | BIT1);    // P1SEL = XXX0 0000
	P1DIR = P1DIR | (BIT5 | BIT4 | BIT3 | BIT2 | BIT1);     // P1DIR = XXX1 1111
	P1OUT = P1OUT & ~(BIT5 | BIT4 | BIT3 | BIT2 | BIT1); // P1OUT = XXX0 0000 = Turn LEDs 4-8 off
}

void swDelay(char numLoops) {
	// This function is a software delay. It performs
	// useless loops to waste a bit of time
	//
	// Input: numLoops = number of delay loops to execute
	// Output: none
	//
	// smj, ECE2049, 25 Aug 2013

	volatile unsigned int i, j;	// volatile to prevent optimization
								// by compiler

	for (j = 0; j < numLoops; j++) {
		i = 50000;					// SW Delay
		while (i > 0)				// could also have used while (i)
			i--;
	}
}

void dispWelcome(void){

	/* Clears the screen */
	GrClearDisplay(&g_sContext);

	/* Displays welcome screen*/
	GrStringDrawCentered(&g_sContext, "Welcome to", AUTO_STRING_LENGTH, 51, 8, TRANSPARENT_TEXT);
	GrStringDrawCentered(&g_sContext, "Simon Says", AUTO_STRING_LENGTH, 51, 24, TRANSPARENT_TEXT);
	GrStringDrawCentered(&g_sContext, "Press X", AUTO_STRING_LENGTH, 51, 40, TRANSPARENT_TEXT);
	GrStringDrawCentered(&g_sContext, "to Start", AUTO_STRING_LENGTH, 51, 56, TRANSPARENT_TEXT);

	/* Write the message to the screen */
	GrFlush(&g_sContext);
}

int getButton(void) {
	/* Checks to see if the user what button the user pressed.
	 * Turns on the LED of the button pressed*/

	/* hold the index number of the button that was pressed according
	 * to address_list.
	 */
	volatile int buttonPressed = NULL_BUTTON; // Default to a number that's not in the address list
	volatile int i;							  // Loop Incrementor

	/* Infinite Loop that checks if a button is pressed */
	do {
		/* Turn off the LED Keys */
		P1OUT = P1OUT & ~(BIT5 | BIT4 | BIT3 | BIT2 | BIT1);

		/* Check cap touch keys
		 * This function returns an element in address_list matching the pressed key. */
		keypressed = (struct Element *) TI_CAPT_Buttons(&keypad);
		__no_operation();   // one instruction delay as a precaution
							// for monitoring cap touch press

		if (keypressed)  	// If some key was pressed
		{
			/*Loop over all the key and find the one in address_list that matches */
			for (i = 0; i < NUM_KEYS; i++) {
				if (keypressed == address_list[i]) {
					P1OUT |= ledMask[i]; // turn on LED of pressed key
					buttonPressed = i;	 // gets the index of the key pressed
					swDelay(1);
				}
			}
		} // End of If key pressed

	} while (buttonPressed == NULL_BUTTON);

	/* Will return NULL_BUTTON if no button is pressed.
	 * Else it will return the index associated with the button
	 * According to address_list.
	 */
	return buttonPressed;
}

void countDown(int *state){
	/* Plays the countdown sequence, then changes the
	 * state from WELCOME to PLAYSEQ
	 */

	/* Clears the screen */
	GrClearDisplay(&g_sContext);

	/* Displays countdown screen*/
	GrStringDrawCentered(&g_sContext, "3", AUTO_STRING_LENGTH, 51, 32, TRANSPARENT_TEXT);
	GrFlush(&g_sContext);
	swDelay(3);

	/* Clears the screen */
	GrClearDisplay(&g_sContext);

	GrStringDrawCentered(&g_sContext, "2", AUTO_STRING_LENGTH, 51, 32, TRANSPARENT_TEXT);
	GrFlush(&g_sContext);
	swDelay(3);

	/* Clears the screen */
	GrClearDisplay(&g_sContext);

	GrStringDrawCentered(&g_sContext, "1", AUTO_STRING_LENGTH, 51, 32, TRANSPARENT_TEXT);
	GrFlush(&g_sContext);
	swDelay(3);

	/* Clears the screen */
	GrClearDisplay(&g_sContext);

	GrStringDrawCentered(&g_sContext, "Go!", AUTO_STRING_LENGTH, 51, 32, TRANSPARENT_TEXT);
	GrFlush(&g_sContext);
	swDelay(3);

	/* After countdown, go on to the Play Sequence State */
	*state = PLAYSEQ;

}

void playSequence(int *sequence, int *diff, int *state){
	 /*Plays the sequence needed
	 * to be mimicked by the player.
	 * Then goes to the next CHECSEQ STATE
	 *
	 * Inputs: The array holding the given sequence
	 *		   The current difficulty of the game
	 */

	/* Turn off the LED Keys */
	P1OUT = P1OUT & ~(BIT5 | BIT4 | BIT3 | BIT2 | BIT1);

	/* The current difficulty of the game */
	int level = *diff;

	/* Holds the randomly generated pad number */
	int randPad;

	/*Incrementor for counter*/
	int playLED;

	/* Clears the screen*/
	GrClearDisplay(&g_sContext);

	/*Tells the user that the sequence is now playing*/
	GrStringDrawCentered(&g_sContext, "Playing", AUTO_STRING_LENGTH, 51, 24, TRANSPARENT_TEXT);
	GrStringDrawCentered(&g_sContext, "Sequence!", AUTO_STRING_LENGTH, 51, 40, TRANSPARENT_TEXT);
	GrFlush(&g_sContext);

	/* Generates a random number
	 * to represent the pad to be touched*/
	randPad = rand() % NUM_KEYS;

	 /*Update the given sequence array*/
	sequence[level] = randPad;

	for (playLED = 0; playLED <= level; playLED++) {

		/* Turn on then off LED */
		P1OUT |= ledMask[ sequence[playLED] ]; // Turn on the LED
		swDelay(getSpeed(level));  // Wait a bit
		P1OUT &= ~ledMask[ sequence[playLED] ];// Turn off the LED
		swDelay(getSpeed(level));  // Wait a bit
	}

	/* Goes to the Check Sequence State */
	*state = CHECSEQ;
}

int getSpeed(int difficulty){
	/* Returns a software delay speed based on
	 * the current difficulty of the game
	 */

	int speed;

	if (difficulty < 13 ) {
		speed = 2;
	}
	else if( (difficulty >= 13) && (difficulty < 26 ) ) {
		speed = 1;
	}
	else{
		speed = 0;
	}

	return speed;
}

void checkSequence(int *sequence, int *diff, int *state){
	/*Asks the user play the sequence that was provided */

	int currentButton;					// Incrementor for loop
	int isSeqCorrect = 1;				// Boolean to hold if the sequence was correct, True by default

	/* The current difficulty of the game */
	int level = *diff;

	/* Clears the screen*/
	GrClearDisplay(&g_sContext);

	/* Cycle through the Played Sequence */
	for (currentButton = 0; currentButton <= level; ++currentButton) {

		/*Display message for the user*/
		GrStringDrawCentered(&g_sContext, "Checking", AUTO_STRING_LENGTH, 51, 8, TRANSPARENT_TEXT);
		GrStringDrawCentered(&g_sContext, "Sequence!", AUTO_STRING_LENGTH, 51, 24, TRANSPARENT_TEXT);
		GrFlush(&g_sContext);

		/*Holds the integer value of the button pressed by the user */
		int button = getButton();

		/* Show the button, then clear the screen */
		displayButton(button);
		GrClearDisplay(&g_sContext);

		/*Display message for the user again*/
		GrStringDrawCentered(&g_sContext, "Checking", AUTO_STRING_LENGTH, 51, 8, TRANSPARENT_TEXT);
		GrStringDrawCentered(&g_sContext, "Sequence!", AUTO_STRING_LENGTH, 51, 24, TRANSPARENT_TEXT);
		GrFlush(&g_sContext);

		/* If the current button is incorrect
		 * go to the ERROR state */
		if (button != sequence[currentButton]){
			/* Goes to the Error State */
			isSeqCorrect = 0;		// The Sequence was incorrect
			break;
		}

	} // End for loop

	/* Takes appropriate action depending on
	 * if the user got the sequence correct
	 */
	if(isSeqCorrect != 0){
		/* The user got the sequence correct */

		/* Clears the screen*/
		GrClearDisplay(&g_sContext);

		/*Display message for the user*/
		GrStringDrawCentered(&g_sContext, "GOOD JOB!", AUTO_STRING_LENGTH, 51, 32, TRANSPARENT_TEXT);
		GrFlush(&g_sContext);

		/* Is this the max level? */
		if (level < MAX_SEQ_LENGTH){
			/* Max Level has not been reached yet */
			*diff = ++level; 	// Increase the difficulty
			*state = PLAYSEQ; 	// Set the state to PLAYSEQ

		}else{
			/* The Max level has been reached,
			 * the player beat the game.
			 */
			*state = CONGRATS; 	// Set the state to CONGRATS
		}
	}
	else{
		/* The user got the sequence incorrect */
		*state = ERROR; 	// Set the state to ERROR

	}

}

void displayButton(int button){
	/*When the user presses a button, display it on the screen */
	switch (button){

	case X:
		GrStringDrawCentered(&g_sContext, "1", AUTO_STRING_LENGTH, 10, 40, TRANSPARENT_TEXT);
		break;

	case Square:
		GrStringDrawCentered(&g_sContext, "2", AUTO_STRING_LENGTH, 30, 40, TRANSPARENT_TEXT);
		break;

	case Octogon:
		GrStringDrawCentered(&g_sContext, "3", AUTO_STRING_LENGTH, 50, 40, TRANSPARENT_TEXT);
		break;

	case Triangle:
		GrStringDrawCentered(&g_sContext, "4", AUTO_STRING_LENGTH, 70, 40, TRANSPARENT_TEXT);
		break;

	case Circle:
		GrStringDrawCentered(&g_sContext, "5", AUTO_STRING_LENGTH, 90, 40, TRANSPARENT_TEXT);
		break;
	}

	GrFlush(&g_sContext);
}

void errorMsg(int *state){

	/* Clears the screen*/
	GrClearDisplay(&g_sContext);

	/* Displays welcome screen*/
	GrStringDrawCentered(&g_sContext, "You Lost!", AUTO_STRING_LENGTH, 51, 8, TRANSPARENT_TEXT);
	GrStringDrawCentered(&g_sContext, "Sorry,", AUTO_STRING_LENGTH, 51, 24, TRANSPARENT_TEXT);
	GrStringDrawCentered(&g_sContext, "Thanks for", AUTO_STRING_LENGTH, 51, 40, TRANSPARENT_TEXT);
	GrStringDrawCentered(&g_sContext, "Playing!", AUTO_STRING_LENGTH, 51, 56, TRANSPARENT_TEXT);
	GrFlush(&g_sContext);

	/* Gives the user sometime to see the message */
	swDelay(1);

	/* Go back to the Welcome screen */
	*state = WELCOME; 	// Set the state to WELCOME
}

void congratsMsg(int *state){
	/* Clears the screen*/
	GrClearDisplay(&g_sContext);

	/* Displays welcome screen*/
	GrStringDrawCentered(&g_sContext, "CONGRATS,!", AUTO_STRING_LENGTH, 51, 8, TRANSPARENT_TEXT);
	GrStringDrawCentered(&g_sContext, "YOU WON!", AUTO_STRING_LENGTH, 51, 24, TRANSPARENT_TEXT);
	GrStringDrawCentered(&g_sContext, "Thanks for", AUTO_STRING_LENGTH, 51, 40, TRANSPARENT_TEXT);
	GrStringDrawCentered(&g_sContext, "Playing!", AUTO_STRING_LENGTH, 51, 56, TRANSPARENT_TEXT);
	GrFlush(&g_sContext);

	/* Gives the user sometime to see the message */
	swDelay(1);

	/* Go back to the Welcome screen */
	*state = WELCOME; 	// Set the state to WELCOME

}
