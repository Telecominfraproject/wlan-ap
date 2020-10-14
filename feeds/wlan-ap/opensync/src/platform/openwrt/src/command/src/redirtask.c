/* SPDX-License-Identifier: BSD-3-Clause */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <unistd.h>
#include <pthread.h>
#include "redirmessagedef.h"
#include "redirtask.h"

static int numTasks = 0;
static redirTask *tasks = NULL;
static redirTask *freeTasks = NULL;
static pthread_mutex_t tasksMutex;
static int numAllocatedTasks = 0;

redirTask* allocTaskFromPool()
{
	redirTask *pTask = NULL;

	pthread_mutex_lock(&tasksMutex);
	if (freeTasks) {
		pTask = freeTasks;
		pTask->allocated = 1;
		freeTasks = freeTasks->nextFreeTask;
		++numAllocatedTasks;
	}
	pthread_mutex_unlock(&tasksMutex);
	return pTask;
}

void releaseTaskBackToPool(redirTask *pTask)
{
	int rc;

	if (pTask->allocated) {
		pthread_mutex_lock(&tasksMutex);
		if ((rc = pthread_attr_destroy(&pTask->threadAttr)) != 0) {
		}
		pTask->allocated = 0;
		pTask->threadSelf = 0;
		pTask->nextFreeTask = freeTasks;
		freeTasks = pTask;
		numAllocatedTasks--;
		pthread_mutex_unlock(&tasksMutex);
	}
}

int createOneTask(Address taskFunc, char *Name, int *entryId)
{
	redirTask *pTask = NULL;

	pTask = allocTaskFromPool();

	if (pTask) {
		int rc;

		*entryId = pTask->entryId;
		strncpy(&pTask->name[0], Name, 16);
		pTask->name[15] = '\0';
		pTask->taskFunc	 		= (void (*)(void))taskFunc;

		if ((rc = pthread_attr_init(&pTask->threadAttr)) != 0) {
		}

		if ((rc = pthread_create(&pTask->threadSelf, &pTask->threadAttr, (void *)taskFunc, pTask)) != 0) {
			releaseTaskBackToPool(pTask);
			return -1;
		}
	}

	return 0;
}

int redirInitTasks()
{
	int i, rc;
	redirTask *pTask;
	pthread_mutexattr_t mAttr;

	numTasks = REDIR_MAX_TASKS;
	tasks = calloc(numTasks, sizeof(redirTask));
	if (!tasks) {
		return -1;
	}

	pTask = tasks;
	for (i = 0; i < numTasks; i++) {
		pTask->entryId = REDIR_TASK_ID_BASE | i;
		pTask->allocated = 0;
		pTask->nextFreeTask = pTask + 1;
		pTask++;
	}

	tasks[numTasks - 1].nextFreeTask = NULL;
	freeTasks = tasks;

	pthread_mutexattr_init(&mAttr);

	if ((rc = pthread_mutex_init(&tasksMutex, &mAttr)) != 0) {
		free(tasks);
		return -1;
	}

	// setup main pTask up in the table.
	pTask = allocTaskFromPool();

	strncpy(&pTask->name[0], "main", 16);
	pTask->taskFunc = NULL;
	pTask->threadSelf = pthread_self();

	return 0;
}

void waitForOtherTaskesToFinish()
{
	redirTask *pTask = tasks;
	void *ret = NULL;
	int i;

	pTask++;
	for (i = 1; i < REDIR_MAX_TASKS; i++) {
		if (pTask->allocated) {
			printf("TASK %d\n", i);
			if (pthread_join(pTask->threadSelf, &ret)) {
				printf("returning\n");
				return;
			}
		}
		pTask++;
                pthread_kill(pTask->threadSelf,0);
		return;
	}
}

