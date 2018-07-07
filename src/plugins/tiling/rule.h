#ifndef PLUGIN_RULE_H
#define PLUGIN_RULE_H

#include <CoreFoundation/CFString.h>
#include <stdint.h>

enum window_rule_flags
{
    Rule_State_Tiled = (1 << 10),
    Rule_Desktop_Changed = (1 << 11)
};
struct window_rule
{
    char *Owner;
    char *Name;
    CFStringRef Role;
    CFStringRef Subrole;
    char *Except;
    char *State;
    char *Desktop;
    bool FollowDesktop;
    char *Level;
    char *Alpha;
    char *GridLayout;
};

void AddWindowRule(window_rule *Rule);

struct macos_window;
void ApplyRulesForWindow(macos_window *Window);
void ApplyRulesForWindowOnTitleChanged(macos_window *Window);
void FreeWindowRules();

static inline bool
RuleChangedDesktop(uint32_t Flags)
{
    bool Result = ((Flags & Rule_Desktop_Changed) != 0);
    return Result;
}

static inline bool
RuleTiledWindow(uint32_t Flags)
{
    bool Result = ((Flags & Rule_State_Tiled) != 0);
    return Result;
}

#endif
