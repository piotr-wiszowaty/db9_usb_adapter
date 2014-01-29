#include "GenericTypeDefs.h"
#include "HardwareProfile.h"
#include "USB/usb.h"
#include "USB/usb_host_hid.h"
#include "USB/usb_host_msd.h"
#include "USB/usb_host_msd_scsi.h"

CLIENT_DRIVER_TABLE usbMediaInterfaceTable = {                                           
    USBHostMSDSCSIInitialize,
    USBHostMSDSCSIEventHandler,
    0
};

CLIENT_DRIVER_TABLE usbClientDrvTable[] = {                                        
    {USBHostHIDInitialize, USBHostHIDEventHandler, 0},
    {USBHostMSDInitialize, USBHostMSDEventHandler, 0}
};

USB_TPL usbTPL[] = {
    {INIT_CL_SC_P( 0x03ul, 0x0ul, 0x0ul ), 0, 0, {TPL_CLASS_DRV}}, // HID Class
    {INIT_CL_SC_P( 8ul, 6ul, 0x50ul ), 0, 1, {TPL_CLASS_DRV}},     // MSD Class
    {INIT_CL_SC_P( 8ul, 5ul, 0x50ul ), 0, 1, {TPL_CLASS_DRV}}      // MSD Class
};

