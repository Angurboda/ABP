/************************************************************************/
/*																		*/
/*	ac97_demo.h	--	Interface Declarations for ac97_demo.c				*/
/*																		*/
/************************************************************************/
/*	Author:	Sam Bobrowicz												*/
/*	Copyright 2011, Digilent Inc.										*/
/************************************************************************/
/*  File Description: 													*/
/*																		*/
/*	This Header file contains the interface and constant definitions	*/
/*	used by ac97_demo.c. 												*/
/*																		*/
/************************************************************************/
/*  Revision History:													*/
/* 																		*/
/*		11/4/2011(SamB): Created										*/
/*																		*/
/************************************************************************/

#ifndef AC97_DEMO_H
#define AC97_DEMO_H

/* ------------------------------------------------------------ */
/*					Constant Definitions						*/
/* ------------------------------------------------------------ */

#define lBtnChannel	1//GPIO channel of the push buttons

/*
 * Push Button bit definitions
 */
#define bitBtnC 0x00000001
#define bitBtnR 0x00000002
#define bitBtnD 0x00000004
#define bitBtnL 0x00000008
#define bitBtnU 0x00000010

/*
 * DIP Switch bit definitions
 */
#define bitSw0 0x00000001

/*
 * LED bit definitions
 */
#define bitRecLED 0x00000001
#define bitPlayLED 0x00000002

/*
 * our LED (test-)definitions
 */
#define volumeLED0 0x00000001
#define volumeLED1 0x00000002
#define volumeLED2 0x00000004
#define volumeLED3 0x00000008
#define volumeLED4 0x00000010
#define volumeLED5 0x00000020
#define volumeLED6 0x00000040
#define volumeLED7 0x00000080
#define allLEDs 0x000000FF

#define firstLED 0x00000001


/*
 * Run action flag set bit definitions
 */
#define bitPlay 0x01
#define bitRec 0x02
#define bitGenWave 0x04
#define bitLEDTest 0x08

/*
 * Recorded data properties
 */
#define lSampleRate 48000
#define lRecLength 2
#define lNumSamples (lSampleRate * lRecLength)

/* ------------------------------------------------------------ */
/*					Procedure Declarations						*/
/* ------------------------------------------------------------ */

void PushBtnHandler(void *CallBackRef);

/* ------------------------------------------------------------ */
/*                our procedure declarations					*/
/* ------------------------------------------------------------ */
void sleepTimer(int time);
void setVolumeLEDs(Xuint32 left, Xuint32 right);
double calculateDBOverload(Xuint32 sample);
void captureSampleReference(Xuint32 left, Xuint32 right);
void addOriginMeta(Xuint32 bufAddress, Xuint32 outAddress);

/* ------------------------------------------------------------ */
#endif
