#include "carbon.h"
#include "event.h"
#include <string.h>
#define internal static

/* NOTE(koekeishiya): A pascal string has the size of the string stored as the first byte. */
internal inline void
CopyPascalStringToC(ConstStr255Param Source, char *Destination)
{
    strncpy(Destination, (char *) Source + 1, Source[0]);
    Destination[Source[0]] = '\0';
}

internal void
ExtractCarbonDetails(ProcessSerialNumber PSN, carbon_application_details *Info)
{
    Str255 ProcessName = {};
    ProcessInfoRec ProcessInfo = {};
    ProcessInfo.processInfoLength = sizeof(ProcessInfoRec);
    ProcessInfo.processName = ProcessName;

    /* NOTE(koekeishiya): Deprecated, consider switching to
     * CFDictionaryRef ProcessInformationCopyDictionary(const ProcessSerialNumber *PSN, UInt32 infoToReturn) */
    GetProcessInformation(&PSN, &ProcessInfo);

    Info->PSN = PSN;
    GetProcessPID(&Info->PSN, &Info->PID);
    Info->ProcessMode = ProcessInfo.processMode;

    if(ProcessInfo.processName)
    {
        Info->ProcessName = (char *) malloc(256);
        CopyPascalStringToC(ProcessInfo.processName, Info->ProcessName);
    }
    else
    {
        Info->ProcessName = strdup("<Unknown Name>");
    }
}

internal carbon_application_details *
BeginCarbonApplicationDetails(ProcessSerialNumber PSN)
{
    carbon_application_details *Info =
        (carbon_application_details *) malloc(sizeof(carbon_application_details));
    memset(Info, 0, sizeof(carbon_application_details));

    ExtractCarbonDetails(PSN, Info);
    if((Info->ProcessMode & modeOnlyBackground) != 0)
    {
        EndCarbonApplicationDetails(Info);
        return NULL;
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
                ConstructEvent(ChunkWM_ApplicationLaunched, Info, false);
            }
        } break;
        case kEventAppTerminated:
        {
            carbon_application_details *Info = BeginCarbonApplicationDetails(PSN);
            if(Info)
            {
                ConstructEvent(ChunkWM_ApplicationTerminated, Info, false);
            }
        } break;
    }

    return noErr;
}

bool BeginCarbonEventHandler(carbon_event_handler *Carbon)
{
    Carbon->EventTarget = GetApplicationEventTarget();
    Carbon->EventHandler = NewEventHandlerUPP(CarbonApplicationEventHandler);
    Carbon->EventType[0].eventClass = kEventClassApplication;
    Carbon->EventType[0].eventKind = kEventAppLaunched;
    Carbon->EventType[1].eventClass = kEventClassApplication;
    Carbon->EventType[1].eventKind = kEventAppTerminated;

    return InstallEventHandler(Carbon->EventTarget, Carbon->EventHandler, 2, Carbon->EventType, NULL, &Carbon->CurHandler) == noErr;
}

bool EndCarbonEventHandler(carbon_event_handler *Carbon)
{
    return RemoveEventHandler(Carbon->CurHandler) == noErr;
}
