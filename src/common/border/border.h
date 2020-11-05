#ifndef CHUNKWM_COMMON_BORDER_H
#define CHUNKWM_COMMON_BORDER_H

struct border_window
{
    int Width;
    int Radius;
    unsigned Color;
    bool Outline;
};

border_window *CreateBorderWindow(int X, int Y, int W, int H, int BorderWidth, int BorderRadius, unsigned int BorderColor, bool BorderOutline);
void UpdateBorderWindowRect(border_window *Border, int X, int Y, int W, int H, bool BorderOutline);
void UpdateBorderWindowColor(border_window *Border, unsigned Color);
void UpdateBorderWindowWidth(border_window *Border, int BorderWidth);
void DestroyBorderWindow(border_window *Border);

#endif
