#pragma once

/* A tiny 2D renderer on top of fixed-function OpenGL.

   Coordinates are in "field" units with (0, 0) at the top-left corner,
   independent of the actual framebuffer size. */

/* Clears the screen and sets up an orthographic projection so that drawing
   happens in a FieldWidth x FieldHeight coordinate space. */
void Render_BeginFrame(int FramebufferWidth, int FramebufferHeight,
                       float FieldWidth, float FieldHeight,
                       float R, float G, float B);

void Render_Rect(float X, float Y, float W, float H, float R, float G, float B);

/* Draws Value using seven-segment digits. Size is the digit height; each
   digit advances the cursor by 0.8 * Size. Negative values draw as 0. */
void Render_Number(float X, float Y, float Size, int Value, float R, float G, float B);

/* Width of Render_Number output, for centering. */
float Render_NumberWidth(float Size, int Value);
