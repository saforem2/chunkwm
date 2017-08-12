#ifndef PRESEL_H
#define PRESEL_H

#define PRESEL_TYPE_NORTH 0
#define PRESEL_TYPE_EAST  1
#define PRESEL_TYPE_SOUTH 2
#define PRESEL_TYPE_WEST  3

struct presel_window
{
    int Type;
    int Width;
    unsigned Color;
};

presel_window *CreatePreselWindow(int Type, int X, int Y, int W, int H, int Width, unsigned Color);
void UpdatePreselWindow(presel_window *Window, int X, int Y, int W, int H);
void DestroyPreselWindow(presel_window *Window);

#endif
