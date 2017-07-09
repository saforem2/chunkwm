#ifndef PLUGIN_RULE_H
#define PLUGIN_RULE_H

#include <stdint.h>

enum window_rule_flags
{
    Rule_State_Tiled = (1 << 0),
    Rule_Desktop_Changed = (1 << 1)
};
struct window_rule
{
    char *Owner;
    char *Name;
    char *Except;
    char *State;
    char *Desktop;
};

void AddWindowRule(window_rule *Rule);

struct macos_window;
uint32_t ApplyRulesForWindow(macos_window *Window);
void FreeWindowRules();

static inline bool
RuleChangedDesktop(uint32_t RuleResult)
{
    bool Result = ((RuleResult & Rule_Desktop_Changed) != 0);
    return Result;
}

static inline bool
RuleTiledWindow(uint32_t RuleResult)
{
    bool Result = ((RuleResult & Rule_State_Tiled) != 0);
    return Result;
}

#endif
