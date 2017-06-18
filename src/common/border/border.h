#ifndef CHUNKWM_COMMON_BORDER_H
#define CHUNKWM_COMMON_BORDER_H

struct border_window
{
    int Width;
    int Radius;
    unsigned Color;
};

border_window *CreateBorderWindow(int X, int Y, int W, int H, int BorderWidth, int BorderRadius, unsigned BorderColor);
void UpdateBorderWindow(border_window *Border, int X, int Y, int W, int H);
void SetBorderWindowColor(border_window *Border, unsigned BorderColor);
void DestroyBorderWindow(border_window *Border);

#endif
