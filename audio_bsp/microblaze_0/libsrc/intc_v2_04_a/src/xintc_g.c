
/*******************************************************************
*
* CAUTION: This file is automatically generated by libgen.
* Version: Xilinx EDK 14.2 EDK_P.28xd
* DO NOT EDIT.
*
* Copyright (c) 1995-2012 Xilinx, Inc.  All rights reserved.

* 
* Description: Driver configuration
*
*******************************************************************/

#include "xparameters.h"
#include "xintc.h"


extern void XNullHandler (void *);

/*
* The configuration table for devices
*/

XIntc_Config XIntc_ConfigTable[] =
{
	{
		XPAR_MICROBLAZE_0_INTC_DEVICE_ID,
		XPAR_MICROBLAZE_0_INTC_BASEADDR,
		XPAR_MICROBLAZE_0_INTC_KIND_OF_INTR,
		XPAR_MICROBLAZE_0_INTC_HAS_FAST,
		XIN_SVC_SGL_ISR_OPTION,
		{
			{
				XNullHandler,
				(void *) XNULL
			}
		}

	}
};

