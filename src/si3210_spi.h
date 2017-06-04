/* dwr512_phonemanager
**
** Author: 	Giuseppe Lippolis
**
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License v2.0.
*/

int InitSpi();
void DestroySpi();
void mvTdmSpiWrite(unsigned char address, unsigned char data);
void mvTdmSpiRead(unsigned char address, unsigned char *data);