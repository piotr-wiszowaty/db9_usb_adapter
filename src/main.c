/**
 *  DB9-USB-Adapter
 *  Copyright (C) 2013,2014  Piotr Wiszowaty
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see {http://www.gnu.org/licenses/}.
 */

#include <p32xxxx.h>
#include <plib.h>
#include <sys/attribs.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "GenericTypeDefs.h"
#include "HardwareProfile.h"
#include "usb_config.h"
#include "USB/usb.h"
#include "USB/usb_host_hid_parser.h"
#include "USB/usb_host_hid.h"
#include "USB/usb_host_msd.h"
#include "USB/usb_host_msd_scsi.h"
#include "MDD File System/FSIO.h"
#include "uart1.h"
#include <debug.h>

#define VIRTUAL_FLASH_START				0xbd000000
#define FLASH_SIZE						131072
#define FLASH_PAGE_SIZE					4096
#define VIRTUAL_FLASH_END				(VIRTUAL_FLASH_START + FLASH_SIZE)
//#define JOY_DESCRIPTIONS_FLASH_OFFSET	(FLASH_SIZE - 2*FLASH_PAGE_SIZE)
#define JOY_DESCRIPTIONS_FLASH_START	(VIRTUAL_FLASH_END - 2*FLASH_PAGE_SIZE)
#define JOY_DESCRIPTIONS_FLASH_END		(VIRTUAL_FLASH_END - FLASH_PAGE_SIZE)

#define MAX_ALLOWED_CURRENT 	500

#pragma config FPLLMUL = MUL_20, FPLLIDIV = DIV_2, FPLLODIV = DIV_2
#pragma config POSCMOD = HS, FNOSC = PRIPLL, FPBDIV = DIV_1
#pragma config FSOSCEN = OFF
#pragma config FWDTEN = OFF
#pragma config ICESEL = ICS_PGx1
#pragma config JTAGEN = OFF
#pragma config UPLLIDIV = DIV_2, UPLLEN = ON
#pragma config FVBUSONIO = ON, FUSBIDIO = ON

#define USAGE_PAGE_GEN_DESKTOP	0x01
#define USAGE_PAGE_BUTTONS		0x09

#define USAGE_JOYSTICK			0x04
#define USAGE_DIRECTION_X		0x30
#define USAGE_DIRECTION_Y		0x31
#define USAGE_HAT_SWITCH		0x39

#define JOY_CONTROL_KIND_MASK	0xf0
#define JOY_CONTROL_KIND_LEFT	0x10
#define JOY_CONTROL_KIND_RIGHT	0x20
#define JOY_CONTROL_KIND_FWD	0x30
#define JOY_CONTROL_KIND_BACK	0x40
#define JOY_CONTROL_KIND_TRG	0x50
#define JOY_CONTROL_OP_MASK		0x0f
#define JOY_CONTROL_OP_EQU		0x01
#define JOY_CONTROL_OP_NEQ		0x02
#define JOY_CONTROL_OP_LT		0x03
#define JOY_CONTROL_OP_GT		0x04

typedef enum {
	IDLE = 0,
	MSD_LOOP,
	HID_LOOP,
	HID_GET_REPORT,
	HID_GET_REPORT_WAIT,
	ERROR
} STATE;

typedef struct {
	WORD report_id;
	WORD size;
	BYTE data[64];
	WORD poll_rate;
} HID_INPUT_REPORT;

typedef struct __attribute__ ((packed)) {
	UINT8 kind_operation;
	UINT8 offset;
	UINT8 mask;
	UINT8 value;
} JOY_CONTROL;

typedef struct __attribute__ ((packed)) {
	UINT8 length;
	UINT16 vid;
	UINT16 pid;
	UINT8 report_id;
	JOY_CONTROL controls[0];
} JOY_DESCRIPTION;

volatile STATE state = IDLE;
volatile BOOL device_attached;
volatile BYTE hid_interface_number;

HID_INPUT_REPORT input_report;
unsigned char previous_input_report_data[128];

HID_DATA_DETAILS buttons_details;
HID_USER_DATA_SIZE buttons[32];
int total_buttons;

int pointer_present;
HID_DATA_DETAILS pointer_details;
HID_USER_DATA_SIZE pointer[8];
int pointer_x_index;
int pointer_y_index;
int pointer_x_min;
int pointer_x_max;
int pointer_y_min;
int pointer_y_max;
int hat_switch_index;

volatile JOY_DESCRIPTION *joy_description;
volatile int n_joy_controls;

typedef enum {
	OK = 0,
	FILE_OPEN,
	FLASH_ERASE,
	FLASH_WRITE,
	FLASH_VERIFY,
	MSD_MOUNT,
} ERR;
ERR error_code = OK;

volatile int delay_ms;

unsigned int load_config()
{
	unsigned int address;
	unsigned int res;
	UINT32 word;
	int n;
	FSFILE *f;

	f = FSfopen("db9usb.dat", "rb");
	if (f == NULL) {
		error_code = FILE_OPEN;
		u1tx_str("E:fopen\r\n");
		return error_code;
	}

	for (address = JOY_DESCRIPTIONS_FLASH_START; address < JOY_DESCRIPTIONS_FLASH_END; address += FLASH_PAGE_SIZE) {
		res = NVMErasePage((void *) address);
		if (res) {
			error_code = FLASH_ERASE;
			u1tx_str("E:erase\r\n");
			return res;
		}
		break;
	}

	for (address = JOY_DESCRIPTIONS_FLASH_START; address < JOY_DESCRIPTIONS_FLASH_END; address += 4) {
		word = 0xffffffff;
		n = FSfread(&word, 1, 4, f);
		if (n == 0) {
			// EOF reached
			break;
		}
		res = NVMWriteWord((void *) address, word);
		if (res) {
			error_code = FLASH_WRITE;
			u1tx_str("E:write\r\n");
			break;
		}
		if (*((unsigned int *) address) != word) {
			res = -2;
			error_code = FLASH_VERIFY;
			u1tx_str("E:verify\r\n");
			break;
		}
		if (n < 4) {
			// EOF reached
			break;
		}
	}

	if (f != NULL) {
		FSfclose(f);
	}

	return res;
}

void find_joy_description(UINT16 vid, UINT16 pid)
{
	void *p = (void *) (JOY_DESCRIPTIONS_FLASH_START);
	int i;

	while (p < (void *) VIRTUAL_FLASH_END) {
		joy_description = p;
		if (joy_description->length > 0 && joy_description->length != 0xff) {
			if (joy_description->vid == vid && joy_description->pid == pid) {
				// description found; calculate number of controls
				i = sizeof(JOY_DESCRIPTION);
				n_joy_controls = 0;
				while (i < joy_description->length) {
					i += sizeof(JOY_CONTROL);
					n_joy_controls++;
				}
				return;
			} else {
				// go to next description
				p += joy_description->length;
			}
		} else {
			// reached end of joy descriptions
			joy_description = NULL;
			return;
		}
	}
}

BOOL USB_HID_DataCollectionHandler()
{
	BYTE num_of_report_item = 0;
	BYTE i;
	BYTE j;
	USB_HID_ITEM_LIST *item_list_ptrs;
	USB_HID_DEVICE_RPT_INFO *device_rpt_info;
	HID_REPORTITEM *report_item;
	HID_USAGEITEM *hid_usage_item;
	BYTE usage_index;
	BYTE report_index;
	BYTE report_id;

	device_rpt_info = USBHostHID_GetCurrentReportInfo();
	item_list_ptrs = USBHostHID_GetItemListPointers();

	total_buttons = 0;
	pointer_present = 0;
	pointer_x_index = -1;
	pointer_y_index = -1;
	hat_switch_index = -1;

	BOOL status = FALSE;
	num_of_report_item = device_rpt_info->reportItems;
	for (i = 0; i < num_of_report_item; i++) {
		report_item = &item_list_ptrs->reportItemList[i];
		if (report_item->reportType == hidReportInput
				&& report_item->dataModes == HIDData_Variable
				&& report_item->globals.usagePage == USAGE_PAGE_BUTTONS) {
			usage_index = report_item->firstUsageItem;
			for (j = 0; j < report_item->usageItems; j++) {
				hid_usage_item = &item_list_ptrs->usageItemList[usage_index + j];
				hid_usage_item->usage;
			}
			report_index = report_item->globals.reportIndex;
			buttons_details.reportLength = (item_list_ptrs->reportList[report_index].inputBits + 7) / 8;
			buttons_details.reportID = (BYTE) report_item->globals.reportID;
			buttons_details.bitOffset = (BYTE) report_item->startBit;
			buttons_details.bitLength = (BYTE) report_item->globals.reportsize;
			buttons_details.count = (BYTE) report_item->globals.reportCount;
			buttons_details.interfaceNum = USBHostHID_ApiGetCurrentInterfaceNum();
			total_buttons = buttons_details.count;
		} else if (report_item->reportType == hidReportInput
				&& report_item->dataModes == HIDData_Variable
				&& report_item->globals.usagePage == USAGE_PAGE_GEN_DESKTOP) {
			if (!pointer_present) {
				usage_index = report_item->firstUsageItem;
				for (j = 0; j < report_item->usageItems; j++) {
					hid_usage_item = &item_list_ptrs->usageItemList[usage_index + j];
					if (hid_usage_item->usage == USAGE_DIRECTION_X) {
						pointer_x_index = j;
						pointer_x_min = report_item->globals.logicalMinimum;
						pointer_x_max = report_item->globals.logicalMaximum;
					} else if (hid_usage_item->usage == USAGE_DIRECTION_Y) {
						pointer_y_index = j;
						pointer_y_min = report_item->globals.logicalMinimum;
						pointer_y_max = report_item->globals.logicalMaximum;
					} else if (hid_usage_item->usage == USAGE_HAT_SWITCH) {
						hat_switch_index = j;
					}
				}
				report_index = report_item->globals.reportIndex;
				report_id = report_item->globals.reportID;
				pointer_details.reportLength = (item_list_ptrs->reportList[report_index].inputBits + 7) / 8;
				pointer_details.reportID = (BYTE) report_item->globals.reportID;
				pointer_details.bitOffset = (BYTE) report_item->startBit;
				pointer_details.bitLength = (BYTE) report_item->globals.reportsize;
				pointer_details.count = (BYTE) report_item->globals.reportCount;
				pointer_present = 1;
			}
		} else {
			report_item->globals.usagePage;
		}
	}

	if (device_rpt_info->reports > 0) {
		input_report.report_id = report_id;
		input_report.size = (item_list_ptrs->reportList[report_index].inputBits + 7) / 8;
		input_report.poll_rate = device_rpt_info->reportPollingRate;
		status = TRUE;
	}

	return status;
}

BOOL USB_ApplicationEventHandler(BYTE address, USB_EVENT event, void *data, DWORD size)
{
	int current;

	switch (event) {
		case EVENT_VBUS_REQUEST_POWER:
			// The data pointer points to a byte that represents the amount of power
			// requested in mA, divided by two. If the device wants too much power,
			// we reject it.
			current = ((USB_VBUS_POWER_EVENT_DATA *) data)->current;
			if (current <= (MAX_ALLOWED_CURRENT / 2)) {
				return TRUE;
			}
			return FALSE;

		case EVENT_VBUS_RELEASE_POWER:
			// Turn off Vbus power.

			// This means that the device was removed
			device_attached = FALSE;
			return TRUE;

		case EVENT_HUB_ATTACH:
			return TRUE;

		case EVENT_UNSUPPORTED_DEVICE:
			return TRUE;

		case EVENT_CANNOT_ENUMERATE:
			return TRUE;

		case EVENT_CLIENT_INIT_ERROR:
			return TRUE;

		case EVENT_OUT_OF_MEMORY:
			return TRUE;

		case EVENT_UNSPECIFIED_ERROR:	// This should never be generated.
			return TRUE;

		case EVENT_DETACH:
			device_attached = FALSE;
			return TRUE;

		case EVENT_HID_RPT_DESC_PARSED:
			return USB_HID_DataCollectionHandler();

		default:
			return FALSE;
	}
}

void __ISR(_TIMER_4_VECTOR, IPL3SOFT) isr_timer4()
{
	IFS0bits.T4IF = 0;

	if (state == HID_LOOP) {
		state = HID_GET_REPORT;
	}
}

void start_delay_ms(int ms)
{
	delay_ms = ms + 1;

	// initialize Timer5
	PR5 = PBCLK / 256 / 1000;	// 1 ms interval
	T5CONbits.TCKPS = 7;		// 1:256
	T5CONbits.ON = 1;
	IPC5bits.T5IP = 2;
	IPC5bits.T5IS = 0;
	IEC0bits.T5IE = 1;
}

void __ISR(_TIMER_5_VECTOR, IPL2SOFT) isr_timer5()
{
	IFS0bits.T5IF = 0;

	if (--delay_ms == 0) {
		T5CONbits.ON = 0;
	}
}

int handle_msd_insertion()
{
	USB_DEVICE_DESCRIPTOR *device_descriptor;
	int result;

	LAT_LED2 = 1;

	u1tx_str("I:MSD detected ");
	device_descriptor = (USB_DEVICE_DESCRIPTOR *) USBHostGetDeviceDescriptor(USB_SINGLE_DEVICE_ADDRESS);
	u1tx_uint16(device_descriptor->idVendor);
	u1tx(':');
	u1tx_uint16(device_descriptor->idProduct);
	u1tx_str("\r\n");

	// try to load config when the attached device is in the right format
	if (FSInit()) {
		u1tx_str("I:loading config\r\n");
		result = load_config();
		if (result) {
			u1tx_str("I:failure\r\n");
		} else {
			u1tx_str("I:success\r\n");
			LAT_LED2 = 0;
		}
	} else {
		error_code = MSD_MOUNT;
		u1tx_str("E:mount:");
		u1tx_int(FSerror());
		u1tx_str("\r\n");
	}

	return error_code;
}

void handle_hid_insertion()
{
	USB_DEVICE_DESCRIPTOR *device_descriptor;

	hid_interface_number = USBHostHID_ApiGetCurrentInterfaceNum();
	device_descriptor = (USB_DEVICE_DESCRIPTOR *) USBHostGetDeviceDescriptor(USB_SINGLE_DEVICE_ADDRESS);
	u1tx_str("I:VID:PID ");
	u1tx_uint16(device_descriptor->idVendor);
	u1tx(':');
	u1tx_uint16(device_descriptor->idProduct);
	u1tx_str("\r\n");
	find_joy_description(device_descriptor->idVendor, device_descriptor->idProduct);
}

void handle_hid_report()
{
	int trigger;
	int forward;
	int back;
	int left;
	int right;
	int i;
	BYTE hat;
	JOY_CONTROL *joy_control;
	UINT8 kind;
	UINT8 op;
	UINT8 value;
	UINT8 data;
	int value_match;

	trigger = 0;
	forward = 0;
	back = 0;
	left = 0;
	right = 0;

	USBHostHID_ApiImportData(input_report.data, input_report.size, buttons, &buttons_details);

	// print & save HID input report when different from previous one
	for (i = 0; i < input_report.size && i < sizeof(previous_input_report_data); i++) {
		if (previous_input_report_data[i] != input_report.data[i]) {
			u1tx_str("I:report:");
			u1tx_uint16(input_report.report_id);
			u1tx('[');
			for (i = 0; i < input_report.size && i < sizeof(previous_input_report_data); i++) {
				if (i > 0) {
					u1tx(' ');
				}
				u1tx_uint8(input_report.data[i]);
				previous_input_report_data[i] = input_report.data[i];
			}
			u1tx_str("]\r\n");
			break;
		}
	}

	if (joy_description == NULL) {
		for (i = 0; i < total_buttons; i++) {
			if (buttons[i]) {
				trigger = 1;
			}
		}
	}

	if (joy_description != NULL) {		// if 'driver' for the connected device was found...
		for (i = 0; i < n_joy_controls; i++) {
			joy_control = (JOY_CONTROL *) &(joy_description->controls[i]);
			op = joy_control->kind_operation & JOY_CONTROL_OP_MASK;
			value = joy_control->value;
			data = input_report.data[joy_control->offset] & joy_control->mask;
			value_match = 0;
			if (op == JOY_CONTROL_OP_EQU) {
				value_match = data == value;
			} else if (op == JOY_CONTROL_OP_NEQ) {
				value_match = data != value;
			} else if (op == JOY_CONTROL_OP_LT) {
				value_match = data < value;
			} else if (op == JOY_CONTROL_OP_GT) {
				value_match = data > value;
			}
			kind = joy_control->kind_operation & JOY_CONTROL_KIND_MASK;
			if (value_match) {
				if (kind == JOY_CONTROL_KIND_LEFT) {
					left = 1;
				} else if (kind == JOY_CONTROL_KIND_RIGHT) {
					right = 1;
				} else if (kind == JOY_CONTROL_KIND_FWD) {
					forward = 1;
				} else if (kind == JOY_CONTROL_KIND_BACK) {
					back = 1;
				} else if (kind == JOY_CONTROL_KIND_TRG) {
					trigger = 1;
				}
			}
		}
	} else if (pointer_present) {
		USBHostHID_ApiImportData(input_report.data, input_report.size, pointer, &pointer_details);
		if (pointer_x_index > -1) {
			if (pointer[pointer_x_index] == pointer_x_min) {
				left = 1;
			} else if (pointer[pointer_x_index] == pointer_x_max) {
				right = 1;
			}
		}
		if (pointer_y_index > -1) {
			if (pointer[pointer_y_index] == pointer_y_min) {
				forward = 1;
			} else if (pointer[pointer_y_index] == pointer_y_max) {
				back = 1;
			}
		}
		if (hat_switch_index > -1) {
			hat = pointer[hat_switch_index];
			if (hat & 0x01) {
				forward = 1;
			}
			if (hat & 0x02) {
				right = 1;
			}
			if (hat & 0x04) {
				back = 1;
			}
			if (hat & 0x08) {
				left = 1;
			}
		}
	}

	// set output pins according to controls' states
	if (trigger) {
		LAT_TRIGGER = 1;
	} else {
		LAT_TRIGGER = 0;
	}
	if (forward) {
		LAT_FORWARD = 1;
		LAT_BACK = 0;
	} else if (back) {
		LAT_FORWARD = 0;
		LAT_BACK = 1;
	} else {
		LAT_FORWARD = 0;
		LAT_BACK = 0;
	}
	if (left) {
		LAT_LEFT = 1;
		LAT_RIGHT = 0;
	} else if (right) {
		LAT_LEFT = 0;
		LAT_RIGHT = 1;
	} else {
		LAT_LEFT = 0;
		LAT_RIGHT = 0;
	}
}

void software_reset()
{
	volatile unsigned int dummy;

	INTDisableInterrupts();
	SYSKEY = 0x00000000;			// force lock
	SYSKEY = 0xaa996655;			// write key1
	SYSKEY = 0x556699aa;			// write key2
	RSWRSTSET = 1;					// arm reset
	dummy = RSWRST;					// trigger reset
	while (1);						// wait for reset
}

void main()
{
	BYTE error_counter = 0;
	BYTE error_driver;
	BYTE number_of_bytes_received;

	ANSELA = 0;
	ANSELB = 0;
#ifdef ANSELC
	ANSELC = 0;
#endif

	joy_description = NULL;

	// initialize LED pins
	LAT_LED1 = 1;
	TRIS_LED1 = 0;
	LAT_LED2 = 1;
	TRIS_LED2 = 0;

	// initialize joystick pins
	LAT_FORWARD = 0;
	LAT_TRIGGER = 0;
	LAT_BACK = 0;
	LAT_LEFT = 0;
	LAT_RIGHT = 0;
	TRIS_FORWARD = 0;
	TRIS_TRIGGER = 0;
	TRIS_BACK = 0;
	TRIS_LEFT = 0;
	TRIS_RIGHT = 0;

	// initialize UART1
	u1_init();

	// initialize Timer4
	PR4 = PBCLK / 64 / 20 - 1;	// interrupt interval: 50 ms
	T4CONbits.ON = 1;
	T4CONbits.TCKPS = 6;		// 1:64
	IPC4bits.T4IP = 3;
	IPC4bits.T4IS = 0;
	IEC0bits.T4IE = 1;

	// enable interrupts
	INTEnableSystemMultiVectoredInt();
	
	// initialize USB
	USBInitialize(0);

	u1tx_str("I:reset\r\n");

	while (1) {
		USBTasks();

		switch (state) {
			case IDLE:
				if (USBHostMSDSCSIMediaDetect()) {			// if pendrive is plugged in ...
					device_attached = TRUE;
					LAT_LED1 = 0;
					if (handle_msd_insertion() == OK) {
						LAT_LED1 = 0;
						state = MSD_LOOP;
					} else {
						software_reset();					// workaround for media-not-ready MDD errors
					}
				} else if (USBHostHID_ApiDeviceDetect()) {	// if HID is plugged in ...
					device_attached = TRUE;
					LAT_LED1 = 0;
					handle_hid_insertion();
					state = HID_LOOP;
				}
				break;

			case MSD_LOOP:
				if (!(device_attached == TRUE)) {
					state = IDLE;
					LAT_LED1 = 1;
					LAT_LED2 = 1;
				}
				break;

			case HID_LOOP:
				if (!(device_attached == TRUE)) {
					state = IDLE;
					LAT_LED1 = 1;
					LAT_LED2 = 1;
				}
				break;

			case HID_GET_REPORT:
				if (!USBHostHID_ApiGetReport(input_report.report_id, 0, input_report.size, input_report.data)) {
					state = HID_GET_REPORT_WAIT;
				}
				break;

			case HID_GET_REPORT_WAIT:
				if (USBHostHID_ApiTransferIsComplete(&error_driver, &number_of_bytes_received)) {
					if (error_driver || (number_of_bytes_received != input_report.size)) {
						error_counter++;
						if (error_driver > 10) {
							state = ERROR;
						}
					} else {
						error_counter = 0;
						handle_hid_report();
					}
					state = HID_LOOP;
				}
				break;

			case ERROR:
				state = HID_LOOP;
				break;
		}
	}
}
