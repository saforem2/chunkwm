#include "rule.h"
#include "controller.h"
#include "misc.h"

#include "../../common/misc/assert.h"
#include "../../common/accessibility/display.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/application.h"
#include "../../common/misc/assert.h"

#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include <vector>
#include <map>

#define internal static

typedef std::map<uint32_t, macos_window *> macos_window_map;
typedef macos_window_map::iterator macos_window_map_it;

extern macos_window_map CopyWindowCache();
extern void TileWindow(macos_window *Window);

// TODO(koekeishiya): This vector is accessed from our daemon thread,
// and also the thread selected to execute the 'plugin->main' function.
//
// We want this to be thread-safe to properly allow for adding rules after
// the initial config has been ran, however, this should in most cases
// not be an issue, as the vector is only accessed upon window creation.
internal std::vector<window_rule *> WindowRules;

internal inline bool
RegexMatchPattern(regex_t *Regex, const char *Match, const char *Pattern)
{
    int Error = regcomp(Regex, Pattern, REG_EXTENDED);
    if(!Error)
    {
        Error = regexec(Regex, Match, 0, NULL, 0);
        regfree(Regex);
    }
    else
    {
        fprintf(stderr, "chunkwm-tiling: window rule - could not compile regex!\n");
    }

    return Error == 0;
}

internal inline void
ApplyWindowRuleState(macos_window *Window, window_rule *Rule, uint32_t *RuleResult)
{
    if(StringEquals(Rule->State, "float"))
    {
        FloatWindow(Window, false);
    }
    else if(StringEquals(Rule->State, "tile"))
    {
        AXLibAddFlags(Window, Window_ForceTile);

        if(!AXLibHasFlags(Window, Window_Minimized))
        {
            macos_space *Space;
            bool Success = AXLibActiveSpace(&Space);
            ASSERT(Success);

            if(AXLibSpaceHasWindow(Space->Id, Window->Id))
            {
                TileWindow(Window);
                (*RuleResult) |= Rule_State_Tiled;
            }

            AXLibDestroySpace(Space);
        }
    }
    else
    {
        fprintf(stderr, "chunkwm-tiling: window rule - invalid state '%s', ignored..\n", Rule->State);
    }
}

internal inline void
ApplyWindowRuleDesktop(macos_window *Window, window_rule *Rule, uint32_t *RuleResult)
{
    if(SendWindowToDesktop(Window, Rule->Desktop))
    {
        (*RuleResult) |= Rule_Desktop_Changed;
    }
}

internal inline void
ApplyWindowRule(macos_window *Window, window_rule *Rule, uint32_t *RuleResult)
{
    regex_t Regex;

    bool Match = true;
    if(Rule->Owner && Window->Owner->Name)
    {
        Match = RegexMatchPattern(&Regex, Window->Owner->Name, Rule->Owner);
        if(!Match) return;
    }

    if(Rule->Name && Window->Name)
    {
        Match &= RegexMatchPattern(&Regex, Window->Name, Rule->Name);
        if(!Match) return;
    }

    if(Rule->Except && Window->Name)
    {
        Match &= !RegexMatchPattern(&Regex, Window->Name, Rule->Except);
        if(!Match) return;
    }

    if(Rule->Desktop)   ApplyWindowRuleDesktop(Window, Rule, RuleResult);
    if(Rule->State)     ApplyWindowRuleState(Window, Rule, RuleResult);
}

uint32_t ApplyRulesForWindow(macos_window *Window)
{
    uint32_t Result = 0;
    for(size_t Index = 0;
        Index < WindowRules.size();
        ++Index)
    {
        window_rule *Rule = WindowRules[Index];
        ApplyWindowRule(Window, Rule, &Result);
    }
    return Result;
}

internal void
ApplyRuleToExistingWindows(window_rule *Rule)
{
    uint32_t Result = 0;
    macos_window_map Windows = CopyWindowCache();
    for(macos_window_map_it It = Windows.begin();
        It != Windows.end();
        ++It)
    {
        macos_window *Window = It->second;
        ApplyWindowRule(Window, Rule, &Result);
    }
}

void AddWindowRule(window_rule *Rule)
{
    window_rule *Result = (window_rule *) malloc(sizeof(window_rule));
    memcpy(Result, Rule, sizeof(window_rule));
    WindowRules.push_back(Result);
    ApplyRuleToExistingWindows(Result);
}

internal void
FreeWindowRule(window_rule *Rule)
{
    ASSERT(Rule);

    if(Rule->Owner)  free(Rule->Owner);
    if(Rule->Name)   free(Rule->Name);
    if(Rule->Except) free(Rule->Except);
    if(Rule->State)  free(Rule->State);
    free(Rule);
}

void FreeWindowRules()
{
    for(size_t Index = 0;
        Index < WindowRules.size();
        ++Index)
    {
        window_rule *Rule = WindowRules[Index];
        FreeWindowRule(Rule);
    }

    WindowRules.clear();
}
