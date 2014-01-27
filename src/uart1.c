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
#include "HardwareProfile.h"
#include "uart1.h"

char tx_buffer[BUFFER_SIZE];
volatile int tx_buffer_wi = 0;
volatile int tx_buffer_ri = 0;

void u1_init()
{
	SETUP_UART1_PINS;
	U1MODEbits.ON = 1;
	U1STAbits.URXEN = 1;
	U1STAbits.UTXEN = 1;
	U1BRG = (PBCLK / (16 * BAUD)) - 1;
	IPC8SET = (0 << _IPC8_U1IS_POSITION) | (3 << _IPC8_U1IP_POSITION);
}

void __ISR(_UART1_VECTOR, IPL3SOFT) isr_uart1()
{
	IFS1CLR = U1TXIF_BIT;

	while (!U1STAbits.UTXBF && tx_buffer_ri != tx_buffer_wi) {
		U1TXREG = tx_buffer[(BUFFER_SIZE - 1) & tx_buffer_ri++];
	}
	if (tx_buffer_ri == tx_buffer_wi) {
		IEC1CLR = U1TXIE_BIT;
	}
}

void u1tx(char c)
{
	tx_buffer[(BUFFER_SIZE - 1) & tx_buffer_wi++] = c;
	IEC1SET = U1TXIE_BIT;
}

char toasciihex(unsigned char i)
{
	if (i > 9) {
		return i + 'A' - 10;
	} else {
		return i + '0';
	}
}

void u1tx_uint8(unsigned char i)
{
	u1tx(toasciihex((i >> 4) & 0x0f));
	u1tx(toasciihex(i & 0x0f));
}

void u1tx_int(int n)
{
	int i = 0;
	char buf[8];

	do {
		buf[i++] = (n % 10) + '0';
		n /= 10;
	} while (n);

	while (i > 0) {
		u1tx(buf[--i]);
	}
}

void u1tx_uint16(unsigned short i)
{
	u1tx_uint8(i >> 8);
	u1tx_uint8(i & 0xff);
}

void u1tx_uint32(unsigned int i)
{
	u1tx_uint16(i >> 16);
	u1tx_uint16(i & 0xffff);
}

void u1tx_str(char *s)
{
	while (*s) {
		u1tx(*s++);
	}
}

int u1rx()
{
	if (U1STAbits.URXDA) {
		return 0xff & U1RXREG;
	} else {
		return -1;
	}
}
