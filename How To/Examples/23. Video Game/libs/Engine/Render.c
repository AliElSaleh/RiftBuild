#include "Engine.h"

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <GL/gl.h>
#elif defined(__APPLE__)
    #define GL_SILENCE_DEPRECATION
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif

void Render_BeginFrame(int FramebufferWidth, int FramebufferHeight,
                       float FieldWidth, float FieldHeight,
                       float R, float G, float B)
{
    glViewport(0, 0, FramebufferWidth, FramebufferHeight);

    glClearColor(R, G, B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, FieldWidth, FieldHeight, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void Render_Rect(float X, float Y, float W, float H, float R, float G, float B)
{
    glColor3f(R, G, B);

    glBegin(GL_QUADS);
    glVertex2f(X,     Y);
    glVertex2f(X + W, Y);
    glVertex2f(X + W, Y + H);
    glVertex2f(X,     Y + H);
    glEnd();
}
