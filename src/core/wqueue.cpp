#include "wqueue.h"
#include "clog.h"
#include "../common/misc/assert.h"

#include <stdio.h>
#include <pthread.h>

#define internal static

#define ArrayCount(Array) (sizeof(Array) / sizeof(*(Array)))

internal bool
DoNextWorkQueueEntry(work_queue *Queue)
{
    uint32_t EntryToRead = Queue->EntryToRead;
    uint32_t NextEntryToRead = (EntryToRead + 1) % ArrayCount(Queue->Entries);
    if (EntryToRead != Queue->EntryToWrite) {
        int Index = __sync_val_compare_and_swap(&Queue->EntryToRead, EntryToRead, NextEntryToRead);
        if (Index == EntryToRead) {
            work_queue_entry Entry = Queue->Entries[Index];
            Entry.Callback(Entry.Data);
            __sync_fetch_and_add(&Queue->EntriesCompleted, 1);
        }

        return false;
    }

    return true;
}

void AddWorkQueueEntry(work_queue *Queue, work_queue_callback *Callback, void *Data)
{
    uint32_t NextEntryToWrite = (Queue->EntryToWrite + 1) % ArrayCount(Queue->Entries);
    ASSERT(NextEntryToWrite != Queue->EntryToRead);

    work_queue_entry *Entry = Queue->Entries + Queue->EntryToWrite;
    Entry->Callback = Callback;
    Entry->Data = Data;
    ++Queue->EntryCount;

    asm volatile("" ::: "memory");
    Queue->EntryToWrite = NextEntryToWrite;
    sem_post(Queue->Semaphore);
}

void *WorkQueueThreadProc(void *Data)
{
    work_queue *Queue = (work_queue *) Data;
    for (;;) {
        if (DoNextWorkQueueEntry(Queue)) {
            int Result = sem_wait(Queue->Semaphore);
            if (Result) {
                uint64_t ID;
                pthread_threadid_np(NULL, &ID);
                c_log(C_LOG_LEVEL_DEBUG, "%lld: sem_wait(..) failed\n", ID);
            }
        }
    }
}

void CompleteWorkQueue(work_queue *Queue)
{
    while (Queue->EntryCount != Queue->EntriesCompleted) {
        DoNextWorkQueueEntry(Queue);
    }

    Queue->EntryCount = 0;
    Queue->EntriesCompleted = 0;
}
