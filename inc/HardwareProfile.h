#ifndef _HARDWARE_PROFILE_H_
#define _HARDWARE_PROFILE_H_

//#define DEBUG_MODE

#define PBCLK	40000000

// PORTA
#define LAT_LED1	LATAbits.LATA9
#define TRIS_LED1	TRISAbits.TRISA9

// PORTB
#define LAT_RIGHT	LATBbits.LATB2
#define TRIS_RIGHT	TRISBbits.TRISB2
#define LAT_LEFT	LATBbits.LATB3
#define TRIS_LEFT	TRISBbits.TRISB3

// PORTC
#define LAT_BACK	LATCbits.LATC0
#define TRIS_BACK	TRISCbits.TRISC0
#define LAT_TRIGGER	LATCbits.LATC1
#define TRIS_TRIGGER	TRISCbits.TRISC1
#define LAT_FORWARD	LATCbits.LATC2
#define TRIS_FORWARD	TRISCbits.TRISC2
#define LAT_LED2	LATCbits.LATC4
#define TRIS_LED2	TRISCbits.TRISC4

// U1RX - RPC3, U1TX - RPB4
#define SETUP_UART1_PINS \
        U1RXR = 7; \
        RPB4R = 1

#define USB_A0_SILICON_WORK_AROUND

// Various clock values
#define GetSystemClock()            40000000UL
#define GetPeripheralClock()        40000000UL
#define GetInstructionClock()       (GetSystemClock() / 2)

// Clock values
#define MILLISECONDS_PER_TICK       10                  // -0.000% error
#define TIMER_PRESCALER             TIMER_PRESCALER_8   // At 60MHz
#define TIMER_PERIOD                37500               // At 60MHz

#define USE_USB_INTERFACE               // USB host MSD library

#define INPUT_PIN   1
#define OUTPUT_PIN  0

#ifdef DEBUG_MODE
#define DEBUG_PutChar(c) u1tx(c)
#define DEBUG_PutString(s) u1tx_str(s)
#define DEBUG_PutHexUINT8(n) u1tx_uint8(n)
#define DEBUG_PutHexUINT16(n) u1tx_uint16(n)
#define UART2PrintString(s) u1tx_str(s)
#define UART2PutChar(c) u1tx(c)
#define UART2PutHex(n) u1tx_uint8(n)
#define UART2PutDec(n) u1tx_int(n)
#define UART2PutHexDWord(n) u1tx_uint32(n)
#endif

#endif
