/* dwr512_phonemanager
**
** Author: 	Giuseppe Lippolis
**
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License v2.0.
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <syslog.h>

typedef struct {
	FILE 		*sf;
	pthread_mutex_t SpiMutex;
} slic_dev_str_type;
slic_dev_str_type	slic_dev_str;

int InitSpi(char *spi_device) {

	slic_dev_str.sf=fopen(spi_device, "r+b");
	syslog(LOG_DEBUG, "opening slic (%s)\n", spi_device);
	if (slic_dev_str.sf == NULL) {
		syslog(LOG_ERR, "cannot open slic (%s)\n", spi_device);
		return EXIT_FAILURE;
	}
	setvbuf ( slic_dev_str.sf , NULL , _IONBF , 1024 );
	syslog(LOG_DEBUG, "init spi slic");
	pthread_mutex_init(&(slic_dev_str.SpiMutex), NULL);
}

void DestroySpi() {
	pthread_mutex_lock (&(slic_dev_str.SpiMutex));
	fclose(slic_dev_str.sf);
	pthread_mutex_unlock (&(slic_dev_str.SpiMutex));
	pthread_mutex_destroy(&(slic_dev_str.SpiMutex));
}


void mvTdmSpiWrite(unsigned char address, unsigned char data) {
	unsigned char cmd;

	cmd = address;
	pthread_mutex_lock (&(slic_dev_str.SpiMutex));
	fputc (cmd, slic_dev_str.sf);
	cmd = data;
	fputc (cmd, slic_dev_str.sf);
	pthread_mutex_unlock (&(slic_dev_str.SpiMutex));
}

void mvTdmSpiRead(unsigned char address, unsigned char *data) {
	unsigned char cmd, ret;

	cmd = 0x80|address;
	pthread_mutex_lock (&(slic_dev_str.SpiMutex));
	fputc (cmd, slic_dev_str.sf);
	*data=(unsigned char)(fgetc(slic_dev_str.sf));
	pthread_mutex_unlock (&(slic_dev_str.SpiMutex));

}


