#ifndef CHUNKWM_OSX_EVENT_H
#define CHUNKWM_OSX_EVENT_H

#include <pthread.h>
#include <queue>

struct chunk_event;
#define EVENT_CALLBACK(name) void name(chunk_event *Event)
typedef EVENT_CALLBACK(EventCallback);

/* NOTE(koekeishiya): Declare chunk_event_type callbacks as external functions. */
extern EVENT_CALLBACK(Callback_ChunkWM_ApplicationLaunched);
extern EVENT_CALLBACK(Callback_ChunkWM_ApplicationTerminated);
extern EVENT_CALLBACK(Callback_ChunkWM_ApplicationActivated);
extern EVENT_CALLBACK(Callback_ChunkWM_ApplicationVisible);
extern EVENT_CALLBACK(Callback_ChunkWM_ApplicationHidden);

extern EVENT_CALLBACK(Callback_ChunkWM_DisplayAdded);
extern EVENT_CALLBACK(Callback_ChunkWM_DisplayRemoved);
extern EVENT_CALLBACK(Callback_ChunkWM_DisplayMoved);
extern EVENT_CALLBACK(Callback_ChunkWM_DisplayResized);

extern EVENT_CALLBACK(Callback_ChunkWM_DisplayChanged);
extern EVENT_CALLBACK(Callback_ChunkWM_SpaceChanged);

extern EVENT_CALLBACK(Callback_ChunkWM_MouseMoved);
extern EVENT_CALLBACK(Callback_ChunkWM_LeftMouseDragged);
extern EVENT_CALLBACK(Callback_ChunkWM_LeftMouseDown);
extern EVENT_CALLBACK(Callback_ChunkWM_LeftMouseUp);
extern EVENT_CALLBACK(Callback_ChunkWM_RightMouseDragged);
extern EVENT_CALLBACK(Callback_ChunkWM_RightMouseDown);
extern EVENT_CALLBACK(Callback_ChunkWM_RightMouseUp);

enum event_type
{
    ChunkWM_ApplicationLaunched,
    ChunkWM_ApplicationTerminated,
    ChunkWM_ApplicationActivated,
    ChunkWM_ApplicationVisible,
    ChunkWM_ApplicationHidden,

    ChunkWM_DisplayAdded,
    ChunkWM_DisplayRemoved,
    ChunkWM_DisplayMoved,
    ChunkWM_DisplayResized,

    ChunkWM_DisplayChanged,
    ChunkWM_SpaceChanged,

    ChunkWM_MouseMoved,
    ChunkWM_LeftMouseDragged,
    ChunkWM_LeftMouseDown,
    ChunkWM_LeftMouseUp,
    ChunkWM_RightMouseDragged,
    ChunkWM_RightMouseDown,
    ChunkWM_RightMouseUp,
};

struct chunk_event
{
    EventCallback *Handle;
    bool Intrinsic;
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
#define ConstructEvent(EventType, EventContext, EventIntrinsic) \
    do { chunk_event Event = {}; \
         Event.Context = EventContext; \
         Event.Intrinsic = EventIntrinsic; \
         Event.Handle = &Callback_##EventType; \
         AddEvent(Event); \
       } while(0)

#endif
