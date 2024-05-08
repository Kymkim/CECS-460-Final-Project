/************************************************************************/
/*																		*/
/*	video_demo.h	--	ZYBO Video demonstration 						*/
/*																		*/
/************************************************************************/
/*	Author: Sam Bobrowicz												*/
/*	Copyright 2015, Digilent Inc.										*/
/************************************************************************/
/*  Module Description: 												*/
/*																		*/
/*		This file contains code for running a demonstration of the		*/
/*		Video input and output capabilities on the ZYBO. It is a good	*/
/*		example of how to properly use the display_ctrl and				*/
/*		video_capture drivers.											*/
/*																		*/
/*																		*/
/************************************************************************/
/*  Revision History:													*/
/* 																		*/
/*		11/25/2015(SamB): Created										*/
/*																		*/
/************************************************************************/

#ifndef VIDEO_DEMO_H_
#define VIDEO_DEMO_H_

/* ------------------------------------------------------------ */
/*				Include File Definitions						*/
/* ------------------------------------------------------------ */

#include "xil_types.h"
#include <stdlib.h>
#include <time.h>
/* ------------------------------------------------------------ */
/*					Miscellaneous Declarations					*/
/* ------------------------------------------------------------ */

#define RED 0
#define GREEN 1
#define BLUE 2
#define VIOLET 3
#define YELLOW 4

#define DEMO_MAX_FRAME (1920*1080*3)
#define DEMO_STRIDE (1920 * 3)

/*
 * Configure the Video capture driver to start streaming on signal
 * detection
 */
#define DEMO_START_ON_DET 1

/* ------------------------------------------------------------ */
/*					Procedure Declarations						*/
/* ------------------------------------------------------------ */

void Initialize();
void Run();
void StartMenu();
void RunSimonSays2x2();
void FillColor2x2(u8 *frame, u32 width, u32 height, u32 stride, int color);
void RunSimonSays3x3();
void FillColor3x3(u8 *frame, u32 width, u32 height, u32 stride, int color);
void DemoISR(void *callBackRef, void *pVideo);

/* ------------------------------------------------------------ */

/************************************************************************/

#endif /* VIDEO_DEMO_H_ */
