#include "Platform.h"

#if PLATFORM_WINDOWS

#include "Log.h"

#include "Uuid.h"
#include "Filesystem.h"
#include "Math/Math.h"
#include "String/BaseString.h"
#include "String/StringUtils.h"
#include "Structures/Array.h"

#include <Windows.h>
#include <windowsx.h>
#include <bcrypt.h>
#include <strsafe.h>
#include <Shlwapi.h>
#include <DbgHelp.h>
#include <process.h>
#include <shellapi.h>
#include <psapi.h>
#include <Shlobj.h>

#include <gs_support.c>

u64 __security_cookie = 0;
u64 __security_cookie_complement = 0;
void __fastcall __security_check_cookie(u64 cookie)
{
    if (cookie != __security_cookie)
        __debugbreak();
}

typedef struct WindowsPlatformState
{
    bool bInitialized;

    HINSTANCE Instance;
    HWND Handle;
    DWORD ThreadID;
    HANDLE ThreadHandle;
    HDC Hdc;
} WindowsPlatformState;

typedef enum WinConsoleForegroundColors
{
    FG_BLACK = 0,
    FG_BLUE = 1,
    FG_GREEN = 2,
    FG_CYAN = 3,
    FG_RED = 4,
    FG_MAGENTA = 5,
    FG_BROWN = 6,
    FG_LIGHTGRAY = 7,
    FG_GRAY = 8,
    FG_LIGHTBLUE = 9,
    FG_LIGHTGREEN = 10,
    FG_LIGHTCYAN = 11,
    FG_LIGHTRED = 12,
    FG_LIGHTMAGENTA = 13,
    FG_YELLOW = 14,
    FG_WHITE = 15
} WinConsoleForegroundColors;

typedef enum WinConsoleBackgroundColor
{
    BG_NAVYBLUE = 16,
    BG_GREEN = 32,
    BG_TEAL = 48,
    BG_MAROON = 64,
    BG_PURPLE = 80,
    BG_OLIVE = 96,
    BG_SILVER = 112,
    BG_GRAY = 128,
    BG_BLUE = 144,
    BG_LIME = 160,
    BG_CYAN = 176,
    BG_RED = 192,
    BG_MAGENTA = 208,
    BG_YELLOW = 224,
    BG_WHITE = 240
} WinConsoleBackgroundColor;

C_LINKAGE_BEGIN
int _fltused = 0;
C_LINKAGE_END

#define CONSOLE_INFO_COLOR (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define CONSOLE_SUCCESS_COLOR (FOREGROUND_INTENSITY | FOREGROUND_GREEN)
#define CONSOLE_WARNING_COLOR 14
#define CONSOLE_ERROR_COLOR (FOREGROUND_INTENSITY | FOREGROUND_RED)
#define CONSOLE_FATAL_COLOR (FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_RED)

#define WM_FILE_WATCHER (WM_USER+1)

// INFO, SUCCESS, WARNING, ERROR, FATAL, NONE
static u8 GConsoleColorLevels[6] = { CONSOLE_INFO_COLOR, CONSOLE_SUCCESS_COLOR, CONSOLE_WARNING_COLOR, CONSOLE_ERROR_COLOR, CONSOLE_FATAL_COLOR, CONSOLE_INFO_COLOR };

static WindowsPlatformState* GWindowsPlatformState = NULL;

#ifndef HEADLESS
static LinearAllocator GPlatformStateMemoryAllocator = {0};
#endif

static CRITICAL_SECTION GCriticalSection = {0};
static bool bCriticalSectionInitialized = false;

typedef struct WindowsMessage
{
    u32 ID;
    const char* Name;
} WindowsMessage;

#ifdef _DEBUG
static WindowsMessage WM_Map[1024] =
{
    {0, "WM_NULL" },
    {1, "WM_CREATE" },
    {2, "WM_DESTROY" },
    {3, "WM_MOVE" },
    {5, "WM_SIZE" },
    {6, "WM_ACTIVATE" },
    {7, "WM_SETFOCUS" },
    {8, "WM_KILLFOCUS" },
    {10, "WM_ENABLE" },
    {11, "WM_SETREDRAW" },
    {12, "WM_SETTEXT" },
    {13, "WM_GETTEXT" },
    {14, "WM_GETTEXTLENGTH" },
    {15, "WM_PAINT" },
    {16, "WM_CLOSE" },
    {17, "WM_QUERYENDSESSION" },
    {18, "WM_QUIT" },
    {19, "WM_QUERYOPEN" },
    {20, "WM_ERASEBKGND" },
    {21, "WM_SYSCOLORCHANGE" },
    {22, "WM_ENDSESSION" },
    {24, "WM_SHOWWINDOW" },
    {25, "WM_CTLCOLOR" },
    {26, "WM_WININICHANGE" },
    {27, "WM_DEVMODECHANGE" },
    {28, "WM_ACTIVATEAPP" },
    {29, "WM_FONTCHANGE" },
    {30, "WM_TIMECHANGE" },
    {31, "WM_CANCELMODE" },
    {32, "WM_SETCURSOR" },
    {33, "WM_MOUSEACTIVATE" },
    {34, "WM_CHILDACTIVATE" },
    {35, "WM_QUEUESYNC" },
    {36, "WM_GETMINMAXINFO" },
    {38, "WM_PAINTICON" },
    {39, "WM_ICONERASEBKGND" },
    {40, "WM_NEXTDLGCTL" },
    {42, "WM_SPOOLERSTATUS" },
    {43, "WM_DRAWITEM" },
    {44, "WM_MEASUREITEM" },
    {45, "WM_DELETEITEM" },
    {46, "WM_VKEYTOITEM" },
    {47, "WM_CHARTOITEM" },
    {48, "WM_SETFONT" },
    {49, "WM_GETFONT" },
    {50, "WM_SETHOTKEY" },
    {51, "WM_GETHOTKEY" },
    {55, "WM_QUERYDRAGICON" },
    {57, "WM_COMPAREITEM" },
    {61, "WM_GETOBJECT" },
    {65, "WM_COMPACTING" },
    {68, "WM_COMMNOTIFY" },
    {70, "WM_WINDOWPOSCHANGING" },
    {71, "WM_WINDOWPOSCHANGED" },
    {72, "WM_POWER" },
    {73, "WM_COPYGLOBALDATA" },
    {74, "WM_COPYDATA" },
    {75, "WM_CANCELJOURNAL" },
    {78, "WM_NOTIFY" },
    {80, "WM_INPUTLANGCHANGEREQUEST" },
    {81, "WM_INPUTLANGCHANGE" },
    {82, "WM_TCARD" },
    {83, "WM_HELP" },
    {84, "WM_USERCHANGED" },
    {85, "WM_NOTIFYFORMAT" },
    {123, "WM_CONTEXTMENU" },
    {124, "WM_STYLECHANGING" },
    {125, "WM_STYLECHANGED" },
    {126, "WM_DISPLAYCHANGE" },
    {127, "WM_GETICON" },
    {128, "WM_SETICON" },
    {129, "WM_NCCREATE" },
    {130, "WM_NCDESTROY" },
    {131, "WM_NCCALCSIZE" },
    {132, "WM_NCHITTEST" },
    {133, "WM_NCPAINT" },
    {134, "WM_NCACTIVATE" },
    {135, "WM_GETDLGCODE" },
    {136, "WM_SYNCPAINT" },
    {160, "WM_NCMOUSEMOVE" },
    {161, "WM_NCLBUTTONDOWN" },
    {162, "WM_NCLBUTTONUP" },
    {163, "WM_NCLBUTTONDBLCLK" },
    {164, "WM_NCRBUTTONDOWN" },
    {165, "WM_NCRBUTTONUP" },
    {166, "WM_NCRBUTTONDBLCLK" },
    {167, "WM_NCMBUTTONDOWN" },
    {168, "WM_NCMBUTTONUP" },
    {169, "WM_NCMBUTTONDBLCLK" },
    {171, "WM_NCXBUTTONDOWN" },
    {172, "WM_NCXBUTTONUP" },
    {173, "WM_NCXBUTTONDBLCLK" },
    {176, "EM_GETSEL" },
    {177, "EM_SETSEL" },
    {178, "EM_GETRECT" },
    {179, "EM_SETRECT" },
    {180, "EM_SETRECTNP" },
    {181, "EM_SCROLL" },
    {182, "EM_LINESCROLL" },
    {183, "EM_SCROLLCARET" },
    {185, "EM_GETMODIFY" },
    {187, "EM_SETMODIFY" },
    {188, "EM_GETLINECOUNT" },
    {189, "EM_LINEINDEX" },
    {190, "EM_SETHANDLE" },
    {191, "EM_GETHANDLE" },
    {192, "EM_GETTHUMB" },
    {193, "EM_LINELENGTH" },
    {194, "EM_REPLACESEL" },
    {195, "EM_SETFONT" },
    {196, "EM_GETLINE" },
    {197, "EM_LIMITTEXT" },
    {197, "EM_SETLIMITTEXT" },
    {198, "EM_CANUNDO" },
    {199, "EM_UNDO" },
    {200, "EM_FMTLINES" },
    {201, "EM_LINEFROMCHAR" },
    {202, "EM_SETWORDBREAK" },
    {203, "EM_SETTABSTOPS" },
    {204, "EM_SETPASSWORDCHAR" },
    {205, "EM_EMPTYUNDOBUFFER" },
    {206, "EM_GETFIRSTVISIBLELINE" },
    {207, "EM_SETREADONLY" },
    {209, "EM_SETWORDBREAKPROC" },
    {209, "EM_GETWORDBREAKPROC" },
    {210, "EM_GETPASSWORDCHAR" },
    {211, "EM_SETMARGINS" },
    {212, "EM_GETMARGINS" },
    {213, "EM_GETLIMITTEXT" },
    {214, "EM_POSFROMCHAR" },
    {215, "EM_CHARFROMPOS" },
    {216, "EM_SETIMESTATUS" },
    {217, "EM_GETIMESTATUS" },
    {224, "SBM_SETPOS" },
    {225, "SBM_GETPOS" },
    {226, "SBM_SETRANGE" },
    {227, "SBM_GETRANGE" },
    {228, "SBM_ENABLE_ARROWS" },
    {230, "SBM_SETRANGEREDRAW" },
    {233, "SBM_SETSCROLLINFO" },
    {234, "SBM_GETSCROLLINFO" },
    {235, "SBM_GETSCROLLBARINFO" },
    {240, "BM_GETCHECK" },
    {241, "BM_SETCHECK" },
    {242, "BM_GETSTATE" },
    {243, "BM_SETSTATE" },
    {244, "BM_SETSTYLE" },
    {245, "BM_CLICK" },
    {246, "BM_GETIMAGE" },
    {247, "BM_SETIMAGE" },
    {248, "BM_SETDONTCLICK" },
    {255, "WM_INPUT" },
    {256, "WM_KEYDOWN" },
    {256, "WM_KEYFIRST" },
    {257, "WM_KEYUP" },
    {258, "WM_CHAR" },
    {259, "WM_DEADCHAR" },
    {260, "WM_SYSKEYDOWN" },
    {261, "WM_SYSKEYUP" },
    {262, "WM_SYSCHAR" },
    {263, "WM_SYSDEADCHAR" },
    {264, "WM_KEYLAST" },
    {265, "WM_UNICHAR" },
    {265, "WM_WNT_CONVERTREQUESTEX" },
    {266, "WM_CONVERTREQUEST" },
    {267, "WM_CONVERTRESULT" },
    {268, "WM_INTERIM" },
    {269, "WM_IME_STARTCOMPOSITION" },
    {270, "WM_IME_ENDCOMPOSITION" },
    {271, "WM_IME_COMPOSITION" },
    {271, "WM_IME_KEYLAST" },
    {272, "WM_INITDIALOG" },
    {273, "WM_COMMAND" },
    {274, "WM_SYSCOMMAND" },
    {275, "WM_TIMER" },
    {276, "WM_HSCROLL" },
    {277, "WM_VSCROLL" },
    {278, "WM_INITMENU" },
    {279, "WM_INITMENUPOPUP" },
    {280, "WM_SYSTIMER" },
    {287, "WM_MENUSELECT" },
    {288, "WM_MENUCHAR" },
    {289, "WM_ENTERIDLE" },
    {290, "WM_MENURBUTTONUP" },
    {291, "WM_MENUDRAG" },
    {292, "WM_MENUGETOBJECT" },
    {293, "WM_UNINITMENUPOPUP" },
    {294, "WM_MENUCOMMAND" },
    {295, "WM_CHANGEUISTATE" },
    {296, "WM_UPDATEUISTATE" },
    {297, "WM_QUERYUISTATE" },
    {306, "WM_CTLCOLORMSGBOX" },
    {307, "WM_CTLCOLOREDIT" },
    {308, "WM_CTLCOLORLISTBOX" },
    {309, "WM_CTLCOLORBTN" },
    {310, "WM_CTLCOLORDLG" },
    {311, "WM_CTLCOLORSCROLLBAR" },
    {312, "WM_CTLCOLORSTATIC" },
    {512, "WM_MOUSEFIRST" },
    {512, "WM_MOUSEMOVE" },
    {513, "WM_LBUTTONDOWN" },
    {514, "WM_LBUTTONUP" },
    {515, "WM_LBUTTONDBLCLK" },
    {516, "WM_RBUTTONDOWN" },
    {517, "WM_RBUTTONUP" },
    {518, "WM_RBUTTONDBLCLK" },
    {519, "WM_MBUTTONDOWN" },
    {520, "WM_MBUTTONUP" },
    {521, "WM_MBUTTONDBLCLK" },
    {521, "WM_MOUSELAST" },
    {522, "WM_MOUSEWHEEL" },
    {523, "WM_XBUTTONDOWN" },
    {524, "WM_XBUTTONUP" },
    {525, "WM_XBUTTONDBLCLK" },
    {528, "WM_PARENTNOTIFY" },
    {529, "WM_ENTERMENULOOP" },
    {530, "WM_EXITMENULOOP" },
    {531, "WM_NEXTMENU" },
    {532, "WM_SIZING" },
    {533, "WM_CAPTURECHANGED" },
    {534, "WM_MOVING" },
    {536, "WM_POWERBROADCAST" },
    {537, "WM_DEVICECHANGE" },
    {544, "WM_MDICREATE" },
    {545, "WM_MDIDESTROY" },
    {546, "WM_MDIACTIVATE" },
    {547, "WM_MDIRESTORE" },
    {548, "WM_MDINEXT" },
    {549, "WM_MDIMAXIMIZE" },
    {550, "WM_MDITILE" },
    {551, "WM_MDICASCADE" },
    {552, "WM_MDIICONARRANGE" },
    {553, "WM_MDIGETACTIVE" },
    {560, "WM_MDISETMENU" },
    {561, "WM_ENTERSIZEMOVE" },
    {562, "WM_EXITSIZEMOVE" },
    {563, "WM_DROPFILES" },
    {564, "WM_MDIREFRESHMENU" },
    {640, "WM_IME_REPORT" },
    {641, "WM_IME_SETCONTEXT" },
    {642, "WM_IME_NOTIFY" },
    {643, "WM_IME_CONTROL" },
    {644, "WM_IME_COMPOSITIONFULL" },
    {645, "WM_IME_SELECT" },
    {646, "WM_IME_CHAR" },
    {648, "WM_IME_REQUEST" },
    {656, "WM_IMEKEYDOWN" },
    {656, "WM_IME_KEYDOWN" },
    {657, "WM_IMEKEYUP" },
    {657, "WM_IME_KEYUP" },
    {672, "WM_NCMOUSEHOVER" },
    {673, "WM_MOUSEHOVER" },
    {674, "WM_NCMOUSELEAVE" },
    {675, "WM_MOUSELEAVE" },
    {768, "WM_CUT" },
    {769, "WM_COPY" },
    {770, "WM_PASTE" },
    {771, "WM_CLEAR" },
    {772, "WM_UNDO" },
    {773, "WM_RENDERFORMAT" },
    {774, "WM_RENDERALLFORMATS" },
    {775, "WM_DESTROYCLIPBOARD" },
    {776, "WM_DRAWCLIPBOARD" },
    {777, "WM_PAINTCLIPBOARD" },
    {778, "WM_VSCROLLCLIPBOARD" },
    {779, "WM_SIZECLIPBOARD" },
    {780, "WM_ASKCBFORMATNAME" },
    {781, "WM_CHANGECBCHAIN" },
    {782, "WM_HSCROLLCLIPBOARD" },
    {783, "WM_QUERYNEWPALETTE" },
    {784, "WM_PALETTEISCHANGING" },
    {785, "WM_PALETTECHANGED" },
    {786, "WM_HOTKEY" },
    {791, "WM_PRINT" },
    {792, "WM_PRINTCLIENT" },
    {793, "WM_APPCOMMAND" },
    {856, "WM_HANDHELDFIRST" },
    {863, "WM_HANDHELDLAST" },
    {864, "WM_AFXFIRST" },
    {895, "WM_AFXLAST" },
    {896, "WM_PENWINFIRST" },
    {897, "WM_RCRESULT" },
    {898, "WM_HOOKRCRESULT" },
    {899, "WM_GLOBALRCCHANGE" },
    {899, "WM_PENMISCINFO" },
    {900, "WM_SKB" },
    {901, "WM_HEDITCTL" },
    {901, "WM_PENCTL" },
    {902, "WM_PENMISC" },
    {903, "WM_CTLINIT" },
    {904, "WM_PENEVENT" },
    {911, "WM_PENWINLAST" },
    {1024, "DDM_SETFMT" },
    {1024, "DM_GETDEFID" },
    {1024, "NIN_SELECT" },
    {1024, "TBM_GETPOS" },
    {1024, "WM_PSD_PAGESETUPDLG" },
    {1024, "WM_USER" },
    {1025, "CBEM_INSERTITEMA" },
    {1025, "DDM_DRAW" },
    {1025, "DM_SETDEFID" },
    {1025, "HKM_SETHOTKEY" },
    {1025, "PBM_SETRANGE" },
    {1025, "RB_INSERTBANDA" },
    {1025, "SB_SETTEXTA" },
    {1025, "TB_ENABLEBUTTON" },
    {1025, "TBM_GETRANGEMIN" },
    {1025, "TTM_ACTIVATE" },
    {1025, "WM_CHOOSEFONT_GETLOGFONT" },
    {1025, "WM_PSD_FULLPAGERECT" },
    {1026, "CBEM_SETIMAGELIST" },
    {1026, "DDM_CLOSE" },
    {1026, "DM_REPOSITION" },
    {1026, "HKM_GETHOTKEY" },
    {1026, "PBM_SETPOS" },
    {1026, "RB_DELETEBAND" },
    {1026, "SB_GETTEXTA" },
    {1026, "TB_CHECKBUTTON" },
    {1026, "TBM_GETRANGEMAX" },
    {1026, "WM_PSD_MINMARGINRECT" },
    {1027, "CBEM_GETIMAGELIST" },
    {1027, "DDM_BEGIN" },
    {1027, "HKM_SETRULES" },
    {1027, "PBM_DELTAPOS" },
    {1027, "RB_GETBARINFO" },
    {1027, "SB_GETTEXTLENGTHA" },
    {1027, "TBM_GETTIC" },
    {1027, "TB_PRESSBUTTON" },
    {1027, "TTM_SETDELAYTIME" },
    {1027, "WM_PSD_MARGINRECT" },
    {1028, "CBEM_GETITEMA" },
    {1028, "DDM_END" },
    {1028, "PBM_SETSTEP" },
    {1028, "RB_SETBARINFO" },
    {1028, "SB_SETPARTS" },
    {1028, "TB_HIDEBUTTON" },
    {1028, "TBM_SETTIC" },
    {1028, "TTM_ADDTOOLA" },
    {1028, "WM_PSD_GREEKTEXTRECT" },
    {1029, "CBEM_SETITEMA" },
    {1029, "PBM_STEPIT" },
    {1029, "TB_INDETERMINATE" },
    {1029, "TBM_SETPOS" },
    {1029, "TTM_DELTOOLA" },
    {1029, "WM_PSD_ENVSTAMPRECT" },
    {1030, "CBEM_GETCOMBOCONTROL" },
    {1030, "PBM_SETRANGE32" },
    {1030, "RB_SETBANDINFOA" },
    {1030, "SB_GETPARTS" },
    {1030, "TB_MARKBUTTON" },
    {1030, "TBM_SETRANGE" },
    {1030, "TTM_NEWTOOLRECTA" },
    {1030, "WM_PSD_YAFULLPAGERECT" },
    {1031, "CBEM_GETEDITCONTROL" },
    {1031, "PBM_GETRANGE" },
    {1031, "RB_SETPARENT" },
    {1031, "SB_GETBORDERS" },
    {1031, "TBM_SETRANGEMIN" },
    {1031, "TTM_RELAYEVENT" },
    {1032, "CBEM_SETEXSTYLE" },
    {1032, "PBM_GETPOS" },
    {1032, "RB_HITTEST" },
    {1032, "SB_SETMINHEIGHT" },
    {1032, "TBM_SETRANGEMAX" },
    {1032, "TTM_GETTOOLINFOA" },
    {1033, "CBEM_GETEXSTYLE" },
    {1033, "CBEM_GETEXTENDEDSTYLE" },
    {1033, "PBM_SETBARCOLOR" },
    {1033, "RB_GETRECT" },
    {1033, "SB_SIMPLE" },
    {1033, "TB_ISBUTTONENABLED" },
    {1033, "TBM_CLEARTICS" },
    {1033, "TTM_SETTOOLINFOA" },
    {1034, "CBEM_HASEDITCHANGED" },
    {1034, "RB_INSERTBANDW" },
    {1034, "SB_GETRECT" },
    {1034, "TB_ISBUTTONCHECKED" },
    {1034, "TBM_SETSEL" },
    {1034, "TTM_HITTESTA" },
    {1034, "WIZ_QUERYNUMPAGES" },
    {1035, "CBEM_INSERTITEMW" },
    {1035, "RB_SETBANDINFOW" },
    {1035, "SB_SETTEXTW" },
    {1035, "TB_ISBUTTONPRESSED" },
    {1035, "TBM_SETSELSTART" },
    {1035, "TTM_GETTEXTA" },
    {1035, "WIZ_NEXT" },
    {1036, "CBEM_SETITEMW" },
    {1036, "RB_GETBANDCOUNT" },
    {1036, "SB_GETTEXTLENGTHW" },
    {1036, "TB_ISBUTTONHIDDEN" },
    {1036, "TBM_SETSELEND" },
    {1036, "TTM_UPDATETIPTEXTA" },
    {1036, "WIZ_PREV" },
    {1037, "CBEM_GETITEMW" },
    {1037, "RB_GETROWCOUNT" },
    {1037, "SB_GETTEXTW" },
    {1037, "TB_ISBUTTONINDETERMINATE" },
    {1037, "TTM_GETTOOLCOUNT" },
    {1038, "CBEM_SETEXTENDEDSTYLE" },
    {1038, "RB_GETROWHEIGHT" },
    {1038, "SB_ISSIMPLE" },
    {1038, "TB_ISBUTTONHIGHLIGHTED" },
    {1038, "TBM_GETPTICS" },
    {1038, "TTM_ENUMTOOLSA" },
    {1039, "SB_SETICON" },
    {1039, "TBM_GETTICPOS" },
    {1039, "TTM_GETCURRENTTOOLA" },
    {1040, "RB_IDTOINDEX" },
    {1040, "SB_SETTIPTEXTA" },
    {1040, "TBM_GETNUMTICS" },
    {1040, "TTM_WINDOWFROMPOINT" },
    {1041, "RB_GETTOOLTIPS" },
    {1041, "SB_SETTIPTEXTW" },
    {1041, "TBM_GETSELSTART" },
    {1041, "TB_SETSTATE" },
    {1041, "TTM_TRACKACTIVATE" },
    {1042, "RB_SETTOOLTIPS" },
    {1042, "SB_GETTIPTEXTA" },
    {1042, "TB_GETSTATE" },
    {1042, "TBM_GETSELEND" },
    {1042, "TTM_TRACKPOSITION" },
    {1043, "RB_SETBKCOLOR" },
    {1043, "SB_GETTIPTEXTW" },
    {1043, "TB_ADDBITMAP" },
    {1043, "TBM_CLEARSEL" },
    {1043, "TTM_SETTIPBKCOLOR" },
    {1044, "RB_GETBKCOLOR" },
    {1044, "SB_GETICON" },
    {1044, "TB_ADDBUTTONSA" },
    {1044, "TBM_SETTICFREQ" },
    {1044, "TTM_SETTIPTEXTCOLOR" },
    {1045, "RB_SETTEXTCOLOR" },
    {1045, "TB_INSERTBUTTONA" },
    {1045, "TBM_SETPAGESIZE" },
    {1045, "TTM_GETDELAYTIME" },
    {1046, "RB_GETTEXTCOLOR" },
    {1046, "TB_DELETEBUTTON" },
    {1046, "TBM_GETPAGESIZE" },
    {1046, "TTM_GETTIPBKCOLOR" },
    {1047, "RB_SIZETORECT" },
    {1047, "TB_GETBUTTON" },
    {1047, "TBM_SETLINESIZE" },
    {1047, "TTM_GETTIPTEXTCOLOR" },
    {1048, "RB_BEGINDRAG" },
    {1048, "TB_BUTTONCOUNT" },
    {1048, "TBM_GETLINESIZE" },
    {1048, "TTM_SETMAXTIPWIDTH" },
    {1049, "RB_ENDDRAG" },
    {1049, "TB_COMMANDTOINDEX" },
    {1049, "TBM_GETTHUMBRECT" },
    {1049, "TTM_GETMAXTIPWIDTH" },
    {1050, "RB_DRAGMOVE" },
    {1050, "TBM_GETCHANNELRECT" },
    {1050, "TB_SAVERESTOREA" },
    {1050, "TTM_SETMARGIN" },
    {1051, "RB_GETBARHEIGHT" },
    {1051, "TB_CUSTOMIZE" },
    {1051, "TBM_SETTHUMBLENGTH" },
    {1051, "TTM_GETMARGIN" },
    {1052, "RB_GETBANDINFOW" },
    {1052, "TB_ADDSTRINGA" },
    {1052, "TBM_GETTHUMBLENGTH" },
    {1052, "TTM_POP" },
    {1053, "RB_GETBANDINFOA" },
    {1053, "TB_GETITEMRECT" },
    {1053, "TBM_SETTOOLTIPS" },
    {1053, "TTM_UPDATE" },
    {1054, "RB_MINIMIZEBAND" },
    {1054, "TB_BUTTONSTRUCTSIZE" },
    {1054, "TBM_GETTOOLTIPS" },
    {1054, "TTM_GETBUBBLESIZE" },
    {1055, "RB_MAXIMIZEBAND" },
    {1055, "TBM_SETTIPSIDE" },
    {1055, "TB_SETBUTTONSIZE" },
    {1055, "TTM_ADJUSTRECT" },
    {1056, "TBM_SETBUDDY" },
    {1056, "TB_SETBITMAPSIZE" },
    {1056, "TTM_SETTITLEA" },
    {1057, "MSG_FTS_JUMP_VA" },
    {1057, "TB_AUTOSIZE" },
    {1057, "TBM_GETBUDDY" },
    {1057, "TTM_SETTITLEW" },
    {1058, "RB_GETBANDBORDERS" },
    {1059, "MSG_FTS_JUMP_QWORD" },
    {1059, "RB_SHOWBAND" },
    {1059, "TB_GETTOOLTIPS" },
    {1060, "MSG_REINDEX_REQUEST" },
    {1060, "TB_SETTOOLTIPS" },
    {1061, "MSG_FTS_WHERE_IS_IT" },
    {1061, "RB_SETPALETTE" },
    {1061, "TB_SETPARENT" },
    {1062, "RB_GETPALETTE" },
    {1063, "RB_MOVEBAND" },
    {1063, "TB_SETROWS" },
    {1064, "TB_GETROWS" },
    {1065, "TB_GETBITMAPFLAGS" },
    {1066, "TB_SETCMDID" },
    {1067, "RB_PUSHCHEVRON" },
    {1067, "TB_CHANGEBITMAP" },
    {1068, "TB_GETBITMAP" },
    {1069, "MSG_GET_DEFFONT" },
    {1069, "TB_GETBUTTONTEXTA" },
    {1070, "TB_REPLACEBITMAP" },
    {1071, "TB_SETINDENT" },
    {1072, "TB_SETIMAGELIST" },
    {1073, "TB_GETIMAGELIST" },
    {1074, "TB_LOADIMAGES" },
    {1074, "EM_CANPASTE" },
    {1074, "TTM_ADDTOOLW" },
    {1075, "EM_DISPLAYBAND" },
    {1075, "TB_GETRECT" },
    {1075, "TTM_DELTOOLW" },
    {1076, "EM_EXGETSEL" },
    {1076, "TB_SETHOTIMAGELIST" },
    {1076, "TTM_NEWTOOLRECTW" },
    {1077, "EM_EXLIMITTEXT" },
    {1077, "TB_GETHOTIMAGELIST" },
    {1077, "TTM_GETTOOLINFOW" },
    {1078, "EM_EXLINEFROMCHAR" },
    {1078, "TB_SETDISABLEDIMAGELIST" },
    {1078, "TTM_SETTOOLINFOW" },
    {1079, "EM_EXSETSEL" },
    {1079, "TB_GETDISABLEDIMAGELIST" },
    {1079, "TTM_HITTESTW" },
    {1080, "EM_FINDTEXT" },
    {1080, "TB_SETSTYLE" },
    {1080, "TTM_GETTEXTW" },
    {1081, "EM_FORMATRANGE" },
    {1081, "TB_GETSTYLE" },
    {1081, "TTM_UPDATETIPTEXTW" },
    {1082, "EM_GETCHARFORMAT" },
    {1082, "TB_GETBUTTONSIZE" },
    {1082, "TTM_ENUMTOOLSW" },
    {1083, "EM_GETEVENTMASK" },
    {1083, "TB_SETBUTTONWIDTH" },
    {1083, "TTM_GETCURRENTTOOLW" },
    {1084, "EM_GETOLEINTERFACE" },
    {1084, "TB_SETMAXTEXTROWS" },
    {1085, "EM_GETPARAFORMAT" },
    {1085, "TB_GETTEXTROWS" },
    {1086, "EM_GETSELTEXT" },
    {1086, "TB_GETOBJECT" },
    {1087, "EM_HIDESELECTION" },
    {1087, "TB_GETBUTTONINFOW" },
    {1088, "EM_PASTESPECIAL" },
    {1088, "TB_SETBUTTONINFOW" },
    {1089, "EM_REQUESTRESIZE" },
    {1089, "TB_GETBUTTONINFOA" },
    {1090, "EM_SELECTIONTYPE" },
    {1090, "TB_SETBUTTONINFOA" },
    {1091, "EM_SETBKGNDCOLOR" },
    {1091, "TB_INSERTBUTTONW" },
    {1092, "EM_SETCHARFORMAT" },
    {1092, "TB_ADDBUTTONSW" },
    {1093, "EM_SETEVENTMASK" },
    {1093, "TB_HITTEST" },
    {1094, "EM_SETOLECALLBACK" },
    {1094, "TB_SETDRAWTEXTFLAGS" },
    {1095, "EM_SETPARAFORMAT" },
    {1095, "TB_GETHOTITEM" },
    {1096, "EM_SETTARGETDEVICE" },
    {1096, "TB_SETHOTITEM" },
    {1097, "EM_STREAMIN" },
    {1097, "TB_SETANCHORHIGHLIGHT" },
    {1098, "EM_STREAMOUT" },
    {1098, "TB_GETANCHORHIGHLIGHT" },
    {1099, "EM_GETTEXTRANGE" },
    {1099, "TB_GETBUTTONTEXTW" },
    {1100, "EM_FINDWORDBREAK" },
    {1100, "TB_SAVERESTOREW" },
    {1101, "EM_SETOPTIONS" },
    {1101, "TB_ADDSTRINGW" },
    {1102, "EM_GETOPTIONS" },
    {1102, "TB_MAPACCELERATORA" },
    {1103, "EM_FINDTEXTEX" },
    {1103, "TB_GETINSERTMARK" },
    {1104, "EM_GETWORDBREAKPROCEX" },
    {1104, "TB_SETINSERTMARK" },
    {1105, "EM_SETWORDBREAKPROCEX" },
    {1105, "TB_INSERTMARKHITTEST" },
    {1106, "EM_SETUNDOLIMIT" },
    {1106, "TB_MOVEBUTTON" },
    {1107, "TB_GETMAXSIZE" },
    {1108, "EM_REDO" },
    {1108, "TB_SETEXTENDEDSTYLE" },
    {1109, "EM_CANREDO" },
    {1109, "TB_GETEXTENDEDSTYLE" },
    {1110, "EM_GETUNDONAME" },
    {1110, "TB_GETPADDING" },
    {1111, "EM_GETREDONAME" },
    {1111, "TB_SETPADDING" },
    {1112, "EM_STOPGROUPTYPING" },
    {1112, "TB_SETINSERTMARKCOLOR" },
    {1113, "EM_SETTEXTMODE" },
    {1113, "TB_GETINSERTMARKCOLOR" },
    {1114, "EM_GETTEXTMODE" },
    {1114, "TB_MAPACCELERATORW" },
    {1115, "EM_AUTOURLDETECT" },
    {1115, "TB_GETSTRINGW" },
    {1116, "EM_GETAUTOURLDETECT" },
    {1116, "TB_GETSTRINGA" },
    {1117, "EM_SETPALETTE" },
    {1118, "EM_GETTEXTEX" },
    {1119, "EM_GETTEXTLENGTHEX" },
    {1120, "EM_SHOWSCROLLBAR" },
    {1121, "EM_SETTEXTEX" },
    {1123, "TAPI_REPLY" },
    {1124, "ACM_OPENA" },
    {1124, "BFFM_SETSTATUSTEXTA" },
    {1124, "CDM_FIRST" },
    {1124, "CDM_GETSPEC" },
    {1124, "EM_SETPUNCTUATION" },
    {1124, "IPM_CLEARADDRESS" },
    {1124, "WM_CAP_UNICODE_START" },
    {1125, "ACM_PLAY" },
    {1125, "BFFM_ENABLEOK" },
    {1125, "CDM_GETFILEPATH" },
    {1125, "EM_GETPUNCTUATION" },
    {1125, "IPM_SETADDRESS" },
    {1125, "PSM_SETCURSEL" },
    {1125, "UDM_SETRANGE" },
    {1125, "WM_CHOOSEFONT_SETLOGFONT" },
    {1126, "ACM_STOP" },
    {1126, "BFFM_SETSELECTIONA" },
    {1126, "CDM_GETFOLDERPATH" },
    {1126, "EM_SETWORDWRAPMODE" },
    {1126, "IPM_GETADDRESS" },
    {1126, "PSM_REMOVEPAGE" },
    {1126, "UDM_GETRANGE" },
    {1126, "WM_CAP_SET_CALLBACK_ERRORW" },
    {1126, "WM_CHOOSEFONT_SETFLAGS" },
    {1127, "ACM_OPENW" },
    {1127, "BFFM_SETSELECTIONW" },
    {1127, "CDM_GETFOLDERIDLIST" },
    {1127, "EM_GETWORDWRAPMODE" },
    {1127, "IPM_SETRANGE" },
    {1127, "PSM_ADDPAGE" },
    {1127, "UDM_SETPOS" },
    {1127, "WM_CAP_SET_CALLBACK_STATUSW" },
    {1128, "BFFM_SETSTATUSTEXTW" },
    {1128, "CDM_SETCONTROLTEXT" },
    {1128, "EM_SETIMECOLOR" },
    {1128, "IPM_SETFOCUS" },
    {1128, "PSM_CHANGED" },
    {1128, "UDM_GETPOS" },
    {1129, "CDM_HIDECONTROL" },
    {1129, "EM_GETIMECOLOR" },
    {1129, "IPM_ISBLANK" },
    {1129, "PSM_RESTARTWINDOWS" },
    {1129, "UDM_SETBUDDY" },
    {1130, "CDM_SETDEFEXT" },
    {1130, "EM_SETIMEOPTIONS" },
    {1130, "PSM_REBOOTSYSTEM" },
    {1130, "UDM_GETBUDDY" },
    {1131, "EM_GETIMEOPTIONS" },
    {1131, "PSM_CANCELTOCLOSE" },
    {1131, "UDM_SETACCEL" },
    {1132, "EM_CONVPOSITION" },
    {1132, "EM_CONVPOSITION" },
    {1132, "PSM_QUERYSIBLINGS" },
    {1132, "UDM_GETACCEL" },
    {1133, "MCIWNDM_GETZOOM" },
    {1133, "PSM_UNCHANGED" },
    {1133, "UDM_SETBASE" },
    {1134, "PSM_APPLY" },
    {1134, "UDM_GETBASE" },
    {1135, "PSM_SETTITLEA" },
    {1135, "UDM_SETRANGE32" },
    {1136, "PSM_SETWIZBUTTONS" },
    {1136, "UDM_GETRANGE32" },
    {1136, "WM_CAP_DRIVER_GET_NAMEW" },
    {1137, "PSM_PRESSBUTTON" },
    {1137, "UDM_SETPOS32" },
    {1137, "WM_CAP_DRIVER_GET_VERSIONW" },
    {1138, "PSM_SETCURSELID" },
    {1138, "UDM_GETPOS32" },
    {1139, "PSM_SETFINISHTEXTA" },
    {1140, "PSM_GETTABCONTROL" },
    {1141, "PSM_ISDIALOGMESSAGE" },
    {1142, "MCIWNDM_REALIZE" },
    {1142, "PSM_GETCURRENTPAGEHWND" },
    {1143, "MCIWNDM_SETTIMEFORMATA" },
    {1143, "PSM_INSERTPAGE" },
    {1144, "EM_SETLANGOPTIONS" },
    {1144, "MCIWNDM_GETTIMEFORMATA" },
    {1144, "PSM_SETTITLEW" },
    {1144, "WM_CAP_FILE_SET_CAPTURE_FILEW" },
    {1145, "EM_GETLANGOPTIONS" },
    {1145, "MCIWNDM_VALIDATEMEDIA" },
    {1145, "PSM_SETFINISHTEXTW" },
    {1145, "WM_CAP_FILE_GET_CAPTURE_FILEW" },
    {1146, "EM_GETIMECOMPMODE" },
    {1147, "EM_FINDTEXTW" },
    {1147, "MCIWNDM_PLAYTO" },
    {1147, "WM_CAP_FILE_SAVEASW" },
    {1148, "EM_FINDTEXTEXW" },
    {1148, "MCIWNDM_GETFILENAMEA" },
    {1149, "EM_RECONVERSION" },
    {1149, "MCIWNDM_GETDEVICEA" },
    {1149, "PSM_SETHEADERTITLEA" },
    {1149, "WM_CAP_FILE_SAVEDIBW" },
    {1150, "EM_SETIMEMODEBIAS" },
    {1150, "MCIWNDM_GETPALETTE" },
    {1150, "PSM_SETHEADERTITLEW" },
    {1151, "EM_GETIMEMODEBIAS" },
    {1151, "MCIWNDM_SETPALETTE" },
    {1151, "PSM_SETHEADERSUBTITLEA" },
    {1152, "MCIWNDM_GETERRORA" },
    {1152, "PSM_SETHEADERSUBTITLEW" },
    {1153, "PSM_HWNDTOINDEX" },
    {1154, "PSM_INDEXTOHWND" },
    {1155, "MCIWNDM_SETINACTIVETIMER" },
    {1155, "PSM_PAGETOINDEX" },
    {1156, "PSM_INDEXTOPAGE" },
    {1157, "DL_BEGINDRAG" },
    {1157, "MCIWNDM_GETINACTIVETIMER" },
    {1157, "PSM_IDTOINDEX" },
    {1158, "DL_DRAGGING" },
    {1158, "PSM_INDEXTOID" },
    {1159, "DL_DROPPED" },
    {1159, "PSM_GETRESULT" },
    {1160, "DL_CANCELDRAG" },
    {1160, "PSM_RECALCPAGESIZES" },
    {1164, "MCIWNDM_GET_SOURCE" },
    {1165, "MCIWNDM_PUT_SOURCE" },
    {1166, "MCIWNDM_GET_DEST" },
    {1167, "MCIWNDM_PUT_DEST" },
    {1168, "MCIWNDM_CAN_PLAY" },
    {1169, "MCIWNDM_CAN_WINDOW" },
    {1170, "MCIWNDM_CAN_RECORD" },
    {1171, "MCIWNDM_CAN_SAVE" },
    {1172, "MCIWNDM_CAN_EJECT" },
    {1173, "MCIWNDM_CAN_CONFIG" },
    {1174, "IE_GETINK" },
    {1174, "IE_MSGFIRST" },
    {1174, "MCIWNDM_PALETTEKICK" },
    {1175, "IE_SETINK" },
    {1176, "IE_GETPENTIP" },
    {1177, "IE_SETPENTIP" },
    {1178, "IE_GETERASERTIP" },
    {1179, "IE_SETERASERTIP" },
    {1180, "IE_GETBKGND" },
    {1181, "IE_SETBKGND" },
    {1182, "IE_GETGRIDORIGIN" },
    {1183, "IE_SETGRIDORIGIN" },
    {1184, "IE_GETGRIDPEN" },
    {1185, "IE_SETGRIDPEN" },
    {1186, "IE_GETGRIDSIZE" },
    {1187, "IE_SETGRIDSIZE" },
    {1188, "IE_GETMODE" },
    {1189, "IE_SETMODE" },
    {1190, "IE_GETINKRECT" },
    {1190, "WM_CAP_SET_MCI_DEVICEW" },
    {1191, "WM_CAP_GET_MCI_DEVICEW" },
    {1204, "WM_CAP_PAL_OPENW" },
    {1205, "WM_CAP_PAL_SAVEW" },
    {1208, "IE_GETAPPDATA" },
    {1209, "IE_SETAPPDATA" },
    {1210, "IE_GETDRAWOPTS" },
    {1211, "IE_SETDRAWOPTS" },
    {1212, "IE_GETFORMAT" },
    {1213, "IE_SETFORMAT" },
    {1214, "IE_GETINKINPUT" },
    {1215, "IE_SETINKINPUT" },
    {1216, "IE_GETNOTIFY" },
    {1217, "IE_SETNOTIFY" },
    {1218, "IE_GETRECOG" },
    {1219, "IE_SETRECOG" },
    {1220, "IE_GETSECURITY" },
    {1221, "IE_SETSECURITY" },
    {1222, "IE_GETSEL" },
    {1223, "IE_SETSEL" },
    {1224, "CDM_LAST" },
    {1224, "EM_SETBIDIOPTIONS" },
    {1224, "IE_DOCOMMAND" },
    {1224, "MCIWNDM_NOTIFYMODE" },
    {1225, "EM_GETBIDIOPTIONS" },
    {1225, "IE_GETCOMMAND" },
    {1226, "EM_SETTYPOGRAPHYOPTIONS" },
    {1226, "IE_GETCOUNT" },
    {1227, "EM_GETTYPOGRAPHYOPTIONS" },
    {1227, "IE_GETGESTURE" },
    {1227, "MCIWNDM_NOTIFYMEDIA" },
    {1228, "EM_SETEDITSTYLE" },
    {1228, "IE_GETMENU" },
    {1229, "EM_GETEDITSTYLE" },
    {1229, "IE_GETPAINTDC" },
    {1229, "MCIWNDM_NOTIFYERROR" },
    {1230, "IE_GETPDEVENT" },
    {1231, "IE_GETSELCOUNT" },
    {1232, "IE_GETSELITEMS" },
    {1233, "IE_GETSTYLE" },
    {1243, "MCIWNDM_SETTIMEFORMATW" },
    {1244, "EM_OUTLINE" },
    {1244, "EM_OUTLINE" },
    {1244, "MCIWNDM_GETTIMEFORMATW" },
    {1245, "EM_GETSCROLLPOS" },
    {1245, "EM_GETSCROLLPOS" },
    {1246, "EM_SETSCROLLPOS" },
    {1246, "EM_SETSCROLLPOS" },
    {1247, "EM_SETFONTSIZE" },
    {1247, "EM_SETFONTSIZE" },
    {1248, "EM_GETZOOM" },
    {1248, "MCIWNDM_GETFILENAMEW" },
    {1249, "EM_SETZOOM" },
    {1249, "MCIWNDM_GETDEVICEW" },
    {1250, "EM_GETVIEWKIND" },
    {1251, "EM_SETVIEWKIND" },
    {1252, "EM_GETPAGE" },
    {1252, "MCIWNDM_GETERRORW" },
    {1253, "EM_SETPAGE" },
    {1254, "EM_GETHYPHENATEINFO" },
    {1255, "EM_SETHYPHENATEINFO" },
    {1259, "EM_GETPAGEROTATE" },
    {1260, "EM_SETPAGEROTATE" },
    {1261, "EM_GETCTFMODEBIAS" },
    {1262, "EM_SETCTFMODEBIAS" },
    {1264, "EM_GETCTFOPENSTATUS" },
    {1265, "EM_SETCTFOPENSTATUS" },
    {1266, "EM_GETIMECOMPTEXT" },
    {1267, "EM_ISIME" },
    {1268, "EM_GETIMEPROPERTY" },
    {1293, "EM_GETQUERYRTFOBJ" },
    {1294, "EM_SETQUERYRTFOBJ" },
    {1536, "FM_GETFOCUS" },
    {1537, "FM_GETDRIVEINFOA" },
    {1538, "FM_GETSELCOUNT" },
    {1539, "FM_GETSELCOUNTLFN" },
    {1540, "FM_GETFILESELA" },
    {1541, "FM_GETFILESELLFNA" },
    {1542, "FM_REFRESH_WINDOWS" },
    {1543, "FM_RELOAD_EXTENSIONS" },
    {1553, "FM_GETDRIVEINFOW" },
    {1556, "FM_GETFILESELW" },
    {1557, "FM_GETFILESELLFNW" },
    {1625, "WLX_WM_SAS" },
    {2024, "SM_GETSELCOUNT" },
    {2024, "UM_GETSELCOUNT" },
    {2024, "WM_CPL_LAUNCH" },
    {2025, "SM_GETSERVERSELA" },
    {2025, "UM_GETUSERSELA" },
    {2025, "WM_CPL_LAUNCHED" },
    {2026, "SM_GETSERVERSELW" },
    {2026, "UM_GETUSERSELW" },
    {2027, "SM_GETCURFOCUSA" },
    {2027, "UM_GETGROUPSELA" },
    {2028, "SM_GETCURFOCUSW" },
    {2028, "UM_GETGROUPSELW" },
    {2029, "SM_GETOPTIONS" },
    {2029, "UM_GETCURFOCUSA" },
    {2030, "UM_GETCURFOCUSW" },
    {2031, "UM_GETOPTIONS" },
    {2032, "UM_GETOPTIONS2" },
    {4096, "LVM_FIRST" },
    {4096, "LVM_GETBKCOLOR" },
    {4097, "LVM_SETBKCOLOR" },
    {4098, "LVM_GETIMAGELIST" },
    {4099, "LVM_SETIMAGELIST" },
    {4100, "LVM_GETITEMCOUNT" },
    {4101, "LVM_GETITEMA" },
    {4102, "LVM_SETITEMA" },
    {4103, "LVM_INSERTITEMA" },
    {4104, "LVM_DELETEITEM" },
    {4105, "LVM_DELETEALLITEMS" },
    {4106, "LVM_GETCALLBACKMASK" },
    {4107, "LVM_SETCALLBACKMASK" },
    {4108, "LVM_GETNEXTITEM" },
    {4109, "LVM_FINDITEMA" },
    {4110, "LVM_GETITEMRECT" },
    {4111, "LVM_SETITEMPOSITION" },
    {4112, "LVM_GETITEMPOSITION" },
    {4113, "LVM_GETSTRINGWIDTHA" },
    {4114, "LVM_HITTEST" },
    {4115, "LVM_ENSUREVISIBLE" },
    {4116, "LVM_SCROLL" },
    {4117, "LVM_REDRAWITEMS" },
    {4118, "LVM_ARRANGE" },
    {4119, "LVM_EDITLABELA" },
    {4120, "LVM_GETEDITCONTROL" },
    {4121, "LVM_GETCOLUMNA" },
    {4122, "LVM_SETCOLUMNA" },
    {4123, "LVM_INSERTCOLUMNA" },
    {4124, "LVM_DELETECOLUMN" },
    {4125, "LVM_GETCOLUMNWIDTH" },
    {4126, "LVM_SETCOLUMNWIDTH" },
    {4127, "LVM_GETHEADER" },
    {4129, "LVM_CREATEDRAGIMAGE" },
    {4130, "LVM_GETVIEWRECT" },
    {4131, "LVM_GETTEXTCOLOR" },
    {4132, "LVM_SETTEXTCOLOR" },
    {4133, "LVM_GETTEXTBKCOLOR" },
    {4134, "LVM_SETTEXTBKCOLOR" },
    {4135, "LVM_GETTOPINDEX" },
    {4136, "LVM_GETCOUNTPERPAGE" },
    {4137, "LVM_GETORIGIN" },
    {4138, "LVM_UPDATE" },
    {4139, "LVM_SETITEMSTATE" },
    {4140, "LVM_GETITEMSTATE" },
    {4141, "LVM_GETITEMTEXTA" },
    {4142, "LVM_SETITEMTEXTA" },
    {4143, "LVM_SETITEMCOUNT" },
    {4144, "LVM_SORTITEMS" },
    {4145, "LVM_SETITEMPOSITION32" },
    {4146, "LVM_GETSELECTEDCOUNT" },
    {4147, "LVM_GETITEMSPACING" },
    {4148, "LVM_GETISEARCHSTRINGA" },
    {4149, "LVM_SETICONSPACING" },
    {4150, "LVM_SETEXTENDEDLISTVIEWSTYLE" },
    {4151, "LVM_GETEXTENDEDLISTVIEWSTYLE" },
    {4152, "LVM_GETSUBITEMRECT" },
    {4153, "LVM_SUBITEMHITTEST" },
    {4154, "LVM_SETCOLUMNORDERARRAY" },
    {4155, "LVM_GETCOLUMNORDERARRAY" },
    {4156, "LVM_SETHOTITEM" },
    {4157, "LVM_GETHOTITEM" },
    {4158, "LVM_SETHOTCURSOR" },
    {4159, "LVM_GETHOTCURSOR" },
    {4160, "LVM_APPROXIMATEVIEWRECT" },
    {4161, "LVM_SETWORKAREAS" },
    {4162, "LVM_GETSELECTIONMARK" },
    {4163, "LVM_SETSELECTIONMARK" },
    {4164, "LVM_SETBKIMAGEA" },
    {4165, "LVM_GETBKIMAGEA" },
    {4166, "LVM_GETWORKAREAS" },
    {4167, "LVM_SETHOVERTIME" },
    {4168, "LVM_GETHOVERTIME" },
    {4169, "LVM_GETNUMBEROFWORKAREAS" },
    {4170, "LVM_SETTOOLTIPS" },
    {4171, "LVM_GETITEMW" },
    {4172, "LVM_SETITEMW" },
    {4173, "LVM_INSERTITEMW" },
    {4174, "LVM_GETTOOLTIPS" },
    {4179, "LVM_FINDITEMW" },
    {4183, "LVM_GETSTRINGWIDTHW" },
    {4191, "LVM_GETCOLUMNW" },
    {4192, "LVM_SETCOLUMNW" },
    {4193, "LVM_INSERTCOLUMNW" },
    {4211, "LVM_GETITEMTEXTW" },
    {4212, "LVM_SETITEMTEXTW" },
    {4213, "LVM_GETISEARCHSTRINGW" },
    {4214, "LVM_EDITLABELW" },
    {4235, "LVM_GETBKIMAGEW" },
    {4236, "LVM_SETSELECTEDCOLUMN" },
    {4237, "LVM_SETTILEWIDTH" },
    {4238, "LVM_SETVIEW" },
    {4239, "LVM_GETVIEW" },
    {4241, "LVM_INSERTGROUP" },
    {4243, "LVM_SETGROUPINFO" },
    {4245, "LVM_GETGROUPINFO" },
    {4246, "LVM_REMOVEGROUP" },
    {4247, "LVM_MOVEGROUP" },
    {4250, "LVM_MOVEITEMTOGROUP" },
    {4251, "LVM_SETGROUPMETRICS" },
    {4252, "LVM_GETGROUPMETRICS" },
    {4253, "LVM_ENABLEGROUPVIEW" },
    {4254, "LVM_SORTGROUPS" },
    {4255, "LVM_INSERTGROUPSORTED" },
    {4256, "LVM_REMOVEALLGROUPS" },
    {4257, "LVM_HASGROUP" },
    {4258, "LVM_SETTILEVIEWINFO" },
    {4259, "LVM_GETTILEVIEWINFO" },
    {4260, "LVM_SETTILEINFO" },
    {4261, "LVM_GETTILEINFO" },
    {4262, "LVM_SETINSERTMARK" },
    {4263, "LVM_GETINSERTMARK" },
    {4264, "LVM_INSERTMARKHITTEST" },
    {4265, "LVM_GETINSERTMARKRECT" },
    {4266, "LVM_SETINSERTMARKCOLOR" },
    {4267, "LVM_GETINSERTMARKCOLOR" },
    {4269, "LVM_SETINFOTIP" },
    {4270, "LVM_GETSELECTEDCOLUMN" },
    {4271, "LVM_ISGROUPVIEWENABLED" },
    {4272, "LVM_GETOUTLINECOLOR" },
    {4273, "LVM_SETOUTLINECOLOR" },
    {4275, "LVM_CANCELEDITLABEL" },
    {4276, "LVM_MAPINDEXTOID" },
    {4277, "LVM_MAPIDTOINDEX" },
    {4278, "LVM_ISITEMVISIBLE" },
    {8192, "OCM__BASE" },
    {8197, "LVM_SETUNICODEFORMAT" },
    {8198, "LVM_GETUNICODEFORMAT" },
    {8217, "OCM_CTLCOLOR" },
    {8235, "OCM_DRAWITEM" },
    {8236, "OCM_MEASUREITEM" },
    {8237, "OCM_DELETEITEM" },
    {8238, "OCM_VKEYTOITEM" },
    {8239, "OCM_CHARTOITEM" },
    {8249, "OCM_COMPAREITEM" },
    {8270, "OCM_NOTIFY" },
    {8465, "OCM_COMMAND" },
    {8468, "OCM_HSCROLL" },
    {8469, "OCM_VSCROLL" },
    {8498, "OCM_CTLCOLORMSGBOX" },
    {8499, "OCM_CTLCOLOREDIT" },
    {8500, "OCM_CTLCOLORLISTBOX" },
    {8501, "OCM_CTLCOLORBTN" },
    {8502, "OCM_CTLCOLORDLG" },
    {8503, "OCM_CTLCOLORSCROLLBAR" },
    {8504, "OCM_CTLCOLORSTATIC" },
    {8720, "OCM_PARENTNOTIFY" },
    {32768, "WM_APP" },
    {52429, "WM_RASDIALEVENT" }
};
#endif

static char ArgumentBuffer[128][512] = {0};

static String GArgV[128] = {0};
static i32 GArgC = 0;

#ifndef NO_LOG 
internal void LogLastError(const String Prefix)
{
    TCHAR Message[4096] = {0};
    DWORD Code = GetLastError();
    u32 Len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, Code,
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                            (LPTSTR)&Message, sizeof(TCHAR)*4095,
                            NULL);

    LOG_ERROR("%S\n        Error Code: %i\n        Reason: %S", Prefix, Code, StrSlice(Message, Len));
}
#else
#define LogLastError(...)
#endif

void Platform_PreInitialize(void)
{
    Platform_GetClockFrequency();

    if (!bCriticalSectionInitialized)
    {
        InitializeCriticalSection(&GCriticalSection);
        bCriticalSectionInitialized = true;
    }

    i32 NumArgs = 0;
    wchar** ArgsW = CommandLineToArgvW(GetCommandLineW(), &NumArgs);

    GArgC = NumArgs;

    for (u16 i = 0; i < 128; i++)
    {
        GArgV[i].Data = ArgumentBuffer[i];
        GArgV[i].Length = 0;
        GArgV[i].Capacity = 511;
    }

    for (i32 i = 1; i < NumArgs; i++)
    {
        char* Buffer = ArgumentBuffer[(i-1)]; // &ArgumentBuffer[(i-1)*1024];

        register u32 Len = 0;
        while (Len < 512 && ArgsW[i][Len] != 0) // arbitrary max length of 512
        {
            Buffer[Len] = (char)ArgsW[i][Len];
            Len++; 
        }

        GArgV[i-1].Length = Len;
        GArgV[i-1].Capacity = Len;
    }

    LocalFree(ArgsW);
}

bool Platform_CreateMutex(const String Name, PlatformMutex* OutMutex)
{
    u32 Diff = Name.Length > 255 ? Name.Length - 255 : 0; // clamp to 255 characters
    String ClampedName = StrShiftF(Name, Diff);

    HANDLE M = CreateMutexA(NULL, TRUE, ClampedName.Data);
    if (M == NULL)
    {
        return false;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        OutMutex->Handle = M;
        OutMutex->Name = ClampedName;
        return false;
    }

    OutMutex->Handle = M;
    OutMutex->Name = ClampedName;
    return true;
}

bool Platform_ReleaseMutex(PlatformMutex* Mutex)
{
    BOOL bResult = ReleaseMutex(Mutex->Handle);
    if (!bResult)
    {
        //LogLastError(S("Failed to release mutex"));
        return false;
    }

    CloseHandle(Mutex->Handle);
    return true;
}

u32 Platform_GetConsoleProcessCount(void)
{
    DWORD Processes[1] = {0};
    DWORD Count = GetConsoleProcessList(Processes, 1);
    return Count;
}

#ifndef HEADLESS
LRESULT CALLBACK Win32ProcessMessage(HWND Handle, UINT Msg, WPARAM wParam, LPARAM lParam);

void* Platform_GetDeviceContext(void)
{
    return GWindowsPlatformState->Hdc;
}

bool Platform_Startup(void* State, const String ApplicationName, i32 X, i32 Y, u32 Width, u32 Height)
{
    Platform_PreInitialize();

    LinearAllocator_Create(Platform_GetMemoryRequirement(), State, &GPlatformStateMemoryAllocator);

    GWindowsPlatformState = LinearAllocator_Allocate(&GPlatformStateMemoryAllocator, sizeof(WindowsPlatformState));
    GWindowsPlatformState->bInitialized = true;
    GWindowsPlatformState->Instance = GetModuleHandle(0);
    GWindowsPlatformState->ThreadID = GetCurrentThreadId();
    GWindowsPlatformState->ThreadHandle = GetCurrentThread();
    
    WNDCLASS WindClass = { 0 };
    WindClass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    WindClass.lpfnWndProc = Win32ProcessMessage;
    WindClass.cbClsExtra = 0;
    WindClass.cbWndExtra = 0;
    WindClass.hInstance = GWindowsPlatformState->Instance;
    WindClass.hIcon = LoadIcon(GWindowsPlatformState->Instance, IDI_APPLICATION);
    WindClass.hCursor = LoadCursor(NULL, IDC_ARROW); // Null. Manage the cursor manually
    WindClass.hbrBackground = NULL; // Transparent
    WindClass.lpszClassName = "RiftEngine_WindClass";

    RegisterClass(&WindClass);

    // Create the window
    {
        i32 WindowX = X;
        i32 WindowY = Y;
        i32 WindowWidth = (i32)Width;
        i32 WindowHeight = (i32)Height;

        u32 WindowStyle = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | CS_OWNDC;
        u32 WindowExStyle = WS_EX_APPWINDOW;

        WindowStyle |= WS_MAXIMIZEBOX;
        WindowStyle |= WS_MINIMIZEBOX;
        WindowStyle |= WS_THICKFRAME;

        // Get the size of the border
        RECT BorderRect = { 0 };
        AdjustWindowRectEx(&BorderRect, WindowStyle, 0, WindowExStyle);

        // Border rectangle is negative, so offset
        WindowX += BorderRect.left;
        WindowY += BorderRect.top;

        WindowWidth += BorderRect.right - BorderRect.left;
        WindowHeight += BorderRect.bottom - BorderRect.top;

        //u32 BorderlessStyle = WS_EX_TOPMOST | WS_POPUP | CS_OWNDC;

        HWND Handle = CreateWindowEx(WindowExStyle, WindClass.lpszClassName, ApplicationName.Data, WindowStyle, WindowX, WindowY, WindowWidth, WindowHeight, 0, 0, WindClass.hInstance, 0);

        if (Handle == NULL)
        {
            LogLastError(S("Failed to create a window"));
            return false;
        }

        GWindowsPlatformState->Handle = Handle;
    }

    PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    // Flags
        PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
        32,                   // Colordepth of the framebuffer.
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,                   // Number of bits for the depthbuffer
        8,                    // Number of bits for the stencilbuffer
        0,                    // Number of Aux buffers in the framebuffer.
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };

    HDC DC = GetDC(GWindowsPlatformState->Handle);
    int Format = ChoosePixelFormat(DC, &pfd);
    SetPixelFormat(DC, Format, &pfd);

    GWindowsPlatformState->Hdc = DC;

    RECT rc = {0};

    GetWindowRect(GWindowsPlatformState->Handle, &rc);

    int xPos = (GetSystemMetrics(SM_CXSCREEN) - rc.right)/2;
    int yPos = (GetSystemMetrics(SM_CYSCREEN) - rc.bottom)/2;

    SetWindowPos(GWindowsPlatformState->Handle, 0, xPos, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

    /// TODO
    //HICON hSmallIcon = (HICON) LoadImage ( 0, "sr2.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE | LR_DEFAULTCOLOR );
	//SendMessage(GetActiveWindow(), WM_SETICON, ICON_SMALL, hSmallIcon);


    //SetWindowLongPtrW(GWindowsPlatformState->Handle, GWL_STYLE, 0);

    #ifndef HID_USAGE_PAGE_GENERIC
    #define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
    #endif
    #ifndef HID_USAGE_GENERIC_MOUSE
    #define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
    #endif

    RAWINPUTDEVICE Rid[1];
    Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC; 
    Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE; 
    Rid[0].dwFlags = RIDEV_INPUTSINK;   
    Rid[0].hwndTarget = GWindowsPlatformState->Handle;
    RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));

    LOG_SUCCESS("Platform subsystem initialized");

    return true;
}

void Platform_ShowWindow(void)
{
    ShowWindow(GWindowsPlatformState->Handle, SW_SHOW);
}

void Platform_Shutdown(void)
{
    UnregisterClass("RiftEngine_WindClass", GWindowsPlatformState->Instance);

    if (GWindowsPlatformState->Handle)
    {
        DestroyWindow(GWindowsPlatformState->Handle);
        GWindowsPlatformState->Handle = NULL;
    }

    Platform_MemZero(GWindowsPlatformState, sizeof(WindowsPlatformState));
    GWindowsPlatformState = NULL;

    DeleteCriticalSection(&GCriticalSection);

    LOG_INFO("Platform subsystem shutdown");
}

u64 Platform_GetMemoryRequirement(void)
{
    return sizeof(WindowsPlatformState);
}
#endif // HEADLESS

NO_RETURN void Platform_Abort(u32 ExitCode)
{
    ExitProcess(ExitCode);
}

StringArray Platform_GetCommandLineArgs(void)
{
    StringArray Args = {0};
    Args.Num = (u32)(GArgC-1 <= 0 ? 0 : (GArgC-1 < 128 ? GArgC-1 : 128));
    Args.List = GArgV;
    return Args;
}

f64 Platform_GetClockFrequency(void)
{
    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);
    const f64 ClockFrequency = 1.0/(f64)Frequency.QuadPart;
    return ClockFrequency;
}

bool Platform_PushMessages(void)
{
    MSG Message;
    while (PeekMessage(&Message, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }

    return true;
}

void* Platform_GetWindowHandle(void)
{
    return GWindowsPlatformState->Handle;
}

void* Platform_MemAlloc(u64 Size)
{
    DWORD dwFlags = HEAP_CREATE_ALIGN_16;
    return HeapAlloc(GetProcessHeap(), dwFlags, Size);
}

void* Platform_MemAllocZero(u64 Size)
{
    DWORD dwFlags = HEAP_ZERO_MEMORY | HEAP_CREATE_ALIGN_16;
    return HeapAlloc(GetProcessHeap(), dwFlags, Size);
}

void* Platform_MemReAlloc(const void* Block, u64 Size)
{
    if (!Block)
        return Platform_MemAlloc(Size);

    DWORD dwFlags = HEAP_ZERO_MEMORY;
    return HeapReAlloc(GetProcessHeap(), dwFlags, (void*)Block, Size);
}

void Platform_MemFree(const void* Block)
{
    HeapFree(GetProcessHeap(), 0, (void*)Block);
}

void* Platform_MemZero(void* Block, u64 Size)
{
    ZeroMemory(Block, Size);
    return Block;
}

void* Platform_MemCopy(void* restrict Dest, const void* restrict Source, u64 Size)
{
    CopyMemory(Dest, Source, Size);
    return Dest;
}

void* Platform_MemMove(void* restrict Dest, const void* restrict Source, u64 Size)
{
    MoveMemory(Dest, Source, Size);
    return Dest;
}

void* Platform_MemSet(void* Dest, i32 Value, u64 Size)
{
    FillMemory(Dest, Size, Value);
    return Dest;
}

bool Platform_MemEqual(const void* Block1, const void* Block2, u64 Size)
{
    return memcmp(Block1, Block2, Size) == 0;
}

/*
internal COORD GetConsoleCursorPosition(HANDLE hConsoleOutput)
{
    CONSOLE_SCREEN_BUFFER_INFO cbsi;
    if (GetConsoleScreenBufferInfo(hConsoleOutput, &cbsi))
    {
        return cbsi.dwCursorPosition;
    }
    else
    {
        // The function failed. Call GetLastError() for details.
        COORD invalid = { 0, 0 };
        return invalid;
    }
}
*/

void Platform_ConsoleWrite(const char* Message, u8 Color, bool bIsError)
{
    Platform_ConsoleWrite_CustomLength(Message, String_GetLength(Message), Color, bIsError);
}

void Platform_ConsoleWrite_CustomLength(const char* Message, u64 Length, u8 Color, bool bIsError)
{
    //PROFILE_SCOPE("Platform_ConsoleWrite") // @todo: make it thread local
    {
        DWORD OutputHandle = STD_ERROR_HANDLE;// bIsError ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
        HANDLE ConsoleHandle = GetStdHandle(OutputHandle);

        SetConsoleTextAttribute(ConsoleHandle, GConsoleColorLevels[Color]);

        bool bIgnoreNewLine = Color == 4 && Message[Length-1] == '\n';
        if (UNLIKELY(bIgnoreNewLine))
            Length--;

        OutputDebugString(Message);
        WriteConsole(ConsoleHandle, Message, (DWORD)Length, NULL, 0);

        // Reset back to white
        SetConsoleTextAttribute(ConsoleHandle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

        if (UNLIKELY(bIgnoreNewLine))
            WriteConsole(ConsoleHandle, "\n", 1, NULL, 0);
    }
}

f64 Platform_GetAbsoluteTime(void)
{
    LARGE_INTEGER Frequency, Now;

    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&Now);

    return (f64)Now.QuadPart * (1.0/(f64)Frequency.QuadPart);
}

SystemTime Platform_GetSystemLocalTime(void)
{
    SYSTEMTIME SysTime = {0};
    GetLocalTime(&SysTime);

    SystemTime EngineTime = {0};
    EngineTime.Year = SysTime.wYear;
    EngineTime.Month = SysTime.wMonth;
    EngineTime.DayOfWeek = SysTime.wDayOfWeek;
    EngineTime.Day = SysTime.wDay;
    EngineTime.Hour = SysTime.wHour;
    EngineTime.Minute = SysTime.wMinute;
    EngineTime.Second = SysTime.wSecond;
    EngineTime.Millisecond = SysTime.wMilliseconds;

    return EngineTime;
}

void Platform_Sleep(f64 ms)
{
	if (ms > 0)
	{
		LARGE_INTEGER Frequency, Now;
		QueryPerformanceCounter(&Now);
		QueryPerformanceFrequency(&Frequency);

		f64 Start = (f64)Now.QuadPart * (1.0/(f64)Frequency.QuadPart);
		f64 Target = ms/1000.0;

		while (1)
		{
			QueryPerformanceCounter(&Now);
			if ((((f64)Now.QuadPart * (1.0/(f64)Frequency.QuadPart)) - Start) >= Target)
				break;
		}
	}
}

void Platform_ShowCursor(bool bShow)
{
    static bool bShown = true;
    if (bShown)
    {
        if (!bShow)
        {
            SetCursor(NULL);
            bShown = false;
        }
    }
    else
    {
        if (bShow)
        {
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            bShown = true;
        }
    }
}

void Platform_GetMousePosition(f32* X, f32* Y)
{
    POINT p;
    GetCursorPos(&p);

    *X = (f32)p.x;
    *Y = (f32)p.y;
}

u64 Platform_GetCurrentThreadID(void)
{
    return GetCurrentThreadId();
}

u64 Platform_GetMainThreadID(void)
{
    if (GWindowsPlatformState)
        return GWindowsPlatformState->ThreadID;

    return GetCurrentThreadId();
}

bool Platform_GetAccountName(String* OutName)
{
    char UserName[256] = {0};
    DWORD Size = 255;
    BOOL bResult = GetUserName(UserName, &Size);
    if (!bResult)
    {
        LogLastError(S("Failed to get the current user name"));
        return false;
    }

    String_Copy(OutName, CStr(UserName));
    return true;
}

bool Platform_GetUserName(String* OutName)
{
    char Path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, Path)))
    {
        String Name = CStr(Path);

        u32 LastSlash = 0;
        if (String_IndexOfLastPathSlash(Name, &LastSlash))
        {
            String_Copy(OutName, StrShiftF(Name, LastSlash+1));
        }
        else
        {
            String_Copy(OutName, Name);
        }

        return true;
    }

    return false;
}

bool Platform_GetUserDirectory(String* OutDirectory)
{
    char Path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, Path)))
    {
        String_Copy(OutDirectory, CStr(Path));

        return true;
    }

    return false;
}

bool Platform_GetCurrentProcessName(String* OutName)
{
    TCHAR FileName[MAX_PATH] = {0};
    u32 Len = GetModuleFileName(NULL, FileName, MAX_PATH);

    for (u32 i = Len; i > 0; i--)
    {
        if (FileName[i] == '\\')
        {
            String_Copy(OutName, StrSlice(&FileName[i+1], Len-i-1));
            break;
        }
    }

    return true;
}

u64 Platform_GetCurrentProcessID(void)
{
    return GetCurrentProcessId();
}

void Platform_GetWorkingDirectory(String* OutPath)
{
    u32 Len = GetCurrentDirectory(OutPath->Capacity, OutPath->Data);
    OutPath->Length = Len;
}

bool Platform_GetEnvironmentVariableValue(String Name, String* OutVariable)
{
#ifdef UNICODE
    String16Local(NameWide, 2048);
    String_ToWide(Name, &NameWide);

    String16Local(TempBuffer, 2048);

    DWORD Len = GetEnvironmentVariable(NameWide.Data, TempBuffer.Data, TempBuffer.Capacity*2);
    TempBuffer.Length = Len;

    String_ToNarrow(TempBuffer, OutVariable);

    return Len != 0;
#else
    StringLocal(NameCopy, 128); // we copy the name because the passed in Name could have had its length altered but not the data, so create a copy with a null terminator at the length so windows gets the correct string
    String_Copy(&NameCopy, Name);

    DWORD Len = GetEnvironmentVariable(NameCopy.Data, OutVariable->Data, OutVariable->Capacity);
    OutVariable->Length = Len;

    return Len != 0;
#endif
}

bool Platform_DoesEnvironmentVariableExist(String Name)
{
    StringLocal(NameCopy, 128); // we copy the name because the passed in Name could have had its length altered but not the data, so create a copy with a null terminator at the length so windows gets the correct string
    String_Copy(&NameCopy, Name);

    DWORD Len = GetEnvironmentVariable(NameCopy.Data, NULL, 0);
    return Len != 0;
}

bool Platform_CaptureStackTrace(LinearAllocator* Arena, TArray(StackTraceData)* OutInfo)
{
    EnterCriticalSection(&GCriticalSection);

    HANDLE ProcessHandle = GetCurrentProcess();
    SymInitialize(ProcessHandle, NULL, true);

    LinearAllocator_Scratch Temp = Memory_GetScratch();

    void* StackAddresses = LinearAllocator_Allocate(Temp.Allocator, sizeof(void*) * 1024);
    const u16 NumFramesCaptured = CaptureStackBackTrace(0, 1024, StackAddresses, NULL);

    SYMBOL_INFO* Symbol = LinearAllocator_Allocate(Temp.Allocator, sizeof(SYMBOL_INFO) + 256 * sizeof(char));
    Symbol->MaxNameLen = 255;
    Symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    u64 ArrayMemoryAmount = _ArrayCalculateMemRequirement(NumFramesCaptured, sizeof(StackTraceData));
    u8* ArrayMemory = LinearAllocator_Allocate(Arena, ArrayMemoryAmount);

    TArray(StackTraceData) StackTraceCache = Array_CreateStatic(StackTraceData, NumFramesCaptured, ArrayMemory);

    for (u16 i = 1; i < NumFramesCaptured; i++)
    {
        SymFromAddr(ProcessHandle, (u64)(((u64*)StackAddresses)[i]), 0, Symbol);

        StackTraceData d;
        d.Name = String_Create(Arena, StrSlice(Symbol->Name, Symbol->NameLen));
        d.Address = Symbol->Address;
        d.Index = NumFramesCaptured - i - 1;

        Array_Add(StackTraceCache, d);
    }

    *OutInfo = StackTraceCache;

    Memory_ReleaseScratch(&Temp);

    LeaveCriticalSection(&GCriticalSection);

    return true;
}

u32 Platform_GetNumLogicalProcessors(void)
{
    SYSTEM_INFO info = {0};
    GetSystemInfo(&info);

    return info.dwNumberOfProcessors;
    //return GetCurrentProcessorNumber();
}

static StringN(64) GThreadNameBuffer = {0};

bool Platform_GetThreadName(void* ThreadHandle, String* OutName)
{
    if (!ThreadHandle)
    {
        StringN_Copy(GThreadNameBuffer, S("Main Thread"));

        String s;
        s.Data = GThreadNameBuffer.Data;
        s.Length = GThreadNameBuffer.Length;
        s.Capacity = GThreadNameBuffer.Capacity;

        *OutName = s;

        return false;
    }

    wchar_t a[64] = {0};
    wchar_t* str = &a[0];

    HANDLE* Handle = (HANDLE*)ThreadHandle;
    if ((u64)Handle == 0xfffffffffffffffe)
    {
        StringN_Copy(GThreadNameBuffer, S(""));

        String s;
        s.Data = GThreadNameBuffer.Data;
        s.Length = GThreadNameBuffer.Length;
        s.Capacity = GThreadNameBuffer.Capacity;

        *OutName = s;

        return false;
    }

    HRESULT Result = GetThreadDescription(Handle, &str);

    if (HRESULT_CODE(Result) == S_OK)
    {
        String ConversionBuffer = {.Data = GThreadNameBuffer.Data, .Length = 0};

        String_ToNarrow(CStr16View(str), &ConversionBuffer);

        GThreadNameBuffer.Length = ConversionBuffer.Length;

        String s;
        s.Data = GThreadNameBuffer.Data;
        s.Length = GThreadNameBuffer.Length;
        s.Capacity = GThreadNameBuffer.Capacity;

        *OutName = s;

        return GThreadNameBuffer.Length > 0;
    }

    return false;
}

#ifndef HEADLESS
internal WPARAM MapLeftRightKeys(WPARAM vk, LPARAM lParam)
{
    // https://stackoverflow.com/questions/5681284/how-do-i-distinguish-between-left-and-right-keys-ctrl-and-alt

    UINT scancode = (lParam & 0x00ff0000) >> 16;
    int extended  = (lParam & 0x01000000) != 0;

    switch (vk)
    {
        case VK_SHIFT:
        return MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);

        case VK_CONTROL:
        return extended ? VK_RCONTROL : VK_LCONTROL;

        case VK_MENU:
        return extended ? VK_RMENU : VK_LMENU;

        default:
        return vk;
    }
}

/*
#ifdef _DEBUG
internal void LogWindowsMessage(u32 Msg)
{
    for (u16 i = 0; i < 1024; ++i)
    {
        if (WM_Map[i].ID == Msg)
        {
            WindowsMessage MsgData;
            MsgData.ID = Msg;
            MsgData.Name = WM_Map[i].Name;
            LOG_INFO("%s (ID: %u)", MsgData.Name, MsgData.ID);
            break;
        }
    }
}
#endif
*/

LRESULT CALLBACK Win32ProcessMessage(HWND Handle, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    //LogWindowsMessage(Msg);

    #ifndef NO_EVENT_LOOP

    switch (Msg)
    {
        case WM_ERASEBKGND:
            // Notify windows that erasing will be handled by the application to prevent flicker
        return 1;

        case WM_CLOSE:
        {
            EventContext Context = {0};
            Event_Fire(EventCode_ApplicationQuit, GWindowsPlatformState, Context);
        }
        return true;

        case WM_DESTROY:
            PostQuitMessage(0);
        return 0;

        case WM_GETMINMAXINFO:
        {
            // todo: configured by the application
            MINMAXINFO* Info = (MINMAXINFO*)lParam;
            Info->ptMinTrackSize.x = 320;
            Info->ptMinTrackSize.y = 240;
        }
        break;

        case WM_SIZE:
        {
            RECT Rect;
            GetClientRect(Handle, &Rect);

            u16 Width = (u16)(Rect.right - Rect.left);
            u16 Height = (u16)(Rect.bottom - Rect.top);

            EventContext Context = { 0 };
            Context.Data.u16[0] = Width;
            Context.Data.u16[1] = Height;
            Event_Fire(EventCode_WindowResize, GWindowsPlatformState, Context);
        }
        break;

        case WM_SETFOCUS:
            EventContext Context = { 0 };
            Event_Fire(EventCode_WindowFocus, GWindowsPlatformState, Context);
        break;

        // Disable normal alt key behaviour
        case WM_SYSCOMMAND:
            if (wParam == SC_KEYMENU || wParam == VK_MENU)
                return 0;
        break;

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
            bool bPressed = (Msg == WM_KEYDOWN || Msg == WM_SYSKEYDOWN);
            EKey Key = (u16)wParam;

            //LOG_INFO("Key %u %s", Key, bPressed ? "pressed" : "released");

            switch (wParam)
            {
                case VK_MENU:
                {
                    WPARAM MappedAltKey = MapLeftRightKeys(VK_MENU, lParam);

                    if (MappedAltKey == VK_RMENU)
                    {
                        Key = Key_RightAlt;
                    }
                    else
                    {
                        Key = Key_LeftAlt;
                    }
                }
                break;

                case VK_SHIFT:
                {
                    WPARAM MappedShiftKey = MapLeftRightKeys(VK_SHIFT, lParam);

                    if (MappedShiftKey == VK_RSHIFT)
                    {
                        Key = Key_RightShift;
                    }
                    else
                    {
                        Key = Key_LeftShift;
                    }
                }
                break;

                case VK_CONTROL:
                {
                    WPARAM MappedControlKey = MapLeftRightKeys(VK_CONTROL, lParam);

                    if (MappedControlKey == VK_RCONTROL)
                    {
                        Key = Key_RightControl;
                    }
                    else
                    {
                        Key = Key_LeftControl;
                    }
                }
                break;

                default:
                break;
            }

            if (Key != Key_Null && Key != Key_Count)
            {
                Input_ProcessKey(Key, bPressed);
            }
        }
        break;

        case WM_INPUT: 
        {
            UINT dwSize = sizeof(RAWINPUT);
            static BYTE lpb[sizeof(RAWINPUT)];

            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

            RAWINPUT* raw = (RAWINPUT*)lpb;

            if (raw->header.dwType == RIM_TYPEMOUSE) 
            {
                int x = -raw->data.mouse.lLastX;
                int y = -raw->data.mouse.lLastY;

                Input_ProcessMouseMove(x, y);
            }
        }
        break;

        /*
        case WM_MOUSEMOVE:
        {
            //i16 MouseX = (i16)GET_X_LPARAM(lParam);
            //i16 MouseY = (i16)GET_Y_LPARAM(lParam);

            f64 dpi = GetDpiForWindow(GWindowsPlatformState->Handle);
            f64 scale = dpi/96.0;

            f64 x = (((f64)GET_X_LPARAM(lParam) / scale) / 1280.0);// * 1280.0f;
            f64 y = (((f64)GET_Y_LPARAM(lParam) / scale) / 720.0);// * 720.0f;

            //LOG("X: %f Y: %f", x, y);

            //Input_ProcessMouseMove(x, y);
        }
        break;
        */

        case WM_MOUSEWHEEL:
        {
            i16 WheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);

            //LOG_INFO("%i", WheelDelta);

            Input_ProcessMouseWheelMove(WheelDelta);
        }
        break;

        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
        {
            bool bPressed = Msg == WM_LBUTTONDOWN || Msg == WM_RBUTTONDOWN || Msg == WM_MBUTTONDOWN;

            EMouseButton MouseButton = MouseButton_Null;

            switch (Msg)
            {
                case WM_LBUTTONDOWN:
                case WM_LBUTTONUP:
                    MouseButton = MouseButton_Left;
                break;

                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                    MouseButton = MouseButton_Right;
                break;

                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:
                    MouseButton = MouseButton_Middle;
                break;
            }

            if (bPressed)
                SetCapture(GWindowsPlatformState->Handle);
            else
                ReleaseCapture();

            Input_ProcessMouseButton(MouseButton, bPressed);
        }
        break;

        case WM_FILE_WATCHER:
            FileWatchCallbackData Data = *((FileWatchCallbackData*)lParam);
            Data.Callback(Data.Event, Data.Path, Data.UserData);
        break;

        default:
        return DefWindowProc(Handle, Msg, wParam, lParam);
    }
    #endif

    return DefWindowProc(Handle, Msg, wParam, lParam);
}
#endif // HEADLESS

Uuid UUID_Generate(void)
{
    uuid_t id = {0};
    UuidCreate(&id);

    return *(Uuid*)&id;
}

bool UUID_IsEqual(Uuid First, Uuid Second)
{
    return Platform_MemEqual(&First, &Second, sizeof(Uuid));
}

void UUID_ToString(Uuid ID, String* OutString)
{
    // @Speed: Make our own uuid to string converter and not use windows heap allocating string

    RPC_CSTR str = {0};
    UuidToString((uuid_t*)&ID, &str);
    String_Copy(OutString, StrSlice(str, GUID_LENGTH-1));
    RpcStringFree(&str);
}

Uuid UUID_FromString(const String IDString)
{
    uuid_t id = {0};
    UuidFromString((const RPC_CSTR)IDString.Data, &id);

    return *(Uuid*)&id;
}

bool Filesystem_Open(const String FilePath, u32 Mode, FileHandle* OutHandle)
{
    DWORD OpenStyle;
    DWORD ShareStyle;
    DWORD Disposition;
    if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) != 0)
    {
        OpenStyle = GENERIC_READ | GENERIC_WRITE;
        ShareStyle = FILE_SHARE_READ | FILE_SHARE_WRITE;
        Disposition = OPEN_ALWAYS;
    }
    else if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) == 0)
    {
        OpenStyle = GENERIC_READ;
        ShareStyle = FILE_SHARE_READ;
        Disposition = OPEN_ALWAYS;
    }
    else if ((Mode & FileMode_Read) == 0 && (Mode & FileMode_Write) != 0)
    {
        OpenStyle = GENERIC_WRITE;
        ShareStyle = FILE_SHARE_WRITE;
        Disposition = CREATE_ALWAYS;
    }
    else
    {
        LOG_ERROR("Invalid mode passed (%u) while trying to open file: %S", Mode, FilePath);
        return false;
    }

    bool bFoundPathSeparator = false;
    u32 NextSlashIndex = 0;

    do
    {
        bFoundPathSeparator = false;

        for (u32 i = NextSlashIndex; i < FilePath.Length; i++)
        {
            if (FilePath.Data[i] == '/' || FilePath.Data[i] == '\\')
            {
                bFoundPathSeparator = true;

                StringLocal(BaseDirectory, MAX_PATH);
                String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i));

                NextSlashIndex = i+1;

                BOOL bDirectoryCreated = Filesystem_DoesDirectoryExist(BaseDirectory);
                if (!bDirectoryCreated)
                {
                    bDirectoryCreated = CreateDirectory(BaseDirectory.Data, NULL);

                    if (!bDirectoryCreated)
                    {
                        StringLocal(Prefix, 512);
                        String_Format(&Prefix, S("Failed to create directory \"%S\""), Prefix.Capacity, BaseDirectory);
                        LogLastError(Prefix);

                        return false;
                    }
                }

                break;
            }
        }
    }
    while (bFoundPathSeparator);

    HANDLE File = CreateFile(FilePath.Data, OpenStyle, ShareStyle, NULL, Disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (File == INVALID_HANDLE_VALUE)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to open file \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);

        return false;
    }

    if (OutHandle)
    {
        OutHandle->Data = File;
        OutHandle->Data2 = NULL;

        //LARGE_INTEGER FileSize;
        //GetFileSizeEx(File, &FileSize);
        //OutHandle->Size = (u64)FileSize.QuadPart;

        //StringN_Copy(OutHandle->Path, FilePath);
    }

    return true;
}

bool Filesystem_NewFile(const String FilePath)
{
    FileHandle f = {0};
    bool bSuccess = Filesystem_Open(FilePath, FileMode_Write, &f);
    Filesystem_Close(&f);

    return bSuccess;
}

bool Filesystem_DeleteFile(String FilePath)
{
    //StringLocal(Copy, MAX_PATH);
    //String_Copy(&Copy, FilePath);
    // figured it be better to just overwrite one character to 0 instead of copying the entire string

    // sigh... if only windows used length delimited strings...
    //char Temp = FilePath.Data[FilePath.Length];
    //FilePath.Data[FilePath.Length] = 0;
    i32 Result = DeleteFile(FilePath.Data) != 0;
    //FilePath.Data[FilePath.Length] = Temp;

    return Result != 0;
}

bool Filesystem_Open_MemoryMapped(const String FilePath, u32 Mode, FileHandle* OutHandle, u8** OutData, u64* OutSize)
{
    if (OutSize)
        *OutSize = 0;

    if (OutData)
        *OutData = NULL;

    if (!IsValidFileHandle(OutHandle))
    {
        Filesystem_Open(FilePath, Mode, OutHandle);
    }

    if (IsValidFileHandle(OutHandle))
    {
        DWORD ProtectFlag;
        if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) != 0)
        {
            ProtectFlag = PAGE_READWRITE;
        }
        else if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) == 0)
        {
            ProtectFlag = PAGE_READONLY;
        }
        else if ((Mode & FileMode_Read) == 0 && (Mode & FileMode_Write) != 0)
        {
            ProtectFlag = PAGE_WRITECOPY;
        }
        else
        {
            Filesystem_Close(OutHandle);
            LOG_ERROR("Invalid mode passed (%u) while trying to map view file: %S", Mode, FilePath);
            return false;
        }

        HANDLE fm = CreateFileMapping(OutHandle->Data, NULL, ProtectFlag, 0, 0, NULL);
        if (fm == NULL || fm == INVALID_HANDLE_VALUE)
        {
            StringLocal(Prefix, 512);
            String_Format(&Prefix, S("Failed to create file mapping for \"%S\""), Prefix.Capacity, FilePath);
            LogLastError(Prefix);
            Filesystem_Close(OutHandle);
            return false;
        }

        OutHandle->Data2 = fm;

        LARGE_INTEGER FileSize;
        GetFileSizeEx(OutHandle->Data, &FileSize);

        DWORD OpenStyle;
        if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) != 0)
        {
            OpenStyle = FILE_MAP_READ | FILE_MAP_WRITE;
        }
        else if ((Mode & FileMode_Read) != 0 && (Mode & FileMode_Write) == 0)
        {
            OpenStyle = FILE_MAP_READ;
        }
        else if ((Mode & FileMode_Read) == 0 && (Mode & FileMode_Write) != 0)
        {
            OpenStyle = FILE_MAP_WRITE;
        }
        else
        {
            LOG_ERROR("Invalid mode passed (%u) while trying to map view file: %S", Mode, FilePath);
            Filesystem_Close(OutHandle);
            return false;
        }

        if (OutSize)
            *OutSize = (u64)FileSize.QuadPart;

        if (OutData)
            *OutData = MapViewOfFile(fm, OpenStyle, 0, 0, (SIZE_T)FileSize.QuadPart);

        return true;
    }

    return false;
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

                StringLocal(BaseDirectory, MAX_PATH);
                if (FilePath.Length-1 == i)
                    String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i+1));
                else
                    String_Copy(&BaseDirectory, StrSlice(FilePath.Data, i));

                NextSlashIndex = i+1;

                BOOL bDirectoryCreated = Filesystem_DoesDirectoryExist(BaseDirectory);
                if (!bDirectoryCreated)
                {
                    bDirectoryCreated = CreateDirectory(BaseDirectory.Data, NULL);

                    if (!bDirectoryCreated)
                    {
                        StringLocal(Prefix, 512);
                        String_Format(&Prefix, S("Failed to create directory \"%S\""), Prefix.Capacity, BaseDirectory);
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
}

bool Filesystem_OpenDirectory_Ex(const String FilePath, FileHandle* OutHandle)
{
    if (!Filesystem_OpenDirectory(FilePath))
    {
        return false;
    }

    HANDLE File = CreateFile(FilePath.Data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (File == INVALID_HANDLE_VALUE)
    {
        StringLocal(Prefix, 512);
        String_Format(&Prefix, S("Failed to open directory \"%S\""), Prefix.Capacity, FilePath);
        LogLastError(Prefix);

        return false;
    }

    if (OutHandle)
    {
        OutHandle->Data = File;

        /*
        LARGE_INTEGER FileSize;
        GetFileSizeEx(File, &FileSize);
        OutHandle->Size = (u64)FileSize.QuadPart;

        StringN_Copy(OutHandle->Path, FilePath);
        */
    }

    return true;
}

bool Filesystem_Close(FileHandle* Handle)
{
    if (Handle->Data2)
        CloseHandle(Handle->Data2);

    if (IsValidFileHandle(Handle))
    {
        CloseHandle(Handle->Data);
        *Handle = FileHandle_Null();
        return true;
    }

    return false;
}

bool Filesystem_Seek(const FileHandle* Handle, i64 Offset)
{
    DWORD Result = SetFilePointer(Handle->Data, (long)Offset, NULL, FILE_CURRENT);
    return Result != INVALID_SET_FILE_POINTER;
}

bool Filesystem_SeekFromBeginning(const FileHandle* Handle, u64 Offset)
{
    DWORD Result = SetFilePointer(Handle->Data, (long)Offset, NULL, FILE_BEGIN);
    return Result != INVALID_SET_FILE_POINTER;
}

bool Filesystem_SeekFromEnd(const FileHandle* Handle, u64 Offset)
{
    DWORD Result = SetFilePointer(Handle->Data, (long)Offset, NULL, FILE_END);
    return Result != INVALID_SET_FILE_POINTER;
}

bool Filesystem_SeekToBeginning(const FileHandle* Handle)
{
    DWORD Result = SetFilePointer(Handle->Data, 0, NULL, FILE_BEGIN);
    return Result != INVALID_SET_FILE_POINTER;
}

bool Filesystem_SeekToEnd(const FileHandle* Handle)
{
    DWORD Result = SetFilePointer(Handle->Data, 0, NULL, FILE_END);
    return Result != INVALID_SET_FILE_POINTER;
}

u64 Filesystem_GetCurrentFilePosition(const FileHandle* Handle)
{
    return SetFilePointer(Handle->Data, 0, NULL, FILE_CURRENT);
}

u64 Filesystem_GetLastWriteTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    FileHandle f = {0};
    FILETIME FileTimeStamp = {0};
    if (Filesystem_Open(FilePath, FileMode_Read, &f))
    {
        GetFileTime(f.Data, NULL, NULL, &FileTimeStamp);
        Filesystem_Close(&f);
    }

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return a;
}

u64 Filesystem_GetLastAccessTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    FileHandle f = {0};
    FILETIME FileTimeStamp = {0};
    if (Filesystem_Open(FilePath, FileMode_Read, &f))
    {
        GetFileTime(f.Data, NULL, &FileTimeStamp, NULL);
        Filesystem_Close(&f);
    }

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return a;
}

u64 Filesystem_GetCreationTime(const String FilePath)
{
    if (!Filesystem_DoesFileExist(FilePath))
        return 0;

    FileHandle f = {0};
    FILETIME FileTimeStamp = {0};
    if (Filesystem_Open(FilePath, FileMode_Read, &f))
    {
        GetFileTime(f.Data, &FileTimeStamp, NULL, NULL);
        Filesystem_Close(&f);
    }

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return a;
}

FileTimeData Filesystem_GetFileTime(const String FilePath)
{
    FileTimeData Time = {0};

    if (!Filesystem_DoesFileExist(FilePath))
        return Time;

    FileHandle f = {0};
    if (Filesystem_Open(FilePath, FileMode_Read, &f))
    {
        FILETIME CreationTime = {0};
        FILETIME LastAccessTime = {0};
        FILETIME LastWriteTime = {0};

        GetFileTime(f.Data, &CreationTime, &LastAccessTime, &LastWriteTime);
        Filesystem_Close(&f);

        Time.CreationTime = (((ULONGLONG)CreationTime.dwHighDateTime) << 32) + CreationTime.dwLowDateTime;
        Time.LastAccessTime = (((ULONGLONG)LastAccessTime.dwHighDateTime) << 32) + LastAccessTime.dwLowDateTime;
        Time.LastWriteTime = (((ULONGLONG)LastWriteTime.dwHighDateTime) << 32) + LastWriteTime.dwLowDateTime;
    }

    return Time;
}

u64 Filesystem_GetLastWriteTimeH(const FileHandle* Handle)
{
    FILETIME FileTimeStamp = {0};
    GetFileTime(Handle->Data, NULL, NULL, &FileTimeStamp);

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return a;
}

u64 Filesystem_GetLastAccessTimeH(const FileHandle* Handle)
{
    FILETIME FileTimeStamp = {0};
    GetFileTime(Handle->Data, NULL, &FileTimeStamp, NULL);

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return a;
}

u64 Filesystem_GetCreationTimeH(const FileHandle* Handle)
{
    FILETIME FileTimeStamp = {0};
    GetFileTime(Handle->Data, &FileTimeStamp, NULL, NULL);

    ULONGLONG a = (((ULONGLONG)FileTimeStamp.dwHighDateTime) << 32) + FileTimeStamp.dwLowDateTime;
    return a;
}

FileTimeData Filesystem_GetFileTimeH(const FileHandle* Handle)
{
    FileTimeData Time = {0};

    FILETIME CreationTime = {0};
    FILETIME LastAccessTime = {0};
    FILETIME LastWriteTime = {0};
    GetFileTime(Handle->Data, &CreationTime, &LastAccessTime, &LastWriteTime);

    Time.CreationTime = (((ULONGLONG)CreationTime.dwHighDateTime) << 32) + CreationTime.dwLowDateTime;
    Time.LastAccessTime = (((ULONGLONG)LastAccessTime.dwHighDateTime) << 32) + LastAccessTime.dwLowDateTime;
    Time.LastWriteTime = (((ULONGLONG)LastWriteTime.dwHighDateTime) << 32) + LastWriteTime.dwLowDateTime;

    return Time;
}

bool Filesystem_ReadPipe(PlatformPipe Handle, u64 DataSize, void* OutData, u64* OutBytesRead)
{
    if (NEVER(Handle[0] == NULL)) return false;
    if (NEVER(Handle[1] == NULL)) return false;

    DWORD BytesRead = 0;
    BOOL Result = ReadFile(Handle[0], OutData, (DWORD)DataSize, &BytesRead, NULL);

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return Result;
}

// todo: no pointer as param
bool Filesystem_Read(const FileHandle* Handle, u64 DataSize, void* OutData, u64* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle)))
        return false;

    DWORD BytesRead = 0;
    BOOL Result = ReadFile(Handle->Data, OutData, (DWORD)DataSize, &BytesRead, NULL);

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return Result;
}

bool Filesystem_ReadEntireFile(const FileHandle* Handle, void* OutData, u64* OutBytesRead)
{
    if (NEVER(!IsValidFileHandle(Handle)))
        return false;

    u64 Size = 0;
    if (!Filesystem_GetFileSize(Handle, &Size))
        return false;

    Filesystem_SeekToBeginning(Handle);

    DWORD BytesRead = 0;
    BOOL Result = ReadFile(Handle->Data, OutData, (DWORD)Size, &BytesRead, NULL);

    if (OutBytesRead)
        *OutBytesRead = BytesRead;

    return Result;
}

bool Filesystem_ReadLine(const FileHandle* Handle, String* LineBuffer)
{
    if (NEVER(!IsValidFileHandle(Handle))) return false;
    if (NEVER(LineBuffer == NULL)) return false;
    if (NEVER(LineBuffer->Data == NULL || LineBuffer->Data == String_Null().Data)) return false;

    DWORD CurrentPosition = SetFilePointer(Handle->Data, 0, NULL, FILE_CURRENT);

    LARGE_INTEGER FileSize;
    GetFileSizeEx(Handle->Data, &FileSize);
    u64 Size = (u64)FileSize.QuadPart;

    if (CurrentPosition >= Size)
    {
        Filesystem_SeekToBeginning(Handle);
        return false;
    }

    char TempBuffer[8192] = {0};
    DWORD BytesRead = 0;
    if (!ReadFile(Handle->Data, TempBuffer, 8192, &BytesRead, NULL))
    {
        LogLastError(S("Filesystem_ReadLine | ReadFile() failed"));

        return false;
    }

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

    SetFilePointer(Handle->Data, (i32)(CurrentPosition + FilePointerOffset), NULL, FILE_BEGIN);

    return true;
}

// todo: make internal function, code duplication
bool Filesystem_ReadLine_Backwards(const FileHandle* Handle, String* LineBuffer)
{
    ASSERT(IsValidFileHandle(Handle));

    if (LineBuffer)
    {
        DWORD CurrentPosition = SetFilePointer(Handle->Data, 0, NULL, FILE_CURRENT);

        if (CurrentPosition == 0)
        {
            return false;
        }

        DWORD BytesRead = 0;
        char Char[2] = {0};

        SetFilePointer(Handle->Data, -2, NULL, FILE_CURRENT);

        bool bFirstNewLineFound = false;

        while (ReadFile(Handle->Data, Char, 1, &BytesRead, NULL))
        {
            if (Char[0] == '\0' || Char[0] == '\n' || Char[0] == '\r')
            {
                if (!bFirstNewLineFound)
                {
                    bFirstNewLineFound = true;
                }
                else
                {
                    break;
                }
            }
            else if (SetFilePointer(Handle->Data, 0, NULL, FILE_CURRENT) == 0)
            {
                break;
            }

            SetFilePointer(Handle->Data, -2, NULL, FILE_CURRENT);
        }

        char TempBuffer[8192] = {0};
        if (!ReadFile(Handle->Data, TempBuffer, 8192, &BytesRead, NULL))
        {
            LogLastError(S("Filesystem_ReadLine_Backwards | ReadFile() failed"));

            return false;
        }

        u32 Counter = 0;
        u32 FilePointerOffset = 0;
        for (u32 i = 0; i < BytesRead; i++)
        {
            if (TempBuffer[i] == '\0' || TempBuffer[i] == '\n' || TempBuffer[i] == '\r')
            {
                FilePointerOffset = Counter;

                char* p = &TempBuffer[i];

                // eat new lines and returns
                while (*p == '\n' || *p == '\r')
                {
                    p++;
                    FilePointerOffset++;
                }

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

        return Counter > 0;
    }

    return false;
}

bool Filesystem_Write(const FileHandle* Handle, u64 DataSize, const void* Data, u64* OutBytesWritten)
{
    ASSERT(IsValidFileHandle(Handle));

    if (DataSize == 0)
        return false;

    Filesystem_SeekToBeginning(Handle);

    DWORD BytesWritten = 0;
    BOOL bResult = WriteFile(Handle->Data, Data, (DWORD)DataSize, &BytesWritten, NULL);

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    if (!bResult)
    {
        StringLocal(Prefix, 2048);
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);
        String_Format(&Prefix, S("Failed to write to file \"%S\""), 2048, Path);
        LogLastError(Prefix);
    }

    return bResult;
}

bool Filesystem_WriteLine(const FileHandle* Handle, const String Text, u64* OutBytesWritten)
{
    ASSERT(IsValidFileHandle(Handle));

    Filesystem_SeekToEnd(Handle);

    DWORD BytesWritten = 0;
    BOOL bResult = WriteFile(Handle->Data, Text.Data, (DWORD)Text.Length, &BytesWritten, NULL);

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    if (!bResult)
    {
        StringLocal(Prefix, 2048);
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);
        String_Format(&Prefix, S("Failed to write line to file \"%S\""), 2048, Path);
        LogLastError(Prefix);
    }

    return bResult;
}

bool Filesystem_WriteLineFormatted(const FileHandle* Handle, const String Text, u64* OutBytesWritten, ...)
{
    ASSERT(IsValidFileHandle(Handle));

    Filesystem_SeekToEnd(Handle);

    va_list Args;
    va_start(Args, OutBytesWritten);
    StringLocal(Buffer, 32768);
    String_FormatV(&Buffer, Text, 32768, Args);
    va_end(Args);

    DWORD BytesWritten = 0;
    BOOL bResult = WriteFile(Handle->Data, Buffer.Data, (DWORD)Buffer.Length, &BytesWritten, NULL);

    if (OutBytesWritten)
        *OutBytesWritten = BytesWritten;

    if (!bResult)
    {
        StringLocal(Prefix, 2048);
        StringLocal(Path, MAX_PATH_LENGTH);
        Filesystem_GetFilePath(Handle, &Path);
        String_Format(&Prefix, S("Failed to write line to file \"%S\""), 2048, Path);
        LogLastError(Prefix);
    }

    return bResult;
}

bool Filesystem_DoesFileExist(const String FilePath)
{
    if (FilePath.Length == 0) return false;

    return PathFileExists(FilePath.Data);
}

bool Filesystem_DoesDirectoryExist(const String FilePath)
{
    if (FilePath.Length == 0) return false;

    DWORD Attrib = GetFileAttributes(FilePath.Data);
    return (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool Filesystem_GetFilePath(const FileHandle* File, String* OutPath)
{
    if (!IsValidFileHandle(File))
    {
        return false;
    }

    u32 Length = GetFinalPathNameByHandle(File->Data, OutPath->Data, MAX_PATH, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (Length == 0)
    {
        LogLastError(S("Filesystem_GetFilePath failed"));
        return false;
    }

    OutPath->Length = Length;
    *OutPath = StrShiftF(*OutPath, 4); // ignore //?/
    return true;
}

bool Filesystem_GetFileSize(const FileHandle* File, u64* OutSize)
{
    if (IsValidFileHandle(File))
    {
        LARGE_INTEGER FileSize;
        BOOL Result = GetFileSizeEx(File->Data, &FileSize);
        *OutSize = (u64)FileSize.QuadPart;
        return Result;
    }

    return false;
}

bool Filesystem_IsFile(const String Path)
{
    DWORD Attrib = GetFileAttributes(Path.Data);
    return (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib & FILE_ATTRIBUTE_NORMAL));
}

bool Filesystem_IsDirectory(const String Path)
{
    DWORD Attrib = GetFileAttributes(Path.Data);
    return (Attrib != INVALID_FILE_ATTRIBUTES && (Attrib & FILE_ATTRIBUTE_DIRECTORY));
}

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

bool Filesystem_IsPathRelative(const String Path)
{
    bool bDriveSymbol = String_IndexOfChar(Path, ':', NULL);

    bool bRelative = !bDriveSymbol;

    return bRelative;
    
    // would this work? i'll test later
    //return PathIsRelative(Path.Data);
}

bool Filesystem_ConvertRelativeToAbsolutePath(String* OutFullPath)
{
    StringLocal(Copy, MAX_PATH);
    String_Copy(&Copy, *OutFullPath);

    bool bResult = PathCanonicalize(OutFullPath->Data, Copy.Data);
    OutFullPath->Length = String_GetLength_Ex(OutFullPath->Data, MAX_PATH);
    return bResult;
}

// transforms paths with " in them to paths without them
// for exmaple: "C:\Program Files"\MyApp -> C:\Program Files\MyApp
// TODO: move to core
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

bool Filesystem_SanitizePath(String* Dest, const String Path)
{
    bool bAnyChange = false;
    for (u32 i = 0; i < Path.Length; i++)
    {
        if (Path.Data[i] == '"')
            continue;
        
        bAnyChange = true;

        #if PLATFORM_WINDOWS
        char C = Path.Data[i] == '/' ? '\\' : Path.Data[i]; 
        #else
        char C = Path.Data[i] == '\\' ? '/' : Path.Data[i]; 
        #endif

        String_AppendChar(Dest, C);
    }

    return bAnyChange;
}

bool Filesystem_SanitizePathAndWrap(String* Dest, const String Path)
{
    if (Path.Length == 0)
        return false;

    bool bAnyChange = false;
    String_AppendChar(Dest, '"');
    for (u32 i = 0; i < Path.Length; i++)
    {
        if (Path.Data[i] == '"')
            continue;

        bAnyChange = true;

        #if PLATFORM_WINDOWS
        char C = Path.Data[i] == '/' ? '\\' : Path.Data[i]; 
        #else
        char C = Path.Data[i] == '\\' ? '/' : Path.Data[i]; 
        #endif

        String_AppendChar(Dest, C);
    }
    String_AppendChar(Dest, '"');

    return bAnyChange;
}

internal void Internal_IterateDirectory(const String RootPath, const String DirectoryPath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    WIN32_FIND_DATA ffd = {0};
    TCHAR Temp[MAX_PATH] = {0};

    HANDLE Find = INVALID_HANDLE_VALUE;

    const String RealDirectoryPath = DirectoryPath.Length == 0 ? S(".") : DirectoryPath;

    HRESULT Result = StringCchCopy(Temp, MAX_PATH, RealDirectoryPath.Data);
    if (Result != S_OK) goto Error;
    
    Result = StringCchCat(Temp, MAX_PATH, "\\*");
    if (Result != S_OK) goto Error;

    Find = FindFirstFile(Temp, &ffd);

    if (Find != INVALID_HANDLE_VALUE)
    {
        do
        {
            String FileName = CStr(ffd.cFileName);

            if (String_IsEqual(FileName, S("."), false) ||
                String_IsEqual(FileName, S(".."), false))
            {
                continue;
            }

            TCHAR FilePath[MAX_PATH] = {0};
            Result = StringCchCopy(FilePath, MAX_PATH, RealDirectoryPath.Data);
            if (Result != S_OK) goto Error;

            u32 Len = RealDirectoryPath.Length;
            if (Len > 0)
            {
                char LastChar = FilePath[Len-1];
                if (LastChar != '/' && LastChar != '\\')
                {
                    Result = StringCchCat(FilePath, MAX_PATH, "\\");
                    if (Result != S_OK) goto Error;
                    Len++;
                }
            }

            Result = StringCchCat(FilePath, MAX_PATH, ffd.cFileName);
            if (Result != S_OK) goto Error;

            Len += FileName.Length;

            String FullPath;
            FullPath.Data = FilePath;
            FullPath.Length = Len;
            FullPath.Capacity = Len;
            String_EatPathSeparatorsInline(&FullPath);

            String RelativePath = CStr(&FilePath[RootPath.Length]);
            String_EatPathSeparatorsInline(&RelativePath);

            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                bool bResult = Callback(FullPath, RelativePath, FileName, 0, true, UserData);
                if (!bResult)
                {
                    break;
                }

                if (bRecursive)
                {
                    Internal_IterateDirectory(RootPath, FullPath, Callback, true, UserData);
                }
            }
            else
            {
                DWORD FileSize = (ffd.nFileSizeHigh * (MAXDWORD+1)) + ffd.nFileSizeLow;
                bool bResult = Callback(FullPath, RelativePath, FileName, FileSize, false, UserData);
                if (!bResult) // the user wants to end the iteration
                {
                    break;
                }
            }
        }
        while (FindNextFile(Find, &ffd) != 0);

        FindClose(Find);
        return;
    }

Error:
    StringLocal(Msg, 512);
    String_Format(&Msg, S("Failed to iterate directory: %S"), Msg.Capacity, RealDirectoryPath);
    LogLastError(Msg);
    if (Find != INVALID_HANDLE_VALUE)
    {
        FindClose(Find);
    }
}

void Filesystem_IterateDirectory(const String BasePath, DirectoryIterator Callback, bool bRecursive)
{
    Internal_IterateDirectory(BasePath, BasePath, Callback, bRecursive, NULL);
}

void Filesystem_IterateDirectory_Ex(const String BasePath, DirectoryIterator Callback, bool bRecursive, void* UserData)
{
    Internal_IterateDirectory(BasePath, BasePath, Callback, bRecursive, UserData);
}

bool Filesystem_DeleteFiles(const String FilePath, const String Wildcard, bool bRecursive)
{
    WIN32_FIND_DATA fd = {0};

    StringLocal(WildcardPath, MAX_PATH_LENGTH);
    String_BuildPath(&WildcardPath, FilePath, Wildcard);

    HANDLE hFind = FindFirstFile(WildcardPath.Data, &fd);

    bool bAnyFilesDeleted = false;

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            String FileName = CStr(fd.cFileName);

            if (String_IsEqual(FileName, S("."), false) ||
                String_IsEqual(FileName, S(".."), false))
            {
                continue;
            }

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                StringLocal(SubPath, MAX_PATH_LENGTH);
                String_BuildPath(&SubPath, FilePath, FileName);

                if (bRecursive)
                {
                    Filesystem_DeleteFiles(SubPath, Wildcard, true);
                }
            }
            else
            {
                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, FilePath, FileName);

                i32 Result = DeleteFile(FullPath.Data);
                if (Result != 0)
                {
                    bAnyFilesDeleted = true;
                }
            }
        }
        while (FindNextFile(hFind, &fd));

        FindClose(hFind);
    }

    return bAnyFilesDeleted;
}

bool Filesystem_DeleteDirectory(const String DirectoryPath)
{
    WIN32_FIND_DATA fd = {0};

    StringLocal(WildcardPath, MAX_PATH_LENGTH);
    String_BuildPath(&WildcardPath, DirectoryPath, S("*"));

    HANDLE hFind = FindFirstFile(WildcardPath.Data, &fd);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            String FileName = CStr(fd.cFileName);

            if (String_IsEqual(FileName, S("."), false) ||
                String_IsEqual(FileName, S(".."), false))
            {
                continue;
            }

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                StringLocal(SubPath, MAX_PATH_LENGTH);
                String_BuildPath(&SubPath, DirectoryPath, FileName);

                Filesystem_DeleteDirectory(SubPath);
            }
            else
            {
                StringLocal(FullPath, MAX_PATH_LENGTH);
                String_BuildPath(&FullPath, DirectoryPath, FileName);

                DeleteFile(FullPath.Data);
            }
        }
        while (FindNextFile(hFind, &fd));

        FindClose(hFind);
    }

    bool bResult = RemoveDirectory(DirectoryPath.Data);

    return bResult;
}

bool Filesystem_Copy(const String Source, const String Destination)
{
    StringLocal(SourceCopy, MAX_PATH);
    StringLocal(DestinationCopy, MAX_PATH);
    String_Copy(&SourceCopy, Source);
    String_Copy(&DestinationCopy, Destination);

    String_ConvertSlashToPlatformSlash(&SourceCopy);
    String_ConvertSlashToPlatformSlash(&DestinationCopy);

    u32 LastSlash = 0;
    String_IndexOfLastPathSlash(SourceCopy, &LastSlash);

    const String FileName = StrShiftF(SourceCopy, LastSlash);
    if (!String_EndsWith(DestinationCopy, FileName, false))
    {
        String_BuildPath(&DestinationCopy, FileName);
    }

    // TODO: allow source to be a direcotry and copy everything from there

    // try to create the directory if it doesn't exist
    String_IndexOfLastPathSlash(DestinationCopy, &LastSlash);
    Filesystem_OpenDirectory(StrSlice(DestinationCopy.Data, LastSlash));

    // remove the read only attribute if we're copying from a source which had a readonly attribute set on it,
    // otherwise the copy will fail if the file already exists at the destination
    if (Filesystem_DoesFileExist(DestinationCopy))
    {
        SetFileAttributes(DestinationCopy.Data, (u32)GetFileAttributes(DestinationCopy.Data) & (u32)~FILE_ATTRIBUTE_READONLY);
    }

    BOOL bResult = CopyFileEx(SourceCopy.Data, DestinationCopy.Data, NULL, NULL, NULL, COPY_FILE_NO_BUFFERING);
    if (bResult == 0)
    {
        StringLocal(Msg, 512);
        String_Format(&Msg, S("Failed to copy \"%S\" to \"%S\""), Msg.Capacity, Source, Destination);
        LogLastError(Msg);
        return false;
    }

    return true;
}

bool Filesystem_ArePathsCommon(String PathA, String PathB)
{
    StringLocal(CommonPath, MAX_PATH);
    i32 Len = PathCommonPrefix(PathA.Data, PathB.Data, CommonPath.Data);
    CommonPath.Length = (u32)Len;

    return String_IsEqual(CommonPath, PathA, false);
}

PlatformHandle Platform_CreateThread(const String Name, u32* OutThreadID, u32 (*ThreadEntryPoint)(void* ThreadParameter), void* UserData)
{
    HANDLE ThreadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadEntryPoint, UserData, CREATE_SUSPENDED, (LPDWORD)OutThreadID);
    SetThreadPriority(ThreadHandle, THREAD_PRIORITY_ABOVE_NORMAL);

    wchar_t ThreadName[MAX_PATH] = {0};
    String16 ThreadNameBetter;
    ThreadNameBetter.Data = ThreadName;
    String_ToWide(Name, &ThreadNameBetter);

    SetThreadDescription(ThreadHandle, ThreadName);
    ResumeThread(ThreadHandle);

    return ThreadHandle;
}

PlatformHandle Platform_RunCommand(const String CmdLine, const String WorkingDirectory)
{
    STARTUPINFO StartupInfo = {0};
    PROCESS_INFORMATION ProcessInfo = {0};
    StartupInfo.cb = sizeof(StartupInfo);

    char* Dir = WorkingDirectory.Length > 0 ? WorkingDirectory.Data : NULL;
    if (!CreateProcess(NULL, CmdLine.Data, NULL, NULL, TRUE, 0, NULL, Dir, &StartupInfo, &ProcessInfo))
    {
        StringLocal(Prefix, Kibibytes(8));
        String_Format(&Prefix, S("Failed to run command: \"%S\""), Prefix.Capacity, CmdLine);
        LogLastError(Prefix);

        return INVALID_HANDLE_VALUE;
    }

    SetPriorityClass(ProcessInfo.hProcess, ABOVE_NORMAL_PRIORITY_CLASS);

    return ProcessInfo.hProcess;
}

PlatformHandle Platform_RunCommand_Ex(const String CmdLine, const String WorkingDirectory, PlatformPipe* StdOutPipe)
{
    STARTUPINFO StartupInfo = {0};
    PROCESS_INFORMATION ProcessInfo = {0};
    SECURITY_ATTRIBUTES saAttr = {0}; 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    HANDLE r = 0, w = 0;
    if (!CreatePipe(&r, &w, &saAttr, 0))
    {
        LogLastError(S("Failed to create pipe"));

        return INVALID_HANDLE_VALUE;
    }

    if (!SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0))
    {
        LogLastError(S("Failed to set pipe information"));

        return INVALID_HANDLE_VALUE;
    }

    (*StdOutPipe)[0] = r;
    (*StdOutPipe)[1] = w;

    StartupInfo.cb = sizeof(STARTUPINFO);
    StartupInfo.hStdError = w;
    StartupInfo.hStdOutput = w;
    StartupInfo.hStdInput = NULL;
    StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

    char* Dir = WorkingDirectory.Length > 0 ? WorkingDirectory.Data : NULL;
    if (!CreateProcess(NULL, CmdLine.Data, NULL, NULL, TRUE, 0, NULL, Dir, &StartupInfo, &ProcessInfo))
    {
        StringLocal(Prefix, Kibibytes(8));
        String_Format(&Prefix, S("Failed to run command: \"%S\""), Prefix.Capacity, CmdLine);
        LogLastError(Prefix);

        return INVALID_HANDLE_VALUE;
    }

    SetPriorityClass(ProcessInfo.hProcess, ABOVE_NORMAL_PRIORITY_CLASS);

    return ProcessInfo.hProcess;
}

bool Platform_TerminateProcess(PlatformHandle Handle, u32 ExitCode)
{
    bool bResult = TerminateProcess(Handle, ExitCode);
    CloseHandle(Handle);
    return bResult;
}

bool Platform_FindProgram(String ProgramName)
{
    return Platform_FindFile_Ex(ProgramName, S(".exe"), NULL);
}

bool Platform_FindProgram_Ex(String ProgramName, String* OutProgramPath)
{
    return Platform_FindFile_Ex(ProgramName, S(".exe"), OutProgramPath);
}

bool Platform_FindFile(String FileName, String ExtensionWithDot)
{
    return Platform_FindFile_Ex(FileName, ExtensionWithDot, NULL);
}

bool Platform_FindFile_Ex(String FileName, String ExtensionWithDot, String* OutFilePath)
{
    if (!String_IsValid(FileName))
    {
        return false;
    }

    char* Ext = NULL;
    if (ExtensionWithDot.Length > 1)
        Ext = ExtensionWithDot.Data;

    char FullPath[MAX_PATH] = {0};
    DWORD Len = SearchPath(NULL, FileName.Data, Ext, MAX_PATH, FullPath, NULL);
    if (Len == 0)
        return false;

    if (OutFilePath)
    {
        String_Copy(OutFilePath, StrSlice(FullPath, Len));
    }

    return true;
}

bool Platform_IsValidHandle(const PlatformHandle Handle)
{
    return Handle != NULL && Handle != INVALID_HANDLE_VALUE && Handle != nullptr;
}

u64 Platform_GetCriticalSectionMemoryRequirement(void)
{
    return sizeof(CRITICAL_SECTION);
}

void Platform_InitializeCriticalSection(PlatformCriticalSection OutCriticalSection)
{
    InitializeCriticalSection(OutCriticalSection);
}

void Platform_DeleteCriticalSection(PlatformCriticalSection CriticalSection)
{
    DeleteCriticalSection(CriticalSection);
}

void Platform_EnterCriticalSection(PlatformCriticalSection CriticalSection)
{
    EnterCriticalSection(CriticalSection);
}

void Platform_ExitCriticalSection(PlatformCriticalSection CriticalSection)
{
    LeaveCriticalSection(CriticalSection);
}

u32 Platform_GetExitCodeForProcess(PlatformHandle Handle)
{
    ASSERT(Platform_IsValidHandle(Handle));
    if (!Platform_IsValidHandle(Handle))
    {
        return 0;
    }

    DWORD ExitCode = 0;
    if (!GetExitCodeProcess(Handle, &ExitCode))
    {
        return UINT32_MAX;
    }

    return ExitCode;
}

u32 Platform_WaitForProcessAndGetExitCode(PlatformHandle Handle)
{
    ASSERT(Platform_IsValidHandle(Handle));
    if (!Platform_IsValidHandle(Handle))
    {
        return 0;
    }

    WaitForSingleObject(Handle, INFINITE);

    DWORD ExitCode = 0;
    if (!GetExitCodeProcess(Handle, &ExitCode))
    {
        return UINT32_MAX;
    }

    return ExitCode;
}

u32 Platform_WaitForMultipleHandles(PlatformHandle* Handles, u32 NumHandles, i32 Milliseconds, bool bWaitAll)
{
    i32 Time = Milliseconds <= 0 ? (i32)INFINITE : Milliseconds;
    return WaitForMultipleObjects(NumHandles, Handles, bWaitAll, (u32)Time);
}

void Platform_WaitForHandle(PlatformHandle Handle, i32 Milliseconds)
{
    ASSERT(Platform_IsValidHandle(Handle));
    if (!Platform_IsValidHandle(Handle))
    {
        return;
    }

    i32 Time = Milliseconds <= 0 ? (i32)INFINITE : Milliseconds;
    WaitForSingleObject(Handle, (u32)Time);
}

void Platform_CloseHandle(PlatformHandle Handle)
{
    ASSERT(Platform_IsValidHandle(Handle));
    if (!Platform_IsValidHandle(Handle))
    {
        return;
    }

    CloseHandle(Handle);
}

bool Platform_IsProgramRunning(const String ProgramName)
{
    DWORD Processes[4096] = {0};
    DWORD BytesRead = 0;
    EnumProcesses(Processes, sizeof Processes, &BytesRead);
    DWORD Count = BytesRead / sizeof(DWORD);

    DWORD ProcessId = GetCurrentProcessId();

    for (u16 i = 0; i < Count; i++)
    {
        TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, Processes[i]);

        if (hProcess != NULL)
        {
            GetModuleFileNameEx(hProcess, NULL, szProcessName, MAX_PATH);
        }

        CloseHandle(hProcess);

        const String ProcessName = CStr(szProcessName);

        StringLocal(ProgramNameCopy, 512);
        String_Copy(&ProgramNameCopy, ProgramName);

        if (!String_EndsWith(ProgramName, S(".exe"), false))
            String_Append(&ProgramNameCopy, S(".exe"));

        if (String_EndsWith(ProcessName, ProgramNameCopy, false) && Processes[i] != ProcessId)
        {
            return true;
        }
    }

    return false;
}

bool Platform_GetTerminalDimensions(u32* OutRows, u32* OutColumns)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi) == 0)
    {
        LogLastError(S("Failed to get console screen buffer info"));
        return false;
    }

    *OutColumns = (u32)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
    *OutRows    = (u32)(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);

    return true;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-prototypes"
FORCENOINLINE BOOL __cdecl _DllMainCRTStartup(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved)
{
    return true;
}

/*
C_LINKAGE int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
    return 0;
}
*/

#pragma clang diagnostic pop

#ifdef RIFT_ASAN
void abort(void)
{
    ExitProcess(1);
}

_CRTIMP int __C_specific_handler(struct _EXCEPTION_RECORD* ExceptionRecord, void* EstablisherFrame, struct _CONTEXT* ContextRecord, struct _DISPATCHER_CONTEXT *DispatcherContext)
{
    return 0;
}
#endif

#endif // PLATFORM_WINDOWS

