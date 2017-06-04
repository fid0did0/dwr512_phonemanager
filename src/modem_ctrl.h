/* dwr512_phonemanager
**
** Author: 	Giuseppe Lippolis
**
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License v2.0.
*/

#include <pthread.h>
#include <sys/queue.h>
#include <sys/select.h>

#define MODEMDEVICE "/dev/ttyUSB1"
#define FALSE 0
#define TRUE 1
#define MSGBUFFSIZE 256

enum ModemEventType {no_modem_event, ringing, callid, callstat, nocarrier, ok, error, nod_id};
struct ModemEventSyntStr {
	char			*DispStr;
	unsigned char		StrLen;
	enum ModemEventType	evnt;
	unsigned char		PrivDataNum;
	char			ScanStr[64];
};

STAILQ_HEAD(listhead, ModemEventQueue) ModemEventQueueHead;
//struct listhead *ModemEventQueueHeadp;		/* List head. */
struct ModemEventQueue {
	enum ModemEventType		evnt;
	char				msg[MSGBUFFSIZE];
	unsigned char			PrivDataNum;
	char				PrivData0[32];
	char				PrivData1[32];
	STAILQ_ENTRY(ModemEventQueue)	entries;	/* List. */
};
pthread_mutex_t 	ModemEventQueueMutex;

typedef struct {
	int 		df;
	fd_set 		rfds;
	pthread_mutex_t ModemMutex;
} modem_dev_str;
modem_dev_str	modem_dev;

pthread_t 	modem_mon_thr;

int  stop_thread;

void PutModemCmd(modem_dev_str *pmodem_dev, char *cmd);
void InitModem(modem_dev_str *pmodem_dev);
void StopModem(modem_dev_str *pmodem_dev);