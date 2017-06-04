/* dwr512_phonemanager
**
** Author: 	Giuseppe Lippolis
**
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License v2.0.
*/

#include <poll.h>

enum slic_event {no_slic_event, onhook, offhook, dtmf1, dtmf2, dtmf3, dtmf4, dtmf5,
			dtmf6, dtmf7, dtmf8, dtmf9, dtmf0, dtmf_star, dtmf_hash};

pthread_t 	slic_mon_thr;

struct pollfd 	fdset[1];

struct SlicEvntQueue{
	enum slic_event		data;
	struct SlicEvntQueue	*next;
};

unsigned int SlicEvntQueueLen;
pthread_mutex_t SlicEvntQueueLenMutex;

struct SlicEvntQueue *HeadSlicEvntQueue;
struct SlicEvntQueue *EndSlicEvntQueue;

char digitstring[32];
int  stop_thread;

enum slic_event getSlicEvent();
int slic_init();
int slic_close();