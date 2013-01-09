/************************************************************************/
/*																		*/
/*	ac97_demo.c	--	AC'97 Demo Main Program								*/
/*																		*/
/************************************************************************/
/*	Author: Sam Bobrowicz												*/
/*	Copyright 2011, Digilent Inc.										*/
/************************************************************************/
/*  Module Description: 												*/
/*																		*/
/*	This program demonstrates the audio processing capabilities of the  */
/*  Atlys Development Board. The onboard push buttons provide the 		*/
/*  following functionality to the user:								*/
/*																		*/
/*  BTNR  Record a short period of audio and store it in memory. SW0	*/
/*		   selects either LINE IN (up) or MIC (down) as the input 		*/
/*		   source. LD0 will be illuminated while recording.				*/
/*	BTNL  Play back the recorded audio data on both LINE OUT and		*/
/*		   HP OUT. LD1 will be illuminated while playing data back.		*/
/*	BTND  Output a brief tone on both LINE OUT and HP OUT				*/
/*																		*/
/************************************************************************/
/*  Revision History:													*/
/* 																		*/
/*		11/4/2011(SamB): Created										*/
/*																		*/
/************************************************************************/

/* ------------------------------------------------------------ */
/*				Include File Definitions						*/
/* ------------------------------------------------------------ */
#include <stdio.h>
#include "platform.h"  //Contains Cache initializing function
#include "xparameters.h"  //The hardware configuration describing constants
#include "xgpio.h"	//GPIO API functions
#include "xintc.h"	//Interrupt Controller API functions
#include "xil_io.h"	//Contains the Xil_Out32 and Xil_In32 functions
#include "mb_interface.h" //Contains the functions for registering the
//interrupt controller with the microblaze MP
#include "ac97_demo.h"
#include "ac97.h"

#include <math.h>
// #include <complex.h>

/* ------------------------------------------------------------ */
/*				XPAR Constants									*/
/* ------------------------------------------------------------ */
/* All constants used from xparameters.h are located here so 	*/
/* that they may be easily accessed.							*/
/**/

#define AC97_BASEADDR XPAR_DIGILENT_AC97_CNTLR_BASEADDR
#define BTNS_BASEADDR XPAR_PUSH_BUTTONS_5BITS_BASEADDR
#define SW_BASEADDR XPAR_DIP_SWITCHES_8BITS_BASEADDR
#define LED_BASEADDR XPAR_LEDS_8BITS_BASEADDR
#define BTNS_DEVICE_ID XPAR_PUSH_BUTTONS_5BITS_DEVICE_ID
#define INTC_DEVICE_ID XPAR_INTC_0_DEVICE_ID
#define BTNS_IRPT_ID XPAR_INTC_SINGLE_DEVICE_ID
#define DDR2_BASEADDR XPAR_MCB_DDR2_S0_AXI_BASEADDR

#define MIN_UINT 0
#define MAX_UINT 0xffffffff
// bits 4 to 19 of Sample
#define AC97_DATA_MASK 0x000ffff0
#define AC97_META_MASK 0x0000000f

#define masterVolumeREG (AC97_BASEADDR + AC97_MASTER_VOLUME_OFFSET)

/* ------------------------------------------------------------ */
/*				Global Variables								*/
/* ------------------------------------------------------------ */

/* our definitions */
// typedef float complex cplx;


void _fftSample(Xuint32 buf[], Xuint32 out[], int n, int step, int inverse);
void fftSample(Xuint32 bufaddress, int n, int inverse);
volatile Xuint32 fftAddress;

volatile float sampleMax;
volatile float sampleMin;
volatile float middle;
volatile int countSamples;
volatile Xuint32 Count_Samples;
Xuint32 pAudioData;
Xuint32 pFFTData;
/* end ours */

volatile u32 lBtnStateOld;
volatile u8 fsRunAction; //flags are set by interrupts to trigger
//different actions

/* ------------------------------------------------------------ */
/*				Procedure Definitions							*/
/* ------------------------------------------------------------ */

int main() {
	init_platform();

	static XGpio pshBtns;
	static XIntc intCtrl;

	Xuint32 Sample_L;
	Xuint32 Sample_R;

	pAudioData = (DDR2_BASEADDR + 0x02000000);
    pFFTData  = (pAudioData + lNumSamples);

	lBtnStateOld = 0x00000000;
	fsRunAction = 0;

	/*
	 *Initialize the driver structs for the Push button and interrupt cores.
	 *This allows the API functions to be used with these cores.
	 */
	XGpio_Initialize(&pshBtns, BTNS_DEVICE_ID);
	XIntc_Initialize(&intCtrl, INTC_DEVICE_ID);

	/*
	 * Connect the function PushBtnHandler to the interrupt controller so that
	 * it is called whenever the Push button GPIO core signals an interrupt.
	 */
	XIntc_Connect(&intCtrl, BTNS_IRPT_ID, PushBtnHandler, &pshBtns);

	/*
	 * Enable interrupts at the interrupt controller
	 */
	XIntc_Enable(&intCtrl, BTNS_IRPT_ID);

	/*
	 * Register the interrupt controller with the microblaze
	 * processor and then start the Interrupt controller so that it begins
	 * listening to the interrupt core for triggers.
	 */
	microblaze_register_handler(XIntc_DeviceInterruptHandler, INTC_DEVICE_ID);
	microblaze_enable_interrupts();
	XIntc_Start(&intCtrl, XIN_REAL_MODE);

	/*
	 * Enable the push button GPIO core to begin sending interrupts to the
	 * interrupt controller in response to changes in the button states
	 */
	XGpio_InterruptEnable(&pshBtns, lBtnChannel);
	XGpio_InterruptGlobalEnable(&pshBtns);

	/*
	 * Wait for AC97 to become ready
	 */
	while (!(AC97_Link_Is_Ready(AC97_BASEADDR)))
		;

	/*
	 * Set TAG to configure codec
	 */
	AC97_Set_Tag_And_Id(AC97_BASEADDR, 0xF800);

	/*
	 * Enable audio output and set volume
	 */
	AC97_Unmute(AC97_BASEADDR, AC97_MASTER_VOLUME_OFFSET);
	AC97_Unmute(AC97_BASEADDR, AC97_HEADPHONE_VOLUME_OFFSET);
	AC97_Unmute(AC97_BASEADDR, AC97_PCM_OUT_VOLUME_OFFSET);

	AC97_Set_Volume(AC97_BASEADDR, AC97_MASTER_VOLUME_OFFSET, BOTH_CHANNELS,
			VOLUME_MAX);
	AC97_Set_Volume(AC97_BASEADDR, AC97_HEADPHONE_VOLUME_OFFSET, BOTH_CHANNELS,
			VOLUME_MIN);
	AC97_Set_Volume(AC97_BASEADDR, AC97_PCM_OUT_VOLUME_OFFSET, BOTH_CHANNELS,
			VOLUME_MAX);

	while (1) {
		/**************************
		 * Play recorded sample
		 **************************/
		if (sampleMax != 0) {
			if (fsRunAction & bitPlay) {
				/*
				 * Set AC'97 codec TAG to send and receive data in the PCM slots
				 */
				AC97_Set_Tag_And_Id(AC97_BASEADDR, 0x9800);

				Count_Samples = 0;

				// Xil_Out32(LED_BASEADDR, bitPlayLED); //turn on LED
				int showLED = 0;
				while ((Count_Samples / 8) < lNumSamples) {
					/*
					 * Block execution until next frame is ready
					 */
					AC97_Wait_For_New_Frame(AC97_BASEADDR);

					/*
					 * Read audio data from memory
					 */
					Sample_L = XIo_In32 (pAudioData + Count_Samples);
					Count_Samples = Count_Samples + 4;
					Sample_R = XIo_In32 (pAudioData + Count_Samples);
					Count_Samples = Count_Samples + 4;

					/* our code */
					/* if(Count_Samples < 100) {  */
					//	printf("Left: %i",(int)Sample_L);
					//	printf("Right: %i",(int)Sample_R);
					/* } */
					if (showLED % 4000 == 0) {
						/*						middleLeft /= 8000;
						 middleRight /= 8000; */
						// Xil_Out32(LED_BASEADDR, Sample_L & AC97_META_MASK);
						// printf("Left: %d",(int)Sample_L);
						// printf("Right: %d",(int)Sample_R);
						setVolumeLEDs(Sample_L & AC97_DATA_MASK,
								Sample_R & AC97_DATA_MASK);
						/*						middleLeft = 0;
						 middleRight = 0;
						 } else {
						 middleLeft += Sample_L;
						 middleRight += Sample_R; */
					}

					/*
					 * Send audio data to codec
					 */XIo_Out32 ((AC97_BASEADDR + AC97_PCM_OUT_L_OFFSET), Sample_L);
					XIo_Out32 ((AC97_BASEADDR + AC97_PCM_OUT_R_OFFSET), Sample_R);
					showLED++;
				}

				Xil_Out32(LED_BASEADDR, 0); //Turn off LED
				fsRunAction = 0; //Forget any button presses that occurred
			}
		}
		/**************************
		 * Output a square wave
		 **************************/
		if (fsRunAction & bitGenWave) {
			//generate square on left, right and then both channels
			GenSquare(AC97_BASEADDR, LEFT_CHANNEL, 1000, 500);
			GenSquare(AC97_BASEADDR, RIGHT_CHANNEL, 1000, 500);
			GenSquare(AC97_BASEADDR, BOTH_CHANNELS, 1000, 500);
			fsRunAction = 0;
		}
		/**************************
		 * LEDTest
		 **************************/
		if (fsRunAction & bitLEDTest) {
			/*	Xil_Out32(LED_BASEADDR, volumeLED0);
			 sleepTimer(1000);
			 Xil_Out32(LED_BASEADDR, volumeLED1);
			 sleepTimer(1000);
			 Xil_Out32(LED_BASEADDR, volumeLED2);
			 sleepTimer(1000);
			 Xil_Out32(LED_BASEADDR, volumeLED3);
			 sleepTimer(1000);
			 Xil_Out32(LED_BASEADDR, volumeLED4);
			 sleepTimer(5000);
			 Xil_Out32(LED_BASEADDR, volumeLED5);
			 sleepTimer(5000);
			 Xil_Out32(LED_BASEADDR, volumeLED6);
			 sleepTimer(5000);
			 Xil_Out32(LED_BASEADDR, volumeLED7);
			 sleepTimer(5000);
			 Xil_Out32(LED_BASEADDR, 0);
			 Xil_Out32(LED_BASEADDR, allLEDs);
			 sleepTimer(5000);
			 Xil_Out32(LED_BASEADDR, 0);
			 fsRunAction = 0;*/
			Xil_Out32(LED_BASEADDR, volumeLED7);
		  	fftSample(pAudioData, lNumSamples, 0);
			Xil_Out32(LED_BASEADDR, 0);
//			fftSample(pFFTData, pAudioData, lNumSamples, 1);
//			addOriginMeta();
//			if (compareValues(pAudioData, pFFTData)) {
//				Xil_Out32(LED_BASEADDR, allLEDs);
//				sleepTimer(5000);
//				Xil_Out32(LED_BASEADDR, 0);
//				Xil_Out32(LED_BASEADDR, allLEDs);
//				sleepTimer(5000);
//				Xil_Out32(LED_BASEADDR, 0);
//			} else {
//				Xil_Out32(LED_BASEADDR, volumeLED0);
//				sleepTimer(1000);
//				Xil_Out32(LED_BASEADDR, volumeLED1);
//				sleepTimer(1000);
//				Xil_Out32(LED_BASEADDR, volumeLED2);
//				sleepTimer(1000);
//				Xil_Out32(LED_BASEADDR, volumeLED3);
//				sleepTimer(1000);
//				Xil_Out32(LED_BASEADDR, volumeLED4);
//			}
			fsRunAction = 0;

		}
		/**************************
		 * Record audio from input
		 **************************/
		if (fsRunAction & bitRec) {
			AC97_Set_Tag_And_Id(AC97_BASEADDR, 0xF800); //Set to configure

			/*
			 * Select input source, enable it, and then set the volume
			 */
			if (Xil_In32(SW_BASEADDR) & bitSw0) {
				AC97_Select_Input(AC97_BASEADDR, BOTH_CHANNELS,
						AC97_LINE_IN_SELECT);
				AC97_Unmute(AC97_BASEADDR, AC97_LINE_IN_VOLUME_OFFSET);
				AC97_Set_Volume(AC97_BASEADDR, AC97_LINE_IN_VOLUME_OFFSET,
						BOTH_CHANNELS, VOLUME_MAX);
			} else {
				AC97_Select_Input(AC97_BASEADDR, BOTH_CHANNELS, AC97_MIC_SELECT);
				AC97_Unmute(AC97_BASEADDR, AC97_MIC_VOLUME_OFFSET);
				AC97_Set_Volume(AC97_BASEADDR, AC97_MIC_VOLUME_OFFSET,
						BOTH_CHANNELS, VOLUME_MID);
			}
			//set record gain
			AC97_Set_Volume(AC97_BASEADDR, AC97_RECORD_GAIN_OFFSET,
					BOTH_CHANNELS, 0x00);

			AC97_Set_Tag_And_Id(AC97_BASEADDR, 0x9800); //Set to Send/Receive data

			Count_Samples = 0;

			Xil_Out32(LED_BASEADDR, bitRecLED); //Turn on LED

			middle = 0;
			sampleMax = MIN_UINT;
			countSamples = 0;
			sampleMin = MAX_UINT;
			while ((Count_Samples / 8) < lNumSamples) {
				AC97_Wait_For_New_Frame(AC97_BASEADDR);

				/*
				 * Read audio data from codec
				 */
				Sample_L = XIo_In32(AC97_BASEADDR + AC97_PCM_IN_L_OFFSET);
				Sample_R = XIo_In32(AC97_BASEADDR + AC97_PCM_IN_R_OFFSET);

				/*
				 * our code
				 */
				captureSampleReference(Sample_L & AC97_DATA_MASK,
						Sample_R & AC97_DATA_MASK);

				/*
				 * Write audio data to memory
				 */

				XIo_Out32 (pAudioData + Count_Samples, Sample_L);
				Count_Samples = Count_Samples + 4;
				XIo_Out32 (pAudioData + Count_Samples, Sample_R);
				Count_Samples = Count_Samples + 4;
			}

			//Set Tag and ID to configure the codec
			AC97_Set_Tag_And_Id(AC97_BASEADDR, 0xF800);

			/*
			 * Disable the input source
			 */
			if (Xil_In32(SW_BASEADDR) & bitSw0) {
				AC97_Mute(AC97_BASEADDR, AC97_LINE_IN_VOLUME_OFFSET);
			} else {
				AC97_Mute(AC97_BASEADDR, AC97_MIC_VOLUME_OFFSET);
			}
			middle = middle / countSamples;
			Xil_Out32(LED_BASEADDR, 0); //Turn off LED
			fsRunAction = 0;
		}

	}

	cleanup_platform();

	return 0;
}

/***	PushBtnHandler
 **
 **	Parameters:
 **		CallBackRef - Pointer to the push button struct (pshBtns) initialized
 **		in main.
 **
 **	Return Value:
 **		None
 **
 **	Errors:
 **		None
 **
 **	Description:
 **		Respond to various button pushes by triggering actions to occur outside
 **		the ISR. Actions are triggered by setting flags within the fsRunAction
 **		flag set variable.
 */
void sleepTimer(int time) {
	int circles = time * 5000;
	while (circles >= 0) {
		circles--;
	}
}

void PushBtnHandler(void *CallBackRef) {
	XGpio *pPushBtn = (XGpio *) CallBackRef;
	u32 lBtnStateNew = XGpio_DiscreteRead(pPushBtn, lBtnChannel);
	u32 lBtnChanges = lBtnStateNew ^ lBtnStateOld;

	if (fsRunAction == 0) {
		if ((lBtnChanges & bitBtnD) && (lBtnStateNew & bitBtnD)) {
			fsRunAction = fsRunAction | bitGenWave;
		}
		if ((lBtnChanges & bitBtnL) && (lBtnStateNew & bitBtnL)) {
			fsRunAction = fsRunAction | bitPlay;
		}
		if ((lBtnChanges & bitBtnR) && (lBtnStateNew & bitBtnR)) {
			fsRunAction = fsRunAction | bitRec;
		}
		if ((lBtnChanges & bitBtnC) && (lBtnStateNew & bitBtnC)) {
			fsRunAction = fsRunAction | bitLEDTest;
		}
	}

	lBtnStateOld = lBtnStateNew;

	XGpio_InterruptClear(pPushBtn, lBtnChannel);
}
void setVolumeLEDs(Xuint32 left, Xuint32 right) {
	Xuint32 median = (left + right) / 2;
	/* float step = sampleMax / 8; */
	/* float whichStep = (median / step); */
	int onLEDs = 0;
	Xuint32 reference = sampleMax;
	/*if(median >= reference) onLEDs = 8;
	 if(median < reference && median > reference-reference/8) onLEDs = 7;
	 if(median < reference-reference/8 && median > reference-2*reference/8) onLEDs = 6;
	 if(median < reference-2*reference/8 && median > reference-3*reference/8) onLEDs = 5;
	 if(median < reference-3*reference/8 && median > reference-4*reference/8) onLEDs = 4;
	 if(median < reference-4*reference/8 && median > reference-5*reference/8) onLEDs = 3;
	 if(median < reference-5*reference/8 && median > reference-6*reference/8) onLEDs = 2;
	 if(median < reference-6*reference/8 && median > reference-7*reference/8) onLEDs = 1;
	 if(median < reference-7*reference/8 && median >= 0) onLEDs = 0;
	 */

	float step = reference / 8;
	/* onLEDs = (int)((median - (middle))/ step); */
	onLEDs = (int) (median / step);
	if (onLEDs < 0)
		onLEDs = 0;
	int mask = 0x00000000;
	while (onLEDs >= 0) {
		/*	Xil_Out32(LED_BASEADDR, firstLED+(1<<onLEDs)); */
		mask = mask | (firstLED << onLEDs);
		onLEDs--;
	}
	mask = ~mask;
	Xil_Out32(LED_BASEADDR, mask);
}

/* double calculateDBOverload(Xuint32 sample)
 {
 double dBOverload;
 dBOverload = 20*log10(sample/sampleMax);
 return dBOverload;
 }
 */

void captureSampleReference(Xuint32 left, Xuint32 right) {
	Xuint32 median = (left + right) / 2;
	middle += median;
	if (sampleMax < median) {
		sampleMax = median;
	}
	if (sampleMin > median) {
		sampleMin = median;
	}

	countSamples++;
}

void fftSample(Xuint32 bufaddress, int n, int inverse) {

	//int test = 12345;
	//Xil_Out32(0x0000ffff,test);

	Xuint32 buf[n];
	Xuint32 out[n];

	int i = 0;
	int count = 0;
	while (i < n) {

		XIo_Out32(buf+count*4,(XIo_In32 (bufaddress + i) & AC97_DATA_MASK)<<11); /*get sample*/
		//XIo_Out32(out+count*4,(XIo_In32 (bufaddress + i) & AC97_DATA_MASK)<<11); /*get sample*/
		i = i + 8;
		count++;
	}

	// _fftSample(buf, out, n, 1,inverse);
     fftAddress = (Xuint32)buf;
}

void _fftSample(Xuint32 buf[], Xuint32 out[], int n, int step, int inverse) {
	// to seperate real and imag
	Xuint32 realMask = 0xffff0000;
	Xuint32 imagMask = 0x0000ffff;
	// for inverse operation
	float iFactor = 1;
	float nFactor = 1;

	if(inverse == 1){
		nFactor = 1/n;
		iFactor = -1;
	}

	if (step < n) {
		_fftSample(out, buf, n, step * 2,inverse);
		_fftSample(out + step, buf + step, n, step * 2,inverse);

		int i;
		Xil_Out32(LED_BASEADDR, volumeLED3);
		for (i = 0; i < n; i += 2 * step) {
			Xil_Out32(LED_BASEADDR, volumeLED0);

			// Xuint32 t = cexp(-I * PI * i / n) * out[i + step];
			Xuint16 realOut =  (Xuint16)((XIo_In32(out + (i + step)*4) & realMask)>>16);
			Xuint16 imagOut =  (Xuint16)(XIo_In32(out + (i + step)*4) & imagMask);

			Xuint16 tReal = (Xuint16)(cos( M_PI * i / n)*realOut + (iFactor) * sin( M_PI * i / n)*imagOut);
			Xuint16 tImag = (Xuint16)(iFactor*(-realOut)*sin( M_PI * i / n) + imagOut*cos( M_PI * i / n));

			Xuint32 t = (Xuint32)(nFactor*((Xuint32)(tReal<<16)|((Xuint32)tImag)));

			XIo_Out32(buf+i*2,XIo_In32(out + i*4) + t);
			XIo_Out32(buf+(i+n)*2,XIo_In32(out + i*4) - t);
			//buf[i / 2] = out[i] + t;
			//buf[(i + n)/2] = out[i] - t;
			Xil_Out32(LED_BASEADDR, volumeLED2);
		}

	}
}

//void fftSample(Xuint32 bufaddress, int n, int inverse) {
//
//	cplx *buf;
//	buf=(cplx*)malloc(n*4);
//
//	cplx *out;
//	out=(cplx*)malloc(n*4);
//
//	int i = 0;
//	int count = 0;
//	while ((i/8) < n) {
//		XIo_Out32 (&buf, XIo_In32 (bufaddress + i) & AC97_DATA_MASK);
//		XIo_Out32 (&out, XIo_In32 (bufaddress + i) & AC97_DATA_MASK);
//		i = i + 8;
////		buf[count] = XIo_In32 (bufaddress + i) & AC97_DATA_MASK; /*get sample*/
////		i = i + 4;
////		count++;
//	}
//
//	 _fftSample(buf, out, n, 1,1);
//     fftAddress = (Xuint32)buf;
//    // pAudioData = (Xuint32)fftAddress;
//}
//
//void _fftSample(cplx *buf, cplx *out, int n, int step, int inverse) {
//	if (step < n) {
//		_fftSample(&out, &buf, n, step * 2,inverse);
//		_fftSample(&out + (step*4), &buf + (step*4), n, step * 2,inverse);
//
//		int i;
//		Xil_Out32(LED_BASEADDR, volumeLED6);
//		for (i = 0; i < n; i += 2 * step) {
//			Xil_Out32(LED_BASEADDR, volumeLED5);
//			cplx t = cexp(-I * PI * i / n) * out[i + step];
//			XIo_Out32 (&buf + (int)(i/2)*4, (XIo_In32 (&out + i*4) & AC97_DATA_MASK)+t);
//			XIo_Out32 (&buf + (int)((i + n)/2)*4, (XIo_In32 (&out + i*4) & AC97_DATA_MASK)-t);
//			//buf[i / 2]     = out[i] + t;
//			//buf[(i + n)/2] = out[i] - t;
//			Xil_Out32(LED_BASEADDR, volumeLED4);
//		}
//
//	}
//}

void addOriginMeta(Xuint32 bufAddress, Xuint32 outAddress) {
	Xuint32 originalSample;
	Xuint32 createdSample;
	Xil_Out32(LED_BASEADDR, volumeLED3);
	while ((Count_Samples / 8) < lNumSamples) {
		originalSample = XIo_In32 (bufAddress + Count_Samples);
		createdSample = XIo_In32 (outAddress + Count_Samples);
		originalSample &= AC97_META_MASK;
		createdSample &= AC97_DATA_MASK;
		createdSample |= originalSample;
		XIo_Out32 (outAddress + Count_Samples, createdSample);
		Count_Samples = Count_Samples + 8;
		Xil_Out32(LED_BASEADDR, volumeLED4);

	}
}

int compareValues(Xuint32 bufAddress, Xuint32 outAddress) {
	Xuint32 transformedSample;
	Xuint32 originalSample;
	int step = 4;
	int i;
	Xil_Out32(LED_BASEADDR, volumeLED5);
	for (i = 0; i < lNumSamples; i += 2 * step) {
		transformedSample = XIo_In32 (outAddress + i + step);
		originalSample = XIo_In32 (bufAddress + i + step);
		if (transformedSample != originalSample)
			return 0;
	}
	return 1;
	Xil_Out32(LED_BASEADDR, volumeLED0);
}

