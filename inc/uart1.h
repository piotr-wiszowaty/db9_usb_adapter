#ifndef _UART1_H
#define _UART1_H

#define BAUD        38400
#define BUFFER_SIZE 2048

#define U1TXIF_BIT	(1 << _IFS1_U1TXIF_POSITION)
#define U1TXIE_BIT	(1 << _IEC1_U1TXIE_POSITION)

void u1_init();
void u1tx(char c);
void u1tx_uint8(unsigned char i);
void u1tx_int(int n);
void u1tx_uint16(unsigned short i);
void u1tx_uint32(unsigned int i);
void u1tx_str(char *s);
int u1rx();

#endif

