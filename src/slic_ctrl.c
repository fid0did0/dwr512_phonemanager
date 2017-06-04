/* dwr512_phonemanager
**
** Author: 	Giuseppe Lippolis
**
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License v2.0.
*/

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <pthread.h>
#include "si3210_spi.h"
#include "slic_ctrl.h"
#include "proslic.h"

// This function initializes the event queue
void initEventQueue(){
	pthread_mutex_init(&SlicEvntQueueLenMutex, NULL);
	pthread_mutex_lock (&SlicEvntQueueLenMutex);
	struct SlicEvntQueue *tmp = (struct SlicEvntQueue*)malloc(sizeof(struct SlicEvntQueue));
	SlicEvntQueueLen  = 0;
	HeadSlicEvntQueue = tmp;
	EndSlicEvntQueue = HeadSlicEvntQueue;
	pthread_mutex_unlock (&SlicEvntQueueLenMutex);
}

// This function adds an event to the event queue
void addEvent(enum slic_event evt){
	pthread_mutex_lock (&SlicEvntQueueLenMutex);
	struct SlicEvntQueue *newElement = (struct SlicEvntQueue*)malloc(sizeof(struct SlicEvntQueue));
	SlicEvntQueueLen++;
	EndSlicEvntQueue->data = evt;
	EndSlicEvntQueue->next = newElement;
	EndSlicEvntQueue = newElement;
	pthread_mutex_unlock (&SlicEvntQueueLenMutex);
}

// This function gets the event from the event queue
enum slic_event getSlicEvent(){
	pthread_mutex_lock (&SlicEvntQueueLenMutex);
	enum slic_event ret = HeadSlicEvntQueue->data;
	struct SlicEvntQueue *tmp = HeadSlicEvntQueue;
	SlicEvntQueueLen--;
	HeadSlicEvntQueue = HeadSlicEvntQueue->next;
	free(tmp);  // free allocated memory
	pthread_mutex_unlock (&SlicEvntQueueLenMutex);
	return ret;
}

// Slic Monitor Thread
void *SlicMon(void *threadarg) {
	char		str[64];
	unsigned char	IntSt2, IntSt3;
	int 		nfds = 1;
	unsigned char	dtmf_digit, hookSt;

	while (!stop_thread) {
		lseek(fdset[0].fd, 0, SEEK_SET);
		read(fdset[0].fd, str, 64);
		poll(fdset, nfds, -1);	// attesa indefinita richiede int event x chiudere il thread !!!
		IntSt2 = readDirectReg (19);
		IntSt3 = readDirectReg (20);
		if (IntSt2&0x02) {
			hookSt=readDirectReg(68);
			if (hookSt==0x04)
				addEvent(onhook);
			else
				addEvent(offhook);
		};
		if (IntSt3&0x01) {
			dtmf_digit=readDirectReg(24)&0x0f;
			switch(dtmf_digit) {
				case 1 : addEvent(dtmf1); break;
				case 2 : addEvent(dtmf2); break;
				case 3 : addEvent(dtmf3); break;
				case 4 : addEvent(dtmf4); break;
				case 5 : addEvent(dtmf5); break;
				case 6 : addEvent(dtmf6); break;
				case 7 : addEvent(dtmf7); break;
				case 8 : addEvent(dtmf8); break;
				case 9 : addEvent(dtmf9); break;
				case 10 : addEvent(dtmf0); break;
				case 11 : addEvent(dtmf_star); break;
				case 12 : addEvent(dtmf_hash); break;
			}
		};
		clearInterrupts();
	}
	pthread_exit(NULL);
	return NULL;
}

int slic_init(char *spi_device) {

	char		int_device[] = "/sys/class/gpio/slic_int/value";
	char		int_edge[] = "/sys/class/gpio/slic_int/edge";
	int		gpio_fd, rc;
	FILE		*edgef;

	InitSpi(spi_device);

	syslog(LOG_DEBUG, "init slic");
	//device_identification();
	version();

	syslog(LOG_DEBUG, "starting slic");
	slicStart();
	enablePCMhighway();
	clearInterrupts();
	goActive();

	syslog(LOG_DEBUG, "setting gpio slic_int");
	edgef=fopen(int_edge, "w");
	fprintf(edgef, "rising\n");
	fclose(edgef);
	gpio_fd=open(int_device, O_RDONLY);
	memset((void*)fdset, 0, sizeof(fdset));
	fdset[0].fd = gpio_fd;
	fdset[0].events = POLLPRI;

	syslog(LOG_DEBUG, "setting slic event queue");
	initEventQueue();

	syslog(LOG_DEBUG, "starting slic monitor thread");
	rc = pthread_create(&slic_mon_thr, NULL, SlicMon, NULL);
	if (rc){
		syslog(LOG_ERR, "return code from slic pthread_create() is %d\n", rc);
		stop_thread=1;
	}
}

void powerDown() {
	writeDirectReg(8, 0x02);
	writeDirectReg(11, 0x33);
	writeDirectReg(64, 0x00);
	writeDirectReg(14, 0x10);
}

int slic_close() {

	pthread_join(slic_mon_thr, NULL);
	pthread_mutex_destroy(&SlicEvntQueueLenMutex);
	close(fdset[0].fd);
	disablePCMhighway();
	powerDown();
	DestroySpi();
}


/*
void PrintSlicEvnt(enum slic_event evt) {
	switch(evt) {
		case onhook : printf("evnt: onhook\n"); break;
		case offhook : printf("evnt: offhook\n"); break;
		case dtmf1 : printf("evnt: dtmf1\n"); break;
		case dtmf2 : printf("evnt: dtmf2\n"); break;
		case dtmf3 : printf("evnt: dtmf3\n"); break;
		case dtmf4 : printf("evnt: dtmf4\n"); break;
		case dtmf5 : printf("evnt: dtmf5\n"); break;
		case dtmf6 : printf("evnt: dtmf6\n"); break;
		case dtmf7 : printf("evnt: dtmf7\n"); break;
		case dtmf8 : printf("evnt: dtmf8\n"); break;
		case dtmf9 : printf("evnt: dtmf9\n"); break;
		case dtmf0 : printf("evnt: dtmf0\n"); break;
		case dtmf_star : printf("evnt: dtmf_star\n"); break;
		case dtmf_hash : printf("evnt: dtmf_hash\n"); break;
	}
}
*/
