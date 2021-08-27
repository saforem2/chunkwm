#ifndef CHUNKWM_OSX_EVENT_H
#define CHUNKWM_OSX_EVENT_H

#include <pthread.h>
#include <semaphore.h>
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

extern CHUNKWM_CALLBACK(Callback_ChunkWM_WindowCreated);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_WindowDestroyed);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_WindowFocused);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_WindowMoved);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_WindowResized);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_WindowMinimized);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_WindowDeminimized);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_WindowTitleChanged);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_WindowSheetCreated);

// NOTE(koekeishiya): This property is not exposed to plugins
extern CHUNKWM_CALLBACK(Callback_ChunkWM_PluginCommand);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_PluginBroadcast);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_PluginLoad);
extern CHUNKWM_CALLBACK(Callback_ChunkWM_PluginUnload);

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

    ChunkWM_WindowCreated,
    ChunkWM_WindowDestroyed,
    ChunkWM_WindowFocused,
    ChunkWM_WindowMoved,
    ChunkWM_WindowResized,
    ChunkWM_WindowMinimized,
    ChunkWM_WindowDeminimized,
    ChunkWM_WindowSheetCreated,
    ChunkWM_WindowTitleChanged,

    // NOTE(koekeishiya): This property is not exposed to plugins
    ChunkWM_PluginCommand,
    ChunkWM_PluginBroadcast,
};

struct chunk_event
{
    chunkwm_callback *Handle;
    void *Context;
    const char *Name;
};

struct event_loop
{
    bool Running;
    pthread_t Thread;
    sem_t *Semaphore;
    pthread_mutex_t Lock;
    std::queue<chunk_event> Queue;
};

bool BeginEventLoop();
void EndEventLoop();

void StartEventLoop();
void StopEventLoop();

void PauseEventLoop();
void ResumeEventLoop();

void AddEvent(chunk_event Event);

/* NOTE(koekeishiya): Construct a chunk_event with the appropriate callback through macro expansion. */
#define ConstructEvent(EventType, EventContext) \
    do { chunk_event Event = {}; \
         Event.Context = EventContext; \
         Event.Handle = &Callback_##EventType; \
         Event.Name = #EventType; \
         AddEvent(Event); \
       } while(0)

#endif
