/*****************************************************************************
 * EE2024 Assignment 2
 *
 * Author: Chang Chu-Ming
 *         Terry Chua
 *
 * Date: 25/10/2015
 *
 ******************************************************************************/
// Global includes
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

// Class includes
#include "task.h"

// CMSIS headers required for setting up SysTick Timer
#include "LPC17xx.h"

// Lib_MCU header files
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_uart.h"

#include "joystick.h"
#include "pca9532.h"
#include "acc.h"
#include "oled.h"
#include "rgbfixed.h"
#include "led7seg.h"
#include "temp.h"
#include "light.h"

//-----------------------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------------------
#define SAMPLING_TIME 2000
#define LIGHTNING_THRESHOLD 3000
#define LIGHTNING_THRESHOLD_TIME 500
#define LIGHTNING_TIME_WINDOW 3000
#define LIGHT_MONITORING 3000
#define TIME_UNIT 250
#define TICK_MILLIS 5 // sysTick ticks every TICK_MILLIS; controls how reactive you want the system to be
#define RANGE_K2 3892
#define NUM_OF_LED 16
#define NUM_OF_STRIPES 100
#define DEBOUNCE_TIME 500
#define MAX_SONG_LENGTH 256
#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);

//-----------------------------------------------------------------------------------------
// Function definitions
//-----------------------------------------------------------------------------------------
void UART3ReceiveInterruptHandler();
static void drawOled(uint8_t joyState);
void sendControlSeq(uint8_t* seq);
void stopCanvas();
void stopMusic();
void sw3Interrupt();
static void lightningInterruptHandler();

//-----------------------------------------------------------------------------------------
// Modes
//-----------------------------------------------------------------------------------------
#define STARTER 0
#define EXPLORER 1
#define SURVIVAL 2
#define CANVAS 3
#define MUSIC 4
int curMode = STARTER; // Default mode - STARTER
int prevMode = -1;
volatile int hasModeChanged = 1;

//-----------------------------------------------------------------------------------------
// Tasks
//-----------------------------------------------------------------------------------------
Task *showStartingSeqTask;
Task *showStartingAniTask;
Task *blinkRGBTask;
Task *getSensorValuesTask;
Task *showLEDSeqTask;
Task *resetLEDSeqTask;
Task *UARTDebounceTask;
Task *readJoystickTask;
Task *joystickDebounceTask;

Task *slowTaskList[30];
int slowTaskCount = 0;
Task *fastTaskList[30];
int fastTaskCount = 0;

//-----------------------------------------------------------------------------------------
// Common variables
//-----------------------------------------------------------------------------------------
int isBlinking = 0;
int isRGBLEDOn = 0;
int isOLEDOn = 0;
int isUARTDebounced = 0;
volatile uint32_t msTicks = 0; // counter for 1ms SysTicks
int curTicks = 0;
int lightningCount = 0;
uint8_t curRGBLEDColor = RGB_BLUE;

//-----------------------------------------------------------------------------------------
// STARTER mode variables
//-----------------------------------------------------------------------------------------
uint8_t startingSeq[13] = {'0', '1', '2', '3', '4', '5', '6', 'A', 'B', 'C', 'D', 'E', 'F'};
int seqLength = sizeof(startingSeq)/sizeof(uint8_t);
int curSeqIndex = 0;
int curAniIndex = 0;
int stripesX[NUM_OF_STRIPES];
int stripesY[NUM_OF_STRIPES];
uint8_t nameSeq[13] = {'H', 'O', 'P', 'E', ' ', 'b', 'y', ' ', 'C', 'M', '&', 'T', 'C'};

//-----------------------------------------------------------------------------------------
// EXPLORER mode variables
//-----------------------------------------------------------------------------------------
int8_t zInitial; // Initial value of z

//-----------------------------------------------------------------------------------------
// SURVIVAL mode variables
//-----------------------------------------------------------------------------------------
uint16_t ledOn = 0xffff;
int curLEDPos = 16;

//-----------------------------------------------------------------------------------------
// CANVAS & MUSIC variables
//-----------------------------------------------------------------------------------------
char *menu[] = {
		"Welcome to Hope.\n\r"
		"Press 1 to see the starting sequence.\n\r"
		"Press 2 to switch to explorer mode.\n\r"
		"Press 3 to switch to survival mode.\n\r"
		"Press 4 to start collaborative canvas.\n\r"
		"Press 5 to send a tune.\n\r"
		"Press any other key to see the menu.\n\r"
		"\n\r",

		"Collaborative canvas\n\r"
		"Use WASD to draw on the screen.\n\r"
		"Use IJKL to move your cursor.\n\r"
		"Press Q to quit.\n\r"
		"\n\r",

		"Music\n\r"
		"Enter a sequence of notes to play.\n\r"
		"Example: C2.C2,D4,C4,F4,E8\n\r"
		"Press Q to quit.\n\r"
		"\n\r"
		};
int curMenuPos = 0;
uint8_t up[4] = "[1A";
uint8_t down[4] = "[1B";
uint8_t left[4] = "[1D";
uint8_t right[4] = "[1C";
uint8_t clear[4] = "[2J";
uint8_t home[7] = "[H";
uint8_t center[8] = "[32;48H";
int isJoystickDebounced = 0;
static uint8_t currX = 48;
static uint8_t currY = 32;
static uint8_t lastX = 0;
static uint8_t lastY = 0;
static uint32_t notes[] = {
        2272, // A - 440 Hz
        2024, // B - 494 Hz
        3816, // C - 262 Hz
        3401, // D - 294 Hz
        3030, // E - 330 Hz
        2865, // F - 349 Hz
        2551, // G - 392 Hz
        1136, // a - 880 Hz
        1012, // b - 988 Hz
        1912, // c - 523 Hz
        1703, // d - 587 Hz
        1517, // e - 659 Hz
        1432, // f - 698 Hz
        1275, // g - 784 Hz
};

// ########################################################################################
// Initialize SSP
// ########################################################################################
static void init_ssp(void)
{
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);
}

// ########################################################################################
// Initialize I2C
// ########################################################################################
static void init_i2c(void)
{
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

// ########################################################################################
// Init GPIO
// ########################################################################################
static void init_GPIO(void)
{
	// Initialize SW4
	PINSEL_CFG_Type PinCfg;

	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 1;
	PinCfg.Pinnum = 31;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(1, 1<<31, 0);

	// Initialize SW3
	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 10;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(1, 2<<10, 0);

	// Initialize light sensor interrupt
	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 5;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(1, 2<<5, 0);
}

// ########################################################################################
// Initialize uart
// ########################################################################################
void init_uart() {
	UART_CFG_Type uartCfg;
	uartCfg.Baud_rate = 115200;
	uartCfg.Databits = UART_DATABIT_8;
	uartCfg.Parity = UART_PARITY_NONE;
	uartCfg.Stopbits = UART_STOPBIT_1;

	// Pinsel select for UART3
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 0;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 1;
	PINSEL_ConfigPin(&PinCfg);

	//supply power & setup working par.s for uart3
	UART_Init(LPC_UART3, &uartCfg);
	//enable transmit for uart3
	UART_TxCmd(LPC_UART3, ENABLE);
}

// ########################################################################################
// Initialize speaker
// ########################################################################################
void initSpeaker() {
    /* ---- Speaker ------> */

    GPIO_SetDir(2, 1<<0, 1);
    GPIO_SetDir(2, 1<<1, 1);

    GPIO_SetDir(0, 1<<27, 1);
    GPIO_SetDir(0, 1<<28, 1);
    GPIO_SetDir(2, 1<<13, 1);
    GPIO_SetDir(0, 1<<26, 1);

    GPIO_ClearValue(0, 1<<27); //LM4811-clk
    GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
    GPIO_ClearValue(2, 1<<13); //LM4811-shutdn
}

// ########################################################################################
// Initialize GPIO Interrupt
// ########################################################################################
void initGPIOInterrupt(void)
{
	// Set priority grouping to 5 (globally)
	NVIC_SetPriorityGrouping(5);

	// Light sensor Enable GPIO Interrupt P2.5
	LPC_GPIOINT->IO2IntEnF |= 1<<5;
    // SW3 - Enable GPIO Interrupt P2.10
    LPC_GPIOINT->IO2IntEnF |= 1<<10;
    // Enable EINT3 interrupt
    NVIC_EnableIRQ(EINT3_IRQn);
    // Set priority of interrupt
    uint32_t prio, PG = 5, PP=0b10, SP=0b000;
	prio = NVIC_EncodePriority(PG, PP, SP);
	NVIC_SetPriority(EINT3_IRQn, prio);
}

// ########################################################################################
// Initialize UART Interrupt
// ########################################################################################
void initUARTInterrupt() {
	UART_IntConfig(LPC_UART3, UART_INTCFG_RBR, ENABLE);
	UART_SetupCbs(LPC_UART3, 0, &UART3ReceiveInterruptHandler);
	NVIC_EnableIRQ(UART3_IRQn);
}

// ########################################################################################
// Initialize timer interrupt
// ########################################################################################
void initTimerInterrupt() {
	TIM_TIMERCFG_Type TIM_ConfigStruct;
	TIM_MATCHCFG_Type TIM_MatchConfigStruct ;

	// Initialize timer 0, prescale count time of 1ms
	TIM_ConfigStruct.PrescaleOption = TIM_PRESCALE_USVAL;
	TIM_ConfigStruct.PrescaleValue	= 1000;
	// use channel 0, MR0
	TIM_MatchConfigStruct.MatchChannel = 0;
	// Enable interrupt when MR0 matches the value in TC register
	TIM_MatchConfigStruct.IntOnMatch   = TRUE;
	//Enable reset on MR0: TIMER will not reset if MR0 matches it
	TIM_MatchConfigStruct.ResetOnMatch = TRUE;
	//Stop on MR0 if MR0 matches it
	TIM_MatchConfigStruct.StopOnMatch  = FALSE;
	//do no thing for external output
	TIM_MatchConfigStruct.ExtMatchOutputType =TIM_EXTMATCH_NOTHING;
	// Set Match value, count value is ms (timer * 1000uS =timer mS )
	TIM_MatchConfigStruct.MatchValue = TICK_MILLIS;

	// Timer 0 - Set configuration for Tim_config and Tim_MatchcConfig
	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &TIM_ConfigStruct);
	TIM_ConfigMatch(LPC_TIM0, &TIM_MatchConfigStruct);
	// To start timer 0
	TIM_Cmd(LPC_TIM0, ENABLE);
	// Set priority
	uint32_t prio, PG = 5, PP=0b11, SP=0b000;
	prio = NVIC_EncodePriority(PG, PP, SP);
	NVIC_SetPriority(TIMER0_IRQn, prio);
	/* Enable interrupt for timer 0 */
	NVIC_EnableIRQ(TIMER0_IRQn);
}

// ########################################################################################
// Common: Blank 7Seg
// ########################################################################################
void blank7Seg() {
	led7seg_setChar(0xFF, TRUE);
}

// ########################################################################################
// Common: Blank OLED
// ########################################################################################
void blankOLED() {
	oled_clearScreen(OLED_COLOR_BLACK);
	isOLEDOn = 0;
}

// ########################################################################################
// Common: Turn off RGB LED
// ########################################################################################
void blankRGBLED() {
	rgb_setLeds(RGB_GREEN);
}

// ########################################################################################
// Common: Send control seq to position cursor
// ########################################################################################
void sendControlSeq(uint8_t* seq) {
	uint8_t data = 27;
	UART_Send(LPC_UART3, &data, 1, BLOCKING);
	UART_Send(LPC_UART3, (uint8_t *)seq , strlen(seq), BLOCKING);
}

// ########################################################################################
// Common: Debounce callback for UART
// ########################################################################################
void UARTDebounceTimeout() {
	isUARTDebounced = 0;
}

// ########################################################################################
// STARTER: Show sequence
// ########################################################################################
void showStartingSeq() {
	  if (curSeqIndex < seqLength) {
		  led7seg_setChar(startingSeq[curSeqIndex], FALSE);
		  curSeqIndex++;
	  }
	  else {
		  blank7Seg();
		  prevMode = curMode;
		  curMode = EXPLORER;
		  hasModeChanged = 1;
	  }
	  if (curSeqIndex==8) {
			// Add starting animation task
			curAniIndex = 0;
			showStartingAniTask->repeatCount = 120;
			showStartingAniTask->runCount = 0;
			addTask(fastTaskList, &fastTaskCount, showStartingAniTask);
	  }
}

// ########################################################################################
// STARTER: Show animation
// ########################################################################################
void showStartingAni() {
	if (curAniIndex < 64) {
		int count;
		for (count = 0; count < NUM_OF_STRIPES; count++) {
			oled_putPixel(stripesX[count], (stripesY[count]+curAniIndex)%(63-curAniIndex), OLED_COLOR_WHITE);
			oled_putPixel(stripesX[count], (stripesY[count]+curAniIndex-5)%(63-curAniIndex), OLED_COLOR_BLACK);
		}
		oled_line(0, 63-curAniIndex, 96, 63-curAniIndex, OLED_COLOR_WHITE);
	} else  if (curAniIndex < 77){
		oled_putChar(8+(curAniIndex-64)*6, 15, nameSeq[curAniIndex-64], OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	} else if (curAniIndex == 77) {
		oled_circle(17, 40, 15, OLED_COLOR_BLACK);
		oled_putChar(15, 37, 'E', OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		oled_circle(82, 40, 10, OLED_COLOR_BLACK);
		oled_putChar(80, 37, 'M', OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	} else if (curAniIndex < 119 && curAniIndex%3!=0){
		oled_putPixel(curAniIndex-46, 40, OLED_COLOR_BLACK);
	}
	curAniIndex++;

}

// ########################################################################################
// EXPLORER & SURVIVAL: Blink indicator
// ########################################################################################
void blinkRGBLED() {
    // Clear RGB LEDs
    if (isRGBLEDOn) {
    	rgb_setLeds(RGB_GREEN);
    	isRGBLEDOn = 0;
    } else {
    	rgb_setLeds(RGB_GREEN|curRGBLEDColor);
    	isRGBLEDOn = 1;
    }
}

// ########################################################################################
// EXPLORER & SURVIVAL: Update blink indicator
// ########################################################################################
void setRGBLEDColor(uint8_t newColor) {
    curRGBLEDColor = newColor;
	// Update RGB LEDs
    if (isRGBLEDOn) {
    	rgb_setLeds(RGB_GREEN|curRGBLEDColor);
    }
}


// ########################################################################################
// EXPLORER & SURVIVAL: Read sensor values, display on OLED and send to home
// ########################################################################################
void getSensorValues() {
	// Get light sensor value
	int l;
	l = light_read();
	// Get temperature
	int32_t t;
	t = temp_read();
	// Get accelerometer readings
	int8_t x,y,z;
    acc_read(&x, &y, &z);
    z = z-zInitial;

    // Send string to home
    char homeString[38];
    snprintf(homeString, sizeof(homeString), "L%d_T%d.%d_AX%d_AY%d_AZ%d\r\n", l, t/10, t%10, x, y, z);
	UART_Send(LPC_UART3, (uint8_t *)homeString , strlen(homeString), BLOCKING);

	if (!isOLEDOn) {
		char lum[5] = "Lum:", temp[6] = "Temp:", xAxis[8] = "X-axis:", yAxis[8] = "Y-axis:", zAxis[8] = "Z-axis:";

		oled_clearScreen(OLED_COLOR_BLACK); // Clear screen
		// Print headers if OLED is off
		uint8_t *myString = (uint8_t *) lum; // Print luminance
		oled_putString(0, 0, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		myString = (uint8_t *) temp; // Print temperature
		oled_putString(0, 10, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		myString = (uint8_t *) xAxis; // Print x-axis
		oled_putString(0, 20, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		myString = (uint8_t *) yAxis; // Print y-axis value
		oled_putString(0, 30, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		myString = (uint8_t *) zAxis; // Print z-axis value
		oled_putString(0, 40, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);

		isOLEDOn = 1;
    }

	// Print values
	char lumC[6], tempC[6], xAxisC[6], yAxisC[6], zAxisC[6];
	//Convert to char
	snprintf(lumC, sizeof lumC, "%d     ", l);
	snprintf(tempC, sizeof lumC, "%d.%d     ", t/10, t%10);
	snprintf(xAxisC, sizeof lumC, "%d     ", x);
	snprintf(yAxisC, sizeof lumC, "%d     ", y);
	snprintf(zAxisC, sizeof lumC, "%d     ", z);
	// Convert to string and print
	uint8_t *myString = (uint8_t *) lumC;
	oled_putString(45, 0, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	myString = (uint8_t *) tempC;
	oled_putString(45, 10, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	myString = (uint8_t *) xAxisC;
	oled_putString(45, 20, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	myString = (uint8_t *) yAxisC;
	oled_putString(45, 30, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	myString = (uint8_t *) zAxisC;
	oled_putString(45, 40, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}

// ########################################################################################
// EXPLORER & SURVIVAL: Update lightning count
// ########################################################################################
void updateLightningCount() {
	if (lightningCount==0) {
		led7seg_setChar(0xFF, TRUE);
	} else {
		char count = lightningCount+'0'; // Convert to char
		led7seg_setChar(count, FALSE);
	}
}

// ########################################################################################
// EXPLORER & SURVIVAL: Callback for lightning timeout
// ########################################################################################
void lightningTimeout() {
	lightningCount--;
	updateLightningCount();
}

// ########################################################################################
// EXPLORER & SURVIVAL: Enable lightning detector
// ########################################################################################
void enableLightningDetector()
{
    light_setHiThreshold(LIGHTNING_THRESHOLD);
    light_setLoThreshold(0);
    light_clearIrqStatus();
}

// ########################################################################################
// EXPLORER & SURVIVAL: Disable lightning detector
// ########################################################################################
void disableLightningDetector()
{
    light_setHiThreshold(RANGE_K2-1);
    light_setLoThreshold(0);
    light_clearIrqStatus();
}

// ########################################################################################
// SURVIVAL: Show all sensor values as S
// ########################################################################################
void blankSensorValues() {
	if (!isOLEDOn) {
		char lum[5] = "Lum:", temp[6] = "Temp:", xAxis[8] = "X-axis:", yAxis[8] = "Y-axis:", zAxis[8] = "Z-axis:";

		oled_clearScreen(OLED_COLOR_BLACK); // Clear screen
		// Print headers if OLED is off
		uint8_t *myString = (uint8_t *) lum; // Print luminance
		oled_putString(0, 0, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		myString = (uint8_t *) temp; // Print temperature
		oled_putString(0, 10, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		myString = (uint8_t *) xAxis; // Print x-axis
		oled_putString(0, 20, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		myString = (uint8_t *) yAxis; // Print y-axis value
		oled_putString(0, 30, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		myString = (uint8_t *) zAxis; // Print z-axis value
		oled_putString(0, 40, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);

		isOLEDOn = 1;
    }

	char s[6] = "S     ";
	uint8_t *myString = (uint8_t *) s;
	oled_putString(45, 0, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	oled_putString(45, 10, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	oled_putString(45, 20, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	oled_putString(45, 30, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	oled_putString(45, 40, myString, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}

// ########################################################################################
// SURVIVAL: Show LED sequence and turns off the next LED
// ########################################################################################
void showLEDSeq() {
	if (curLEDPos < 0) {
		prevMode = curMode;
		curMode = EXPLORER;
		hasModeChanged = 1;
	} else {
		pca9532_setLeds(ledOn, 0xffff);
		ledOn &= ~(1 << curLEDPos);
		curLEDPos--;
	}
//	printf("LED: %d\n", curLEDPos);
}

// ########################################################################################
// SURVIVAL: Reset LED sequence
// ########################################################################################
void resetLEDSeq() {
	showLEDSeqTask->runCount = 0;
	showLEDSeqTask->repeatCount = NUM_OF_LED+2;
	curLEDPos = 16;
	ledOn = 0xffff;
}

// ########################################################################################
// SURVIVAL: Turn off LED sequence
// ########################################################################################
void turnOffLEDSeq() {
	pca9532_setLeds(0x0000, 0xffff);
}

// ########################################################################################
// CANVAS: Read joystick and perform action
// ########################################################################################
void readJoystick() {
	uint8_t state = 0;
	state = joystick_read();
	if (state != 0)
		drawOled(state);
}

// ########################################################################################
// CANVAS: Draw on OLED based on joystick
// ########################################################################################
static void drawOled(uint8_t joyState) {
    static int wait = 0;
    uint8_t data;
    static int isDrawing = 1;

    if ((joyState & JOYSTICK_CENTER && !isJoystickDebounced) != 0) {
        isDrawing = !isDrawing;
        isJoystickDebounced = 1;
        joystickDebounceTask->runCount = 0;
        addTask(fastTaskList, &fastTaskCount, joystickDebounceTask);
        return;
    }

    if (wait++ < 3)
        return;

    wait = 0;

    if ((joyState & JOYSTICK_UP) != 0 && currY > 0) {
        currY--;

        if (isDrawing) {
            data = 124;
            UART_Send(LPC_UART3, &data, 1, BLOCKING);

            sendControlSeq(left);
        }
        sendControlSeq(up);
    }

    if ((joyState & JOYSTICK_DOWN) != 0 && currY < OLED_DISPLAY_HEIGHT-1) {
        currY++;

        if (isDrawing) {
            data = 124;
            UART_Send(LPC_UART3, &data, 1, BLOCKING);

            sendControlSeq(left);
        }
        sendControlSeq(down);
    }

    if ((joyState & JOYSTICK_RIGHT) != 0 && currX < OLED_DISPLAY_WIDTH-1) {
        currX++;

        if (isDrawing) {
            data = 45;
            UART_Send(LPC_UART3, &data, 1, BLOCKING);
        } else {
        	sendControlSeq(right);
        }

    }

    if ((joyState & JOYSTICK_LEFT) != 0 && currX > 0) {
        currX--;

        if (isDrawing) {
            data = 45;
            UART_Send(LPC_UART3, &data, 1, BLOCKING);

            sendControlSeq(left);
        }
        sendControlSeq(left);
    }

    if (lastX != currX || lastY != currY) {
        oled_putPixel(currX, currY, OLED_COLOR_WHITE);
        if (!isDrawing) {
        	oled_putPixel(lastX, lastY, OLED_COLOR_BLACK);
        }
        lastX = currX;
        lastY = currY;
    }
}

// ########################################################################################
// CANVAS: Joystick debounce callback
// ########################################################################################
void joystickDebounceTimeout() {
	isJoystickDebounced = 0;
}

// ########################################################################################
// MUSIC: Play a note
// ########################################################################################
static void playNote(uint32_t note, uint32_t durationMs) {

    uint32_t t = 0;

    if (note > 0) {

        while (t < (durationMs*1000)) {
            NOTE_PIN_HIGH();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            NOTE_PIN_LOW();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            t += note;
        }

    }
    else {
    	Timer0_Wait(durationMs);
        //delay32Ms(0, durationMs);
    }
}

// ########################################################################################
// MUSIC: Get note
// ########################################################################################
static uint32_t getNote(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'G')
        return notes[ch - 'A'];

    if (ch >= 'a' && ch <= 'g')
        return notes[ch - 'a' + 7];

    return 0;
}

// ########################################################################################
// MUSIC: Get duration of note
// ########################################################################################
static uint32_t getDuration(uint8_t ch)
{
    if (ch < '0' || ch > '9')
        return 400;

    /* number of ms */

    return (ch - '0') * 200;
}

// ########################################################################################
// MUSIC: Get duration of pause
// ########################################################################################
static uint32_t getPause(uint8_t ch)
{
    switch (ch) {
    case '+':
        return 0;
    case ',':
        return 5;
    case '.':
        return 20;
    case '_':
        return 30;
    default:
        return 5;
    }
}

// ########################################################################################
// MUSIC: Play a song based on a string of characters
// ########################################################################################
static void playSong(uint8_t *song) {
    uint32_t note = 0;
    uint32_t dur  = 0;
    uint32_t pause = 0;

    /*
     * A song is a collection of tones where each tone is
     * a note, duration and pause, e.g.
     *
     * "E2,F4,"
     */

    while(*song != '\0') {
        note = getNote(*song++);
        if (*song == '\0')
            break;
        dur  = getDuration(*song++);
        if (*song == '\0')
            break;
        pause = getPause(*song++);

        playNote(note, dur);
        //delay32Ms(0, pause);
        Timer0_Wait(pause);

    }
}

// ########################################################################################
// Interrupt: EINT3 Interrupt Handler (GPIO Interrupt Handler)
// ########################################################################################
void EINT3_IRQHandler(void)
{
//	int i;
	// Determine whether GPIO Interrupt P2.10 has occurred (SW3)
	if ((LPC_GPIOINT->IO2IntStatF>>10)& 0x1)
	{
		sw3Interrupt();

        // Clear GPIO Interrupt P2.10
        LPC_GPIOINT->IO2IntClr = 1<<10;
	}
	// Determine whether GPIO Interrupt P2.5 has occurred (Light sensor)
	if ((LPC_GPIOINT->IO2IntStatF>>5)& 0x1)
	{
        lightningInterruptHandler();

        // Clear GPIO Interrupt P2.5
        LPC_GPIOINT->IO2IntClr = 1<<5;
	}
}

// ########################################################################################
// Interrupt (Common): when trigger button (SW3) is pressed
// ########################################################################################
void sw3Interrupt() {
	Task *triggerSensorTask = newTask(&getSensorValues, 0, 1, TICK_MILLIS);
	triggerSensorTask->runCount = 0;
	addTask(slowTaskList, &slowTaskCount, triggerSensorTask);
}

// ########################################################################################
// Interrupt (EXPLORER & SURVIVAL): Interrupt when light goes above or below LIGHTNING_THRESHOLD
// ########################################################################################
static void lightningInterruptHandler()
{
	static int lightningStatus = 0;
	static int lightningStartTicks = 0;

	if (lightningStatus==0)
	{
		lightningStartTicks = msTicks;
		light_setHiThreshold(RANGE_K2-1); // Disable high threshold
		light_setLoThreshold(LIGHTNING_THRESHOLD);

		if (curMode == SURVIVAL) {
			resetLEDSeqTask->repeatCount = -1; // Start resetting LED
			addTask(fastTaskList, &fastTaskCount, resetLEDSeqTask);
		}
	} else {
		if (msTicks-lightningStartTicks<LIGHTNING_THRESHOLD_TIME) {
			lightningCount++;
			if (curMode == EXPLORER && lightningCount >= 3) {
				prevMode = curMode;
				curMode = SURVIVAL;
				hasModeChanged = 1;
			}
			Task *lightningTimeoutTask;
			lightningTimeoutTask = newTask(&lightningTimeout, LIGHTNING_TIME_WINDOW-(msTicks-lightningStartTicks), 1, TICK_MILLIS);
			addTask(fastTaskList, &fastTaskCount, lightningTimeoutTask);
			updateLightningCount();
		}
		if (curMode == SURVIVAL) {
			resetLEDSeqTask->repeatCount = 0; // Stop resetting LED
		}
		light_setHiThreshold(LIGHTNING_THRESHOLD);
		light_setLoThreshold(0); // Disable low threshold
	}
	lightningStatus = !lightningStatus;
    light_clearIrqStatus();
}

// ########################################################################################
// Interrupt: Timer0 interrupt handler - used for running fastTaskList
// ########################################################################################
void TIMER0_IRQHandler(void) {
	TIM_Cmd(LPC_TIM0, DISABLE);
	TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT);
	TIM_ResetCounter(LPC_TIM0);
	TIM_Cmd(LPC_TIM0, ENABLE);

	// Run tasks from fast list
	removeFinishedTasks(fastTaskList, &fastTaskCount);
	checkAndRunTasks(fastTaskList, &fastTaskCount);
}

// ########################################################################################
// Interrupt: UART3 interrupt handler - calls standard UART interrupt handler
// ########################################################################################
void UART3_IRQHandler(void) {
	// Call Standard UART 3 interrupt handler
	UART3_StdIntHandler();
}

// ########################################################################################
// Interrupt: UART3 receive interrupt handler - triggered when data received
// ########################################################################################
void UART3ReceiveInterruptHandler() {
	uint8_t input = 0;
	uint8_t data = 0;
	uint32_t len = 0;
	uint8_t song[MAX_SONG_LENGTH];

	// Receives a character
	UART_Receive(LPC_UART3, &input, 1, NONE_BLOCKING);

	if (!isUARTDebounced) {
		isUARTDebounced = 1;
		UARTDebounceTask->runCount = 0;
		addTask(fastTaskList, &fastTaskCount, UARTDebounceTask);

		switch (curMenuPos) {
		//Main menu
			case 0:
				// Clear
				sendControlSeq(clear);
				//Home
				sendControlSeq(home);

				switch (input) {
					case '1':
						prevMode = curMode;
						curMode = STARTER;
						hasModeChanged = 1;
						UART_Send(LPC_UART3, (uint8_t *)menu[curMenuPos] , strlen(menu[curMenuPos]), BLOCKING);
						break;
					case '2':
						prevMode = curMode;
						curMode = EXPLORER;
						hasModeChanged = 1;
						UART_Send(LPC_UART3, (uint8_t *)menu[curMenuPos] , strlen(menu[curMenuPos]), BLOCKING);
						break;
					case '3':
						prevMode = curMode;
						curMode = SURVIVAL;
						hasModeChanged = 1;
						UART_Send(LPC_UART3, (uint8_t *)menu[curMenuPos] , strlen(menu[curMenuPos]), BLOCKING);
						break;
					case '4':
						curMenuPos = 1;
						prevMode = curMode;
						curMode = CANVAS;
						hasModeChanged = 1;

						// Information
						UART_Send(LPC_UART3, (uint8_t *)menu[curMenuPos] , strlen(menu[curMenuPos]), BLOCKING);

						// Center
						sendControlSeq(center);

						// Reset OLED
						currX = 48;
						currY = 32;
						lastX = 0;
						lastY = 0;
						break;
					case '5':
						curMenuPos = 2;
						prevMode = curMode;
						curMode = MUSIC;
						hasModeChanged = 1;
						UART_Send(LPC_UART3, (uint8_t *)menu[curMenuPos] , strlen(menu[curMenuPos]), BLOCKING);
						break;
					default:
						UART_Send(LPC_UART3, (uint8_t *)menu[curMenuPos] , strlen(menu[curMenuPos]), BLOCKING);
						break;
				}
				break;
			// Canvas
			case 1:
				switch (input) {
					case 'w':
						// Draw on teraterm
						data = '|';
						UART_Send(LPC_UART3, &data, 1, BLOCKING);

						sendControlSeq(left);
						sendControlSeq(up);

						// Draw on OLED
						currY--;
						oled_putPixel(currX, currY, OLED_COLOR_WHITE);
						lastY = currY;
						break;
					case 'a':
						// Draw on teraterm
						data = '-';
						UART_Send(LPC_UART3, &data, 1, BLOCKING);

						sendControlSeq(left);
						sendControlSeq(left);

						// Draw on OLED
						currX--;
						oled_putPixel(currX, currY, OLED_COLOR_WHITE);
						lastX = currX;
						break;
					case 's':
						// Draw on teraterm
						data = '|';
						UART_Send(LPC_UART3, &data, 1, BLOCKING);

						sendControlSeq(left);
						sendControlSeq(down);

						// Draw on OLED
						currY++;
						oled_putPixel(currX, currY, OLED_COLOR_WHITE);
						lastY = currY;
						break;
					case 'd':
						// Draw on teraterm
						data = '-';
						UART_Send(LPC_UART3, &data, 1, BLOCKING);

						// Draw on OLED
						currX++;
						oled_putPixel(currX, currY, OLED_COLOR_WHITE);
						lastX = currX;
						break;
					case 'i':
						currY--;
						lastY = currY;
						sendControlSeq(up);
						break;
					case 'j':
						currX--;
						lastX = currX;
						sendControlSeq(left);
						break;
					case 'k':
						currY++;
						lastY = currY;
						sendControlSeq(down);
						break;
					case 'l':
						currX++;
						lastX = currX;
						sendControlSeq(right);
						break;
					case 'q':
						// Clear
						sendControlSeq(clear);
						//Home
						sendControlSeq(home);

						curMenuPos = 0;
						UART_Send(LPC_UART3, (uint8_t *)menu[curMenuPos] , strlen(menu[curMenuPos]), BLOCKING);
						// Stop canvas mode
						stopCanvas();
						curMode = -1;
						break;
					default:
						break;
				}
				isUARTDebounced = 0; // Don't debounce
				break;
			// Music
			case 2:
				if (input == 'q') {
					// Clear
					sendControlSeq(clear);
					//Home
					sendControlSeq(home);

					curMenuPos = 0;
					UART_Send(LPC_UART3, (uint8_t *)menu[curMenuPos] , strlen(menu[curMenuPos]), BLOCKING);

					// Quit music mode
					stopMusic();
					curMode = -1;
				} else {
					// Receive string to play
					len = 1;
					song[0] = input;
					UART_Send(LPC_UART3, &input, 1, BLOCKING);
					do {
						UART_Receive(LPC_UART3, &data, 1, BLOCKING);
						UART_Send(LPC_UART3, &data, 1, BLOCKING);
						if (data != '\r') {
							len++;
							song[len-1] = data;
						}
					} while ((len<MAX_SONG_LENGTH) && (data != '\r'));
					song[len]=',';
					song[len+1]=0;
					// Break line
					data = 10;
					UART_Send(LPC_UART3, &data, 1, BLOCKING);
					playSong(song);
				}
				break;
			default:
				break;
		}
	}
}

// ########################################################################################
// systick_delay - creates a delay of the appropriate number of Systicks (happens every 1 ms)
// ########################################################################################
__INLINE void systick_delay (uint32_t delayTicks) {
  uint32_t curTicks;

  curTicks = msTicks;	// read current tick counter
  // Now loop until required number of ticks passes
  while ((msTicks - curTicks) < delayTicks);
}

// ########################################################################################
// Return msTicks
// ########################################################################################
uint32_t getTicks(void) {
	return msTicks;
}

// ########################################################################################
//  SysTick_Handler - just increment SysTick counter
// ########################################################################################
void SysTick_Handler(void) {
	msTicks++;
}

// ########################################################################################
// STARTER: Stop starter mode
// ########################################################################################
void stopStarter() {
	// Stop showing starting sequence
	showStartingSeqTask->repeatCount = 0;
	// Stop showing starting animation
	showStartingAniTask->repeatCount = 0;
	// Blank 7 segment
	blank7Seg();
	// Blank Oled
	blankOLED();
}

// ########################################################################################
// EXPLORER: Stop explorer mode
// ########################################################################################
void stopExplorer() {
	// Remove get sensor task
	getSensorValuesTask->repeatCount = 0;
}

// ########################################################################################
// SURVIVAL: Stop survival mode
// ########################################################################################
void stopSurvival() {
	// Remove showing LED Task
	showLEDSeqTask->repeatCount = 0;
	// Turn off led sequence
	turnOffLEDSeq();
}

// ########################################################################################
// CANVAS: Stop canvas mode
// ########################################################################################
void stopCanvas() {
	// Blank Oled
	blankOLED();
	// Stop reading joystick
	readJoystickTask->repeatCount = 0;
}

// ########################################################################################
// MUSIC: Stop music mode
// ########################################################################################
void stopMusic() {
    /* ---- Speaker ------> */
    GPIO_SetDir(0, 1<<27, 0);
    GPIO_SetDir(0, 1<<28, 0);
    GPIO_SetDir(2, 1<<13, 0);
    /* <---- Speaker ------ */
}

// ########################################################################################
// STARTER: Start starter mode
// ########################################################################################
void startStarter() {
	// Remove blinking led
	if (isBlinking) {
		blinkRGBTask->repeatCount = 0;
		isBlinking = 0;
		blankRGBLED();
	}
	// Blank 7 segment
	blank7Seg();
	// Blank Oled
	blankOLED();
    // Add starting sequence task
	curSeqIndex = 0;
    showStartingSeqTask->repeatCount = seqLength+1;
    showStartingSeqTask->runCount = 0;
	runTaskOnce(showStartingSeqTask);
	addTask(fastTaskList, &fastTaskCount, showStartingSeqTask);
}

// ########################################################################################
// EXPLORER: Start explorer mode
// ########################################################################################
void startExplorer() {
	// If not blinking
	if (!isBlinking) {
		// Initialize GPIO interrupts
		initGPIOInterrupt();
		// Enable lightning detector
		enableLightningDetector();
		// Set RGB led to blink blue
		blinkRGBTask->repeatCount = -1;
		runTaskOnce(blinkRGBTask);
		addTask(fastTaskList, &fastTaskCount, blinkRGBTask);
		isBlinking = 1; // Run only once
	}
	// Change RGB color to blue
	setRGBLEDColor(RGB_BLUE);
	// Get sensor values every SAMPLING_TIME
	getSensorValuesTask->repeatCount = -1;
	runTaskOnce(getSensorValuesTask);
	addTask(slowTaskList, &slowTaskCount, getSensorValuesTask);
}

// ########################################################################################
// SURVIVAL: Start survival mode
// ########################################################################################
void startSurvival() {
	// If not blinking
	if (!isBlinking) {
		// Initialize GPIO interrupts
		initGPIOInterrupt();
		// Enable lightning detector
		enableLightningDetector();
		// Set RGB led to blink blue
		blinkRGBTask->repeatCount = -1;
		runTaskOnce(blinkRGBTask);
		addTask(fastTaskList, &fastTaskCount, blinkRGBTask);
		isBlinking = 1; // Run only once
	}
	// Blank sensor values
	blankSensorValues();
	// Change RGB color to red
	setRGBLEDColor(RGB_RED);
	// Reset and show sequence
	resetLEDSeq();
	showLEDSeqTask->repeatCount = NUM_OF_LED+2;
	showLEDSeqTask->runCount = 0;
	runTaskOnce(showLEDSeqTask);
	addTask(fastTaskList, &fastTaskCount, showLEDSeqTask);
}

// ########################################################################################
// CANVAS: Start canvas mode
// ########################################################################################
void startCanvas() {
	// Remove blinking led
	if (isBlinking) {
		blinkRGBTask->repeatCount = 0;
		isBlinking = 0;
		blankRGBLED();
	}
	// Blank 7 segment
	blank7Seg();
	// Blank Oled
	blankOLED();

	// Start reading Joystick
	readJoystickTask->repeatCount = -1;
	addTask(slowTaskList, &slowTaskCount, readJoystickTask);
}

// ########################################################################################
// MUSIC: Start music mode
// ########################################################################################
void startMusic() {
	// Remove blinking led
	if (isBlinking) {
		blinkRGBTask->repeatCount = 0;
		isBlinking = 0;
		blankRGBLED();
	}
	// Blank 7 segment
	blank7Seg();
	// Blank Oled
	blankOLED();

    /* ---- Speaker ------> */
    GPIO_SetDir(2, 1<<0, 1);
    GPIO_SetDir(2, 1<<1, 1);

    GPIO_SetDir(0, 1<<27, 1);
    GPIO_SetDir(0, 1<<28, 1);
    GPIO_SetDir(2, 1<<13, 1);
    GPIO_SetDir(0, 1<<26, 1);

    GPIO_ClearValue(0, 1<<27); //LM4811-clk
    GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
    GPIO_ClearValue(2, 1<<13); //LM4811-shutdn
    /* <---- Speaker ------ */
}

// ########################################################################################
// Main function
// ########################################################################################
int main (void) {

    // Initialization functions
    init_i2c();
    init_ssp();
    init_GPIO();
	init_uart();

    pca9532_init();
    joystick_init();
    acc_init();
    oled_init();
    led7seg_init();
    rgb_init();
    temp_init(&getTicks);
    light_enable();

	// Setup SysTick Timer to interrupt at 1 msec intervals
	SysTick_Config(SystemCoreClock / 1000);

	// Initialize timer interrupts
	initTimerInterrupt();
	// Initialize UART interrupts
	initUARTInterrupt();
	// Get initial z-value
	int8_t xDiscard, yDiscard;
    acc_read(&xDiscard, &yDiscard, &zInitial);
    // Set light sensor range
    light_setRange(LIGHT_RANGE_4000);
    // Disable lightning detector at start
    disableLightningDetector();
    // Show starting menu
    sendControlSeq(clear);
    sendControlSeq(home);
    UART_Send(LPC_UART3, (uint8_t *)menu[0] , strlen(menu[0]), BLOCKING);
    // Initialize stripes array
    int count;
	srand(time(NULL));
    for (count=0;count<NUM_OF_STRIPES;count++) {
    	stripesX[count] = rand() % 96;
    	stripesY[count] = rand() % 64;
    }

    // Initialize tasks
    showStartingSeqTask = newTask(&showStartingSeq, 1000, seqLength+1, TICK_MILLIS);
    showStartingAniTask = newTask(&showStartingAni, 50, 120, TICK_MILLIS);
    blinkRGBTask = newTask(&blinkRGBLED, 1000, -1, TICK_MILLIS);
    getSensorValuesTask = newTask(&getSensorValues, SAMPLING_TIME, -1, TICK_MILLIS);
    resetLEDSeqTask = newTask(&resetLEDSeq, TICK_MILLIS, -1, TICK_MILLIS);
    showLEDSeqTask = newTask(&showLEDSeq, TIME_UNIT, NUM_OF_LED+2, TICK_MILLIS);
    UARTDebounceTask = newTask(&UARTDebounceTimeout, DEBOUNCE_TIME, 1, TICK_MILLIS);
    readJoystickTask = newTask(&readJoystick, 1, -1, TICK_MILLIS);
    joystickDebounceTask = newTask(&joystickDebounceTimeout, DEBOUNCE_TIME, 1, TICK_MILLIS);

    while (1) {
    	// Respond to mode changes
    	if (hasModeChanged) {
    		if (prevMode != curMode) {
    			switch (prevMode) {
					case STARTER:
						stopStarter();
						break;
					case EXPLORER:
						stopExplorer();
						break;
					case SURVIVAL:
						stopSurvival();
						break;
					case CANVAS:
						stopCanvas();
						break;
					case MUSIC:
						stopMusic();
						break;
					default:
						break;
				}
				switch (curMode) {
					case STARTER:
						startStarter();
						break;
					case EXPLORER:
						startExplorer();
						break;
					case SURVIVAL:
						startSurvival();
						break;
					case CANVAS:
						startCanvas();
						break;
					case MUSIC:
						startMusic();
						break;
					default:
						break;
				}
    		}
        	hasModeChanged = 0;
    	}

    	// Run tasks from slow list
    	if(msTicks-curTicks >= TICK_MILLIS) {
    		curTicks = msTicks;
    		removeFinishedTasks(slowTaskList, &slowTaskCount);
    		checkAndRunTasks(slowTaskList, &slowTaskCount);
    	}

    }
}
