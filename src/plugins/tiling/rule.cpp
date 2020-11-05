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

internal std::vector<window_rule *> WindowRules;

internal inline bool
RegexMatchPattern(regex_t *Regex, const char *Match, const char *Pattern)
{
    int Error = regcomp(Regex, Pattern, REG_EXTENDED);
    if (!Error) {
        Error = regexec(Regex, Match, 0, NULL, 0);
        regfree(Regex);
    } else {
        c_log(C_LOG_LEVEL_WARN, "chunkwm-tiling: window rule - could not compile regex!\n");
    }

    return Error == 0;
}

internal inline void
ApplyWindowRuleState(macos_window *Window, window_rule *Rule)
{
    if (StringEquals(Rule->State, "float")) {
        if (!AXLibHasFlags(Window, Window_Float)) {
            UntileWindow(Window);
            FloatWindow(Window);
        }
    } else if (StringEquals(Rule->State, "sticky")) {
        ExtendedDockSetWindowSticky(Window, 1);
        AXLibAddFlags(Window, Window_Sticky);

        if (!AXLibHasFlags(Window, Window_Float)) {
            UntileWindow(Window);
            FloatWindow(Window);
        }
    } else if (StringEquals(Rule->State, "tile")) {
        if (!AXLibHasFlags(Window, Window_ForceTile)) {
            UnfloatWindow(Window);
            AXLibAddFlags(Window, Window_ForceTile);

            if (!AXLibHasFlags(Window, Window_Minimized)) {
                macos_space *Space;
                bool Success = AXLibActiveSpace(&Space);
                ASSERT(Success);

                if (AXLibSpaceHasWindow(Space->Id, Window->Id)) {
                    TileWindow(Window);
                    AXLibAddFlags(Window, Rule_State_Tiled);
                }

                AXLibDestroySpace(Space);
            }
        }
    } else if (StringEquals(Rule->State, "native-fullscreen")) {
        if (!AXLibIsWindowFullscreen(Window->Ref)) {
            AXLibSetWindowFullscreen(Window->Ref, true);
            AXLibAddFlags(Window, Rule_Desktop_Changed);
        }
    } else {
        c_log(C_LOG_LEVEL_WARN, "chunkwm-tiling: window rule - invalid state '%s', ignored..\n", Rule->State);
    }
}

internal inline void
ApplyWindowRuleDesktop(macos_window *Window, window_rule *Rule)
{
    if (SendWindowToDesktop(Window, Rule->Desktop)) {
        AXLibAddFlags(Window, Rule_Desktop_Changed);
        if (Rule->FollowDesktop) {
            FocusDesktop(Rule->Desktop);
            AXLibSetFocusedWindow(Window->Ref);
            AXLibSetFocusedApplication(Window->Owner->PSN);
        }
    }
}

internal inline void
ApplyWindowRuleMonitor(macos_window *Window, window_rule *Rule)
{
    if (SendWindowToMonitor(Window, Rule->Monitor)) {
        AXLibAddFlags(Window, Rule_Desktop_Changed);
        if (Rule->FollowDesktop) {
            FocusMonitor(Rule->Monitor);
            AXLibSetFocusedWindow(Window->Ref);
            AXLibSetFocusedApplication(Window->Owner->PSN);
        }
    }
}

internal inline void
ApplyWindowRuleLevel(macos_window *Window, window_rule *Rule)
{
    int LevelKey;
    if (sscanf(Rule->Level, "%d", &LevelKey) == 1) {
        ExtendedDockSetWindowLevel(Window, LevelKey);
    }
}

internal inline void
ApplyWindowRuleAlpha(macos_window *Window, window_rule *Rule)
{
    float Alpha;
    if (sscanf(Rule->Alpha, "%f", &Alpha) == 1) {
        ExtendedDockSetWindowAlpha(Window->Id, Alpha);
        AXLibAddFlags(Window, Rule_Alpha_Changed);
    }
}

internal inline void
ApplyWindowRuleGridLayout(macos_window *Window, window_rule *Rule)
{
    GridLayout(Window, Rule->GridLayout);
}

internal inline void
ApplyWindowRule(macos_window *Window, window_rule *Rule)
{
    regex_t Regex;

    bool Match = true;
    if (Rule->Owner && Window->Owner->Name) {
        Match = RegexMatchPattern(&Regex, Window->Owner->Name, Rule->Owner);
        if (!Match) return;
    }

    if (Rule->Name && Window->Name) {
        Match &= RegexMatchPattern(&Regex, Window->Name, Rule->Name);
        if (!Match) return;
    }

    if (Rule->Role && Window->Mainrole) {
        Match &= CFEqual(Rule->Role, Window->Mainrole);
        if (!Match) return;
    }

    if (Rule->Subrole && Window->Subrole) {
        Match &= CFEqual(Rule->Subrole, Window->Subrole);
        if (!Match) return;
    }

    if (Rule->Except && Window->Name) {
        Match &= !RegexMatchPattern(&Regex, Window->Name, Rule->Except);
        if (!Match) return;
    }

    if (Rule->Desktop)    ApplyWindowRuleDesktop(Window, Rule);
    if (Rule->Monitor)    ApplyWindowRuleMonitor(Window, Rule);
    if (Rule->State)      ApplyWindowRuleState(Window, Rule);
    if (Rule->Level)      ApplyWindowRuleLevel(Window, Rule);
    if (Rule->Alpha)      ApplyWindowRuleAlpha(Window, Rule);
    if (Rule->GridLayout) ApplyWindowRuleGridLayout(Window, Rule);
}

void ApplyRulesForWindow(macos_window *Window)
{
    for (size_t Index = 0; Index < WindowRules.size(); ++Index) {
        window_rule *Rule = WindowRules[Index];
        ApplyWindowRule(Window, Rule);
    }
}

void ApplyRulesForWindowOnTitleChanged(macos_window *Window)
{
    for (size_t Index = 0; Index < WindowRules.size(); ++Index) {
        window_rule *Rule = WindowRules[Index];
        if (Rule->Name || Rule->Except) {
            ApplyWindowRule(Window, Rule);
        }
    }
}

internal void
ApplyRuleToExistingWindows(window_rule *Rule)
{
    macos_window_map Windows = CopyWindowCache();
    for (macos_window_map_it It = Windows.begin(); It != Windows.end(); ++It) {
        macos_window *Window = It->second;
        ApplyWindowRule(Window, Rule);
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
    if (Rule->Owner)      free(Rule->Owner);
    if (Rule->Name)       free(Rule->Name);
    if (Rule->Role)       CFRelease(Rule->Role);
    if (Rule->Subrole)    CFRelease(Rule->Subrole);
    if (Rule->Except)     free(Rule->Except);
    if (Rule->State)      free(Rule->State);
    if (Rule->Level)      free(Rule->Level);
    if (Rule->Alpha)      free(Rule->Alpha);
    if (Rule->GridLayout) free(Rule->GridLayout);
    free(Rule);
}

void FreeWindowRules()
{
    for (size_t Index = 0; Index < WindowRules.size(); ++Index) {
        window_rule *Rule = WindowRules[Index];
        FreeWindowRule(Rule);
    }

    WindowRules.clear();
}
