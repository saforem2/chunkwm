#include "carbon.h"
#include "event.h"
#include <string.h>
#include <unordered_map>
#define internal static

struct psn_hash {
size_t operator() (const ProcessSerialNumber &PSN) const
{
    size_t Result = 17;
    Result = (Result << 5) - Result + std::hash<unsigned long>()(PSN.lowLongOfPSN);
    Result = (Result << 5) - Result + std::hash<unsigned long>()(PSN.highLongOfPSN);
    return Result;
}
};

bool operator==(const ProcessSerialNumber &Lhs,
                const ProcessSerialNumber &Rhs)
{
    Boolean Result;
    SameProcess(&Lhs, &Rhs, &Result);
    return Result == 1;
}

typedef std::unordered_map<ProcessSerialNumber, carbon_application_details *, psn_hash> carbon_application_cache;
typedef std::unordered_map<ProcessSerialNumber, carbon_application_details *, psn_hash>::iterator carbon_application_cache_iter;

/*
 * NOTE(koekeishiya): By the time our application has received the kEventAppTerminated event,
 * the terminating application has already quit. Thus we can't get information about that
 * application using GetProcessInformation and have to cache the information in advance.
 * */
internal carbon_application_cache CarbonApplicationCache;

internal carbon_application_details *
CopyCarbonApplicationDetails(carbon_application_details *Info)
{
    carbon_application_details *Result =
        (carbon_application_details *) malloc(sizeof(carbon_application_details));

    Result->PID = Info->PID;
    Result->PSN = Info->PSN;
    Result->ProcessMode = Info->ProcessMode;
    Result->ProcessName = strdup(Info->ProcessName);

    return Result;
}

internal carbon_application_details *
SearchCarbonApplicationDetailsCache(ProcessSerialNumber PSN)
{
    carbon_application_details *Result = NULL;

    carbon_application_cache_iter It = CarbonApplicationCache.find(PSN);
    if(It != CarbonApplicationCache.end())
    {
        Result = It->second;
    }

    return Result;
}

/* NOTE(koekeishiya): A pascal string has the size of the string stored as the first byte. */
internal inline void
CopyPascalStringToC(ConstStr255Param Source, char *Destination)
{
    strncpy(Destination, (char *) Source + 1, Source[0]);
    Destination[Source[0]] = '\0';
}

internal inline void
PrintCarbonApplicationDetails(carbon_application_details *Info)
{
    printf("PID: %d\nPSN: %d %d\nMode: %d\nName: %s\n\n",
            Info->PID,
            Info->PSN.lowLongOfPSN,
            Info->PSN.highLongOfPSN,
            Info->ProcessMode,
            Info->ProcessName);
}

internal carbon_application_details *
BeginCarbonApplicationDetails(ProcessSerialNumber PSN)
{
    carbon_application_details *Info =
        (carbon_application_details *) malloc(sizeof(carbon_application_details));

    Str255 ProcessName = {};
    ProcessInfoRec ProcessInfo = {};
    ProcessInfo.processInfoLength = sizeof(ProcessInfoRec);
    ProcessInfo.processName = ProcessName;

    /* NOTE(koekeishiya): Deprecated, consider switching to
     * CFDictionaryRef ProcessInformationCopyDictionary(const ProcessSerialNumber *PSN,
     *                                                  UInt32 infoToReturn);
     * */
    GetProcessInformation(&PSN, &ProcessInfo);

    Info->PSN = PSN;
    GetProcessPID(&Info->PSN, &Info->PID);
    Info->ProcessMode = ProcessInfo.processMode;

    if(ProcessInfo.processName)
    {
        Info->ProcessName = (char *) malloc(ProcessInfo.processName[0] + 1);
        CopyPascalStringToC(ProcessInfo.processName, Info->ProcessName);
    }
    else
    {
        Info->ProcessName = strdup("<Unknown Name>");
    }

    return Info;
}

// NOTE(koekeishiya): Make sure that the correct module frees memory.
void EndCarbonApplicationDetails(carbon_application_details *Info)
{
    if(Info)
    {
        if(Info->ProcessName)
        {
            free(Info->ProcessName);
        }

        free(Info);
    }
}

/*
 * NOTE(koekeishiya): We have to cache information about processes that have
 * already been launched before us, such that we can properly perform our lookup
 * and report when any of these applications are terminated.
 * */
internal void
CacheRunningProcesses()
{
    ProcessSerialNumber PSN = { kNoProcess, kNoProcess };
    while(GetNextProcess(&PSN) == noErr)
    {
        carbon_application_details *Info = BeginCarbonApplicationDetails(PSN);
        if(Info)
        {
            PrintCarbonApplicationDetails(Info);
            CarbonApplicationCache[PSN] = Info;
        }
    }
}

internal OSStatus
CarbonApplicationEventHandler(EventHandlerCallRef HandlerCallRef, EventRef Event, void *Refcon)
{
    ProcessSerialNumber PSN;
    if(GetEventParameter(Event,
                         kEventParamProcessID,
                         typeProcessSerialNumber,
                         NULL,
                         sizeof(PSN),
                         NULL,
                         &PSN) != noErr)
    {
        printf("Carbon: Could not get event parameter.\n");
        return -1;
    }

    uint32_t Type = GetEventKind(Event);
    switch(Type)
    {
        case kEventAppLaunched:
        {
            carbon_application_details *Info = BeginCarbonApplicationDetails(PSN);
            if(Info)
            {
                carbon_application_details *Copy = CopyCarbonApplicationDetails(Info);
                CarbonApplicationCache[PSN] = Copy;
                ConstructEvent(ChunkWM_ApplicationLaunched, Info);
            }
        } break;
        case kEventAppTerminated:
        {
            carbon_application_details *Info = SearchCarbonApplicationDetailsCache(PSN);
            if(Info)
            {
                CarbonApplicationCache.erase(PSN);
                ConstructEvent(ChunkWM_ApplicationTerminated, Info);
            }
        } break;
    }

    return noErr;
}

bool BeginCarbonEventHandler(carbon_event_handler *Carbon)
{
    CacheRunningProcesses();

    Carbon->EventTarget = GetApplicationEventTarget();
    Carbon->EventHandler = NewEventHandlerUPP(CarbonApplicationEventHandler);
    Carbon->EventType[0].eventClass = kEventClassApplication;
    Carbon->EventType[0].eventKind = kEventAppLaunched;
    Carbon->EventType[1].eventClass = kEventClassApplication;
    Carbon->EventType[1].eventKind = kEventAppTerminated;

    return InstallEventHandler(Carbon->EventTarget,
                               Carbon->EventHandler,
                               2,
                               Carbon->EventType,
                               NULL,
                               &Carbon->CurHandler) == noErr;
}

bool EndCarbonEventHandler(carbon_event_handler *Carbon)
{
    return RemoveEventHandler(Carbon->CurHandler) == noErr;
}
