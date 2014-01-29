#ifndef _USB_CONFIG_H_
#define _USB_CONFIG_H

#include <p32xxxx.h>
#include "plib.h"

#define _USB_CONFIG_VERSION_MAJOR 1
#define _USB_CONFIG_VERSION_MINOR 0
#define _USB_CONFIG_VERSION_DOT   4
#define _USB_CONFIG_VERSION_BUILD 0

// Supported USB Configurations

#define USB_SUPPORT_HOST

// Hardware Configuration

#define USB_PING_PONG_MODE  USB_PING_PONG__FULL_PING_PONG

// Host Configuration

#define NUM_TPL_ENTRIES 2
#define USB_NUM_CONTROL_NAKS 200
#define USB_SUPPORT_INTERRUPT_TRANSFERS
#define USB_NUM_INTERRUPT_NAKS 20
#define USB_INITIAL_VBUS_CURRENT (100/2)
#define USB_INSERT_TIME (250+1)
#define USB_SUPPORT_BULK_TRANSFERS
#define USB_NUM_BULK_NAKS 200
#define USB_HOST_APP_EVENT_HANDLER USB_ApplicationEventHandler

// Host HID Client Driver Configuration

#define USB_MAX_HID_DEVICES 1

// Host Mass Storage Client Driver Configuration

//#define USB_ENABLE_TRANSFER_EVENT

#define USB_MAX_MASS_STORAGE_DEVICES 1

#define USBTasks()                  \
    {                               \
        USBHostTasks();             \
	USBHostMSDTasks();          \
        USBHostHIDTasks();          \
    }

#define USBInitialize(x)            \
    {                               \
        USBHostInit(x);             \
    }

#endif

