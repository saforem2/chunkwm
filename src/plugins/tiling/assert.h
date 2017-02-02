#ifndef PLUGIN_ASSERT_H
#define PLUGIN_ASSERT_H

#define Assert(Condition) do { if(!(Condition)) {\
    printf("#%d:%s assert failed '%s'\n",        \
            __LINE__, __FUNCTION__, #Condition); \
    *(int volatile *)0 = 0;                      \
    } } while(0)

#endif
