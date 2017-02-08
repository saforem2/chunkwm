#include "carbon.h"
#include "workspace.h"

/*
 * NOTE(koekeishiya): The following files must also be linked against:
 *
 * common/misc/workspace.mm
 *
 * */

carbon_application_details *BeginCarbonApplicationDetails(ProcessSerialNumber PSN)
{
    carbon_application_details *Info =
        (carbon_application_details *) malloc(sizeof(carbon_application_details));

    ProcessInfoRec ProcessInfo = {};
    ProcessInfo.processInfoLength = sizeof(ProcessInfoRec);
    GetProcessInformation(&PSN, &ProcessInfo);

    Info->PSN = PSN;
    GetProcessPID(&Info->PSN, &Info->PID);
    Info->ProcessName = WorkspaceCopyProcessNameAndPolicy(Info->PID, &Info->ProcessPolicy);
    Info->ProcessBackground = (ProcessInfo.processMode & modeOnlyBackground) != 0;

    return Info;
}

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
