#ifndef CHUNKWM_OSX_EVENT_H
#define CHUNKWM_OSX_EVENT_H

#include <pthread.h>
#include <queue>

struct chunk_event;
#define CHUNKWM_CALLBACK(name) void name(chunk_event *Event)
typedef CHUNKWM_CALLBACK(chunkwm_callback);

/* NOTE(koekeishiya): Declare chunk_event_type callbacks as external functions. */
extern CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationLaunched);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationTerminated);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationActivated);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationDeactivated);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationVisible);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_ApplicationHidden);

extern CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayAdded);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayRemoved);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayMoved);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayResized);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_DisplayChanged);

extern CHUNKWM_CALLBACK(Callback_ChunkWM_SpaceChanged);

enum event_type
{
    ChunkWM_ApplicationLaunched,
    ChunkWM_ApplicationTerminated,
    ChunkWM_ApplicationActivated,
    ChunkWM_ApplicationDeactivated,
    ChunkWM_ApplicationVisible,
    ChunkWM_ApplicationHidden,

    ChunkWM_DisplayAdded,
    ChunkWM_DisplayRemoved,
    ChunkWM_DisplayMoved,
    ChunkWM_DisplayResized,
    ChunkWM_DisplayChanged,
    ChunkWM_SpaceChanged,
};

struct chunk_event
{
    chunkwm_callback *Handle;
    void *Context;
};

struct event_loop
{
    pthread_cond_t State;
    pthread_mutex_t StateLock;
    pthread_mutex_t WorkerLock;
    pthread_t Worker;
    bool Running;
    std::queue<chunk_event> Queue;
};

bool StartEventLoop();
void StopEventLoop();

void PauseEventLoop();
void ResumeEventLoop();

void AddEvent(chunk_event Event);

/* NOTE(koekeishiya): Construct a chunk_event with the appropriate callback through macro expansion. */
#define ConstructEvent(EventType, EventContext) \
    do { chunk_event Event = {}; \
         Event.Context = EventContext; \
         Event.Handle = &Callback_##EventType; \
         AddEvent(Event); \
       } while(0)

#endif
