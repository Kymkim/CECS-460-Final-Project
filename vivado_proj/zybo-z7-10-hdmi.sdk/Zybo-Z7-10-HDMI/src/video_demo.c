/* ------------------------------------------------------------ */
/*				Include File Definitions						*/
/* ------------------------------------------------------------ */

#include "video_demo.h"
#include "video_capture/video_capture.h"
#include "display_ctrl/display_ctrl.h"
#include "intc/intc.h"
#include <stdio.h>
#include "xuartps.h"
#include "math.h"
#include <ctype.h>
#include <stdlib.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "timer_ps/timer_ps.h"
#include "xparameters.h"
#include "xscutimer.h"

/*
 * XPAR redefines
 */
#define DYNCLK_BASEADDR 		XPAR_AXI_DYNCLK_0_BASEADDR
#define VDMA_ID 				XPAR_AXIVDMA_0_DEVICE_ID
#define HDMI_OUT_VTC_ID 		XPAR_V_TC_OUT_DEVICE_ID
#define HDMI_IN_VTC_ID 			XPAR_V_TC_IN_DEVICE_ID
#define HDMI_IN_GPIO_ID 		XPAR_AXI_GPIO_VIDEO_DEVICE_ID
#define HDMI_IN_VTC_IRPT_ID 	XPAR_FABRIC_V_TC_IN_IRQ_INTR
#define HDMI_IN_GPIO_IRPT_ID 	XPAR_FABRIC_AXI_GPIO_VIDEO_IP2INTC_IRPT_INTR
#define SCU_TIMER_ID 			XPAR_SCUTIMER_DEVICE_ID
#define UART_BASEADDR 			XPAR_PS7_UART_1_BASEADDR

/* ------------------------------------------------------------ */
/*				Global Variables								*/
/* ------------------------------------------------------------ */

/*
 * Display and Video Driver structs
 */
DisplayCtrl dispCtrl;
XAxiVdma vdma;
VideoCapture videoCapt;
INTC intc;
char fRefresh; //flag used to trigger a refresh of the Menu on video detect

/*
 * Framebuffers for video data
 */
u8 frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME] __attribute__((aligned(0x20)));
u8 *pFrames[DISPLAY_NUM_FRAMES]; //array of pointers to the frame buffers

/*
 * Interrupt vector table
 */
const ivt_t ivt[] = {
	videoGpioIvt(HDMI_IN_GPIO_IRPT_ID, &videoCapt),
	videoVtcIvt(HDMI_IN_VTC_IRPT_ID, &(videoCapt.vtc))
};

/* ------------------------------------------------------------ */
/*				Procedure Definitions							*/
/* ------------------------------------------------------------ */

int main(void)
{
	Initialize();

	Run();

	return 0;
}


void Initialize()
{

	int Status;
	XAxiVdma_Config *vdmaConfig;
	int i;

	/*
	 * Initialize an array of pointers to the 3 frame buffers
	 */
	for (i = 0; i < DISPLAY_NUM_FRAMES; i++)
	{
		pFrames[i] = frameBuf[i];
	}

	/*
	 * Initialize a timer used for a simple delay
	 */
	TimerInitialize(SCU_TIMER_ID);

	/*
	 * Initialize VDMA driver
	 */
	vdmaConfig = XAxiVdma_LookupConfig(VDMA_ID);
	if (!vdmaConfig)
	{
		xil_printf("No video DMA found for ID %d\r\n", VDMA_ID);
		return;
	}
	Status = XAxiVdma_CfgInitialize(&vdma, vdmaConfig, vdmaConfig->BaseAddress);
	if (Status != XST_SUCCESS)
	{
		xil_printf("VDMA Configuration Initialization failed %d\r\n", Status);
		return;
	}

	/*
	 * Initialize the Display controller and start it
	 */
	Status = DisplayInitialize(&dispCtrl, &vdma, HDMI_OUT_VTC_ID, DYNCLK_BASEADDR, pFrames, DEMO_STRIDE);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Display Ctrl initialization failed during demo initialization%d\r\n", Status);
		return;
	}
	Status = DisplayStart(&dispCtrl);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Couldn't start display during demo initialization%d\r\n", Status);
		return;
	}

	/*
	 * Initialize the Interrupt controller and start it.
	 */
	Status = fnInitInterruptController(&intc);
	if(Status != XST_SUCCESS) {
		xil_printf("Error initializing interrupts");
		return;
	}
	fnEnableInterrupts(&intc, &ivt[0], sizeof(ivt)/sizeof(ivt[0]));

	/*
	 * Initialize the Video Capture device
	 */
	Status = VideoInitialize(&videoCapt, &intc, &vdma, HDMI_IN_GPIO_ID, HDMI_IN_VTC_ID, HDMI_IN_VTC_IRPT_ID, pFrames, DEMO_STRIDE, DEMO_START_ON_DET);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Video Ctrl initialization failed during demo initialization%d\r\n", Status);
		return;
	}

	/*
	 * Set the Video Detect callback to trigger the menu to reset, displaying the new detected resolution
	 */
	VideoSetCallback(&videoCapt, DemoISR, &fRefresh);

	FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 1);

	return;
}
void Run()
{
	char userInput = 0;

	/* Flush UART FIFO */
	while (XUartPs_IsReceiveData(UART_BASEADDR))
	{
		XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
	}

	while (userInput != 'q')
	{
		fRefresh = 0;
		StartMenu();

		/* Wait for data on UART */
		while (!XUartPs_IsReceiveData(UART_BASEADDR) && !fRefresh)
		{}

		/* Store the first character in the UART receive FIFO and echo it */
		if (XUartPs_IsReceiveData(UART_BASEADDR))
		{
			userInput = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
			xil_printf("%c", userInput);
		}
		else  //Refresh triggered by video detect interrupt
		{
			userInput = 'r';
		}

		switch (userInput)
		{
		case '1':
			RunSimonSays2x2();
			break;
		case '2':
			RunSimonSays3x3();
			break;
		default :
			xil_printf("\n\rInvalid Selection");
			TimerDelay(500000);
		}
	}

	return;
}

void StartMenu()
{
	xil_printf("\x1B[H"); //Set cursor to top left of terminal
	xil_printf("\x1B[2J"); //Clear terminal
	xil_printf("**************************************************\n\r");
	xil_printf("*                SIMON SAYS GAME                 *\n\r");
	xil_printf("**************************************************\n\r");
	xil_printf("\n\r");
	xil_printf("1 - Play Simon Says 2X2\n\r");
	xil_printf("2 - Play Simon Says 3X3\n\r");
	xil_printf("q - Quit\n\r");
	xil_printf("\n\r");
	xil_printf("\n\r");
	xil_printf("Enter a selection:");
}

void RunSimonSays2x2(){

	xil_printf("\x1B[H"); //Set cursor to top left of terminal
	xil_printf("\x1B[2J"); //Clear terminal

	//Up to 10 sequence
	int SeqLen = 20;
	int sequence[SeqLen];
	int guessSeq[SeqLen];

	//Generate random sequence
	for(int i = 0; i < SeqLen; i++){
		if ((i % 2) == 0){
			sequence[i] = (rand() % 4);
		}else{
			sequence[i] = 4;
		}
		guessSeq[i] = 0;
	}

	FillColor2x2(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 4);


	int round = 1;
	int gameStop = 0;

	xil_printf("\n\rRules: Repeat the pattern shown in the screen!");
	xil_printf("\n\rPress Enter To Continue\n");

	/* Wait for data on UART */
	while (!XUartPs_IsReceiveData(UART_BASEADDR) && !fRefresh){}

	while(gameStop == 0){

		xil_printf("\x1B[H"); //Set cursor to top left of terminal
		xil_printf("\x1B[2J"); //Clear terminal

		FillColor2x2(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 4);

		xil_printf("Displaying Colors...");
		//Show The Colors
		for(int c = 0; c < round + 1; c++){
			FillColor2x2(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, sequence[c]);
			TimerDelay(500000*2);
		}
		xil_printf("\n\rDisplaying Color Done! What is the sequence? (MAKE SURE ALL CAPS)...");
		xil_printf("\n\rR = Red, G= Green, B=Blue, Y=Yellow\n\r");


		for(int a = 0; a < round + 1; a+=2){
			char userInput = 0;
			/* Flush UART FIFO */
			while (XUartPs_IsReceiveData(UART_BASEADDR))
			{
				XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
			}

			fRefresh = 0;

			/* Wait for data on UART */
			while (!XUartPs_IsReceiveData(UART_BASEADDR) && !fRefresh){}

			/* Store the first character in the UART receive FIFO and echo it */
			if (XUartPs_IsReceiveData(UART_BASEADDR))
			{
				userInput = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
				xil_printf("%c", userInput);
			}

			switch(userInput)
			{
				case 'R':
					FillColor2x2(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 0);
					guessSeq[a] = 0;
					guessSeq[a+1] = 4;
					break;
				case 'G':
					FillColor2x2(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 2);
					guessSeq[a] = 2;
					guessSeq[a+1] = 4;
					break;
				case 'B':
					FillColor2x2(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 1);
					guessSeq[a] = 1;
					guessSeq[a+1] = 4;
					break;
				case 'Y':
					FillColor2x2(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 3);
					guessSeq[a] = 3;
					guessSeq[a+1] = 4;
					break;
				default :
					FillColor2x2(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 4);
					guessSeq[a] = 4;
					guessSeq[a+1] = 4;
			}


		}

		xil_printf("\n\rChecking Answer! Please wait!");
		for(int i = 0; i < round + 1; i++){
			if(guessSeq[i] != sequence[i]){
				xil_printf("\n\rYou Lose!!");
				gameStop = 1;
				break;
			}
		}

		if (gameStop == 0){
			xil_printf("\n\rCorrect!");
		}

		round += 2;

		if (round > SeqLen){
			gameStop = 1;
		}

		TimerDelay(500000*2);
	}

	TimerDelay(500000*2);
}

void RunSimonSays3x3(){

	xil_printf("\x1B[H"); //Set cursor to top left of terminal
	xil_printf("\x1B[2J"); //Clear terminal

	//Up to 10 sequence
	int SeqLen = 20;
	int sequence[SeqLen];
	int guessSeq[SeqLen];

	//Generate random sequence
	for(int i = 0; i < SeqLen; i++){
		if ((i % 2) == 0){
			sequence[i] = (rand() % 7) + 1;
		}else{
			sequence[i] = 0;
		}
		guessSeq[i] = 0;
	}

	FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 0);


	int round = 1;
	int gameStop = 0;

	xil_printf("\n\rRules: Repeat the pattern shown in the screen!");
	xil_printf("\n\rPress Enter To Continue\n");

	/* Wait for data on UART */
	while (!XUartPs_IsReceiveData(UART_BASEADDR) && !fRefresh){}

	while(gameStop == 0){

		xil_printf("\x1B[H"); //Set cursor to top left of terminal
		xil_printf("\x1B[2J"); //Clear terminal

		FillColor2x2(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 8);

		xil_printf("Displaying Colors...");
		//Show The Colors
		for(int c = 0; c < round + 1; c++){
			FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, sequence[c]);
			TimerDelay(500000*2);
		}
		xil_printf("\n\rDisplaying Color Done! What is the sequence? (MAKE SURE ALL CAPS)...");
		xil_printf("\n\rUse Your Num Pad!\n\r");
		xil_printf("\n\r 7 8 9 \n\r");
		xil_printf("\n\r 4 5 6 \n\r");
		xil_printf("\n\r 1 2 3 \n\r");


		for(int a = 0; a < round + 1; a+=2){
			char userInput = 0;
			/* Flush UART FIFO */
			while (XUartPs_IsReceiveData(UART_BASEADDR))
			{
				XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
			}

			fRefresh = 0;

			/* Wait for data on UART */
			while (!XUartPs_IsReceiveData(UART_BASEADDR) && !fRefresh){}

			/* Store the first character in the UART receive FIFO and echo it */
			if (XUartPs_IsReceiveData(UART_BASEADDR))
			{
				userInput = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
				xil_printf("%c", userInput);
			}

			switch(userInput)
			{
				case '7':
					FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 7);
					guessSeq[a] = 7;
					guessSeq[a+1] = 0;
					break;
				case '8':
					FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 8);
					guessSeq[a] = 8;
					guessSeq[a+1] = 0;
					break;
				case '9':
					FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 9);
					guessSeq[a] = 9;
					guessSeq[a+1] = 0;
					break;
				case '4':
					FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 4);
					guessSeq[a] = 4;
					guessSeq[a+1] = 0;
					break;
				case '5':
					FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 5);
					guessSeq[a] = 5;
					guessSeq[a+1] = 0;
					break;
				case '6':
					FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 6);
					guessSeq[a] = 6;
					guessSeq[a+1] = 0;
					break;
				case '1':
					FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 1);
					guessSeq[a] = 1;
					guessSeq[a+1] = 0;
					break;
				case '2':
					FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 2);
					guessSeq[a] = 2;
					guessSeq[a+1] = 0;
					break;
				case '3':
					FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 3);
					guessSeq[a] = 3;
					guessSeq[a+1] = 0;
					break;
				default :
					FillColor3x3(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, 0);
					guessSeq[a] = 0;
					guessSeq[a+1] = 0;
			}


		}

		xil_printf("\n\rChecking Answer! Please wait!");
		for(int i = 0; i < round + 1; i++){
			if(guessSeq[i] != sequence[i]){
				xil_printf("\n\rYou Lose!!");
				gameStop = 1;
				break;
			}
		}

		if (gameStop == 0){
			xil_printf("\n\rCorrect!");
		}

		round += 2;

		if (round > SeqLen){
			gameStop = 1;
		}

		TimerDelay(500000*2);
	}

	TimerDelay(500000*2);
}

void FillColor3x3(u8 *frame, u32 width, u32 height, u32 stride, int color){
	u32 xcoi, ycoi;
	u32 iPixelAddr;

	for(xcoi = 0; xcoi < (width*3); xcoi+=3){
		iPixelAddr = xcoi;
		for(ycoi = 0; ycoi < height; ycoi++){
			if (xcoi < ((width*3) / 3)){
				if (ycoi < (height/3)){
					frame[iPixelAddr+2] = 214;
					frame[iPixelAddr+1] = 48;
					frame[iPixelAddr] = 38;
					if (color != 7){
						for (int i = 0; i < 3; i++){
							frame[iPixelAddr + i] /= 2;
						}
					}
				}else if (ycoi < ((height/3)*2)){
					frame[iPixelAddr+2] = 254;
					frame[iPixelAddr+1] = 224;
					frame[iPixelAddr] = 138;
					if (color != 4){
						for (int i = 0; i < 3; i++){
							frame[iPixelAddr + i] /= 2;
						}
					}
				}else{
					frame[iPixelAddr+2] = 166;
					frame[iPixelAddr+1] = 218;
					frame[iPixelAddr] = 106;
					if (color != 1){
						for (int i = 0; i < 3; i++){
							frame[iPixelAddr + i] /= 2;
						}
					}
				}
			}else if (xcoi < ( ((width*3) / 3) * 2 )){
				if (ycoi < (height/3)){
					frame[iPixelAddr+2] = 244;
					frame[iPixelAddr+1] = 108;
					frame[iPixelAddr] = 66;
					if (color != 8){
						for (int i = 0; i < 3; i++){
							frame[iPixelAddr + i] /= 2;
						}
					}
				}else if (ycoi < ((height/3)*2)){
					frame[iPixelAddr+2] = 254;
					frame[iPixelAddr+1] = 254;
					frame[iPixelAddr] = 192;
					if (color != 5){
						for (int i = 0; i < 3; i++){
							frame[iPixelAddr + i] /= 2;
						}
					}
				}else{
					frame[iPixelAddr+2] = 102;
					frame[iPixelAddr+1] = 188;
					frame[iPixelAddr] = 98;
					if (color != 2){
						for (int i = 0; i < 3; i++){
							frame[iPixelAddr + i] /= 2;
						}
					}
				}
			}else{
				if (ycoi < (height/3)){
					frame[iPixelAddr+2] = 254;
					frame[iPixelAddr+1] = 174;
					frame[iPixelAddr] = 98;
					if (color != 9){
						for (int i = 0; i < 3; i++){
							frame[iPixelAddr + i] /= 2;
						}
					}
				}else if (ycoi < ((height/3)*2)){
					frame[iPixelAddr+2] = 218;
					frame[iPixelAddr+1] = 238;
					frame[iPixelAddr] = 138;
					if (color != 6){
						for (int i = 0; i < 3; i++){
							frame[iPixelAddr + i] /= 2;
						}
					}
				}else{
					frame[iPixelAddr+2] = 26;
					frame[iPixelAddr+1] = 152;
					frame[iPixelAddr] = 80;
					if (color != 3){
						for (int i = 0; i < 3; i++){
							frame[iPixelAddr + i] /= 2;
						}
					}
				}
			}
			iPixelAddr += stride;
		}
	}


	Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);

}

void FillColor2x2(u8 *frame, u32 width, u32 height, u32 stride, int color){
	u32 xcoi, ycoi;
	u32 iPixelAddr;

	switch (color){
	//RED
	case 0:
		for(xcoi = 0; xcoi < (width*3); xcoi+=3){
			iPixelAddr = xcoi;
			for(ycoi = 0; ycoi < height; ycoi++){
				if (xcoi < ((width*3) / 2)){
					if (ycoi < (height/2)){
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 0;
						frame[iPixelAddr+2] = 255;
					}else{
						frame[iPixelAddr] = 55;
						frame[iPixelAddr+1] = 0;
						frame[iPixelAddr+2] = 0;
					}
				}else{
					if (ycoi < (height/2)){
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 55;
						frame[iPixelAddr+2] = 0;
					}else{
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 55;
						frame[iPixelAddr+2] = 55;
					}
				}
				iPixelAddr += stride;
			}
		}
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;

	//BLUE
	case 1:
		for(xcoi = 0; xcoi < (width*3); xcoi+=3){
			iPixelAddr = xcoi;
			for(ycoi = 0; ycoi < height; ycoi++){
				if (xcoi < ((width*3) / 2)){
					if (ycoi < (height/2)){
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 0;
						frame[iPixelAddr+2] = 55;
					}else{
						frame[iPixelAddr] = 255;
						frame[iPixelAddr+1] = 0;
						frame[iPixelAddr+2] = 0;
					}
				}else{
					if (ycoi < (height/2)){
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 55;
						frame[iPixelAddr+2] = 0;
					}else{
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 55;
						frame[iPixelAddr+2] = 55;
					}
				}
				iPixelAddr += stride;
			}
		}
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;

	//GREEN
	case 2:
		for(xcoi = 0; xcoi < (width*3); xcoi+=3){
			iPixelAddr = xcoi;
			for(ycoi = 0; ycoi < height; ycoi++){
				if (xcoi < ((width*3) / 2)){
					if (ycoi < (height/2)){
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 0;
						frame[iPixelAddr+2] = 55;
					}else{
						frame[iPixelAddr] = 55;
						frame[iPixelAddr+1] = 0;
						frame[iPixelAddr+2] = 0;
					}
				}else{
					if (ycoi < (height/2)){
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 255;
						frame[iPixelAddr+2] = 0;
					}else{
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 55;
						frame[iPixelAddr+2] = 55;
					}
				}
				iPixelAddr += stride;
			}
		}
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;

	//YELLOW
	case 3:
		for(xcoi = 0; xcoi < (width*3); xcoi+=3){
			iPixelAddr = xcoi;
			for(ycoi = 0; ycoi < height; ycoi++){
				if (xcoi < ((width*3) / 2)){
					if (ycoi < (height/2)){
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 0;
						frame[iPixelAddr+2] = 55;
					}else{
						frame[iPixelAddr] = 55;
						frame[iPixelAddr+1] = 0;
						frame[iPixelAddr+2] = 0;
					}
				}else{
					if (ycoi < (height/2)){
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 55;
						frame[iPixelAddr+2] = 0;
					}else{
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 255;
						frame[iPixelAddr+2] = 255;
					}
				}
			iPixelAddr += stride;
			}
		}
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;

	//Empty
	case 4:
		for(xcoi = 0; xcoi < (width*3); xcoi+=3){
			iPixelAddr = xcoi;
			for(ycoi = 0; ycoi < height; ycoi++){
				if (xcoi < ((width*3) / 2)){
					if (ycoi < (height/2)){
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 0;
						frame[iPixelAddr+2] = 55;
					}else{
						frame[iPixelAddr] = 55;
						frame[iPixelAddr+1] = 0;
						frame[iPixelAddr+2] = 0;
					}
				}else{
					if (ycoi < (height/2)){
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 55;
						frame[iPixelAddr+2] = 0;
					}else{
						frame[iPixelAddr] = 0;
						frame[iPixelAddr+1] = 55;
						frame[iPixelAddr+2] = 55;
					}
				}
			iPixelAddr += stride;
			}
		}
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;
	}


}



void DemoISR(void *callBackRef, void *pVideo)
{
	char *data = (char *) callBackRef;
	*data = 1; //set fRefresh to 1
}


