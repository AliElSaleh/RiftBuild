#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main(void)
{
    int Result = 1;

#ifdef _WIN32
    // Icon writes the exe icon as resource id 1, so the shell prefers it over the named
    // ones (a numeric id sorts before a name). Each Icon.<NAME> adds one more group.
    HRSRC ExeIcon  = FindResourceA(NULL, MAKEINTRESOURCEA(1), (LPCSTR)RT_GROUP_ICON);
    HRSRC GlfwIcon = FindResourceA(NULL, "GLFW_ICON", (LPCSTR)RT_GROUP_ICON);
    HRSRC TrayIcon = FindResourceA(NULL, "TRAY_ICON", (LPCSTR)RT_GROUP_ICON);

    if (ExeIcon == NULL)
    {
        printf("FAIL: no icon at resource id 1 - Icon did not write the exe icon\n");
    }
    else if (GlfwIcon == NULL)
    {
        printf("FAIL: no icon named GLFW_ICON - Icon.GLFW_ICON was dropped\n");
    }
    else if (TrayIcon == NULL)
    {
        printf("FAIL: no icon named TRAY_ICON - Icon.TRAY_ICON was dropped\n");
    }
    else
    {
        printf("OK named icons: id 1, GLFW_ICON and TRAY_ICON all present\n");
        Result = 0;
    }
#else
    printf("OK named icons: skipped (icon resource names are windows-only)\n");
    Result = 0;
#endif

    return Result;
}
