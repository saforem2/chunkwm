#include "event.h"

#define internal static
internal event_loop EventLoop = {};

/* NOTE(koekeishiya): Must be thread-safe! Called through ConstructEvent macro */
void AddEvent(chunk_event Event)
{
    if(EventLoop.Running && Event.Handle)
    {
        pthread_mutex_lock(&EventLoop.WorkerLock);
        EventLoop.Queue.push(Event);
        pthread_mutex_unlock(&EventLoop.WorkerLock);
        sem_post(EventLoop.Semaphore);
    }
}

internal void *
ProcessEventQueue(void *)
{
    while(EventLoop.Running)
    {
        pthread_mutex_lock(&EventLoop.StateLock);
        while(!EventLoop.Queue.empty())
        {
            pthread_mutex_lock(&EventLoop.WorkerLock);
            chunk_event Event = EventLoop.Queue.front();
            EventLoop.Queue.pop();
            pthread_mutex_unlock(&EventLoop.WorkerLock);

            (*Event.Handle)(&Event);
        }

        int Result = sem_wait(EventLoop.Semaphore);
        if(Result)
        {
            uint64_t ID;
            pthread_threadid_np(NULL, &ID);
            fprintf(stderr, "%lld: sem_wait(..) failed\n", ID);
        }

        pthread_mutex_unlock(&EventLoop.StateLock);
    }

    return NULL;
}

/* NOTE(koekeishiya): Initialize required mutexes and semaphore for the eventloop */
internal bool
BeginEventLoop()
{
    bool Result = true;

    if((EventLoop.Semaphore = sem_open("eventloop_semaphore", O_CREAT, 0644, 0)) == SEM_FAILED)
    {
        fprintf(stderr, "chunkwm: could not initialize semaphore!");
        goto sem_err;
    }

    if(pthread_mutex_init(&EventLoop.WorkerLock, NULL) != 0)
    {
        fprintf(stderr, "chunkwm: could not initialize work mutex!");
        goto work_err;
    }

    if(pthread_mutex_init(&EventLoop.StateLock, NULL) != 0)
    {
        fprintf(stderr, "chunkwm: could not initialize state mutex!");
        goto state_err;
    }

    goto out;

state_err:
    pthread_mutex_destroy(&EventLoop.WorkerLock);

work_err:
    sem_destroy(EventLoop.Semaphore);

sem_err:
    Result = false;

out:
    return Result;
}

/* NOTE(koekeishiya): Destroy mutexes and condition used by the event-loop */
internal void
EndEventLoop()
{
    pthread_mutex_destroy(&EventLoop.StateLock);
    pthread_mutex_destroy(&EventLoop.WorkerLock);
    sem_destroy(EventLoop.Semaphore);
}

void PauseEventLoop()
{
    if(EventLoop.Running)
    {
        pthread_mutex_lock(&EventLoop.StateLock);
    }
}

void ResumeEventLoop()
{
    if(EventLoop.Running)
    {
        pthread_mutex_unlock(&EventLoop.StateLock);
    }
}

bool StartEventLoop()
{
    if((!EventLoop.Running) &&
       (BeginEventLoop()))
    {
        EventLoop.Running = true;
        pthread_create(&EventLoop.Worker, NULL, &ProcessEventQueue, NULL);
        return true;
    }

    return false;
}

void StopEventLoop()
{
    if(EventLoop.Running)
    {
        EventLoop.Running = false;
        pthread_join(EventLoop.Worker, NULL);
        EndEventLoop();
    }
}
