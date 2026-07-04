#include <iostream>
#include <GLFW/glfw3.h>
#include "MyLib.h"

int main()
{
	std::cout << "Hello World" << std::endl;

	MyLib lib;
	lib.setNumber(45698536);

	std::cout << "lib: " << lib.getNumber() << std::endl;

	int ExitCode = -1;

	if (glfwInit())
	{
		GLFWwindow* window = glfwCreateWindow(640, 480, "Hello Worldo", NULL, NULL);
		if (window)
		{
			glfwMakeContextCurrent(window);

			while (!glfwWindowShouldClose(window))
			{
				glfwSwapBuffers(window);
				glfwPollEvents();
			}

			ExitCode = 0;
		}
	}

	glfwTerminate();

	return ExitCode;
}
