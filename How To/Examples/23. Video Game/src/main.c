#include <GLFW/glfw3.h>
#include "game.h"

static int IsKeyHeld(GLFWwindow* Window, int KeyA, int KeyB)
{
    return glfwGetKey(Window, KeyA) == GLFW_PRESS ||
           glfwGetKey(Window, KeyB) == GLFW_PRESS;
}

int main(void)
{
    int ExitCode = -1;

    if (glfwInit())
    {
        GLFWwindow* Window = glfwCreateWindow(GAME_WIDTH, GAME_HEIGHT, "Breakout", NULL, NULL);
        if (Window)
        {
            glfwMakeContextCurrent(Window);
            glfwSwapInterval(1); /* vsync */

            Game_Init();

            double LastTime = glfwGetTime();

            while (!glfwWindowShouldClose(Window))
            {
                double Now = glfwGetTime();
                float DeltaTime = (float)(Now - LastTime);
                LastTime = Now;

                /* Never step the simulation by more than 50ms, no matter how
                   long the window was stalled (dragged, minimized, ...). */
                if (DeltaTime > 0.05f)
                {
                    DeltaTime = 0.05f;
                }

                if (glfwGetKey(Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                {
                    glfwSetWindowShouldClose(Window, 1);
                }

                GameInput Input;
                Input.bLeft    = IsKeyHeld(Window, GLFW_KEY_LEFT,  GLFW_KEY_A);
                Input.bRight   = IsKeyHeld(Window, GLFW_KEY_RIGHT, GLFW_KEY_D);
                Input.bLaunch  = glfwGetKey(Window, GLFW_KEY_SPACE) == GLFW_PRESS;
                Input.bRestart = glfwGetKey(Window, GLFW_KEY_R) == GLFW_PRESS;

                Game_Update(&Input, DeltaTime);

                int FramebufferWidth = 0;
                int FramebufferHeight = 0;
                glfwGetFramebufferSize(Window, &FramebufferWidth, &FramebufferHeight);

                Game_Render(FramebufferWidth, FramebufferHeight);

                glfwSwapBuffers(Window);
                glfwPollEvents();
            }

            ExitCode = 0;
        }

        glfwTerminate();
    }

    return ExitCode;
}
