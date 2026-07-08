/* Breakout. Pure game logic - no GLFW in here. Windowing and input live in
   main.c, drawing primitives live in the Engine library. */

#include "game.h"
#include "Engine.h"
#include <math.h>

#define PADDLE_WIDTH   100.0f
#define PADDLE_HEIGHT  14.0f
#define PADDLE_Y       ((float)GAME_HEIGHT - 40.0f)
#define PADDLE_SPEED   520.0f

#define BALL_SIZE      10.0f
#define BALL_SPEED     380.0f  /* starting speed, px/s        */
#define BALL_MAX_SPEED 700.0f  /* paddle hits speed it up     */
#define BALL_MAX_ANGLE 1.05f   /* ~60 degrees off vertical    */

#define BRICK_COLS     10
#define BRICK_ROWS     6
#define BRICK_WIDTH    70.0f
#define BRICK_HEIGHT   24.0f
#define BRICK_GAP      8.0f
#define BRICK_TOP      70.0f
#define BRICK_LEFT     (((float)GAME_WIDTH - ((float)BRICK_COLS * BRICK_WIDTH + (float)(BRICK_COLS - 1) * BRICK_GAP)) * 0.5f)

typedef enum
{
    State_Ready,    /* ball sits on the paddle, waiting for launch */
    State_Playing,
    State_GameOver,
    State_Won
} GameState;

static struct
{
    GameState State;
    float     PaddleX;
    float     BallX, BallY;   /* top-left corner of the ball */
    float     BallVX, BallVY;
    float     BallSpeed;
    int       Bricks[BRICK_ROWS][BRICK_COLS]; /* 1 = alive */
    int       BricksLeft;
    int       Score;
    int       Lives;
    float     LaunchSide;     /* alternates so serves are not all identical */
    float     Time;           /* for blink effects */
} G;

static const float RowColors[BRICK_ROWS][3] =
{
    { 0.90f, 0.25f, 0.25f },
    { 0.95f, 0.55f, 0.20f },
    { 0.95f, 0.85f, 0.25f },
    { 0.35f, 0.80f, 0.35f },
    { 0.30f, 0.55f, 0.95f },
    { 0.65f, 0.40f, 0.90f },
};

static float BrickX(int Col)
{
    return BRICK_LEFT + (float)Col * (BRICK_WIDTH + BRICK_GAP);
}

static float BrickY(int Row)
{
    return BRICK_TOP + (float)Row * (BRICK_HEIGHT + BRICK_GAP);
}

static int RectsOverlap(float AX, float AY, float AW, float AH,
                        float BX, float BY, float BW, float BH)
{
    return AX < BX + BW && AX + AW > BX &&
           AY < BY + BH && AY + AH > BY;
}

static void PlaceBallOnPaddle(void)
{
    G.BallX = G.PaddleX + PADDLE_WIDTH * 0.5f - BALL_SIZE * 0.5f;
    G.BallY = PADDLE_Y - BALL_SIZE - 2.0f;
    G.BallVX = 0.0f;
    G.BallVY = 0.0f;
}

void Game_Init(void)
{
    G.State = State_Ready;
    G.PaddleX = ((float)GAME_WIDTH - PADDLE_WIDTH) * 0.5f;
    G.BallSpeed = BALL_SPEED;
    G.Score = 0;
    G.Lives = 3;
    G.LaunchSide = 1.0f;
    G.Time = 0.0f;
    G.BricksLeft = BRICK_ROWS * BRICK_COLS;

    for (int Row = 0; Row < BRICK_ROWS; Row++)
    {
        for (int Col = 0; Col < BRICK_COLS; Col++)
        {
            G.Bricks[Row][Col] = 1;
        }
    }

    PlaceBallOnPaddle();
}

static void MovePaddle(const GameInput* Input, float DeltaTime)
{
    if (Input->bLeft)
    {
        G.PaddleX -= PADDLE_SPEED * DeltaTime;
    }

    if (Input->bRight)
    {
        G.PaddleX += PADDLE_SPEED * DeltaTime;
    }

    if (G.PaddleX < 0.0f)
    {
        G.PaddleX = 0.0f;
    }

    if (G.PaddleX > (float)GAME_WIDTH - PADDLE_WIDTH)
    {
        G.PaddleX = (float)GAME_WIDTH - PADDLE_WIDTH;
    }
}

static void BounceOffPaddle(void)
{
    /* Where the ball lands on the paddle picks the bounce angle: dead center
       goes straight up, the edges send it out at BALL_MAX_ANGLE. This is what
       makes Breakout aimable. */
    float BallCenter = G.BallX + BALL_SIZE * 0.5f;
    float PaddleCenter = G.PaddleX + PADDLE_WIDTH * 0.5f;
    float Offset = (BallCenter - PaddleCenter) / (PADDLE_WIDTH * 0.5f);

    if (Offset < -1.0f)
    {
        Offset = -1.0f;
    }

    if (Offset > 1.0f)
    {
        Offset = 1.0f;
    }

    /* Each return also speeds the ball up a little. */
    G.BallSpeed *= 1.04f;
    if (G.BallSpeed > BALL_MAX_SPEED)
    {
        G.BallSpeed = BALL_MAX_SPEED;
    }

    float Angle = Offset * BALL_MAX_ANGLE;
    G.BallVX = sinf(Angle) * G.BallSpeed;
    G.BallVY = -cosf(Angle) * G.BallSpeed;
    G.BallY = PADDLE_Y - BALL_SIZE;
}

static void HitBricks(void)
{
    int bDone = 0;

    for (int Row = 0; Row < BRICK_ROWS && !bDone; Row++)
    {
        for (int Col = 0; Col < BRICK_COLS && !bDone; Col++)
        {
            if (G.Bricks[Row][Col] &&
                RectsOverlap(G.BallX, G.BallY, BALL_SIZE, BALL_SIZE,
                             BrickX(Col), BrickY(Row), BRICK_WIDTH, BRICK_HEIGHT))
            {
                G.Bricks[Row][Col] = 0;
                G.BricksLeft--;
                G.Score += (BRICK_ROWS - Row) * 10; /* top rows are worth more */

                /* Bounce along the axis of least penetration: if the ball
                   overlaps the brick more horizontally than vertically, it
                   came in from the top or bottom, and vice versa. */
                float PenetrationX = (G.BallX + BALL_SIZE < BrickX(Col) + BRICK_WIDTH * 0.5f)
                                         ? G.BallX + BALL_SIZE - BrickX(Col)
                                         : BrickX(Col) + BRICK_WIDTH - G.BallX;
                float PenetrationY = (G.BallY + BALL_SIZE < BrickY(Row) + BRICK_HEIGHT * 0.5f)
                                         ? G.BallY + BALL_SIZE - BrickY(Row)
                                         : BrickY(Row) + BRICK_HEIGHT - G.BallY;

                if (PenetrationX < PenetrationY)
                {
                    G.BallVX = -G.BallVX;
                }
                else
                {
                    G.BallVY = -G.BallVY;
                }

                bDone = 1; /* one brick per frame keeps bounces sane */
            }
        }
    }
}

static void UpdatePlaying(float DeltaTime)
{
    G.BallX += G.BallVX * DeltaTime;
    G.BallY += G.BallVY * DeltaTime;

    /* side and top walls */
    if (G.BallX < 0.0f)
    {
        G.BallX = 0.0f;
        G.BallVX = -G.BallVX;
    }

    if (G.BallX > (float)GAME_WIDTH - BALL_SIZE)
    {
        G.BallX = (float)GAME_WIDTH - BALL_SIZE;
        G.BallVX = -G.BallVX;
    }

    if (G.BallY < 0.0f)
    {
        G.BallY = 0.0f;
        G.BallVY = -G.BallVY;
    }

    /* paddle - only when the ball is heading down */
    if (G.BallVY > 0.0f &&
        RectsOverlap(G.BallX, G.BallY, BALL_SIZE, BALL_SIZE,
                     G.PaddleX, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT))
    {
        BounceOffPaddle();
    }

    HitBricks();

    if (G.BricksLeft == 0)
    {
        G.State = State_Won;
    }

    /* bottom - lose a life */
    if (G.BallY > (float)GAME_HEIGHT)
    {
        G.Lives--;
        G.BallSpeed = BALL_SPEED;

        if (G.Lives > 0)
        {
            G.State = State_Ready;
            PlaceBallOnPaddle();
        }
        else
        {
            G.State = State_GameOver;
        }
    }
}

void Game_Update(const GameInput* Input, float DeltaTime)
{
    G.Time += DeltaTime;

    if (Input->bRestart)
    {
        Game_Init();
    }

    if (G.State == State_Ready)
    {
        MovePaddle(Input, DeltaTime);
        PlaceBallOnPaddle();

        if (Input->bLaunch)
        {
            float Angle = 0.35f * G.LaunchSide;
            G.LaunchSide = -G.LaunchSide;
            G.BallVX = sinf(Angle) * G.BallSpeed;
            G.BallVY = -cosf(Angle) * G.BallSpeed;
            G.State = State_Playing;
        }
    }
    else if (G.State == State_Playing)
    {
        MovePaddle(Input, DeltaTime);
        UpdatePlaying(DeltaTime);
    }

    /* State_GameOver and State_Won just wait for bRestart. */
}

void Game_Render(int FramebufferWidth, int FramebufferHeight)
{
    int bGameEnded = G.State == State_GameOver || G.State == State_Won;

    if (G.State == State_Won)
    {
        Render_BeginFrame(FramebufferWidth, FramebufferHeight,
                          (float)GAME_WIDTH, (float)GAME_HEIGHT, 0.05f, 0.16f, 0.09f);
    }
    else
    {
        Render_BeginFrame(FramebufferWidth, FramebufferHeight,
                          (float)GAME_WIDTH, (float)GAME_HEIGHT, 0.07f, 0.07f, 0.10f);
    }

    /* bricks (dimmed once the game has ended) */
    float Dim = bGameEnded ? 0.35f : 1.0f;
    for (int Row = 0; Row < BRICK_ROWS; Row++)
    {
        for (int Col = 0; Col < BRICK_COLS; Col++)
        {
            if (G.Bricks[Row][Col])
            {
                Render_Rect(BrickX(Col), BrickY(Row), BRICK_WIDTH, BRICK_HEIGHT,
                            RowColors[Row][0] * Dim, RowColors[Row][1] * Dim, RowColors[Row][2] * Dim);
            }
        }
    }

    /* paddle - red when the game is lost */
    if (G.State == State_GameOver)
    {
        Render_Rect(G.PaddleX, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT, 0.85f, 0.20f, 0.20f);
    }
    else
    {
        Render_Rect(G.PaddleX, PADDLE_Y, PADDLE_WIDTH, PADDLE_HEIGHT, 0.85f, 0.85f, 0.90f);
    }

    /* ball - blinks while waiting on the paddle */
    int bBlinkOn = fmodf(G.Time, 0.8f) < 0.5f;
    if (G.State == State_Playing || (G.State == State_Ready && bBlinkOn))
    {
        Render_Rect(G.BallX, G.BallY, BALL_SIZE, BALL_SIZE, 0.95f, 0.95f, 0.95f);
    }

    /* HUD: score top-left, lives as little paddles top-right */
    Render_Number(14.0f, 12.0f, 26.0f, G.Score, 0.9f, 0.9f, 0.9f);

    for (int i = 0; i < G.Lives; i++)
    {
        Render_Rect((float)GAME_WIDTH - 14.0f - (float)(i + 1) * 30.0f, 20.0f,
                    22.0f, 8.0f, 0.85f, 0.85f, 0.90f);
    }

    /* final score, front and center, when the game has ended */
    if (bGameEnded)
    {
        float Size = 64.0f;
        float X = ((float)GAME_WIDTH - Render_NumberWidth(Size, G.Score)) * 0.5f;

        if (G.State == State_Won)
        {
            Render_Number(X, 280.0f, Size, G.Score, 0.4f, 0.95f, 0.5f);
        }
        else
        {
            Render_Number(X, 280.0f, Size, G.Score, 0.95f, 0.4f, 0.4f);
        }
    }
}
