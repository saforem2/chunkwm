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

        pthread_cond_signal(&EventLoop.State);
        pthread_mutex_unlock(&EventLoop.WorkerLock);
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

        while(EventLoop.Queue.empty() && EventLoop.Running)
            pthread_cond_wait(&EventLoop.State, &EventLoop.StateLock);

        pthread_mutex_unlock(&EventLoop.StateLock);
    }

    return NULL;
}

/* NOTE(koekeishiya): Initialize required mutexes and condition for the event-loop */
internal bool
BeginEventLoop()
{
   if(pthread_mutex_init(&EventLoop.WorkerLock, NULL) != 0)
   {
       return false;
   }

   if(pthread_mutex_init(&EventLoop.StateLock, NULL) != 0)
   {
       pthread_mutex_destroy(&EventLoop.WorkerLock);
       return false;
   }

   if(pthread_cond_init(&EventLoop.State, NULL) != 0)
   {
        pthread_mutex_destroy(&EventLoop.WorkerLock);
        pthread_mutex_destroy(&EventLoop.StateLock);
        return false;
   }

   return true;
}

/* NOTE(koekeishiya): Destroy mutexes and condition used by the event-loop */
internal void
EndEventLoop()
{
    pthread_cond_destroy(&EventLoop.State);
    pthread_mutex_destroy(&EventLoop.StateLock);
    pthread_mutex_destroy(&EventLoop.WorkerLock);
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
        pthread_cond_signal(&EventLoop.State);
        pthread_join(EventLoop.Worker, NULL);
        EndEventLoop();
    }
}
