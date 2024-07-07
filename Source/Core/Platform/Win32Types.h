#pragma once

// Because including windows header files doubles our compile times
// if we manually add the parts we need we speed up our compile times by at least 2x

#ifdef  _WIN64
typedef unsigned long long size_t;
#else
typedef unsigned int       size_t;
#endif

typedef unsigned long DWORD;
typedef long HRESULT;

// macros
#define SUCCEEDED(hr)   (((HRESULT)(hr)) >= 0)
#define FAILED(hr)      (((HRESULT)(hr)) < 0)

#define S_OK            ((HRESULT)0L)


#define HRESULT_CODE(hr)    ((hr) & 0xFFFF)
#define SCODE_CODE(sc)      ((sc) & 0xFFFF)

#ifndef far
#define far
#endif
#ifndef near
#define near
#endif

#ifndef CONST
#define CONST const
#endif

#define TRUE 1
#define FALSE 0

#define MAX_PATH          260

#ifdef _MAC
#define CALLBACK    PASCAL
#define WINAPI      CDECL
#define WINAPIV     CDECL
#define APIENTRY    WINAPI
#define APIPRIVATE  CDECL
#ifdef _68K_
#define PASCAL      __pascal
#else
#define PASCAL
#endif
#elif (_MSC_VER >= 800) || defined(_STDCALL_SUPPORTED)
#define CALLBACK    __stdcall
#define WINAPI      __stdcall
#define WINAPIV     __cdecl
#define APIENTRY    WINAPI
#define APIPRIVATE  __stdcall
#define PASCAL      __stdcall
#else
#define CALLBACK
#define WINAPI
#define WINAPIV
#define APIENTRY    WINAPI
#define APIPRIVATE
#define PASCAL      pascal
#endif

#if defined(_WIN64)
    typedef signed long long INT_PTR, *PINT_PTR;
    typedef unsigned long long  UINT_PTR, *PUINT_PTR;

    typedef signed long long LONG_PTR, *PLONG_PTR;
    typedef unsigned long long ULONG_PTR, *PULONG_PTR;

    #define __int3264   unsigned long long

#else
    typedef int INT_PTR, *PINT_PTR;
    typedef unsigned int UINT_PTR, *PUINT_PTR;

    typedef long LONG_PTR, *PLONG_PTR;
    typedef unsigned long ULONG_PTR, *PULONG_PTR;

    #define __int3264   int
#endif

typedef void*               HANDLE;
typedef void*               HDC;
typedef void*               PVOID;
typedef void*               HICON;
typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef short               SHORT;
typedef long                LONG;
typedef FLOAT               *PFLOAT;
typedef BOOL near           *PBOOL;
typedef BOOL far            *LPBOOL;
typedef BYTE near           *PBYTE;
typedef BYTE far            *LPBYTE;
typedef int near            *PINT;
typedef int far             *LPINT;
typedef WORD near           *PWORD;
typedef WORD far            *LPWORD;
typedef long far            *LPLONG;
typedef DWORD near          *PDWORD;
typedef DWORD far           *LPDWORD;
typedef void far            *LPVOID;
typedef CONST void far      *LPCVOID;
typedef SHORT               *PSHORT;
typedef LONG                *PLONG;

typedef HANDLE near         *SPHANDLE;
typedef HANDLE far          *LPHANDLE;
typedef HANDLE              HGLOBAL;
typedef HANDLE              HLOCAL;
typedef HANDLE              GLOBALHANDLE;
typedef HANDLE              LOCALHANDLE;

typedef unsigned long long ULONG64, *PULONG64;
typedef unsigned long long DWORD64, *PDWORD64;

/* Types use for passing & returning polymorphic values */
typedef UINT_PTR            WPARAM;
typedef LONG_PTR            LPARAM;
typedef LONG_PTR            LRESULT;

typedef ULONG_PTR   DWORD_PTR;
typedef LONG_PTR    SSIZE_T;
typedef ULONG_PTR   SIZE_T;

typedef unsigned long ULONG;
typedef ULONG *PULONG;
typedef unsigned short USHORT;
typedef USHORT *PUSHORT;
typedef unsigned char UCHAR;
typedef UCHAR *PUCHAR;

typedef int                 INT;
typedef unsigned int        UINT;
typedef unsigned int        *PUINT;

typedef char CHAR;
typedef short SHORT;
typedef long LONG;

typedef ULONG_PTR SIZE_T, *PSIZE_T;
typedef LONG_PTR SSIZE_T, *PSSIZE_T;

typedef long RPC_STATUS;

//
// ANSI (Multi-byte Character) types
//
typedef CHAR *PCHAR, *LPCH, *PCH;
typedef CONST CHAR *LPCCH, *PCCH;

typedef CHAR *NPSTR, *LPSTR, *PSTR;
typedef PSTR *PZPSTR;
typedef CONST PSTR *PCZPSTR;
typedef CONST CHAR *LPCSTR, *PCSTR;
typedef PCSTR *PZPCSTR;
typedef CONST PCSTR *PCZPCSTR;

typedef CHAR *PZZSTR;
typedef CONST CHAR *PCZZSTR;

typedef CHAR *PNZCH;
typedef CONST CHAR *PCNZCH;

//
// UNICODE (Wide Character) types
//

typedef unsigned short wchar_t;

#ifndef _MAC
typedef wchar_t WCHAR;    // wc,   16-bit UNICODE character
#else
// some Macintosh compilers don't define wchar_t in a convenient location, or define it as a char
typedef unsigned short WCHAR;    // wc,   16-bit UNICODE character
#endif

typedef WCHAR *PWCHAR, *LPWCH, *PWCH;
typedef CONST WCHAR *LPCWCH, *PCWCH;

typedef WCHAR *NWPSTR, *LPWSTR, *PWSTR;
typedef PWSTR *PZPWSTR;
typedef CONST PWSTR *PCZPWSTR;
typedef CONST WCHAR *LPCWSTR, *PCWSTR;
typedef PCWSTR *PZPCWSTR;
typedef CONST PCWSTR *PCZPCWSTR;

typedef WCHAR *PZZWSTR;
typedef CONST WCHAR *PCZZWSTR;

typedef WCHAR *PNZWCH;
typedef CONST WCHAR *PCNZWCH;

typedef char TCHAR, *PTCHAR;
typedef unsigned char TBYTE , *PTBYTE ;

 typedef signed long long LONGLONG;
 typedef unsigned long long ULONGLONG;

typedef LPCH LPTCH, PTCH;
typedef LPCCH LPCTCH, PCTCH;
typedef LPSTR PTSTR, LPTSTR, PUTSTR, LPUTSTR;
typedef LPCSTR PCTSTR, LPCTSTR, PCUTSTR, LPCUTSTR;
typedef PZZSTR PZZTSTR, PUZZTSTR;
typedef PCZZSTR PCZZTSTR, PCUZZTSTR;
typedef PZPSTR PZPTSTR;
typedef PNZCH PNZTCH, PUNZTCH;
typedef PCNZCH PCNZTCH, PCUNZTCH;

typedef union _LARGE_INTEGER {
  struct {
    DWORD LowPart;
    LONG  HighPart;
  };
  LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _FILETIME {
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef struct _SYSTEM_INFO {
    union {
        DWORD dwOemId;          // Obsolete field...do not use
        struct {
            WORD wProcessorArchitecture;
            WORD wReserved;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
    DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress;
    LPVOID lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD dwNumberOfProcessors;
    DWORD dwProcessorType;
    DWORD dwAllocationGranularity;
    WORD wProcessorLevel;
    WORD wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

typedef struct _TIME_ZONE_INFORMATION {
  LONG       Bias;
  WCHAR      StandardName[32];
  SYSTEMTIME StandardDate;
  LONG       StandardBias;
  WCHAR      DaylightName[32];
  SYSTEMTIME DaylightDate;
  LONG       DaylightBias;
} TIME_ZONE_INFORMATION, *PTIME_ZONE_INFORMATION, *LPTIME_ZONE_INFORMATION;

typedef struct _SYMBOL_INFO {
    ULONG       SizeOfStruct;
    ULONG       TypeIndex;        // Type Index of symbol
    ULONG64     Reserved[2];
    ULONG       Index;
    ULONG       Size;
    ULONG64     ModBase;          // Base Address of module comtaining this symbol
    ULONG       Flags;
    ULONG64     Value;            // Value of symbol, ValuePresent should be 1
    ULONG64     Address;          // Address of symbol including base address of module
    ULONG       Register;         // register holding value or pointer to value
    ULONG       Scope;            // scope of the symbol
    ULONG       Tag;              // pdb classification
    ULONG       NameLen;          // Actual length of name
    ULONG       MaxNameLen;
    CHAR        Name[1];          // Name of symbol
} SYMBOL_INFO, *PSYMBOL_INFO;

typedef struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;

typedef GUID UUID;
#ifndef uuid_t
#define uuid_t UUID
#endif

typedef unsigned char* RPC_CSTR;
typedef unsigned short* RPC_WSTR;

#ifndef _VA_LIST_DEFINED
    #define _VA_LIST_DEFINED
    #ifdef _M_CEE_PURE
        typedef System::ArgIterator va_list;
    #else
        typedef char* va_list;
    #endif
#endif

typedef struct _WIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    CHAR   cFileName[ MAX_PATH ];
    CHAR   cAlternateFileName[ 14 ];
#ifdef _MAC
    DWORD dwFileType;
    DWORD dwCreatorType;
    WORD  wFinderFlags;
#endif
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;
typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    WCHAR  cFileName[ MAX_PATH ];
    WCHAR  cAlternateFileName[ 14 ];
#ifdef _MAC
    DWORD dwFileType;
    DWORD dwCreatorType;
    WORD  wFinderFlags;
#endif
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;
#ifdef UNICODE
typedef WIN32_FIND_DATAW WIN32_FIND_DATA;
typedef PWIN32_FIND_DATAW PWIN32_FIND_DATA;
typedef LPWIN32_FIND_DATAW LPWIN32_FIND_DATA;
#else
typedef WIN32_FIND_DATAA WIN32_FIND_DATA;
typedef PWIN32_FIND_DATAA PWIN32_FIND_DATA;
typedef LPWIN32_FIND_DATAA LPWIN32_FIND_DATA;
#endif // UNICODE

typedef
DWORD (WINAPI *LPPROGRESS_ROUTINE)(
    LARGE_INTEGER TotalFileSize,
    LARGE_INTEGER TotalBytesTransferred,
    LARGE_INTEGER StreamSize,
    LARGE_INTEGER StreamBytesTransferred,
    DWORD dwStreamNumber,
    DWORD dwCallbackReason,
    HANDLE hSourceFile,
    HANDLE hDestinationFile,
    LPVOID lpData
);

typedef DWORD (WINAPI *PTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);
typedef PTHREAD_START_ROUTINE LPTHREAD_START_ROUTINE;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        } DUMMYSTRUCTNAME;
        PVOID Pointer;
    } DUMMYUNIONNAME;

    HANDLE  hEvent;
} OVERLAPPED, *LPOVERLAPPED;

typedef struct _OVERLAPPED_ENTRY {
    ULONG_PTR lpCompletionKey;
    LPOVERLAPPED lpOverlapped;
    ULONG_PTR Internal;
    DWORD dwNumberOfBytesTransferred;
} OVERLAPPED_ENTRY, *LPOVERLAPPED_ENTRY;

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
} PROCESS_INFORMATION, *PPROCESS_INFORMATION, *LPPROCESS_INFORMATION;

typedef struct _STARTUPINFOA {
    DWORD   cb;
    LPSTR   lpReserved;
    LPSTR   lpDesktop;
    LPSTR   lpTitle;
    DWORD   dwX;
    DWORD   dwY;
    DWORD   dwXSize;
    DWORD   dwYSize;
    DWORD   dwXCountChars;
    DWORD   dwYCountChars;
    DWORD   dwFillAttribute;
    DWORD   dwFlags;
    WORD    wShowWindow;
    WORD    cbReserved2;
    LPBYTE  lpReserved2;
    HANDLE  hStdInput;
    HANDLE  hStdOutput;
    HANDLE  hStdError;
} STARTUPINFOA, *LPSTARTUPINFOA;
typedef struct _STARTUPINFOW {
    DWORD   cb;
    LPWSTR  lpReserved;
    LPWSTR  lpDesktop;
    LPWSTR  lpTitle;
    DWORD   dwX;
    DWORD   dwY;
    DWORD   dwXSize;
    DWORD   dwYSize;
    DWORD   dwXCountChars;
    DWORD   dwYCountChars;
    DWORD   dwFillAttribute;
    DWORD   dwFlags;
    WORD    wShowWindow;
    WORD    cbReserved2;
    LPBYTE  lpReserved2;
    HANDLE  hStdInput;
    HANDLE  hStdOutput;
    HANDLE  hStdError;
} STARTUPINFOW, *LPSTARTUPINFOW;
#ifdef UNICODE
typedef STARTUPINFOW STARTUPINFO;
typedef LPSTARTUPINFOW LPSTARTUPINFO;
#else
typedef STARTUPINFOA STARTUPINFO;
typedef LPSTARTUPINFOA LPSTARTUPINFO;
#endif // UNICODE

typedef struct _SMALL_RECT {
    SHORT Left;
    SHORT Top;
    SHORT Right;
    SHORT Bottom;
} SMALL_RECT, *PSMALL_RECT;

typedef struct _COORD {
    SHORT X;
    SHORT Y;
} COORD, *PCOORD;

typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
    COORD dwSize;
    COORD dwCursorPosition;
    WORD  wAttributes;
    SMALL_RECT srWindow;
    COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO, *PCONSOLE_SCREEN_BUFFER_INFO;


#define  _WIN32_WINNT   0x0A00
#define  WINVER         0x0A00

#define MOVEFILE_REPLACE_EXISTING       0x00000001
#define MOVEFILE_COPY_ALLOWED           0x00000002
#define MOVEFILE_DELAY_UNTIL_REBOOT     0x00000004
#define MOVEFILE_WRITE_THROUGH          0x00000008
#if (_WIN32_WINNT >= 0x0500)
#define MOVEFILE_CREATE_HARDLINK        0x00000010
#define MOVEFILE_FAIL_IF_NOT_TRACKABLE  0x00000020
#endif // (_WIN32_WINNT >= 0x0500)

#define PROCESS_TERMINATE                  (0x0001)  
#define PROCESS_CREATE_THREAD              (0x0002)  
#define PROCESS_SET_SESSIONID              (0x0004)  
#define PROCESS_VM_OPERATION               (0x0008)  
#define PROCESS_VM_READ                    (0x0010)  
#define PROCESS_VM_WRITE                   (0x0020)  
#define PROCESS_DUP_HANDLE                 (0x0040)  
#define PROCESS_CREATE_PROCESS             (0x0080)  
#define PROCESS_SET_QUOTA                  (0x0100)  
#define PROCESS_SET_INFORMATION            (0x0200)  
#define PROCESS_QUERY_INFORMATION          (0x0400)  
#define PROCESS_SUSPEND_RESUME             (0x0800)  
#define PROCESS_QUERY_LIMITED_INFORMATION  (0x1000)  
#define PROCESS_SET_LIMITED_INFORMATION    (0x2000)  
//

#if (NTDDI_VERSION >= NTDDI_VISTA)
#define PROCESS_ALL_ACCESS        (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | \
                                   0xFFFF)
#else
#define PROCESS_ALL_ACCESS        (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | \
                                   0xFFF)
#endif

//
#define THREAD_TERMINATE                 (0x0001)  
#define THREAD_SUSPEND_RESUME            (0x0002)  
#define THREAD_GET_CONTEXT               (0x0008)  
#define THREAD_SET_CONTEXT               (0x0010)  
#define THREAD_QUERY_INFORMATION         (0x0040)  
#define THREAD_SET_INFORMATION           (0x0020)  
#define THREAD_SET_THREAD_TOKEN          (0x0080)
#define THREAD_IMPERSONATE               (0x0100)
#define THREAD_DIRECT_IMPERSONATION      (0x0200)
#define THREAD_SET_LIMITED_INFORMATION   (0x0400)  // winnt
#define THREAD_QUERY_LIMITED_INFORMATION (0x0800)  // winnt
#define THREAD_RESUME                    (0x1000)  // winnt

#if (NTDDI_VERSION >= NTDDI_VISTA)
#define THREAD_ALL_ACCESS         (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | \
                                   0xFFFF)
#else
#define THREAD_ALL_ACCESS         (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | \
                                   0x3FF)
#endif

#define INFINITE            0xFFFFFFFF  // Infinite timeout

#define HANDLE_FLAG_INHERIT             0x00000001
#define HANDLE_FLAG_PROTECT_FROM_CLOSE  0x00000002

#define STARTF_USESHOWWINDOW       0x00000001
#define STARTF_USESIZE             0x00000002
#define STARTF_USEPOSITION         0x00000004
#define STARTF_USECOUNTCHARS       0x00000008
#define STARTF_USEFILLATTRIBUTE    0x00000010
#define STARTF_RUNFULLSCREEN       0x00000020  // ignored for non-x86 platforms
#define STARTF_FORCEONFEEDBACK     0x00000040
#define STARTF_FORCEOFFFEEDBACK    0x00000080
#define STARTF_USESTDHANDLES       0x00000100

#define MINCHAR     0x80        
#define MAXCHAR     0x7f        
#define MINSHORT    0x8000      
#define MAXSHORT    0x7fff      
#define MINLONG     0x80000000  
#define MAXLONG     0x7fffffff  
#define MAXBYTE     0xff        
#define MAXWORD     0xffff      
#define MAXDWORD    0xffffffff  

#define GENERIC_READ                     (0x80000000L)
#define GENERIC_WRITE                    (0x40000000L)
#define GENERIC_EXECUTE                  (0x20000000L)
#define GENERIC_ALL                      (0x10000000L)

#define FILE_SHARE_READ                 0x00000001  
#define FILE_SHARE_WRITE                0x00000002  
#define FILE_SHARE_DELETE               0x00000004  

#define FILE_ATTRIBUTE_READONLY             0x00000001  
#define FILE_ATTRIBUTE_HIDDEN               0x00000002  
#define FILE_ATTRIBUTE_SYSTEM               0x00000004  
#define FILE_ATTRIBUTE_DIRECTORY            0x00000010  
#define FILE_ATTRIBUTE_ARCHIVE              0x00000020  
#define FILE_ATTRIBUTE_DEVICE               0x00000040  
#define FILE_ATTRIBUTE_NORMAL               0x00000080  
#define FILE_ATTRIBUTE_TEMPORARY            0x00000100  
#define FILE_ATTRIBUTE_SPARSE_FILE          0x00000200  
#define FILE_ATTRIBUTE_REPARSE_POINT        0x00000400  
#define FILE_ATTRIBUTE_COMPRESSED           0x00000800  
#define FILE_ATTRIBUTE_OFFLINE              0x00001000  
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED  0x00002000  
#define FILE_ATTRIBUTE_ENCRYPTED            0x00004000  
#define FILE_ATTRIBUTE_INTEGRITY_STREAM     0x00008000  
#define FILE_ATTRIBUTE_VIRTUAL              0x00010000  
#define FILE_ATTRIBUTE_NO_SCRUB_DATA        0x00020000  
#define FILE_ATTRIBUTE_EA                   0x00040000  
#define FILE_ATTRIBUTE_PINNED               0x00080000  
#define FILE_ATTRIBUTE_UNPINNED             0x00100000  
#define FILE_ATTRIBUTE_RECALL_ON_OPEN       0x00040000  
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000 

#define FILE_FLAG_WRITE_THROUGH         0x80000000
#define FILE_FLAG_OVERLAPPED            0x40000000
#define FILE_FLAG_NO_BUFFERING          0x20000000
#define FILE_FLAG_RANDOM_ACCESS         0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN       0x08000000
#define FILE_FLAG_DELETE_ON_CLOSE       0x04000000
#define FILE_FLAG_BACKUP_SEMANTICS      0x02000000
#define FILE_FLAG_POSIX_SEMANTICS       0x01000000
#define FILE_FLAG_SESSION_AWARE         0x00800000
#define FILE_FLAG_OPEN_REPARSE_POINT    0x00200000
#define FILE_FLAG_OPEN_NO_RECALL        0x00100000
#define FILE_FLAG_FIRST_PIPE_INSTANCE   0x00080000

#define FILE_BEGIN           0
#define FILE_CURRENT         1
#define FILE_END             2

#define VOLUME_NAME_DOS  0x0      //default
#define VOLUME_NAME_GUID 0x1
#define VOLUME_NAME_NT   0x2
#define VOLUME_NAME_NONE 0x4

#define FILE_NAME_NORMALIZED 0x0  //default
#define FILE_NAME_OPENED     0x8

#define CREATE_NEW          1
#define CREATE_ALWAYS       2
#define OPEN_EXISTING       3
#define OPEN_ALWAYS         4
#define TRUNCATE_EXISTING   5

#define INVALID_FILE_SIZE ((DWORD)0xFFFFFFFF)
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)

#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

#define COPY_FILE_COPY_SYMLINK                0x00000800
#define COPY_FILE_NO_BUFFERING                0x00001000

#define PAGE_NOACCESS           0x01    
#define PAGE_READONLY           0x02    
#define PAGE_READWRITE          0x04    
#define PAGE_WRITECOPY          0x08    
#define PAGE_EXECUTE            0x10    
#define PAGE_EXECUTE_READ       0x20    
#define PAGE_EXECUTE_READWRITE  0x40    
#define PAGE_EXECUTE_WRITECOPY  0x80    
#define PAGE_GUARD             0x100    
#define PAGE_NOCACHE           0x200    
#define PAGE_WRITECOMBINE      0x400    
#define PAGE_GRAPHICS_NOACCESS           0x0800    
#define PAGE_GRAPHICS_READONLY           0x1000    
#define PAGE_GRAPHICS_READWRITE          0x2000    
#define PAGE_GRAPHICS_EXECUTE            0x4000    
#define PAGE_GRAPHICS_EXECUTE_READ       0x8000    
#define PAGE_GRAPHICS_EXECUTE_READWRITE 0x10000    
#define PAGE_GRAPHICS_COHERENT          0x20000    
#define PAGE_GRAPHICS_NOCACHE           0x40000    
#define PAGE_ENCLAVE_THREAD_CONTROL 0x80000000  
#define PAGE_REVERT_TO_FILE_MAP     0x80000000  
#define PAGE_TARGETS_NO_UPDATE      0x40000000  
#define PAGE_TARGETS_INVALID        0x40000000  
#define PAGE_ENCLAVE_UNVALIDATED    0x20000000  
#define PAGE_ENCLAVE_MASK           0x10000000  
#define PAGE_ENCLAVE_DECOMMIT       (PAGE_ENCLAVE_MASK | 0) 
#define PAGE_ENCLAVE_SS_FIRST       (PAGE_ENCLAVE_MASK | 1) 
#define PAGE_ENCLAVE_SS_REST        (PAGE_ENCLAVE_MASK | 2) 
#define MEM_COMMIT                      0x00001000  
#define MEM_RESERVE                     0x00002000  
#define MEM_REPLACE_PLACEHOLDER         0x00004000  
#define MEM_RESERVE_PLACEHOLDER         0x00040000  
#define MEM_RESET                       0x00080000  
#define MEM_TOP_DOWN                    0x00100000  
#define MEM_WRITE_WATCH                 0x00200000  
#define MEM_PHYSICAL                    0x00400000  
#define MEM_ROTATE                      0x00800000  
#define MEM_DIFFERENT_IMAGE_BASE_OK     0x00800000  
#define MEM_RESET_UNDO                  0x01000000  
#define MEM_LARGE_PAGES                 0x20000000  
#define MEM_4MB_PAGES                   0x80000000  
#define MEM_64K_PAGES                   (MEM_LARGE_PAGES | MEM_PHYSICAL)  
#define MEM_UNMAP_WITH_TRANSIENT_BOOST  0x00000001  
#define MEM_COALESCE_PLACEHOLDERS       0x00000001  
#define MEM_PRESERVE_PLACEHOLDER        0x00000002  
#define MEM_DECOMMIT                    0x00004000  
#define MEM_RELEASE                     0x00008000  
#define MEM_FREE                        0x00010000  

#define SECTION_QUERY                0x0001
#define SECTION_MAP_WRITE            0x0002
#define SECTION_MAP_READ             0x0004
#define SECTION_MAP_EXECUTE          0x0008
#define SECTION_EXTEND_SIZE          0x0010
#define SECTION_MAP_EXECUTE_EXPLICIT 0x0020 // not included in SECTION_ALL_ACCESS

#define SECTION_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED|SECTION_QUERY|\
                            SECTION_MAP_WRITE |      \
                            SECTION_MAP_READ |       \
                            SECTION_MAP_EXECUTE |    \
                            SECTION_EXTEND_SIZE)

#define FILE_MAP_WRITE            SECTION_MAP_WRITE
#define FILE_MAP_READ             SECTION_MAP_READ
#define FILE_MAP_ALL_ACCESS       SECTION_ALL_ACCESS

#define FILE_MAP_EXECUTE          SECTION_MAP_EXECUTE_EXPLICIT  // not included in FILE_MAP_ALL_ACCESS

#define FILE_MAP_COPY             0x00000001

#define FILE_MAP_RESERVE          0x80000000
#define FILE_MAP_TARGETS_INVALID  0x40000000
#define FILE_MAP_LARGE_PAGES      0x20000000

#define STD_INPUT_HANDLE    ((DWORD)-10)
#define STD_OUTPUT_HANDLE   ((DWORD)-11)
#define STD_ERROR_HANDLE    ((DWORD)-12)

#define HEAP_NO_SERIALIZE               0x00000001      
#define HEAP_GROWABLE                   0x00000002      
#define HEAP_GENERATE_EXCEPTIONS        0x00000004      
#define HEAP_ZERO_MEMORY                0x00000008      
#define HEAP_REALLOC_IN_PLACE_ONLY      0x00000010      
#define HEAP_TAIL_CHECKING_ENABLED      0x00000020      
#define HEAP_FREE_CHECKING_ENABLED      0x00000040      
#define HEAP_DISABLE_COALESCE_ON_FREE   0x00000080      
#define HEAP_CREATE_ALIGN_16            0x00010000      
#define HEAP_CREATE_ENABLE_TRACING      0x00020000      
#define HEAP_CREATE_ENABLE_EXECUTE      0x00040000      
#define HEAP_MAXIMUM_TAG                0x0FFF              
#define HEAP_PSEUDO_TAG_FLAG            0x8000              
#define HEAP_TAG_SHIFT                  18                  
#define HEAP_CREATE_SEGMENT_HEAP        0x00000100      
#define HEAP_CREATE_HARDENED            0x00000200      

#define DEBUG_PROCESS                     0x00000001
#define DEBUG_ONLY_THIS_PROCESS           0x00000002
#define CREATE_SUSPENDED                  0x00000004
#define DETACHED_PROCESS                  0x00000008

#define CREATE_NEW_CONSOLE                0x00000010
#define NORMAL_PRIORITY_CLASS             0x00000020
#define IDLE_PRIORITY_CLASS               0x00000040
#define HIGH_PRIORITY_CLASS               0x00000080
#define BELOW_NORMAL_PRIORITY_CLASS       0x00004000
#define ABOVE_NORMAL_PRIORITY_CLASS       0x00008000

#define REALTIME_PRIORITY_CLASS           0x00000100
#define CREATE_NEW_PROCESS_GROUP          0x00000200
#define CREATE_UNICODE_ENVIRONMENT        0x00000400
#define CREATE_SEPARATE_WOW_VDM           0x00000800

#define THREAD_DYNAMIC_CODE_ALLOW   1     // Opt-out of dynamic code generation.

#define THREAD_BASE_PRIORITY_LOWRT  15  // value that gets a thread to LowRealtime-1
#define THREAD_BASE_PRIORITY_MAX    2   // maximum thread base priority boost
#define THREAD_BASE_PRIORITY_MIN    (-2)  // minimum thread base priority boost
#define THREAD_BASE_PRIORITY_IDLE   (-15) // value that gets a thread to idle

#define THREAD_PRIORITY_LOWEST          THREAD_BASE_PRIORITY_MIN
#define THREAD_PRIORITY_BELOW_NORMAL    (THREAD_PRIORITY_LOWEST+1)
#define THREAD_PRIORITY_NORMAL          0
#define THREAD_PRIORITY_HIGHEST         THREAD_BASE_PRIORITY_MAX
#define THREAD_PRIORITY_ABOVE_NORMAL    (THREAD_PRIORITY_HIGHEST-1)
#define THREAD_PRIORITY_ERROR_RETURN    (MAXLONG)

#define THREAD_PRIORITY_TIME_CRITICAL   THREAD_BASE_PRIORITY_LOWRT
#define THREAD_PRIORITY_IDLE            THREAD_BASE_PRIORITY_IDLE

#define THREAD_MODE_BACKGROUND_BEGIN    0x00010000
#define THREAD_MODE_BACKGROUND_END      0x00020000

/*
 * PeekMessage() Options
 */
#define PM_NOREMOVE         0x0000
#define PM_REMOVE           0x0001
#define PM_NOYIELD          0x0002

#define ERROR_ALREADY_EXISTS             183L

#ifndef DECLARE_HANDLE
#define DECLARE_HANDLE(name) struct name##__{int unused;}; typedef struct name##__ *name
DECLARE_HANDLE(HINSTANCE);
typedef HINSTANCE HMODULE;      /* HMODULEs can be used in place of HINSTANCEs */
DECLARE_HANDLE(HWND);

typedef HANDLE *PHANDLE;

#ifdef __cplusplus
    #define EXTERN_C       extern "C"
    #define EXTERN_C_START extern "C" {
    #define EXTERN_C_END   }

    #if _MSC_VER >= 1900
        #define WIN_NOEXCEPT noexcept
    #else
        #define WIN_NOEXCEPT throw()
    #endif

    // 'noexcept' on typedefs is invalid prior to C++17
    #if _MSVC_LANG >= 201703
        #define WIN_NOEXCEPT_PFN noexcept
    #else
        #define WIN_NOEXCEPT_PFN
    #endif
#else
    #define EXTERN_C       extern
    #define EXTERN_C_START
    #define EXTERN_C_END
    #define WIN_NOEXCEPT
    #define WIN_NOEXCEPT_PFN
#endif
#endif

#ifndef DECLSPEC_IMPORT
#if (defined(_M_IX86) || defined(_M_IA64) || defined(_M_AMD64) || defined(_M_ARM) || defined(_M_ARM64)) && !defined(MIDL_PASS)
#define DECLSPEC_IMPORT __declspec(dllimport)
#else
#define DECLSPEC_IMPORT
#endif
#endif

#ifndef _MAC
typedef HICON HCURSOR;      /* HICONs & HCURSORs are polymorphic */
#else
DECLARE_HANDLE(HCURSOR);    /* HICONs & HCURSORs are not polymorphic */
#endif

#define IS_INTRESOURCE(_r) ((((ULONG_PTR)(_r)) >> 16) == 0)
#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCEW(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))
#ifdef UNICODE
#define MAKEINTRESOURCE  MAKEINTRESOURCEW
#else
#define MAKEINTRESOURCE  MAKEINTRESOURCEA
#endif // !UNICODE

/*
 * Standard Cursor IDs
 */
#define IDC_ARROW           MAKEINTRESOURCE(32512)
#define IDC_IBEAM           MAKEINTRESOURCE(32513)
#define IDC_WAIT            MAKEINTRESOURCE(32514)
#define IDC_CROSS           MAKEINTRESOURCE(32515)
#define IDC_UPARROW         MAKEINTRESOURCE(32516)
#define IDC_SIZE            MAKEINTRESOURCE(32640)  /* OBSOLETE: use IDC_SIZEALL */
#define IDC_ICON            MAKEINTRESOURCE(32641)  /* OBSOLETE: use IDC_ARROW */
#define IDC_SIZENWSE        MAKEINTRESOURCE(32642)
#define IDC_SIZENESW        MAKEINTRESOURCE(32643)
#define IDC_SIZEWE          MAKEINTRESOURCE(32644)
#define IDC_SIZENS          MAKEINTRESOURCE(32645)
#define IDC_SIZEALL         MAKEINTRESOURCE(32646)
#define IDC_NO              MAKEINTRESOURCE(32648) /*not in win3.1 */
#if(WINVER >= 0x0500)
#define IDC_HAND            MAKEINTRESOURCE(32649)
#endif /* WINVER >= 0x0500 */
#define IDC_APPSTARTING     MAKEINTRESOURCE(32650) /*not in win3.1 */
#if(WINVER >= 0x0400)
#define IDC_HELP            MAKEINTRESOURCE(32651)
#endif /* WINVER >= 0x0400 */

#if(WINVER >= 0x0606)
#define IDC_PIN            MAKEINTRESOURCE(32671)
#define IDC_PERSON         MAKEINTRESOURCE(32672)
#endif /* WINVER >= 0x0606 */

#define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200
#define FORMAT_MESSAGE_FROM_STRING     0x00000400
#define FORMAT_MESSAGE_FROM_HMODULE    0x00000800
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
#define FORMAT_MESSAGE_ARGUMENT_ARRAY  0x00002000
#define FORMAT_MESSAGE_MAX_WIDTH_MASK  0x000000FF

#define FOREGROUND_BLUE      0x0001 // text color contains blue.
#define FOREGROUND_GREEN     0x0002 // text color contains green.
#define FOREGROUND_RED       0x0004 // text color contains red.
#define FOREGROUND_INTENSITY 0x0008 // text color is intensified.
#define BACKGROUND_BLUE      0x0010 // background color contains blue.
#define BACKGROUND_GREEN     0x0020 // background color contains green.
#define BACKGROUND_RED       0x0040 // background color contains red.
#define BACKGROUND_INTENSITY 0x0080 // background color is intensified.
#define COMMON_LVB_LEADING_BYTE    0x0100 // Leading Byte of DBCS
#define COMMON_LVB_TRAILING_BYTE   0x0200 // Trailing Byte of DBCS
#define COMMON_LVB_GRID_HORIZONTAL 0x0400 // DBCS: Grid attribute: top horizontal.
#define COMMON_LVB_GRID_LVERTICAL  0x0800 // DBCS: Grid attribute: left vertical.
#define COMMON_LVB_GRID_RVERTICAL  0x1000 // DBCS: Grid attribute: right vertical.
#define COMMON_LVB_REVERSE_VIDEO   0x4000 // DBCS: Reverse fore/back ground attribute.
#define COMMON_LVB_UNDERSCORE      0x8000 // DBCS: Underscore.

typedef struct _LIST_ENTRY {
   struct _LIST_ENTRY *Flink;
   struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY, *PRLIST_ENTRY;

typedef struct _RTL_CRITICAL_SECTION_DEBUG {
    WORD   Type;
    WORD   CreatorBackTraceIndex;
    struct _RTL_CRITICAL_SECTION *CriticalSection;
    LIST_ENTRY ProcessLocksList;
    DWORD EntryCount;
    DWORD ContentionCount;
    DWORD Flags;
    WORD   CreatorBackTraceIndexHigh;
    WORD   Identifier;
} RTL_CRITICAL_SECTION_DEBUG, *PRTL_CRITICAL_SECTION_DEBUG, RTL_RESOURCE_DEBUG, *PRTL_RESOURCE_DEBUG;

//
// These flags define the upper byte of the critical section SpinCount field
//
#define RTL_CRITICAL_SECTION_FLAG_NO_DEBUG_INFO         0x01000000
#define RTL_CRITICAL_SECTION_FLAG_DYNAMIC_SPIN          0x02000000
#define RTL_CRITICAL_SECTION_FLAG_STATIC_INIT           0x04000000
#define RTL_CRITICAL_SECTION_FLAG_RESOURCE_TYPE         0x08000000
#define RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO      0x10000000
#define RTL_CRITICAL_SECTION_ALL_FLAG_BITS              0xFF000000
#define RTL_CRITICAL_SECTION_FLAG_RESERVED              (RTL_CRITICAL_SECTION_ALL_FLAG_BITS & (~(RTL_CRITICAL_SECTION_FLAG_NO_DEBUG_INFO | RTL_CRITICAL_SECTION_FLAG_DYNAMIC_SPIN | RTL_CRITICAL_SECTION_FLAG_STATIC_INIT | RTL_CRITICAL_SECTION_FLAG_RESOURCE_TYPE | RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO)))

//
// These flags define possible values stored in the Flags field of a critsec debuginfo.
//
#define RTL_CRITICAL_SECTION_DEBUG_FLAG_STATIC_INIT     0x00000001

#pragma pack(push, 8)

typedef struct _RTL_CRITICAL_SECTION {
    PRTL_CRITICAL_SECTION_DEBUG DebugInfo;

    //
    //  The following three fields control entering and exiting the critical
    //  section for the resource
    //

    LONG LockCount;
    LONG RecursionCount;
    HANDLE OwningThread;        // from the thread's ClientId->UniqueThread
    HANDLE LockSemaphore;
    ULONG_PTR SpinCount;        // force size on 64-bit systems when packed
} RTL_CRITICAL_SECTION, *PRTL_CRITICAL_SECTION;

#pragma pack(pop)

typedef RTL_CRITICAL_SECTION CRITICAL_SECTION;
typedef PRTL_CRITICAL_SECTION PCRITICAL_SECTION;
typedef PRTL_CRITICAL_SECTION LPCRITICAL_SECTION;

typedef struct tagPOINT
{
    LONG  x;
    LONG  y;
} POINT, *PPOINT, near *NPPOINT, far *LPPOINT;

typedef struct tagMSG {
    HWND        hwnd;
    UINT        message;
    WPARAM      wParam;
    LPARAM      lParam;
    DWORD       time;
    POINT       pt;
#ifdef _MAC
    DWORD       lPrivate;
#endif
} MSG, *PMSG, *NPMSG, *LPMSG;


#define MAKELANGID(p, s)       ((((WORD  )(s)) << 10) | (WORD  )(p))
#define PRIMARYLANGID(lgid)    ((WORD  )(lgid) & 0x3ff)
#define SUBLANGID(lgid)        ((WORD  )(lgid) >> 10)



#ifndef NULL
#define NULL 0
#endif

typedef unsigned short wchar_t;

// shell32 APIs that are also exported from shfolder
#ifndef SHFOLDERAPI
#if defined(_SHFOLDER_) || defined(_SHELL32_)
#define SHFOLDERAPI           STDAPI
#else
#define SHFOLDERAPI           EXTERN_C DECLSPEC_IMPORT HRESULT __stdcall
#endif
#endif

SHFOLDERAPI SHGetFolderPathA(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR  pszPath);
SHFOLDERAPI SHGetFolderPathW(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPWSTR pszPath);

void GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime);
DWORD GetCurrentProcessId(void);
DWORD GetCurrentThreadId(void);
ULONGLONG GetTickCount64(void);

BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);
DWORD GetLastError(void);

DWORD FormatMessageA(
    DWORD   dwFlags,
    LPCVOID lpSource,
    DWORD   dwMessageId,
    DWORD   dwLanguageId,
    LPTSTR  lpBuffer,
    DWORD   nSize,
    va_list *Arguments
);

DWORD FormatMessageW(
    DWORD   dwFlags,
    LPCVOID lpSource,
    DWORD   dwMessageId,
    DWORD   dwLanguageId,
    LPTSTR  lpBuffer,
    DWORD   nSize,
    va_list *Arguments
);

void InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount);
LPWSTR* CommandLineToArgvW(LPCWSTR lpCmdLine, int* pNumArgs); 
LPWSTR GetCommandLineW(void);
HLOCAL LocalFree(HLOCAL hMem);
HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
BOOL ReleaseMutex(HANDLE hMutex);
BOOL CloseHandle(HANDLE hObject);
DWORD WINAPI GetConsoleProcessList(LPDWORD lpdwProcessList, DWORD dwProcessCount);
void ExitProcess(UINT uExitCode);
BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);

#ifdef UNICODE
    #define PeekMessage          PeekMessageW

    #define GetModuleHandle      GetModuleHandleW
    #define GetModuleFileNameEx  K32GetModuleFileNameExW
    #define GetModuleFileName    GetModuleFileNameW
    #define DispatchMessage      DispatchMessageW

    #define OutputDebugString    OutputDebugStringW
    #define WriteConsole         WriteConsoleW

    #define LoadCursor           LoadCursorW
    
    #define GetUserName          GetUserNameW
    #define SHGetFolderPath      SHGetFolderPathW

    #define FormatMessage        FormatMessageW

    #define GetCurrentDirectory  GetCurrentDirectoryW
    #define GetEnvironmentVariable GetEnvironmentVariableW

    #define UuidToString         UuidToStringW
    #define UuidFromString       UuidFromStringW

    #define RpcStringFree        RpcStringFreeW

    #define GetFileAttributes    GetFileAttributesW

    #define PathFileExists       PathFileExistsW

    #define CreateDirectory      CreateDirectoryW
    #define CreateFile           CreateFileW
    #define CreateFileMapping    CreateFileMappingW
    #define DeleteFile           DeleteFileW

    #define GetFinalPathNameByHandle GetFinalPathNameByHandleW

    #define PathCanonicalize     PathCanonicalizeW
    #define FindFirstFile        FindFirstFileW
    #define FindNextFile         FindNextFileW

    #define RemoveDirectory      RemoveDirectoryW

    #define SetFileAttributes    SetFileAttributesW

    #define CopyFileEx           CopyFileExW
    #define MoveFileEx           MoveFileExW

    #define PathCommonPrefix     PathCommonPrefixW

    #define CreateProcess        CreateProcessW
    #define SearchPath           SearchPathW

#else
    #define PeekMessage          PeekMessageA

    #define GetModuleHandle      GetModuleHandleA
    #define GetModuleFileNameEx  K32GetModuleFileNameExA
    #define GetModuleFileName    GetModuleFileNameA
    #define DispatchMessage      DispatchMessageA

    #define OutputDebugString    OutputDebugStringA
    #define WriteConsole         WriteConsoleA

    #define LoadCursor           LoadCursorA

    #define GetUserName          GetUserNameA
    #define SHGetFolderPath      SHGetFolderPathA

    #define FormatMessage        FormatMessageA

    #define GetCurrentDirectory  GetCurrentDirectoryA
    #define GetEnvironmentVariable GetEnvironmentVariableA

    #define UuidToString         UuidToStringA
    
    #define RpcStringFree        RpcStringFreeA
    #define UuidFromString       UuidFromStringA

    #define GetFileAttributes    GetFileAttributesA

    #define PathFileExists       PathFileExistsA

    #define CreateDirectory      CreateDirectoryA
    #define CreateFile           CreateFileA
    #define CreateFileMapping    CreateFileMappingA
    #define DeleteFile           DeleteFileA

    #define GetFinalPathNameByHandle GetFinalPathNameByHandleA

    #define PathCanonicalize     PathCanonicalizeA
    #define FindFirstFile        FindFirstFileA
    #define FindNextFile         FindNextFileA

    #define RemoveDirectory      RemoveDirectoryA

    #define SetFileAttributes    SetFileAttributesA

    #define CopyFileEx           CopyFileExA
    #define MoveFileEx           MoveFileExA

    #define PathCommonPrefix     PathCommonPrefixA

    #define CreateProcess        CreateProcessA
    #define SearchPath           SearchPathA

#endif

#define EnumProcesses         K32EnumProcesses
#define CaptureStackBackTrace RtlCaptureStackBackTrace

BOOL PeekMessageA(
    LPMSG lpMsg,
    HWND  hWnd,
    UINT  wMsgFilterMin,
    UINT  wMsgFilterMax,
    UINT  wRemoveMsg
);

BOOL PeekMessageW(
    LPMSG lpMsg,
    HWND  hWnd,
    UINT  wMsgFilterMin,
    UINT  wMsgFilterMax,
    UINT  wRemoveMsg
);

BOOL TranslateMessage(const MSG *lpMsg);
LRESULT WINAPI DispatchMessageA(const MSG *lpMsg);
LRESULT WINAPI DispatchMessageW(const MSG *lpMsg);

HANDLE GetProcessHeap(void);
LPVOID HeapAlloc(HANDLE hHeap, DWORD  dwFlags, SIZE_T dwBytes);
LPVOID HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes);
BOOL HeapFree(HANDLE hHeap, DWORD  dwFlags, LPVOID lpMem);
void RtlZeroMemory(PVOID Destination, SIZE_T Length);
void RtlCopyMemory(PVOID Destination, const void* Source, SIZE_T Length);
void RtlMoveMemory(PVOID Destination, const void* Source, SIZE_T Length);
void RtlFillMemory(PVOID Destination, SIZE_T Length, BYTE Fill);
HANDLE WINAPI GetStdHandle(DWORD nStdHandle);

BOOL WINAPI SetConsoleTextAttribute(HANDLE hConsoleOutput, WORD wAttributes);
void OutputDebugStringA(LPCSTR lpOutputString);
void OutputDebugStringW(LPCWSTR lpOutputString);
BOOL WINAPI WriteConsoleA(
    HANDLE hConsoleOutput,
    const void* lpBuffer,
    DWORD nNumberOfCharsToWrite,
    LPDWORD lpNumberOfCharsWritten,
    LPVOID lpReserved
);

BOOL WINAPI WriteConsoleW(
    HANDLE hConsoleOutput,
    const void* lpBuffer,
    DWORD nNumberOfCharsToWrite,
    LPDWORD lpNumberOfCharsWritten,
    LPVOID lpReserved
);

void GetLocalTime(LPSYSTEMTIME lpSystemTime);

HCURSOR SetCursor(HCURSOR hCursor);
HCURSOR LoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);
HCURSOR LoadCursorW(HINSTANCE hInstance, LPCWSTR lpCursorName);
BOOL GetCursorPos(LPPOINT lpPoint);

BOOL GetUserNameA(LPSTR lpBuffer, LPDWORD pcbBuffer);
BOOL GetUserNameW(LPWSTR lpBuffer, LPDWORD pcbBuffer);

SHFOLDERAPI SHGetFolderPathA(
    HWND   hwnd,
    int    csidl,
    HANDLE hToken,
    DWORD  dwFlags,
    LPSTR  pszPath
);

SHFOLDERAPI SHGetFolderPathW(
    HWND   hwnd,
    int    csidl,
    HANDLE hToken,
    DWORD  dwFlags,
    LPWSTR  pszPath
);

DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);

DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPTSTR lpBuffer);
DWORD GetCurrentDirectoryW(DWORD nBufferLength, LPTSTR lpBuffer);

DWORD GetEnvironmentVariableA(LPCTSTR lpName, LPTSTR lpBuffer, DWORD nSize);
DWORD GetEnvironmentVariableW(LPCTSTR lpName, LPTSTR lpBuffer, DWORD nSize);

void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
void DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

HANDLE GetCurrentProcess(void);

#define IMAGEAPI __stdcall

BOOL IMAGEAPI SymInitialize(HANDLE hProcess, PCSTR  UserSearchPath, BOOL   fInvadeProcess);

USHORT WINAPI RtlCaptureStackBackTrace(
    ULONG  FramesToSkip,
    ULONG  FramesToCapture,
    PVOID  *BackTrace,
    PULONG BackTraceHash
);

BOOL IMAGEAPI SymFromAddr(
    HANDLE       hProcess,
    DWORD64      Address,
    PDWORD64     Displacement,
    PSYMBOL_INFO Symbol
);

void GetSystemInfo(LPSYSTEM_INFO lpSystemInfo);
HRESULT GetThreadDescription(HANDLE hThread, PWSTR* ppszThreadDescription);

RPC_STATUS UuidCreate(UUID *Uuid);

RPC_STATUS UuidToStringA(const UUID *Uuid, RPC_CSTR* StringUuid);
RPC_STATUS UuidToStringW(const UUID *Uuid, RPC_WSTR* StringUuid);

RPC_STATUS RpcStringFreeA(RPC_CSTR *String);
RPC_STATUS RpcStringFreeW(RPC_WSTR *String);

RPC_STATUS UuidFromStringA(RPC_CSTR StringUuid, UUID *Uuid);
RPC_STATUS UuidFromStringW(RPC_WSTR StringUuid, UUID *Uuid);

BOOL CreateDirectoryA(LPCTSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
BOOL CreateDirectoryW(LPCTSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);

HANDLE CreateFile(
    LPCSTR                lpFileName,
    DWORD                 dwDesiredAccess,
    DWORD                 dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD                 dwCreationDisposition,
    DWORD                 dwFlagsAndAttributes,
    HANDLE                hTemplateFile
);

BOOL DeleteFile(LPCTSTR lpFileName);

HANDLE CreateFileMapping(
    HANDLE                hFile,
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    DWORD                 flProtect,
    DWORD                 dwMaximumSizeHigh,
    DWORD                 dwMaximumSizeLow,
    LPCSTR                lpName
);

BOOL GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize);

LPVOID MapViewOfFile(
    HANDLE hFileMappingObject,
    DWORD  dwDesiredAccess,
    DWORD  dwFileOffsetHigh,
    DWORD  dwFileOffsetLow,
    SIZE_T dwNumberOfBytesToMap
);

DWORD SetFilePointer(
    HANDLE hFile,
    LONG   lDistanceToMove,
    PLONG  lpDistanceToMoveHigh,
    DWORD  dwMoveMethod
);

BOOL GetFileTime(
    HANDLE     hFile,
    LPFILETIME lpCreationTime,
    LPFILETIME lpLastAccessTime,
    LPFILETIME lpLastWriteTime
);

BOOL ReadFile(
    HANDLE       hFile,
    LPVOID       lpBuffer,
    DWORD        nNumberOfBytesToRead,
    LPDWORD      lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
);

BOOL WriteFile(
    HANDLE       hFile,
    LPCVOID      lpBuffer,
    DWORD        nNumberOfBytesToWrite,
    LPDWORD      lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped
);

BOOL PathFileExistsA(LPCSTR pszPath);
BOOL PathFileExistsW(LPWSTR pszPath);

DWORD GetFileAttributesA(LPCSTR lpFileName);
DWORD GetFileAttributesW(LPCWSTR lpFileName);

DWORD GetFinalPathNameByHandleA(
    HANDLE hFile,
    LPSTR  lpszFilePath,
    DWORD  cchFilePath,
    DWORD  dwFlags
);

DWORD GetFinalPathNameByHandleW(
    HANDLE hFile,
    LPWSTR  lpszFilePath,
    DWORD  cchFilePath,
    DWORD  dwFlags
);

BOOL PathCanonicalizeA(LPSTR pszBuf, LPCSTR pszPath);
BOOL PathCanonicalizeW(LPWSTR pszBuf, LPCWSTR pszPath);

HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
HANDLE FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);

BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
BOOL FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);

BOOL FindClose(HANDLE hFindFile);

BOOL RemoveDirectoryA(LPCSTR lpPathName);
BOOL RemoveDirectoryW(LPCWSTR lpPathName);

BOOL SetFileAttributesA(LPCSTR lpFileName, DWORD dwFileAttributes);
BOOL SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes);

BOOL CopyFileExA(
    LPCSTR             lpExistingFileName,
    LPCSTR             lpNewFileName,
    LPPROGRESS_ROUTINE lpProgressRoutine,
    LPVOID             lpData,
    LPBOOL             pbCancel,
    DWORD              dwCopyFlags
);

BOOL CopyFileExW(
    LPCSTR             lpExistingFileName,
    LPCSTR             lpNewFileName,
    LPPROGRESS_ROUTINE lpProgressRoutine,
    LPVOID             lpData,
    LPBOOL             pbCancel,
    DWORD              dwCopyFlags
);

BOOL MoveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD  dwFlags);
BOOL MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD  dwFlags);

int PathCommonPrefixA(LPCSTR pszFile1, LPCSTR pszFile2, LPSTR  achPath);
int PathCommonPrefixW(LPCWSTR pszFile1, LPCWSTR pszFile2, LPWSTR  achPath);

HANDLE CreateThread(
    LPSECURITY_ATTRIBUTES   lpThreadAttributes,
    SIZE_T                  dwStackSize,
    LPTHREAD_START_ROUTINE  lpStartAddress,
    LPVOID                  lpParameter,
    DWORD                   dwCreationFlags,
    LPDWORD                 lpThreadId
);

BOOL SetThreadPriority(HANDLE hThread, int nPriority);
HRESULT SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription);
DWORD ResumeThread(HANDLE hThread);

BOOL CreateProcess(
    LPCSTR                lpApplicationName,
    LPSTR                 lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL                  bInheritHandles,
    DWORD                 dwCreationFlags,
    LPVOID                lpEnvironment,
    LPCSTR                lpCurrentDirectory,
    LPSTARTUPINFOA        lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation
);

BOOL CreateProcessW(
    LPCWSTR               lpApplicationName,
    LPWSTR                lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL                  bInheritHandles,
    DWORD                 dwCreationFlags,
    LPVOID                lpEnvironment,
    LPCWSTR               lpCurrentDirectory,
    LPSTARTUPINFOA        lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation
);

BOOL SetPriorityClass(HANDLE hProcess, DWORD dwPriorityClass);

BOOL CreatePipe(
    PHANDLE               hReadPipe,
    PHANDLE               hWritePipe,
    LPSECURITY_ATTRIBUTES lpPipeAttributes,
    DWORD                 nSize
);

BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags);

BOOL TerminateProcess( HANDLE hProcess, UINT uExitCode);

DWORD SearchPathA(
    LPCSTR lpPath,
    LPCSTR lpFileName,
    LPCSTR lpExtension,
    DWORD  nBufferLength,
    LPSTR  lpBuffer,
    LPSTR  *lpFilePart
);

DWORD SearchPathW(
    LPCWSTR lpPath,
    LPCWSTR lpFileName,
    LPCWSTR lpExtension,
    DWORD  nBufferLength,
    LPWSTR  lpBuffer,
    LPWSTR  *lpFilePart
);

BOOL GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode);
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DWORD WaitForMultipleObjects(
    DWORD        nCount,
    const HANDLE *lpHandles,
    BOOL         bWaitAll,
    DWORD        dwMilliseconds
);


BOOL K32EnumProcesses(DWORD* lpidProcess, DWORD cb, LPDWORD lpcbNeeded);

HANDLE OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);

DWORD WINAPI K32GetModuleFileNameExA(
    HANDLE  hProcess,
    HMODULE hModule,
    LPSTR   lpFilename,
    DWORD   nSize
);

DWORD WINAPI K32GetModuleFileNameExW(
    HANDLE  hProcess,
    HMODULE hModule,
    LPWSTR  lpFilename,
    DWORD   nSize
);

HMODULE GetModuleHandleA(LPCSTR lpModuleName);
HMODULE GetModuleHandleW(LPCWSTR lpModuleName);

HANDLE GetCurrentThread(void);

BOOL WINAPI GetConsoleScreenBufferInfo(
    HANDLE                      hConsoleOutput,
    PCONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo
);

DWORD GetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation);

SHORT GetAsyncKeyState(int vKey); 
