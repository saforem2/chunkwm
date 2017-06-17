#ifndef PLUGIN_RULE_H
#define PLUGIN_RULE_H

struct window_rule
{
    char *Owner;
    char *Name;
    char *State;
};

void AddWindowRule(window_rule *Rule);

struct macos_window;
void ApplyRulesForWindow(macos_window *Window);
void FreeWindowRules();

#endif
