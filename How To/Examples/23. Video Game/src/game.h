#pragma once

/* Field size in game units - also the initial window size. */
#define GAME_WIDTH  800
#define GAME_HEIGHT 600

typedef struct
{
    int bLeft;    /* left arrow or A held  */
    int bRight;   /* right arrow or D held */
    int bLaunch;  /* space held            */
    int bRestart; /* R held                */
} GameInput;

void Game_Init(void);
void Game_Update(const GameInput* Input, float DeltaTime);
void Game_Render(int FramebufferWidth, int FramebufferHeight);
