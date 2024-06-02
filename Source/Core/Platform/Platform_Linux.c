#include "Platform.h"

#if PLATFORM_LINUX
#include "Input/Input.h"
#include "Log.h"

#include "Event/Event.h"
#include "FileWatcher.h"
#include "Profiling/ProfilingSubsystem.h"
#include "Uuid.h"
#include "Filesystem.h"
#include "Math/Math.h"
#include "String/BaseString.h"
#include "String/StringUtils.h"
#include "Structures/Array.h"

#define _XOPEN_SOURCE 700

#include <signal.h>
#include <stdio.h>


#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#define __USE_FILE_OFFSET64
#define __USE_GNU
//#define __USE_POSIX
#define __USE_MISC
//#define __USE_XOPEN_EXTENDED
//#define __USE_POSIX199309

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <ftw.h>
#include <fcntl.h>
#include <pwd.h>
#include <uuid/uuid.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <spawn.h>
#include <termios.h>
#include <semaphore.h>

extern int fileno (FILE *__stream) __THROW __wur;

// todo: pull these in locally
#undef internal // fucking x11 dude....
#include <xcb/xcb.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#define internal static 

struct WMSizeHints
{
  uint32_t flags;
  int32_t  x, y;
  int32_t  width, height;
  int32_t  min_width, min_height;
  int32_t  max_width, max_height;
  int32_t  width_inc, height_inc;
  int32_t  min_aspect_num, min_aspect_den;
  int32_t  max_aspect_num, max_aspect_den;
  int32_t  base_width, base_height;
  uint32_t win_gravity;
};

enum WMSizeHintsFlag
{
  WM_SIZE_HINT_US_POSITION   = 1U << 0,
  WM_SIZE_HINT_US_SIZE       = 1U << 1,
  WM_SIZE_HINT_P_POSITION    = 1U << 2,
  WM_SIZE_HINT_P_SIZE        = 1U << 3,
  WM_SIZE_HINT_P_MIN_SIZE    = 1U << 4,
  WM_SIZE_HINT_P_MAX_SIZE    = 1U << 5,
  WM_SIZE_HINT_P_RESIZE_INC  = 1U << 6,
  WM_SIZE_HINT_P_ASPECT      = 1U << 7,
  WM_SIZE_HINT_BASE_SIZE     = 1U << 8,
  WM_SIZE_HINT_P_WIN_GRAVITY = 1U << 9
};

#ifndef NO_VULKAN
#define VK_USE_PLATFORM_XCB_KHR
#include "Renderer/Vulkan/VulkanTypes.inl"
#include "Renderer/Vulkan/VulkanPlatform.h"
#include "Renderer/Vulkan/volk.h"
#endif

STRUCT(LinuxPlatformState)
{
    Display* Display;
    xcb_connection_t* Connection;
    xcb_window_t Window;
    xcb_screen_t* Screen;
    xcb_atom_t wm_protocols;
    xcb_atom_t wm_delete_win;

    #ifndef NO_VULKAN
    VkSurfaceKHR Surface;
    #endif

    bool bInitialized;
    u64 ThreadID;
};

#ifndef HEADLESS
static LinearAllocator GPlatformStateMemoryAllocator = {0};
static LinuxPlatformState* GLinuxPlatformState = NULL;
static u16 GCachedWidth = 0;
static u16 GCachedHeight = 0;
#endif

internal void LogLastError(const String Prefix)
{
    StringLocal(Message, 4096);
    String_Copy(&Message, CStr(strerror(errno)));

    LOG_ERROR("%S\n        errno %i\n        Reason: %S\n", Prefix, errno, Message);
}

bool Platform_Startup(void* State, const String ApplicationName, i32 X, i32 Y, u32 Width, u32 Height)
{
#ifndef HEADLESS
    LinearAllocator_Create(Platform_GetMemoryRequirement(), State, &GPlatformStateMemoryAllocator);

    GLinuxPlatformState = LinearAllocator_Allocate(&GPlatformStateMemoryAllocator, sizeof(LinuxPlatformState));
    GLinuxPlatformState->bInitialized = true;

    GLinuxPlatformState->Display = XOpenDisplay(NULL);

    XAutoRepeatOff(GLinuxPlatformState->Display);

    GLinuxPlatformState->Connection = XGetXCBConnection(GLinuxPlatformState->Display);

    GCachedWidth = (u16)Width;
    GCachedHeight = (u16)Height;

    if (xcb_connection_has_error(GLinuxPlatformState->Connection))
    {
        LOG_ERROR("Failed to connect to X server via XCB");
        return false;
    }

    const struct xcb_setup_t* Setup = xcb_get_setup(GLinuxPlatformState->Connection);

    // Lopp through screens using iterator
    i32 ScreenP = 0;
    xcb_screen_iterator_t It = xcb_setup_roots_iterator(Setup);
    for (i32 s = ScreenP; s > 0; s--)
    {
        xcb_screen_next(&It);
    }

    GLinuxPlatformState->Screen = It.data;
    GLinuxPlatformState->Window = xcb_generate_id(GLinuxPlatformState->Connection);

    // Register event types
    // XCB_CW_BACK_PIXEL = filling the window background with a color
    // XCB_CW_EVENT_MASK = which events the window should receive
    u32 EventMask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

    // Listen for keyboard and mouse buttons
    u32 EventValues = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                      XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                      XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_POINTER_MOTION |
                      XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    
    // Values to be sent over XCB
    u32 ValuesList[] = {GLinuxPlatformState->Screen->black_pixel, EventValues};

    xcb_void_cookie_t Cookie = xcb_create_window(GLinuxPlatformState->Connection, 
                                XCB_COPY_FROM_PARENT, // depth
                                GLinuxPlatformState->Window, 
                                GLinuxPlatformState->Screen->root, // parent window
                                (i16)X, (i16)Y, (u16)Width, (u16)Height, 
                                0, // no border
                                XCB_WINDOW_CLASS_INPUT_OUTPUT, 
                                GLinuxPlatformState->Screen->root_visual, 
                                EventMask, 
                                ValuesList);

    xcb_change_property(GLinuxPlatformState->Connection,
                         XCB_PROP_MODE_REPLACE, 
                         GLinuxPlatformState->Window,
                         XCB_ATOM_WM_NAME, 
                         XCB_ATOM_STRING,
                         8,
                         ApplicationName.Length,
                         ApplicationName.Data);

    // tell the server to notify when the window manager is trying to close the window
    const String WmDeleteWindow = S("WM_DELETE_WINDOW");
    const String WmProtocols = S("WM_PROTOCOLS");
    xcb_intern_atom_cookie_t DeleteCookie = xcb_intern_atom(GLinuxPlatformState->Connection, 0, (u16)WmDeleteWindow.Length, WmDeleteWindow.Data);
    xcb_intern_atom_cookie_t ProtocolsCookie = xcb_intern_atom(GLinuxPlatformState->Connection, 0, (u16)WmProtocols.Length, WmProtocols.Data);
    xcb_intern_atom_reply_t* DeleteReply = xcb_intern_atom_reply(GLinuxPlatformState->Connection, DeleteCookie, NULL);
    xcb_intern_atom_reply_t* ProtocolsReply = xcb_intern_atom_reply(GLinuxPlatformState->Connection, ProtocolsCookie, NULL);

    GLinuxPlatformState->wm_delete_win = DeleteReply->atom;
    GLinuxPlatformState->wm_protocols = ProtocolsReply->atom;

    xcb_change_property(GLinuxPlatformState->Connection,
                        XCB_PROP_MODE_REPLACE, 
                        GLinuxPlatformState->Window,
                        ProtocolsReply->atom, 
                        4,
                        32,
                        1,
                        &DeleteReply->atom);

    struct WMSizeHints hints = {0};

    // todo: to be configured by the application
    hints.flags = WM_SIZE_HINT_P_MIN_SIZE | WM_SIZE_HINT_P_WIN_GRAVITY;
    hints.win_gravity = XCB_GRAVITY_CENTER;
    hints.min_width = 320;
    hints.min_height = 240;
    
    xcb_change_property(GLinuxPlatformState->Connection,
                        XCB_PROP_MODE_REPLACE, 
                        GLinuxPlatformState->Window,
                        XCB_ATOM_WM_NORMAL_HINTS, 
                        XCB_ATOM_WM_SIZE_HINTS,
                        32,
                        sizeof(struct WMSizeHints) >> 2,
                        &hints);

    // hide the window from the screen (the application will show it when the engine is ready)
    xcb_map_window(GLinuxPlatformState->Connection, GLinuxPlatformState->Window);
    i32 StreamResult = xcb_flush(GLinuxPlatformState->Connection);
    if (StreamResult <= 0)
    {
        LOG_ERROR("Failed to flush the stream: %i", StreamResult);
        return false;
    }
#endif

    return true;
}

void Platform_Shutdown(void)
{
    #ifndef HEADLESS
    XAutoRepeatOn(GLinuxPlatformState->Display);

    xcb_destroy_window(GLinuxPlatformState->Connection, GLinuxPlatformState->Window);

    XCloseDisplay(GLinuxPlatformState->Display);
    #endif
}

u64 Platform_GetMemoryRequirement(void)
{
    return sizeof(LinuxPlatformState);
}

#ifndef HEADLESS
internal EKey TranslateKeycode(u64 XKeyCode)
{
    // todo: use a flat array instead of a switch
    // like this: return XKeyToEKey[XKeyCode];

    switch (XKeyCode)
    {
        case XK_0:
        return Key_0;

        case XK_1:
        return Key_1;

        case XK_2:
        return Key_2;

        case XK_3:
        return Key_3;

        case XK_4:
        return Key_4;

        case XK_5:
        return Key_5;

        case XK_6:
        return Key_6;

        case XK_7:
        return Key_7;

        case XK_8:
        return Key_8;

        case XK_9:
        return Key_9;

        case XK_a:
        case XK_A:
        return Key_A;

        case XK_b:
        case XK_B:
        return Key_B;

        case XK_c:
        case XK_C:
        return Key_C;

        case XK_d:
        case XK_D:
        return Key_D;

        case XK_e:
        case XK_E:
        return Key_E;

        case XK_f:
        case XK_F:
        return Key_F;

        case XK_g:
        case XK_G:
        return Key_G;

        case XK_h:
        case XK_H:
        return Key_H;

        case XK_i:
        case XK_I:
        return Key_I;

        case XK_j:
        case XK_J:
        return Key_J;

        case XK_k:
        case XK_K:
        return Key_K;

        case XK_l:
        case XK_L:
        return Key_L;

        case XK_m:
        case XK_M:
        return Key_M;

        case XK_n:
        case XK_N:
        return Key_N;

        case XK_o:
        case XK_O:
        return Key_O;

        case XK_p:
        case XK_P:
        return Key_P;

        case XK_q:
        case XK_Q:
        return Key_Q;

        case XK_r:
        case XK_R:
        return Key_R;

        case XK_s:
        case XK_S:
        return Key_S;

        case XK_t:
        case XK_T:
        return Key_T;

        case XK_u:
        case XK_U:
        return Key_U;

        case XK_v:
        case XK_V:
        return Key_V;

        case XK_w:
        case XK_W:
        return Key_W;

        case XK_x:
        case XK_X:
        return Key_X;

        case XK_y:
        case XK_Y:
        return Key_Y;

        case XK_z:
        case XK_Z:
        return Key_Z;

        case XK_asciitilde:
        case XK_grave:
        return Key_Grave;

        case XK_BackSpace:
        return Key_Backspace;

        case XK_Tab:
        return Key_Tab;

        case XK_Return:
        return Key_Enter;

        case XK_Shift_L:
        return Key_LeftShift;

        case XK_Shift_R:
        return Key_RightShift;

        case XK_Control_L:
        return Key_LeftControl;

        case XK_Control_R:
        return Key_RightControl;

        case XK_Alt_L:
        return Key_LeftAlt;

        case XK_Alt_R:
        return Key_RightAlt;

        case XK_Caps_Lock:
        return Key_Capital;

        case XK_Escape:
        return Key_Escape;

        case XK_space:
        return Key_Space;

        case XK_Left:
        return Key_Left;

        case XK_Up:
        return Key_Up;

        case XK_Right:
        return Key_Right;

        case XK_Down:
        return Key_Down;

        case XK_Delete:
        return Key_Delete;

        case XK_Home:
        return Key_Home;

        case XK_End:
        return Key_End;

        //case XK_Page_Up:
        //return Key_PageUp;
        //case XK_Page_Down:
        //return Key_PageDown;

        case XK_Insert:
        return Key_Insert;

        case XK_F1:
        return Key_F1;

        case XK_F2:
        return Key_F2;

        case XK_F3:
        return Key_F3;

        case XK_F4:
        return Key_F4;

        case XK_F5:
        return Key_F5;

        case XK_F6:
        return Key_F6;

        case XK_F7:
        return Key_F7;

        case XK_F8:
        return Key_F8;

        case XK_F9:
        return Key_F9;

        case XK_F10:
        return Key_F10;

        case XK_F11:
        return Key_F11;

        case XK_F12:
        return Key_F12;

        default:
        return Key_Null;
    }
}
#endif

bool Platform_PushMessages(void)
{
    #ifndef HEADLESS
    xcb_generic_event_t* Event = NULL;
    xcb_client_message_event_t* CM = NULL;

    bool bQuitFlagged = false;

    while (1)
    {
        Event = xcb_poll_for_event(GLinuxPlatformState->Connection);
        if (Event == NULL)
            break;

        switch (Event->response_type & ~0x80)
        {
            case XCB_KEY_PRESS:
            case XCB_KEY_RELEASE:
            {
                xcb_key_press_event_t* KeyEvent = (xcb_key_press_event_t*)Event;
                bool bPressed = Event->response_type == XCB_KEY_PRESS;
                KeySym Sym = XkbKeycodeToKeysym(GLinuxPlatformState->Display, KeyEvent->detail, 0, KeyEvent->detail & ShiftMask ? 1 : 0);
                if (Sym == XK_Escape)
                {
                    bQuitFlagged = true;
                }

                //LOG("Key: 0x%02X", Sym);
                EKey Key = TranslateKeycode(Sym);

                Input_ProcessKey(Key, bPressed);
            }
            break;

            case XCB_BUTTON_PRESS:
            case XCB_BUTTON_RELEASE:
            {
                xcb_button_press_event_t* ButtonEvent = (xcb_button_press_event_t*)Event;
                bool bPressed = Event->response_type == XCB_BUTTON_PRESS;
                EMouseButton ButtonType = MouseButton_Null;
                switch (ButtonEvent->detail)
                {
                    case XCB_BUTTON_INDEX_1:
                    ButtonType = MouseButton_Left;
                    break;
                    
                    case XCB_BUTTON_INDEX_2:
                    ButtonType = MouseButton_Middle;
                    break;

                    case XCB_BUTTON_INDEX_3:
                    ButtonType = MouseButton_Right;
                    break;
                }

                if (ButtonType != MouseButton_Null)
                {
                    Input_ProcessMouseButton(ButtonType, bPressed);
                }
            }
            break;

            case XCB_MOTION_NOTIFY:
            {
                xcb_motion_notify_event_t* MotionEvent = (xcb_motion_notify_event_t*)Event;

                Input_ProcessMouseMove(MotionEvent->event_x, MotionEvent->event_y);
            }
            break;

            case XCB_CONFIGURE_NOTIFY:
            {
                xcb_configure_notify_event_t* Configure = (xcb_configure_notify_event_t*)Event;
                if (((Configure->width != GCachedWidth) || (Configure->height != GCachedHeight)))
                {
                    u16 Width = Configure->width;
                    u16 Height = Configure->height;
                    GCachedWidth = Width;
                    GCachedHeight = Height;

                    EventContext Context = { 0 };
                    Context.Data.u16[0] = Width;
                    Context.Data.u16[1] = Height;
                    Event_Fire(EventCode_WindowResize, GLinuxPlatformState, Context);
                }
            }
            break;

            case XCB_CLIENT_MESSAGE:
            {
                CM = (xcb_client_message_event_t*)Event;

                // window close event
                if (CM->data.data32[0] == GLinuxPlatformState->wm_delete_win)
                {
                    bQuitFlagged = true;
                    Event_Fire(EventCode_ApplicationQuit, GLinuxPlatformState, EventContext_Null());
                }
            }

            default:
            break;
        }

        // what the actual fuck....
        free(Event);
    }

    return !bQuitFlagged;
    #endif // HEADLESS

    return true;
}

void Platform_ShowWindow(void)
{
    #ifndef HEADLESS
    xcb_map_window(GLinuxPlatformState->Connection, GLinuxPlatformState->Window);
    xcb_flush(GLinuxPlatformState->Connection);
    #endif
}

void Platform_HideWindow(void)
{
    #ifndef HEADLESS
    xcb_unmap_window(GLinuxPlatformState->Connection, GLinuxPlatformState->Window);
    xcb_flush(GLinuxPlatformState->Connection);
    #endif
}

internal void Internal_SignalHandler(int signal)
{
    Platform_Shutdown(); // will call XKeyReapeatOn and xcb_flush

    exit(1);
}

void Platform_PreInitialize(void)
{
    Platform_GetClockFrequency();

    struct sigaction act = {0};
    act.sa_handler = &Internal_SignalHandler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGKILL, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGILL, &act, NULL);
    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGSEGV, &act, NULL);
}

f64 Platform_GetClockFrequency(void)
{
    return 0;
}

void* Platform_GetWindowHandle(void)
{
    #ifndef HEADLESS
    return GLinuxPlatformState->Screen;
    #endif

    return NULL;
}

NO_RETURN void Platform_Abort(u32 ExitCode)
{
    exit((i32)ExitCode);
}

void* Platform_MemAlloc(u64 Size)
{
    return malloc(Size);
}

void* Platform_MemAllocZero(u64 Size)
{
    void* Mem = malloc(Size);
    memset(Mem, 0, Size);
    return Mem;
}

void* Platform_MemReAlloc(const void* Block, u64 Size)
{
    return realloc((void*)Block, Size);
}

void  Platform_MemFree(const void* Block)
{
    free((void*)Block);
}

void* Platform_MemZero(void* Block, u64 Size)
{
    return memset(Block, 0, Size);
}

void* Platform_MemCopy(void* restrict Dest, const void* restrict Source, u64 Size)
{
    return memcpy(Dest, Source, Size);
}

void* Platform_MemMove(void* restrict Dest, const void* restrict Source, u64 Size)
{
    return memmove(Dest, Source, Size);
}

void* Platform_MemSet(void* Dest, i32 Value, u64 Size)
{
    return memset(Dest, Value, Size);
}

bool Platform_MemEqual(const void* Block1, const void* Block2, u64 Size)
{
    return memcmp(Block1, Block2, Size) == 0;
}

/*
//General Formatting
#define GEN_FORMAT_RESET                "0"
#define GEN_FORMAT_BRIGHT               "1"
#define GEN_FORMAT_DIM                  "2"
#define GEN_FORMAT_UNDERSCORE           "3"
#define GEN_FORMAT_BLINK                "4"
#define GEN_FORMAT_REVERSE              "5"
#define GEN_FORMAT_HIDDEN               "6"

//Foreground Colors
#define FOREGROUND_COL_BLACK            "30"
#define FOREGROUND_COL_RED              "31"
#define FOREGROUND_COL_GREEN            "32"
#define FOREGROUND_COL_YELLOW           "33"
#define FOREGROUND_COL_BLUE             "34"
#define FOREGROUND_COL_MAGENTA          "35"
#define FOREGROUND_COL_CYAN             "36"
#define FOREGROUND_COL_WHITE            "37"

//Background Colors
#define BACKGROUND_COL_BLACK            "40"
#define BACKGROUND_COL_RED              "41"
#define BACKGROUND_COL_GREEN            "42"
#define BACKGROUND_COL_YELLOW           "43"
#define BACKGROUND_COL_BLUE             "44"
#define BACKGROUND_COL_MAGENTA          "45"
#define BACKGROUND_COL_CYAN             "46"
#define BACKGROUND_COL_WHITE            "47"
*/

void Platform_ConsoleWrite(const char* Message, u8 Color, bool bIsError)
{
    Platform_ConsoleWrite_CustomLength(Message, String_GetLength(Message), 0, false);
}

void Platform_ConsoleWrite_CustomLength(const char* Message, u64 Length, u8 Color, bool bIsError)
{
    static const char* colors[] = {"0;37", "0;32", "1;33", "1;31", "0;41", "0;37"};

    bool bIgnoreNewLine = Color == 4 && Message[Length-1] == '\n';
    if (UNLIKELY(bIgnoreNewLine))
        Length--;

    fprintf(UNLIKELY(bIsError) ? stderr : stdout, "\033[%.*sm%.*s\033[0m", 4, LIKELY(Color < 6) ? colors[Color] : "0:37", (i32)Length, Message);
    //printf("\033[%.*sm%.*s\033[0m", 4, LIKELY(Color < 6) ? colors[Color] : "0:37", (i32)Length, Message);

    if (UNLIKELY(bIgnoreNewLine))
        printf("\n");

    fflush(stdout);

    //fwrite(Message, 1, Length, bIsError ? stderr : stdout); // this shit prints nothing in some cases wtf??!?
}

PlatformHandle Platform_CreateThread(const String Name, u32* OutThreadID, u32 (*ThreadEntryPoint)(void* ThreadParameter), void* UserData)
{
    //UNIMPLEMENTED;
    //pthread_create();
    return -1;
}

PlatformHandle Platform_RunCommand(const String CmdLine, const String WorkingDirectory)
{
    String Command;
    StringLocal(Copy, MAX_PATH_LENGTH*2);
    if (WorkingDirectory.Length > 0)
    {
        String_Append(&Copy, S("cd \""));
        String_Append(&Copy, WorkingDirectory);
        String_EatPathSeparatorsInlineFromEnd(&Copy);
        String_Append(&Copy, S("\"; "));
        String_Append(&Copy, CmdLine);

        Command = Copy;
    }
    else
    {
        Command = CmdLine;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        LogLastError(S("fork() failed"));
    }
    else if (pid > 0)
    {
        return pid;
    }
    else
    {
        execvp("/bin/sh", (char*[]){"sh", "-c", Command.Data, NULL});

        exit(0);
    }

    return pid;
}

PlatformHandle Platform_RunCommand_Ex(const String CmdLine, const String WorkingDirectory, PlatformPipe* StdOutPipe)
{
    String Command;
    StringLocal(Copy, MAX_PATH_LENGTH*2);
    if (WorkingDirectory.Length > 0)
    {
        String_Append(&Copy, S("cd \""));
        String_Append(&Copy, WorkingDirectory);
        String_EatPathSeparatorsInlineFromEnd(&Copy);
        String_Append(&Copy, S("\"; "));
        String_Append(&Copy, CmdLine);

        Command = Copy;
    }
    else
    {
        Command = CmdLine;
    }

    i32 PipeData[2] = {0};
    if (pipe(PipeData) != 0)
    {
        LogLastError(S("pipe() failed"));
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        close(PipeData[0]);
        close(PipeData[1]);
        LogLastError(S("fork() failed"));
    }
    else if (pid > 0) // parent path
    {
        close(PipeData[0]);
        close(PipeData[1]);
        return pid;
    }
    else // child path
    {
        close(PipeData[0]);
        dup2(PipeData[1], STDOUT_FILENO);

        (*StdOutPipe)[0] = PipeData[0]; // read pipe
        (*StdOutPipe)[1] = PipeData[1]; // write pipe

        execvp("/bin/sh", (char*[]){"sh", "-c", Command.Data, NULL});
        exit(0);
    }

    return pid;
}

bool Platform_FindProgram(String ProgramName)
{
    return Platform_FindFile_Ex(ProgramName, S(""), NULL);
}

bool Platform_FindProgram_Ex(String FileName, String* OutFilePath)
{
    return Platform_FindFile_Ex(FileName, S(""), OutFilePath);
}

bool Platform_FindFile(String FileName, String ExtensionWithDot)
{
    return Platform_FindFile_Ex(FileName, ExtensionWithDot, NULL);
}

bool Platform_FindFile_Ex(String FileName, String ExtensionWithDot, String* OutFilePath)
{
    char* Path = getenv("PATH");
    const String PathStr = CStr(Path);

    bool bFound = false;

    StringLocal(P, MAX_PATH_LENGTH);
    u32 Offset = 0;
    u32 Len = 0;
    for (u32 i = 0; i < PathStr.Length; i++)
    {
        if (PathStr.Data[i] == ':' || i == PathStr.Length-1) // end of an entry, process it
        {
            String_Copy(&P, StrSlice(StrShiftF(PathStr, Offset).Data, Len));
            Offset = i+1;
            Len = 0;

            if (bFound)
                break;

            DIR* dir = opendir(P.Data);
            if (dir == NULL)
            {
                continue;
            }

            struct dirent* Entry = NULL;
            while ((Entry = readdir(dir)))
            {
                if (Entry->d_name[0] == '.' && 
                    (!Entry->d_name[1] || (Entry->d_name[1] == '.' && !Entry->d_name[2])))
                {
                    continue;
                }

                const String EntryName = CStr(Entry->d_name);

                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, P, EntryName);
                
                if (Entry->d_type != DT_DIR)
                {
                    u64 FileSize = 0;
                    struct stat filestat = {0};
                    stat(FullPath.Data, &filestat);
                    FileSize = (u64)filestat.st_size;

                    if (FileSize > 0)
                    {
                        // is this file an executeable?
                        if (((filestat.st_mode & S_IXUSR) || (filestat.st_mode & S_IXGRP) || (filestat.st_mode & S_IXOTH)))
                        {
                            //LOG("%S", EntryName);
                            if (ExtensionWithDot.Length > 0)
                            {
                                u32 LastDot = 0;
                                if (String_IndexOfLastChar(EntryName, '.', &LastDot))
                                {
                                    if (String_IsEqual(FileName, StrSlice(EntryName.Data, LastDot), false) &&
                                        String_EndsWith(FileName, ExtensionWithDot, true))
                                    {
                                        bFound = true;
                                    }
                                }
                            }
                            else
                            {
                                if (String_IsEqual(FileName, EntryName, false))
                                {
                                    bFound = true;
                                }
                            }

                            if (bFound)
                            {
                                if (OutFilePath)
                                    String_Copy(OutFilePath, FullPath);

                                break;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            Len++;
        }
    }

    return bFound;
}

u32 Platform_GetExitCodeForProcess(PlatformHandle Handle)
{
    if (Handle == 0)
        return 0;

    i32 PidStatus;
    pid_t pid = waitpid(Handle, &PidStatus, 0); // if you call this twice on the same pid, linux wont return the same exit code like windows does... sadge :(
    if (pid == -1)
    {
        return 0;
    }

    return WEXITSTATUS(PidStatus);
}

u32 Platform_WaitForProcessAndGetExitCode(PlatformHandle Handle)
{
    if (Handle == 0)
        return 0;

    i32 PidStatus;
    pid_t pid = waitpid(Handle, &PidStatus, 0);
    if (pid == -1)
    {
        return 0;
    }

    return WEXITSTATUS(PidStatus);
}

void Platform_WaitForHandle(PlatformHandle Handle, i32 Milliseconds)
{
    waitpid(Handle, NULL, 0);
}

u32 Platform_WaitForMultipleHandles(PlatformHandle* Handles, u32 NumHandles, i32 Milliseconds, bool bWaitAll)
{
    if (bWaitAll)
    {
        for (u32 i = 0; i < NumHandles; i++)
        {
            waitpid(Handles[i], NULL, 0);
        }

        return 0;
    }

    //bool bNeedsReset = false;
    for (u32 i = 0; i < NumHandles; i++)
    {
        Platform_WaitForHandle(Handles[i], Milliseconds);
        if (!bWaitAll)
        {
            return i;
        }
    }

    return 0;
}

void Platform_CloseHandle(PlatformHandle Handle)
{
    close(Handle);
}

bool Platform_IsValidHandle(const PlatformHandle Handle)
{
    return Handle >= 0;
}

u64 Platform_GetCriticalSectionMemoryRequirement(void)
{
    return 4;
}

void Platform_InitializeCriticalSection(PlatformCriticalSection OutCriticalSection)
{
}

void Platform_DeleteCriticalSection(PlatformCriticalSection CriticalSection)
{
}

void Platform_EnterCriticalSection(PlatformCriticalSection CriticalSection)
{
}

void Platform_ExitCriticalSection(PlatformCriticalSection CriticalSection)
{
}

bool Platform_CreateMutex(const String Name, PlatformMutex* OutMutex)
{
    if ((NEVER(Name.Length == 0)) || (NEVER(OutMutex == NULL)))
    {
        return false;
    }

    u32 Diff = Name.Length > 30 ? Name.Length - 30 : 0; // 31 is max name length for posix semaphores
    String ClampedName = StrShiftF(Name, Diff);

    sem_t* Semaphore = sem_open(ClampedName.Data, O_CREAT|O_EXCL, 0644, 1);
    if (Semaphore == SEM_FAILED)
    {
        if (errno == EACCES || errno == EEXIST)
        {
            return false;
        }

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to create mutex %S"), Prefix.Capacity, ClampedName);
        LogLastError(Prefix);
        return false;
    }

    OutMutex->Handle = Semaphore;
    OutMutex->Name = ClampedName;
    return true;
}

bool Platform_ReleaseMutex(PlatformMutex* Mutex)
{
    if ((NEVER(Mutex->Name.Length == 0)) || (NEVER(Mutex == NULL)))
    {
        return false;
    }

    if (sem_close(Mutex->Handle) == -1)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to release mutex %S"), Prefix.Capacity, Mutex->Name);
        LogLastError(Prefix);
        return false;
    }

    // for some fucking reason semaphores have kernel persistence, so we need to unlink them, otherwise the user will have to shutdown their machine
    // which is why again windows dominates the market, go look at the code in Platform_Windows.c!!!
    if (sem_unlink(Mutex->Name.Data) == -1)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to unlink mutex %S"), Prefix.Capacity, Mutex->Name);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

f64 Platform_GetAbsoluteTime(void)
{
    struct timespec t = {0};
    clock_gettime(CLOCK_REALTIME, &t);
    const f64 a = (f64)t.tv_sec + ((f64)t.tv_nsec * 0.000000001); // 1e-9
    return a;
}

SystemTime Platform_GetSystemLocalTime(void)
{
    time_t mytime = time(0);
    ctime(&mytime);

    struct tm* lt = localtime(&mytime);

    SystemTime t;
    t.Year = (u16)lt->tm_year + 1900;
    t.Month = (u16)lt->tm_mon + 1;
    t.DayOfWeek = 0;
    t.Day = (u16)lt->tm_mday;
    t.Hour = (u16)lt->tm_hour;
    t.Minute = (u16)lt->tm_min;
    t.Second = (u16)lt->tm_sec;
    t.Millisecond = 0;

    return t;
}

void Platform_Sleep(f64 ms)
{
    PROFILE_FUNCTION()
    {
        if (ms > 0)
        {
            struct timespec t = {0};
            clock_gettime(CLOCK_REALTIME, &t);
            const f64 Start = (f64)t.tv_sec + ((f64)t.tv_nsec * 0.000000001); // 1e-9

            f64 Target = ms/1000.0;

            while (1)
            {
                clock_gettime(CLOCK_REALTIME, &t);
                const f64 Now = (f64)t.tv_sec + ((f64)t.tv_nsec * 0.000000001); // 1e-9
                if ((Now-Start) >= Target)
                    break;
            }
        }

        //usleep((u32)(ms * 1000));
    }
}

void Platform_ShowCursor(bool bShow)
{
    UNIMPLEMENTED;
}

void Platform_GetMousePosition(f32* X, f32* Y)
{
    UNIMPLEMENTED;
}

u64 Platform_GetCurrentThreadID(void)
{
    u32 x = (u32)syscall(__NR_gettid);
    return x;
}

u64 Platform_GetMainThreadID(void)
{
    return (u64)getpid();
}

u32 Platform_GetConsoleProcessCount(void)
{
    // TODO
    return 0;
}

void Platform_GetWorkingDirectory(String* OutPath)
{
    getcwd(OutPath->Data, MAX_PATH_LENGTH);
    OutPath->Length = String_GetLength_Ex(OutPath->Data, MAX_PATH_LENGTH);
}

bool Platform_GetEnvironmentVariableValue(String Name, String* OutVariable)
{
    char* Value = getenv(Name.Data);
    if (Value == NULL)
    {
        return false;
    }

    String_Copy(OutVariable, CStr(Value));
    return true;
}

bool Platform_DoesEnvironmentVariableExist(String Name)
{
    char* Value = getenv(Name.Data);
    if (Value == NULL)
    {
        return false;
    }

    return true;
}

bool Platform_CaptureStackTrace(LinearAllocator* Arena, TArray(StackTraceData)* OutInfo)
{
    UNIMPLEMENTED;
    return false;
}

u32 Platform_GetNumLogicalProcessors(void)
{
    u32 number_of_processors = (u32)sysconf(_SC_NPROCESSORS_ONLN);

    return number_of_processors;
}

bool Platform_GetAccountName(String* OutName)
{
    return Platform_GetUserName(OutName);
}

bool Platform_GetUserName(String* OutName)
{
    struct passwd pwd = {0};
    struct passwd* result = NULL;
    char Buffer[4096] = {0};

    getpwuid_r(getuid(), &pwd, Buffer, 4096, &result);
    if (result == NULL)
    {
        StringLocal(Message, 512);
        String_Format(&Message, S("Failed to get user name"), Message.Capacity);
        LogLastError(Message);
        return false;
    }

    String_Copy(OutName, CStr(pwd.pw_name));
    return true;
}

bool Platform_GetUserDirectory(String* OutDirectory)
{
    struct passwd pwd = {0};
    struct passwd* result = NULL;
    char Buffer[4096] = {0};

    getpwuid_r(getuid(), &pwd, Buffer, 4096, &result);
    if (result == NULL)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to get user directory"), Prefix.Capacity);
        LogLastError(Prefix);
        return false;
    }

    String_Copy(OutDirectory, CStr(pwd.pw_dir));
    return true;
}

bool Filesystem_GetFilePath(const FileHandle* Handle, String* OutPath)
{
    if (!IsValidFileHandle(Handle))
    {
        return false;
    }

    char Path[PATH_MAX] = {0};

    const i32 fd = fileno(Handle->Data);
    snprintf(Path, PATH_MAX, "/proc/self/fd/%d", fd);

    ssize_t Result = readlink(Path, OutPath->Data, OutPath->Capacity);
    if (Result == -1)
    {
        LogLastError(S("Failed to retrieve file path for file handle"));
        return false;
    }

    OutPath->Length = (u32)Result;
    return true;
}

bool Filesystem_IsPathRelative(const String Path)
{
    return Path.Data[0] != '/';
}

bool Filesystem_SanitizeQuotes(String* Dest, const String Path)
{
    bool bHasQuote = false;
    for (u32 i = 0; i < Path.Length; i++)
    {
        char c = Path.Data[i];
        if (c == '"' && bHasQuote)
        {
            // ignore all subsequent quotes
            continue;
        }

        String_AppendChar(Dest, c);

        if (c == '"')
        {
            bHasQuote = true;
        }
    }

    if (bHasQuote)
    {
        String_AppendChar(Dest, '"');
    }

    return Dest->Length > 0;
}

bool Platform_GetCurrentProcessName(String* OutName)
{
    // todo: from cmdline args
    return false;
}

u64  Platform_GetCurrentProcessID(void)
{
    return (u64)getpid();
}

bool Platform_GetThreadName(void* ThreadHandle, String* OutName)
{
    //UNIMPLEMENTED;
    return false;
}

bool Platform_IsProgramRunning(const String ProgramName)
{
    return false;
}

static String GArgV[128] = {0};
static i32 GArgC = 0;
static char** GEnv = NULL;

static String GProgramName = { 0 };
static char GEmptyBuffer[16] = {0};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-prototypes"
void pre_main(int argc, char* argv[], char* env[])
{
    GArgC = argc;
    GEnv  = env;

    for (u16 i = 0; i < 128; i++)
    {
        GArgV[i].Data = GEmptyBuffer;
        GArgV[i].Length = 0;
        GArgV[i].Capacity = 15;
    }

    for (int i = 1; i < argc; ++i)
    {
        GArgV[i-1].Data = argv[i];
        GArgV[i-1].Length = String_GetLength_Ex(argv[i], UINT16_MAX);
        GArgV[i-1].Capacity = GArgV[i-1].Length;
    }

    GProgramName.Data = argv[0];
    GProgramName.Length = String_GetLength_Ex(argv[0], UINT16_MAX);
    GProgramName.Capacity = GProgramName.Length;
}
#pragma clang diagnostic pop

StringArray Platform_GetCommandLineArgs(void)
{
    StringArray Args = {0};
    Args.List = GArgV;
    Args.Num = (u32)(GArgC-1 <= 0 ? 0 : (GArgC-1 < 128 ? GArgC-1 : 128));
    return Args;
}

void* Platform_GetDeviceContext(void)
{
    return NULL;
}

bool Filesystem_Open(const String FilePath, u32 Mode, FileHandle* OutHandle)
{
    String ModeStr = String_Null();

    if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) != 0)) // read and write
    {
        ModeStr = S("a+");
    }
    else if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) == 0)) // read only
    {
        ModeStr = S("r");
    }
    else if (((Mode & FileMode_Read) == 0) && ((Mode & FileMode_Write) != 0)) // write only
    {
        ModeStr = S("w");
    }
    else
    {
        LOG_WARNING("Invalid mode passed (%u) while trying to open file \"%S\"", Mode, FilePath);
        return false;
    }

    u32 LastSlash = 0;
    String_IndexOfLastPathSlash(FilePath, &LastSlash);

    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, StrSlice(FilePath.Data, LastSlash));

    Filesystem_OpenDirectory(Copy);

    FILE* File = fopen(FilePath.Data, ModeStr.Data); // refactor to just use open() instead of fopen()
    if (!File)
    {
        String ModeString;
        if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) != 0)) // read and write
        {
            ModeString = S("for reading/writing");
        }
        else if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) == 0)) // read only
        {
            ModeString = S("for reading");
        }
        else
        {
            ModeString = S("for writing");
        }

        StringLocal(Message, MAX_PATH_LENGTH);
        String_Format(&Message, S("Failed to open file %S -> \"%S\""), MAX_PATH_LENGTH, ModeString, FilePath);
        LogLastError(Message);
        return false;
    }

    ASSERT(OutHandle != NULL);

    OutHandle->Data = File;
    OutHandle->Data2 = NULL;
    //fseek(File, 0, SEEK_END);
    //OutHandle->Size = (u64)ftell(File);
    //fseek(File, 0, SEEK_SET);

    //StringN_Copy(OutHandle->Path, FilePath);

    return true;
}

// todo: move to core
bool Filesystem_NewFile(const String FilePath)
{
    FileHandle f = {0};
    bool bSuccess = Filesystem_Open(FilePath, FileMode_Write, &f);
    Filesystem_Close(&f);

    return bSuccess;
}

bool Filesystem_DeleteFile(String FilePath)
{
    UNIMPLEMENTED;
    return false;
}

bool Filesystem_Copy(const String Source, const String Destination)
{
    UNIMPLEMENTED;
    return false;
}

bool Filesystem_Open_MemoryMapped(const String FilePath, u32 Mode, FileHandle* OutHandle, u8** OutData, u64* OutSize)
{
    if (OutSize)
        *OutSize = 0;

    if (OutData)
        *OutData = NULL;

    if (!IsValidFileHandle(OutHandle))
    {
        if (!Filesystem_Open(FilePath, Mode, OutHandle))
            return false;
    }

    int ProtectFlags = 0;

    if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) != 0)) // read and write
    {
        ProtectFlags = PROT_READ | PROT_WRITE;
    }
    else if (((Mode & FileMode_Read) != 0) && ((Mode & FileMode_Write) == 0)) // read only
    {
        ProtectFlags = PROT_READ;
    }
    else if (((Mode & FileMode_Read) == 0) && ((Mode & FileMode_Write) != 0)) // write only
    {
        ProtectFlags = PROT_WRITE;
    }

    u64 Size = 0;
    Filesystem_GetFileSize(OutHandle, &Size);

    void* Address = mmap(NULL, Size, ProtectFlags, MAP_SHARED, fileno(OutHandle->Data), 0);

    if (Address == MAP_FAILED)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to memory map file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return false;
    }

    OutHandle->Data2 = Address;

    if (OutData)
        *OutData = (u8*)Address;

    if (OutSize) 
        *OutSize = Size;

    return 0;
}

bool Filesystem_OpenDirectory(const String FilePath)
{
    if (Filesystem_DoesDirectoryExist(FilePath))
        return true;
    
    bool bAnySuccess = false;
    bool bFoundPathSeparator = false;
    u32 NextSlashIndex = 0;

    do
    {
        bFoundPathSeparator = false;

        for (u32 i = NextSlashIndex; i < FilePath.Length; i++)
        {
            if (FilePath.Data[i] == '/' || FilePath.Data[i] == '\\' || FilePath.Length-1 == i)
            {
                bFoundPathSeparator = true;

                StringLocal(BaseDirectory, MAX_PATH_LENGTH);
                if (FilePath.Length-1 == i)
                    String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i+1));
                else
                    String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i));

                NextSlashIndex = i+1;

                bool bDirectoryCreated = Filesystem_DoesDirectoryExist(BaseDirectory) || BaseDirectory.Length == 0;
                if (!bDirectoryCreated)
                {
                    i32 ErrorCode = mkdir(BaseDirectory.Data, 0700);
                    if (ErrorCode == -1)
                    {
                        StringLocal(Prefix, MAX_PATH_LENGTH);
                        String_Format(&Prefix, S("Failed to open directory \"%S\""), Prefix.Capacity, BaseDirectory);
                        LogLastError(Prefix);
                        return false;
                    }

                    bAnySuccess = true;
                }

                break;
            }
        }
    }
    while (bFoundPathSeparator);

    return bAnySuccess;

/*
    i32 ErrorCode = mkdir(FilePath.Data, 0700);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to open directory \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return false;
    }

    return true;*/
}

bool Filesystem_OpenDirectory_Ex(const String FilePath, FileHandle* OutHandle)
{
    return Filesystem_OpenDirectory(FilePath);
}

bool Filesystem_Close(FileHandle* Handle)
{
    bool bFailedUnmap = false;
    if (Handle->Data2)
    {
        Handle->Data2 = NULL;

        u64 Size = 0;
        Filesystem_GetFileSize(Handle, &Size);

        if (munmap(Handle->Data2, Size) == -1)
        {
            StringLocal(Path, MAX_PATH_LENGTH);
            Filesystem_GetFilePath(Handle, &Path);

            StringLocal(Prefix, 512);
            String_Format(&Prefix, S("Failed to unmap memory for file \"%S\""), Prefix.Capacity, Path);
            LogLastError(Prefix);
            bFailedUnmap = true;
        }
    }

    if (IsValidFileHandle(Handle))
    {
        fclose(Handle->Data);
        *Handle = FileHandle_Null();
        return !bFailedUnmap;
    }
    
    return false;
}

bool Filesystem_Seek(const FileHandle* Handle, i64 Offset)
{
    i32 ErrorCode = fseek(Handle->Data, Offset, SEEK_CUR);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Seek failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekFromBeginning(const FileHandle* Handle, u64 Offset)
{
    i32 ErrorCode = fseek(Handle->Data, (i32)Offset, SEEK_SET);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekFromBeginning failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekFromEnd(const FileHandle* Handle, u64 Offset)
{
    i32 ErrorCode = fseek(Handle->Data, (i32)Offset, SEEK_END);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekFromEnd failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekToBeginning(const FileHandle* Handle)
{
    i32 ErrorCode = fseek(Handle->Data, 0, SEEK_SET);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekToBeginning failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_SeekToEnd(const FileHandle* Handle)
{
    i32 ErrorCode = fseek(Handle->Data, 0, SEEK_END);
    if (ErrorCode == -1)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("SeekToEnd failed for \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    return true;
}

u64  Filesystem_GetCurrentFilePosition(const FileHandle* Handle)
{
    return (u64)ftell(Handle->Data);
}

u64  Filesystem_GetLastWriteTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    struct stat FileStat = {0};
    i32 ErrorCode = stat(FilePath.Data, &FileStat);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve last write time for file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return 0;
    }

    return (u64)FileStat.st_mtime;
}

u64  Filesystem_GetLastAccessTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    struct stat FileStat = {0};
    i32 ErrorCode = stat(FilePath.Data, &FileStat);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve last access time for file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return 0;
    }

    return (u64)FileStat.st_atime;
}

FileTimeData  Filesystem_GetFileTime(const String FilePath)
{
    FileTimeData a = {0};

    if (!Filesystem_DoesFileExist(FilePath))
        return a;

    a.CreationTime = Filesystem_GetCreationTime(FilePath);
    a.LastAccessTime = Filesystem_GetLastAccessTime(FilePath);
    a.LastWriteTime = Filesystem_GetLastWriteTime(FilePath);
    return a;
}

#ifdef HAVE_ST_BIRTHTIME
#define birthtime(x) x.st_birthtime
#else
#define birthtime(x) x.st_ctime
#endif

u64  Filesystem_GetCreationTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    struct stat FileStat = {0};
    i32 ErrorCode = stat(FilePath.Data, &FileStat);
    if (ErrorCode == -1)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to retrieve creation time for file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);
        return 0;
    }

    return (u64)birthtime(FileStat);
}

u64  Filesystem_GetLastWriteTimeH(const FileHandle* Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(Handle, &Path);

    return Filesystem_GetLastWriteTime(Path);
}

u64  Filesystem_GetLastAccessTimeH(const FileHandle* Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(Handle, &Path);

    return Filesystem_GetLastAccessTime(Path);
}

u64  Filesystem_GetCreationTimeH(const FileHandle* Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(Handle, &Path);

    return Filesystem_GetCreationTime(Path);
}

FileTimeData  Filesystem_GetFileTimeH(const FileHandle* Handle)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(Handle, &Path);

    FileTimeData a = {0};
    a.CreationTime = Filesystem_GetCreationTime(Path);
    a.LastAccessTime = Filesystem_GetLastAccessTime(Path);
    a.LastWriteTime = Filesystem_GetLastWriteTime(Path);
    return a;
}

bool Filesystem_ReadPipe(PlatformPipe Handle, u64 DataSize, void* OutData, u64* OutBytesRead)
{
    if (NEVER(Handle[0] == -1)) return false;
    if (NEVER(Handle[1] == -1)) return false;

    i64 BytesRead = read(Handle[0], OutData, DataSize);
    if (BytesRead < 0)
    {
        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to read pipe from handle -> Read: %i | Write: %i"), Prefix.Capacity, Handle[0], Handle[1]);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = (u64)BytesRead;

    return true;
}

bool Filesystem_Read(const FileHandle* Handle, u64 DataSize, void* OutData, u64* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    u64 BytesRead = fread(OutData, 1, DataSize, Handle->Data);
    if (BytesRead == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to read file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return true;
}

bool Filesystem_ReadEntireFile(const FileHandle* Handle, void* OutData, u64* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    // todo: remove this?
    Filesystem_SeekToBeginning(Handle);
    
    u64 Size = 0;
    Filesystem_GetFileSize(Handle, &Size);

    u64 BytesRead = fread(OutData, 1, Size, Handle->Data);
    if (BytesRead == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to entire read file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return true;
}

// todo: move to core
bool Filesystem_ReadLine(const FileHandle* Handle, String* LineBuffer)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;
    if (NEVER(LineBuffer == NULL)) return false;
    if (NEVER(LineBuffer->Data == NULL || LineBuffer->Data == String_Null().Data)) return false;

    u64 CurrentPosition = Filesystem_GetCurrentFilePosition(Handle);

    u64 Size = 0;
    Filesystem_GetFileSize(Handle, &Size);
    u64 FileSize = Size;

    if (CurrentPosition >= FileSize)
    {
        Filesystem_SeekToBeginning(Handle);
        return false;
    }

    char TempBuffer[8192] = {0};
    u64 BytesRead = fread(TempBuffer, 1, 8191, Handle->Data);
    if (BytesRead == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to read line for file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    // todo: move to core?
    /////////////////
    u32 Counter = 0;
    u32 FilePointerOffset = 0;
    for (u32 i = 0; i < BytesRead; i++)
    {
        if (TempBuffer[i] == '\0' || TempBuffer[i] == '\n' || TempBuffer[i] == '\r')
        {
            FilePointerOffset = Counter;

            if (TempBuffer[i] == '\r' && TempBuffer[i+1] == '\n') // todo: bounds check
                FilePointerOffset += 2;
            else
                FilePointerOffset++;

            TempBuffer[i] = 0;

            break;
        }

        Counter++;
    }

    if (FilePointerOffset == 0) // did not find a new line char, possibly at end of file
    {
        FilePointerOffset = Counter;
    }

    if (FilePointerOffset == 0)
    {
        return false;
    }

    u32 MaxLength = Min(LineBuffer->Capacity, 8192);
    u32 LineLength = Min(MaxLength-1, Counter);

    if (LineLength > 0)
    {
        String_Copy(LineBuffer, StrSlice(TempBuffer, LineLength));
    }
    else
    {
        String_Empty(LineBuffer);
    }

    Filesystem_SeekFromBeginning(Handle, CurrentPosition + FilePointerOffset);

    return true;
}

bool Filesystem_ReadLine_Backwards(const FileHandle* Handle, String* LineBuffer)
{
    UNIMPLEMENTED;
    return 0;
}

bool Filesystem_Write(const FileHandle* Handle, u64 DataSize, const void* Data, u64* OutBytesWritten)
{
    ASSERT(IsValidFileHandle(Handle));

    if (DataSize == 0)
        return false;

    Filesystem_SeekToBeginning(Handle);

    u64 BytesWritten = fwrite(Data, 1, DataSize, Handle->Data);
    if (BytesWritten == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to write to file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    return true;
}

bool Filesystem_WriteLineFormatted(const FileHandle* Handle, const String Text, u64* OutBytesWritten, ...)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;

    Filesystem_SeekToEnd(Handle);
    
    va_list Args;
    va_start(Args, OutBytesWritten);
    StringLocal(Buffer, 32768);
    String_FormatV(&Buffer, Text, 32768, Args);
    va_end(Args);

    u64 BytesWritten = fwrite(Buffer.Data, 1, Buffer.Length, Handle->Data);
    if (BytesWritten == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to write line to file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    return true;
}

bool Filesystem_WriteLine(const FileHandle* Handle, const String Text, u64* OutBytesWritten)
{
    ASSERT(IsValidFileHandle(Handle));

    Filesystem_SeekToEnd(Handle);

    u64 BytesWritten = fwrite(Text.Data, 1, Text.Length, Handle->Data);
    if (BytesWritten == 0)
    {
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);

        StringLocal(Prefix, MAX_PATH_LENGTH);
        String_Format(&Prefix, S("Failed to write line to file \"%S\""), Prefix.Capacity, Path);
        LogLastError(Prefix);
        return false;
    }

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    return true;
}

bool Filesystem_DoesFileExist(const String FilePath)
{
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, FilePath);

    if (access(Copy.Data, F_OK) == 0)
    {
        return true;
    }
    
    return false;
}

bool Filesystem_DoesDirectoryExist(const String FilePath)
{
    DIR* Found = opendir(FilePath.Data);
    if (!Found)
    {
        //StringLocal(Prefix, MAX_PATH_LENGTH);
        //String_Format(&Prefix, S("Failed to open directory \"%S\""), Prefix.Capacity, FilePath);
        //LogLastError(Prefix);
        return false;
    }

    return true;
}

bool Filesystem_GetFileSize(const FileHandle* File, u64* OutSize)
{
    StringLocal(Path, MAX_PATH_LENGTH);
    Filesystem_GetFilePath(File, &Path);

    struct stat filestat = {0};
    stat(Path.Data, &filestat);
    *OutSize = (u64)filestat.st_size;
    return true;
}

bool Filesystem_IsFile(const String Path)
{
    struct stat filestat = {0};
    stat(Path.Data, &filestat);
    return S_ISREG(filestat.st_mode);
}

bool Filesystem_IsDirectory(const String Path)
{
    struct stat filestat = {0};
    stat(Path.Data, &filestat);
    return S_ISDIR(filestat.st_mode);
}

// todo: move into platform core?
bool Filesystem_IsNewer(const String PathA, const String PathB)
{
    u64 a = Filesystem_GetLastWriteTime(PathA);
    u64 b = Filesystem_GetLastWriteTime(PathB);
    return a > b;
}

bool Filesystem_IsOlder(const String PathA, const String PathB)
{
    u64 a = Filesystem_GetLastWriteTime(PathA);
    u64 b = Filesystem_GetLastWriteTime(PathB);
    return a < b;
}

bool Filesystem_ConvertRelativeToAbsolutePath(String* OutFullPath)
{
    StringLocal(Copy, MAX_PATH_LENGTH);
    String_Copy(&Copy, *OutFullPath);

    char* Result = realpath(Copy.Data, OutFullPath->Data);
    if (Result == NULL)
    {
        StringLocal(Format, MAX_PATH_LENGTH);
        String_Format(&Format, S("Failed to convert \"%S\" to an absolute path"), MAX_PATH_LENGTH, Copy);
        LogLastError(Format);
        return false;
    }

    OutFullPath->Length = String_GetLength(Result);

    return true;
}

internal void Internal_IterateDirectory(const String BasePath, const String DirectoryPath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    struct dirent* entry = NULL;
    DIR* dp = opendir(BasePath.Data);
    if (!dp)
    {
        StringLocal(Message, MAX_PATH_LENGTH);
        String_Format(&Message, S("Failed to iterate directory for path \"%S\""), MAX_PATH_LENGTH, BasePath);
        LogLastError(Message);
        return;
    }
    
    while ((entry = readdir(dp)))
    {
        if (entry->d_type != DT_REG && entry->d_type != DT_DIR)
        {
            continue;
        }

        if (entry->d_name[0] == '.' && 
            (!entry->d_name[1] || (entry->d_name[1] == '.' && !entry->d_name[2])))
        {
            continue;
        }

        //LOG("%S", CStr(entry->d_name));

        const String EntryName = CStr(entry->d_name);

        StringLocal(FullPath, MAX_PATH_LENGTH);
        String_BuildPath(&FullPath, BasePath, EntryName);
        
        StringLocal(RelativePath, MAX_PATH_LENGTH);

        if (entry->d_type == DT_DIR)
        {
            String_BuildPath(&RelativePath, DirectoryPath, EntryName);

            bool bResult = Callback(FullPath, RelativePath, EntryName, 0, true, UserData);
            if (!bResult) break;

            if (bRecursive)
            {
                Internal_IterateDirectory(FullPath, RelativePath, Callback, true, UserData);
            }
        }
        else
        {
            u64 FileSize = 0;
            struct stat filestat = {0};
            stat(FullPath.Data, &filestat);
            FileSize = (u64)filestat.st_size;

            if (DirectoryPath.Length > 0)
            {
                String_BuildPath(&RelativePath, DirectoryPath, EntryName);
            }
            else
            {
                String_Copy(&RelativePath, EntryName);
            }

            bool bResult = Callback(FullPath, RelativePath, EntryName, FileSize, false, UserData);
            if (!bResult) break;
        }
    }
    
    closedir(dp);
}

void Filesystem_IterateDirectory(const String BasePath, DirectoryIterator Callback, bool bRecursive)
{
    Internal_IterateDirectory(BasePath, S(""), Callback, bRecursive, NULL);
}

void Filesystem_IterateDirectory_Ex(const String BasePath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    Internal_IterateDirectory(BasePath, S(""), Callback, bRecursive, UserData);
}

bool Filesystem_DeleteFiles(const String FilePath, const String Wildcard, bool bRecursive)
{
    StringLocal(Cmd, MAX_PATH_LENGTH);
    String_Append(&Cmd, S("rm -f "));
    if (bRecursive)
        String_Append(&Cmd, S("-r \""));
    String_Append(&Cmd, FilePath);
    String_AppendPathSeparator_Checked(&Cmd);
    String_AppendChar(&Cmd, '"');
    String_Append(&Cmd, Wildcard);
    String_Append(&Cmd, S(" 2> /dev/null"));
    i32 Result = system(Cmd.Data);
    return Result == 0;
}

bool Filesystem_DeleteDirectory(const String DirectoryPath)
{
    StringLocal(Cmd, MAX_PATH_LENGTH);
    String_Append(&Cmd, S("rm "));
    //if (bRecursive)
    //    String_Append(&Cmd, S("-r \""));A
    String_Append(&Cmd, DirectoryPath);
    String_AppendPathSeparator_Checked(&Cmd);
    //String_AppendChar(&Cmd, '"');
    //String_Append(&Cmd, Wildcard);
    String_Append(&Cmd, S(" 2> /dev/null"));
    i32 Result = system(Cmd.Data);
    return Result == 0;
}

bool Filesystem_ArePathsCommon(String PathA, String PathB)
{
    bool bPrefixMatch = String_StartsWith(PathB, PathA, true);
    
    return bPrefixMatch;
}

i32 Rand(void)
{
    return rand();
}

// https://stackoverflow.com/questions/4768180/rand-implementation
static u32 next = 1;

void RandSeed(void)
{
	next = (u32)Platform_GetAbsoluteTime();
}

i32 RandFast(void)
{
    next = (u32)Platform_GetAbsoluteTime();
    next *= 1103515245 + 12345;

    return (next/65536) % 32768;
}

f32 FRand(void)
{
    // inline Absi32 function
    i32 Value = Rand();
	i32 Temp = Value >> 31;
	Value ^= Temp;
	Value += Temp & 1;

	return (f32)Value / (f32)INT32_MAX;
}

f32 FRandFast(void)
{
    next = (u32)Platform_GetAbsoluteTime();
    next *= 1103515245 + 12345;
    
    f32 RandFastResult = (f32)((next/65536) % 32768);
	return RandFastResult / (f32)RAND_MAX;
}

Uuid UUID_Generate(void)
{
    uuid_t id;
    uuid_generate(id);

    return *(Uuid*)id;
}

bool UUID_IsEqual(Uuid First, Uuid Second)
{
    //return Platform_MemEqual(&First, &Second, sizeof(Uuid));

    unsigned char* a = (unsigned char*)&First;
    unsigned char* b = (unsigned char*)&Second;

    const bool bSame = uuid_compare(a, b) == 0;
    return bSame;
}

void UUID_ToString(Uuid ID, String* OutString)
{
    StringLocal(Temp, GUID_LENGTH);

    unsigned char* a = (unsigned char*)&ID;
    uuid_unparse(a, Temp.Data);

    String_Copy(OutString, Temp);
}

Uuid UUID_FromString(const String IDString)
{
    uuid_t id;
    uuid_parse(IDString.Data, id);

    return *(Uuid*)id;
}

bool FileWatcher_WatchFileOrDirectory(FileWatchReference* Reference)
{
    //UNIMPLEMENTED;
    return false;
}

bool FileWatcher_Initialize(void* Memory)
{
    //UNIMPLEMENTED;
    return true;
}

void FileWatcher_Shutdown(void)
{
    //UNIMPLEMENTED;
}

u64 FileWatcher_GetMemoryRequirement(void)
{
    return 4;
}

#ifndef NO_VULKAN
static const char* GExtensionListBuffer[255] =
{
    VK_KHR_XCB_SURFACE_EXTENSION_NAME
};
#endif

#ifndef NO_VULKAN
const char** Platform_GetRequiredExtensionNames(void)
{
    return &GExtensionListBuffer[0];
}

bool Platform_CreateVulkanSurface(struct VulkanContext* Context)
{
    VkXcbSurfaceCreateInfoKHR CreateInfo = { 0 };
    CreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    CreateInfo.connection = GLinuxPlatformState->Connection;
    CreateInfo.window = GLinuxPlatformState->Window;

    VkResult Result = vkCreateXcbSurfaceKHR(Context->Instance, &CreateInfo, Context->Allocator, &GLinuxPlatformState->Surface);
    if (Result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create vulkan surface");
        return false;
    }

    Context->Surface = GLinuxPlatformState->Surface;
    return true;
}
#endif

u8 ConvertInputKeyToUnicodeCharacter(EKey Key)
{
    return (u8)Key;
    /*
    char keys_return[256] = {0};
    XQueryKeymap(GLinuxPlatformState->Display, keys_return);
    KeyCode kc2 = XKeysymToKeycode(GLinuxPlatformState->Display, Key);
    return kc2;
    */

    //bool bShiftPressed = !!( keys_return[ kc2>>3 ] & ( 1<<(kc2&7) ) );

    //printf("Shift is %spressed\n", bShiftPressed ? "" : "not ");

    //return (u8)Key;
}

bool Platform_GetTerminalDimensions(u32* OutRows, u32* OutColumns)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        LogLastError(S("Failed to get terminal dimensions"));
        return false;
    }

    *OutRows = w.ws_row;
    *OutColumns = w.ws_col;

    return true;
}

#endif // PLATFORM_LINUX
