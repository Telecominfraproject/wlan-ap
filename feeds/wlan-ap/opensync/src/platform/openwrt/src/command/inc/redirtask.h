/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _REDIRTASK_H_
#define _REDIRTASK_H_

#define REDIR_MAX_TASKS     50
#define REDIR_TASK_ID_BASE  0x10000000

typedef unsigned long   Address;

typedef struct _redirTask
{
	struct _redirTask *nextFreeTask;     // pointer to the next redirTask entry
	int entryId;                        // Unique ID for this entry. We could add type to it if needed.
	int allocated;                      // Whether this entry is used or not.
	char name[16];                      // Name of the pthread.
	unsigned priority;
	pthread_t threadSelf;               // The pthread that is associated with this entry.
	pthread_attr_t threadAttr;
	void (*taskFunc)(void);             // The start routine what runs in the pthread.
} redirTask;

extern redirTask* allocTaskFromPool();
extern void releaseTaskBackToPool(redirTask *pTask);
extern int createOneTask(Address taskFunc, char *Name, int *taskId);
extern int redirInitTasks();
extern void waitForOtherTaskesToFinish();


#endif /* _REDIRTASK_H_ */

