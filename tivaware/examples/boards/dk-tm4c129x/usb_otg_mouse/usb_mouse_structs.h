//*****************************************************************************
//
// usb_mouse_structs.h - Data structures defining the mouse USB device.
//
// Copyright (c) 2013-2016 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.3.156 of the DK-TM4C129X Firmware Package.
//
//*****************************************************************************

#ifndef _USB_MOUSE_STRUCTS_H_
#define _USB_MOUSE_STRUCTS_H_

extern uint32_t MouseHandler(void *pvCBData, uint32_t ui32Event, 
                             uint32_t ui32MsgData, void *pvMsgData);

extern tUSBDHIDMouseDevice g_sMouseDevice;

#endif
