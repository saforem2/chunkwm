#include "rule.h"
#include "controller.h"
#include "misc.h"

#include "../../common/misc/assert.h"
#include "../../common/accessibility/window.h"
#include "../../common/accessibility/application.h"
#include "../../common/misc/assert.h"

#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include <vector>

#define internal static

// TODO(koekeishiya): Thread-Safety !!!
internal std::vector<window_rule *> WindowRules;

inline bool
RegexMatchPattern(regex_t *Regex, const char *Match, const char *Pattern)
{
    int Result = regcomp(Regex, Pattern, REG_EXTENDED);
    if(Result)
    {
        fprintf(stderr, "tiling: window rule - could not compile regex!\n");
        goto out;
    }

    Result = regexec(Regex, Match, 0, NULL, 0);
    regfree(Regex);

out:;
    return Result == 0;
}

internal void
ApplyWindowRuleState(macos_window *Window, window_rule *Rule)
{
    if(StringEquals(Rule->State, "float"))
    {
        FloatWindow(Window, false);
    }
    else if(StringEquals(Rule->State, "tile"))
    {
        AXLibAddFlags(Window, Window_ForceTile);
    }
    else
    {
        fprintf(stderr, "tiling: window rule - invalid state '%s', ignored..\n", Rule->State);
    }
}

internal void
ApplyWindowRule(macos_window *Window, window_rule *Rule)
{
    regex_t Regex;
    bool MatchOwner, MatchName;
    bool FilterOwner, FilterName;

    FilterOwner = Rule->Owner != NULL;
    FilterName = Rule->Name != NULL;

    MatchOwner = (Window->Owner->Name && Rule->Owner)
               ? RegexMatchPattern(&Regex, Window->Owner->Name, Rule->Owner)
               : false;

    MatchName = (Window->Name && Rule->Name)
              ? RegexMatchPattern(&Regex, Window->Name, Rule->Name)
              : false;

    if((FilterOwner) &&
       (FilterName))
    {
        if((MatchOwner) &&
           (MatchName))
        {
            ApplyWindowRuleState(Window, Rule);
        }
    }
    else if(FilterOwner)
    {
        if(MatchOwner)
        {
            ApplyWindowRuleState(Window, Rule);
        }
    }
    else if(FilterName)
    {
        if(MatchName)
        {
            ApplyWindowRuleState(Window, Rule);
        }
    }
}

void ApplyRulesForWindow(macos_window *Window)
{
    for(size_t Index = 0;
        Index < WindowRules.size();
        ++Index)
    {
        window_rule *Rule = WindowRules[Index];
        ApplyWindowRule(Window, Rule);
    }
}

void AddWindowRule(window_rule *Rule)
{
    window_rule *Result = (window_rule *) malloc(sizeof(window_rule));
    memcpy(Result, Rule, sizeof(window_rule));
    WindowRules.push_back(Result);
}

internal void
FreeWindowRule(window_rule *Rule)
{
    ASSERT(Rule);

    if(Rule->Owner) free(Rule->Owner);
    if(Rule->Name)  free(Rule->Name);
    if(Rule->State) free(Rule->State);
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
