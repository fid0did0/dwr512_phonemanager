/* dwr512_phonemanager
**
** Author: 	Giuseppe Lippolis
**
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License v2.0.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <syslog.h>

#include "modem_ctrl.h"

struct ModemEventSyntStr ModemEventSynt[] =
		{	{"RING", 4, ringing, 0, "RIN%[G]"},
			{"+CLIP", 5, callid, 1, "+CLIP: \"%30[+0123456789]\""},
			{"+CLCC", 5, callstat, 1, "+CLCC: %*d,%*d,%2[+0123456789],%*d"},
			{"NO CARRIER", 10, nocarrier, 0, "NO CARRIE%[R]"},
			{"ERROR", 5, error, 0, "ERRO%[R]"},
			{"OK", 2, ok, 0, "O%[K]"}
		};

#define ModemEventSyntNum (sizeof(ModemEventSynt)/sizeof(struct ModemEventSyntStr))

void *ModemMon(void *threadarg) {
	modem_dev_str *pmodem_dev = (modem_dev_str *)threadarg;
	struct ModemEventQueue *np;
	struct timeval timeout;
	char buf[MSGBUFFSIZE], rdbuf[MSGBUFFSIZE], *nlrbp, *bufep, *tp, *pch;
	char str0[32], str1[32];
	int k, kk, tmp, rdnum;

	buf[0]='\0';
	bufep=buf;
	while (!stop_thread) {
		timeout.tv_sec=1L;
		timeout.tv_usec=000000;
		FD_ZERO(&(pmodem_dev->rfds));
		FD_SET(pmodem_dev->df, &(pmodem_dev->rfds));

		rdnum=select(pmodem_dev->df+1,&(pmodem_dev->rfds),NULL,NULL,&timeout);

		if (rdnum) {
			pthread_mutex_lock (&(pmodem_dev->ModemMutex));
			rdnum=read(pmodem_dev->df, rdbuf, MSGBUFFSIZE-1);
			pthread_mutex_unlock (&(pmodem_dev->ModemMutex));
			rdbuf[rdnum]='\0';

			tmp=0; k=0;
			while((tmp==0)&&(k<ModemEventSyntNum)) {
				if(ModemEventSynt[k].PrivDataNum==0)
					tmp=sscanf(rdbuf, ModemEventSynt[k].ScanStr, str0);
				if(ModemEventSynt[k].PrivDataNum==1)
					tmp=sscanf(rdbuf, ModemEventSynt[k].ScanStr, str0);
				if(ModemEventSynt[k].PrivDataNum==2)
					tmp=sscanf(rdbuf, ModemEventSynt[k].ScanStr, str0, str1);

				if (tmp) {
					np = malloc(sizeof(struct ModemEventQueue));
					np->evnt=ModemEventSynt[k].evnt;
					np->PrivDataNum=ModemEventSynt[k].PrivDataNum;
					strncpy(np->msg, rdbuf, MSGBUFFSIZE);
					strncpy(np->PrivData0, str0, 32);
					strncpy(np->PrivData1, str1, 32);
					pthread_mutex_lock (&ModemEventQueueMutex);
					STAILQ_INSERT_TAIL(&ModemEventQueueHead, np, entries);
					pthread_mutex_unlock (&ModemEventQueueMutex);
				}
				k++;
			}
		}
	}

	pthread_exit(NULL);
	return NULL;
}

void PutModemCmd(modem_dev_str *pmodem_dev, char *cmd) {
	int wrnum;

	pthread_mutex_lock (&(pmodem_dev->ModemMutex));
	wrnum=write(pmodem_dev->df, cmd, strlen(cmd));
	pthread_mutex_unlock (&(pmodem_dev->ModemMutex));
}

void InitModem(modem_dev_str *pmodem_dev) {
	struct	sigaction saio;	/* definition of signal action */
	struct	termios tty;  	/* termios: svbuf=before, stbuf=while */
	int	rc;

	STAILQ_INIT(&ModemEventQueueHead);	/* Initialize the list. */
	pthread_mutex_init(&ModemEventQueueMutex, NULL);
	pthread_mutex_init(&(pmodem_dev->ModemMutex), NULL);
	syslog(LOG_DEBUG, "modem mutex initialized");

	if (pmodem_dev->df > 0 && isatty(pmodem_dev->df)) {
		if (tcgetattr(pmodem_dev->df, &tty) < 0)
			syslog(LOG_WARNING, "Can't get tty attributes\n");
		else {
/*			cfmakeraw(&tty); // set to "raw" mode
			tty.c_cflag = CRTSCTS | CS8 | CLOCAL | CREAD;
			tty.c_iflag = IGNPAR | ICRNL;
			tty.c_oflag = 0;
			tty.c_cc[VMIN]=1;
			tty.c_iflag |= BRKINT;*/
			tty.c_lflag = ICANON;
			tty.c_cc[VTIME]=10;
			tty.c_cc[VEOL]='\n';
			tty.c_cc[VEOL2]='\r';
			if (tcsetattr(pmodem_dev->df, TCSANOW, &tty) < 0)
				syslog(LOG_WARNING, "Can't set tty attributes\n");
		}
	}
	syslog(LOG_DEBUG, "modem tty initialized");

	write(pmodem_dev->df, "AT+CFUN=1\n", 10);
	write(pmodem_dev->df, "AT+CLIP=1\n", 10);
	syslog(LOG_DEBUG, "activated GSM modem");

	rc = pthread_create(&modem_mon_thr, NULL, ModemMon, (void *)pmodem_dev);
	if (rc){
		syslog(LOG_ERR, "cannot create modem thread(%d)", rc);
		stop_thread=1;
	}
}

void StopModem(modem_dev_str *pmodem_dev) {

	pthread_join(modem_mon_thr, NULL);
	pthread_mutex_destroy(&ModemEventQueueMutex);
	pthread_mutex_destroy(&(pmodem_dev->ModemMutex));
	close(pmodem_dev->df);

}
