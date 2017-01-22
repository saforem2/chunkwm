#ifndef CHUNKWM_CORE_WQUEUE_H
#define CHUNKWM_CORE_WQUEUE_H

#include <stdint.h>
#include <semaphore.h>

#define WORK_QUEUE_CALLBACK(name) void name(void *Data)
typedef WORK_QUEUE_CALLBACK(work_queue_callback);

struct work_queue_entry
{
    work_queue_callback *Callback;
    void *Data;
};

struct work_queue
{
    uint32_t volatile EntriesCompleted;
    uint32_t volatile EntryCount;
    uint32_t volatile EntryToWrite;
    uint32_t volatile EntryToRead;
    sem_t *Semaphore;

    work_queue_entry Entries[256];
};

void CompleteWorkQueue(work_queue *Queue);
void AddWorkQueueEntry(work_queue *Queue, work_queue_callback *Callback, void *Data);
void *WorkQueueThreadProc(void *Data);

#endif
