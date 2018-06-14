#include "carbon.h"
#include "event.h"
#include "../state.h"
#include "../clog.h"

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
 */
internal carbon_application_cache CarbonApplicationCache;

internal carbon_application_details *
SearchCarbonApplicationDetailsCache(ProcessSerialNumber PSN)
{
    carbon_application_details *Result = NULL;

    carbon_application_cache_iter It = CarbonApplicationCache.find(PSN);
    if (It != CarbonApplicationCache.end()) {
        Result = It->second;
    }

    return Result;
}

internal inline void
PrintCarbonApplicationDetails(carbon_application_details *Info)
{
    c_log(C_LOG_LEVEL_DEBUG,
          "carbon: process details\nName: %s\nPID: %d\nPSN: %d %d\nPolicy: %d\nBackground: %d\n",
          Info->ProcessName,
          Info->PID,
          Info->PSN.lowLongOfPSN,
          Info->PSN.highLongOfPSN,
          Info->ProcessPolicy,
          Info->ProcessBackground);
}

/*
 * NOTE(koekeishiya): We have to cache information about processes that have
 * already been launched before us, such that we can properly perform our lookup
 * and report when any of these applications are terminated.
 */
internal void
CacheRunningProcesses()
{
    ProcessSerialNumber PSN = { kNoProcess, kNoProcess };
    while (GetNextProcess(&PSN) == noErr) {
        carbon_application_details *Info = BeginCarbonApplicationDetails(PSN);
        Info->State = Carbon_Application_State_Finished;
        CarbonApplicationCache[PSN] = Info;
    }
}

internal OSStatus
CarbonApplicationEventHandler(EventHandlerCallRef HandlerCallRef, EventRef Event, void *Refcon)
{
    ProcessSerialNumber PSN;
    if (GetEventParameter(Event,
                          kEventParamProcessID,
                          typeProcessSerialNumber,
                          NULL,
                          sizeof(PSN),
                          NULL,
                          &PSN) != noErr) {
        c_log(C_LOG_LEVEL_ERROR, "carbon: could not get serialnumber of process\n");
        return -1;
    }

    uint32_t Type = GetEventKind(Event);
    switch (Type) {
    case kEventAppLaunched: {
        carbon_application_details *Info = BeginCarbonApplicationDetails(PSN);
        CarbonApplicationCache[PSN] = Info;
        PrintCarbonApplicationDetails(Info);
        ConstructAndAddApplication(Info);
    } break;
    case kEventAppTerminated: {
        carbon_application_details *Info = SearchCarbonApplicationDetailsCache(PSN);
        if (Info) {
            CarbonApplicationCache.erase(PSN);

            if (Info->State == Carbon_Application_State_In_Progress) {
                Info->State = Carbon_Application_State_Invalid;
            } else if ((Info->State == Carbon_Application_State_Finished) ||
                       (Info->State == Carbon_Application_State_Failed)) {
                ConstructEvent(ChunkWM_ApplicationTerminated, Info);
            }
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
