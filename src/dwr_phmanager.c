/* dwr512_phonemanager
**
** Author: 	Giuseppe Lippolis
**
** 04.06.2017
** Phone manager to handle the 3G modem and the proslic telephone
** interface embedded in the D-link dwr 512 router.
**
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License v2.0.
*/

#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include "proslic.h"
#include "modem_ctrl.h"
#include "slic_ctrl.h"

#define TIMEOUT_TC 40;

//#define debug_print(...)	syslog(LOG_DEBUG, __VA_ARGS__)
#define debug_print(...)	printf(__VA_ARGS__)

enum PhoneStateType {idle, offhook_idle, get_digitstring, IncomingCall1,
			IncomingCall2, IncomingCall3, ActiveCall,
			Offhook_idle, Offhook_timeout,
			GetDialStr, AnalyzeDialStr, Dial,
			not_available
		} PhoneState;

enum timout_stateType {no_active, timeout, no_timeout} timout_state;
unsigned int 	TimeoutCnt;
pthread_mutex_t TimeoutCntMutex;
pthread_t 	timeout_thr;

char	 	DialStr[64];

int usleep(unsigned useconds) {
	struct timespec tv = {
		.tv_sec = useconds/1000000,
		.tv_nsec = (useconds%1000000)*1000
	};
	return nanosleep(&tv, &tv);
}


int stop_thread = 0;
void intHandler(int dummy) {
    stop_thread = 1;
}

void *TimeoutThread(void *threadarg) {
	while (!stop_thread) {
		if(timout_state==no_timeout) {
			pthread_mutex_lock(&TimeoutCntMutex);
			TimeoutCnt--;
			if (!TimeoutCnt) {
				timout_state=timeout;
				//printf("Timeout Event\n");
			}
			pthread_mutex_unlock(&TimeoutCntMutex);
		}
		usleep(100000);
	}
}
void ResetTimeout(unsigned int timeout_tc) {
	//printf("ResetTimeout\n");
	pthread_mutex_lock(&TimeoutCntMutex);
	timout_state=no_timeout;
	if (timeout_tc)
		TimeoutCnt=timeout_tc;
	else
		TimeoutCnt=TIMEOUT_TC;
	pthread_mutex_unlock(&TimeoutCntMutex);
}
void DisableTimeout() {
	//printf("DisableTimeout\n");
	pthread_mutex_lock(&TimeoutCntMutex);
	timout_state=no_active;
	TimeoutCnt=0;
	pthread_mutex_unlock(&TimeoutCntMutex);
}
void SlicStateTransition(modem_dev_str *pmodem_dev) {
	struct ModemEventQueue	*np;
	enum slic_event 	slic_evnt;
	enum ModemEventType	modem_evnt;
	unsigned int 		auxcnt;
	char 			tmpstr[64];

	modem_evnt=no_modem_event;
	slic_evnt=no_slic_event;
	np = STAILQ_FIRST(&ModemEventQueueHead);
	if (np!=NULL) {
		modem_evnt=np->evnt;
		debug_print("gsm modem event: %d | %s |\n", np->evnt, np->msg);
	}
	if (SlicEvntQueueLen>0)
		slic_evnt=getSlicEvent();

	switch(PhoneState) {
		case idle :
			switch(slic_evnt) {
				case offhook :
					dialTone();
					ResetTimeout(0);
					PhoneState=Offhook_idle;
					debug_print("PhoneState-> Offhook_idle\n");
				break;
			}
			switch(modem_evnt) {
				case ringing :
					activateRinging();
					PhoneState=IncomingCall1;
					ResetTimeout(0);
					debug_print("PhoneState-> IncomingCall1\n");
				break;
			}
		break; //idle
		case IncomingCall1 :	// start ring
			switch(modem_evnt) {
				if (timout_state==timeout) {
					syslog(LOG_NOTICE, "incoming call from unid number");
					ResetTimeout(0);
					PhoneState=IncomingCall2;
					debug_print("PhoneState-> IncomingCall2\n");
				}
				case callid :
					syslog(LOG_NOTICE, "incoming call from %s", np->PrivData0);
					ResetTimeout(0);
					PhoneState=IncomingCall2;
					debug_print("PhoneState-> IncomingCall2\n");
				break;
			}
		break; //IncomingCall1
		case IncomingCall2 :	// wait for offhook and answer modem
			if (timout_state==timeout) {
				stopRinging();
				PutModemCmd(pmodem_dev, "ATH\n");
				DisableTimeout();
				PhoneState=idle;
				debug_print("PhoneState-> idle\n");
				syslog(LOG_NOTICE, "Lost incoming call");
			}
			switch(modem_evnt) {
				case ringing : ResetTimeout(0);	break;
			}
			switch(slic_evnt) {
				case offhook :
					PutModemCmd(pmodem_dev, "ATA\n");
					PhoneState=IncomingCall3;
					debug_print("PhoneState-> IncomingCall3\n");
				break;
			}
		break; //IncomingCall2
		case IncomingCall3 :	// go active
			switch(modem_evnt) {
				case ok :
					ResetTimeout(30);
					PhoneState=ActiveCall;
					debug_print("PhoneState-> ActiveCall\n");
				break;
				case error:
					PhoneState=IncomingCall2;
					debug_print("PhoneState-> IncomingCall2\n");
				break;
			}
		break; //IncomingCall3
		case ActiveCall :	// active call
			if (timout_state==timeout) {
				ResetTimeout(30);
				PutModemCmd(pmodem_dev, "AT+CLCC\n");
			}
			switch(modem_evnt) {
				case callstat :
					debug_print("call status %s\n", np->PrivData0);
					/*if (!strcmp(np->PrivData0,"?")) {
						PutModemCmd(pmodem_dev, "ATH\n");
						busyTone();
						DisableTimeout();
						PhoneState=Offhook_timeout;
						debug_print("PhoneState-> Offhook_timeout\n");
					}*/
				break;
				case nocarrier :
					PutModemCmd(pmodem_dev, "ATH\n");
					busyTone();
					DisableTimeout();
					PhoneState=Offhook_timeout;
					debug_print("PhoneState-> Offhook_timeout\n");
					syslog(LOG_NOTICE, "active call ended");
				break;
			}
			switch(slic_evnt) {
				case onhook :
					PutModemCmd(pmodem_dev, "ATH\n");
					DisableTimeout();
					PhoneState=idle;
					debug_print("PhoneState-> idle\n");
					syslog(LOG_NOTICE, "active call ended");
				break;
			}
		break; //ActiveCall
		case Offhook_idle :	// Offhook_idle
			if (timout_state==timeout) {
				busyTone();
				DisableTimeout();
				PhoneState=Offhook_timeout;
				debug_print("PhoneState-> Offhook_timeout\n");
			}
			switch(slic_evnt) {
				case dtmf0 : strncpy(DialStr,"0",63); break;
				case dtmf1 : strncpy(DialStr,"1",63); break;
				case dtmf2 : strncpy(DialStr,"2",63); break;
				case dtmf3 : strncpy(DialStr,"3",63); break;
				case dtmf4 : strncpy(DialStr,"4",63); break;
				case dtmf5 : strncpy(DialStr,"5",63); break;
				case dtmf6 : strncpy(DialStr,"6",63); break;
				case dtmf7 : strncpy(DialStr,"7",63); break;
				case dtmf8 : strncpy(DialStr,"8",63); break;
				case dtmf9 : strncpy(DialStr,"9",63); break;
				case dtmf_star : strncpy(DialStr,"*",63); break;
				case dtmf_hash : strncpy(DialStr,"#",63); break;
				case onhook :
					stopTone();
					PhoneState=idle;
					debug_print("PhoneState-> idle\n");
				break;
			}
			if ((slic_evnt==dtmf0)||(slic_evnt==dtmf1)||(slic_evnt==dtmf2)||
				(slic_evnt==dtmf3)||(slic_evnt==dtmf4)||(slic_evnt==dtmf5)||
				(slic_evnt==dtmf6)||(slic_evnt==dtmf7)||(slic_evnt==dtmf8)||
				(slic_evnt==dtmf9)||(slic_evnt==dtmf_star)||(slic_evnt==dtmf_hash)) {
					stopTone();
					ResetTimeout(0);
					PhoneState=GetDialStr;
					debug_print("PhoneState-> GetDialStr\n");
			}
		break; //Offhook_idle
		case GetDialStr :	// GetDialStr
			if (timout_state==timeout) {
				DisableTimeout();
				PhoneState=AnalyzeDialStr;
				debug_print("PhoneState-> AnalyzeDialStr\n");
			}
			switch(slic_evnt) {
				case dtmf0 : strncat(DialStr,"0",63); ResetTimeout(0); break;
				case dtmf1 : strncat(DialStr,"1",63); ResetTimeout(0); break;
				case dtmf2 : strncat(DialStr,"2",63); ResetTimeout(0); break;
				case dtmf3 : strncat(DialStr,"3",63); ResetTimeout(0); break;
				case dtmf4 : strncat(DialStr,"4",63); ResetTimeout(0); break;
				case dtmf5 : strncat(DialStr,"5",63); ResetTimeout(0); break;
				case dtmf6 : strncat(DialStr,"6",63); ResetTimeout(0); break;
				case dtmf7 : strncat(DialStr,"7",63); ResetTimeout(0); break;
				case dtmf8 : strncat(DialStr,"8",63); ResetTimeout(0); break;
				case dtmf9 : strncat(DialStr,"9",63); ResetTimeout(0); break;
				case dtmf_star : strncat(DialStr,"*",63); ResetTimeout(0); break;
				case dtmf_hash : strncat(DialStr,"#",63); ResetTimeout(0); break;
			}
		break; //GetDialStr
		case AnalyzeDialStr :	// AnalyzeDialStr
			debug_print("DialStr: %s\n", DialStr);
			auxcnt=sscanf(DialStr, "00%s", tmpstr);
			if (auxcnt>0)
				sprintf(DialStr, "+%s", tmpstr);
			syslog(LOG_NOTICE, "Dialing %s", DialStr);
			sprintf(tmpstr, "ATD%s;\n", DialStr);
			PutModemCmd(pmodem_dev, tmpstr);
			ResetTimeout(20);
			auxcnt=15;
			PhoneState=Dial;
			debug_print("PhoneState-> Dial\n");
		break; //AnalyzeDialStr
		case Dial :	// Dial
			if (timout_state==timeout) {
				if (auxcnt) {
					ResetTimeout(20);
					PutModemCmd(pmodem_dev, "AT+CLCC\n");
				} else {
					PutModemCmd(pmodem_dev, "ATH\n");
					busyTone();
					DisableTimeout();
					PhoneState=Offhook_timeout;
					debug_print("PhoneState-> Offhook_timeout\n");
				}
				auxcnt--;
			}
			switch(modem_evnt) {
				case callstat :
					//syslog(LOG_INFO, "incoming call from %s", np->PrivData0);
					debug_print("call status %s\n", np->PrivData0);
					//if (!strcmp(np->PrivData0,"2"))
					if (!strcmp(np->PrivData0,"0")) {
						ResetTimeout(30);
						PhoneState=ActiveCall;
						syslog(LOG_NOTICE, "On call with %s", DialStr);
						debug_print("PhoneState-> ActiveCall\n");
					}
				break;
			}
			switch(slic_evnt) {
				case onhook :
					PutModemCmd(pmodem_dev, "ATH\n");
					stopTone();
					DisableTimeout();
					PhoneState=idle;
					debug_print("PhoneState-> idle\n");
				break;
			}
			//

			//ringBackTone();
		break; //Dial
		case Offhook_timeout :	// Offhook_timeout
			switch(slic_evnt) {
				case onhook :
					stopTone();
					DisableTimeout();
					PutModemCmd(pmodem_dev, "ATH\n");
					PhoneState=idle;
					debug_print("PhoneState-> idle\n");
				break;
			}
		break; //Offhook_timeout


	}

	if (np!=NULL) {
		pthread_mutex_lock (&ModemEventQueueMutex);
		STAILQ_REMOVE_HEAD(&ModemEventQueueHead, entries);
		free(np);
		pthread_mutex_unlock (&ModemEventQueueMutex);
	}
};

int main(int argc, char *argv[]) {
	modem_dev_str		*pmodem_dev;
	struct ModemEventQueue	*np;
	int 			rc;
	int 			debug_lev, debug_mask;
	int			argget, invalid_arg;
	char			spi_dev_arg[16];
	char			spi_device[] = "/dev/spidev1.0";

	debug_lev=2;
	invalid_arg=1;
	if (argc == 1)
		invalid_arg=0;
        if (argc == 2) {
		argget=sscanf(argv[1], "-g%d", &debug_lev);
		if (argget)
			invalid_arg=0;
                else {
			argget=sscanf(argv[1], "/dev/spidev%s", spi_dev_arg);
			if (argget) {
				sprintf(spi_device, "/dev/spidev%s", spi_dev_arg);
				invalid_arg=0;
			}
		}
	}

	if (argc == 3) {
		printf("argv[2] = %s\t", argv[1]);
		argget=sscanf(argv[1], "-g%d", &debug_lev);
		argget+=sscanf(argv[2], "/dev/spidev%s", spi_dev_arg);
		if (argget>1) {
			sprintf(spi_device, "/dev/spidev%s", spi_dev_arg);
			invalid_arg=0;
		}
	}

	printf("debug: %s -g%d %s\n", argv[0], debug_lev, spi_device);

	if (invalid_arg) {
		printf("syntax: %s -g <level>\n", argv[0]);
                return 1;
	}

	debug_mask=LOG_ERR;
	if (debug_lev>0) debug_mask=LOG_WARNING;
	if (debug_lev>1) debug_mask=LOG_NOTICE;
	if (debug_lev>2) debug_mask=LOG_INFO;
	if (debug_lev>3) debug_mask=LOG_DEBUG;
	setlogmask(LOG_UPTO(debug_mask));
	openlog("dwr_phmanager", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);

	slic_init(spi_device);
	syslog(LOG_INFO, "slic_ctrl started");

	pmodem_dev=&modem_dev;
	pmodem_dev->df = open(MODEMDEVICE, O_RDWR|O_NONBLOCK);
	if (pmodem_dev->df <0) {syslog(LOG_ERR, "cannot open the modem");perror(MODEMDEVICE); exit(-1); }
	InitModem(pmodem_dev);
	syslog(LOG_INFO, "modem_ctrl started");

	PhoneState=idle;
	syslog(LOG_DEBUG, "PhoneState-> idle\n");

	pthread_mutex_init(&TimeoutCntMutex, NULL);
	rc = pthread_create(&timeout_thr, NULL, TimeoutThread, NULL);
	if (rc){
		syslog(LOG_ERR, "cannot create the timeout thread (%d)", rc);
		stop_thread=1;
	}
	syslog(LOG_DEBUG, "timeout thread started");

	signal(SIGINT, intHandler);

	np = STAILQ_FIRST(&ModemEventQueueHead);
	while (np!=NULL) {
		printf("Remove fake event\n");
		pthread_mutex_lock (&ModemEventQueueMutex);
		STAILQ_REMOVE_HEAD(&ModemEventQueueHead, entries);
		free(np);
		pthread_mutex_unlock (&ModemEventQueueMutex);
	}

	syslog(LOG_DEBUG, "starting main loop");
	while (!stop_thread) {
		SlicStateTransition(pmodem_dev);
		usleep(100000);
	}

	syslog(LOG_DEBUG, "stopping thread");
	pthread_join(timeout_thr, NULL);
	pthread_mutex_destroy(&TimeoutCntMutex);
	StopModem(pmodem_dev);
	slic_close();

	return EXIT_SUCCESS;

}

