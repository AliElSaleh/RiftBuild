// Copyright (c) Artisan Softworks
// Licensed under the BSD 3-Clause License. See the LICENSE file for details.

#include "MicrosoftCraziness.h"

#if PLATFORM_WINDOWS

#ifndef UNITY_BUILD
#include "Core/Win32Types.h"
#include "Core/StringUtils.h"
#include "Core/Filesystem.h"
#include "Core/Allocators.h"
#endif

#define COBJMACROS

// -------------------- COM interfaces (ISetupConfiguration) --------------------
// These are the minimal declarations so we don't need setup.configuration.h

// My god... what were they thinking!??!??

#ifndef __ISetupInstance_INTERFACE_DEFINED__
#define __ISetupInstance_INTERFACE_DEFINED__

// {B41463C3-8866-43B5-BC33-2B0676F7F42E}
DEFINE_GUID(IID_ISetupInstance, 0xB41463C3,0x8866,0x43B5,0xBC,0x33,0x2B,0x06,0x76,0xF7,0xF4,0x2E);

typedef struct ISetupInstance ISetupInstance;

typedef struct ISetupInstanceVtbl
{
    // IUnknown
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(ISetupInstance*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(ISetupInstance*);
    ULONG   (STDMETHODCALLTYPE* Release)(ISetupInstance*);

    // ISetupInstance
    HRESULT (STDMETHODCALLTYPE* GetInstanceId)(ISetupInstance*, WCHAR** pwcharInstanceId);
    HRESULT (STDMETHODCALLTYPE* GetInstallDate)(ISetupInstance*, LPFILETIME pInstallDate);
    HRESULT (STDMETHODCALLTYPE* GetInstallationName)(ISetupInstance*, WCHAR** pwcharInstallationName);
    HRESULT (STDMETHODCALLTYPE* GetInstallationPath)(ISetupInstance*, WCHAR** pwcharInstallationPath);
    HRESULT (STDMETHODCALLTYPE* GetInstallationVersion)(ISetupInstance*, WCHAR** pwcharInstallationVersion);
    HRESULT (STDMETHODCALLTYPE* GetDisplayName)(ISetupInstance*, LCID lcid, WCHAR** pwcharDisplayName);
    HRESULT (STDMETHODCALLTYPE* GetDescription)(ISetupInstance*, LCID lcid, WCHAR** pwcharDescription);
    HRESULT (STDMETHODCALLTYPE* ResolvePath)(ISetupInstance*, const WCHAR* pwszRelativePath, WCHAR** pwcharAbsolutePath);
} ISetupInstanceVtbl;

struct ISetupInstance
{
    ISetupInstanceVtbl* lpVtbl;
};

#endif

#ifndef __IEnumSetupInstances_INTERFACE_DEFINED__
#define __IEnumSetupInstances_INTERFACE_DEFINED__

// {6380BCFF-41D3-4B2E-8B2E-BF8A6810C848}
DEFINE_GUID(IID_IEnumSetupInstances, 0x6380BCFF,0x41D3,0x4B2E,0x8B,0x2E,0xBF,0x8A,0x68,0x10,0xC8,0x48);

typedef struct IEnumSetupInstances IEnumSetupInstances;

typedef struct IEnumSetupInstancesVtbl
{
    // IUnknown
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IEnumSetupInstances*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(IEnumSetupInstances*);
    ULONG   (STDMETHODCALLTYPE* Release)(IEnumSetupInstances*);

    // IEnumSetupInstances
    HRESULT (STDMETHODCALLTYPE* Next)(IEnumSetupInstances*, ULONG celt, ISetupInstance** rgelt, ULONG* pceltFetched);
    HRESULT (STDMETHODCALLTYPE* Skip)(IEnumSetupInstances*, ULONG celt);
    HRESULT (STDMETHODCALLTYPE* Reset)(IEnumSetupInstances*);
    HRESULT (STDMETHODCALLTYPE* Clone)(IEnumSetupInstances*, IEnumSetupInstances** ppenum);
} IEnumSetupInstancesVtbl;

struct IEnumSetupInstances
{
    IEnumSetupInstancesVtbl* lpVtbl;
};

#endif

#ifndef __ISetupConfiguration_INTERFACE_DEFINED__
#define __ISetupConfiguration_INTERFACE_DEFINED__

// {42843719-DB4C-46C2-8E7C-64F1816EFD5B}
DEFINE_GUID(IID_ISetupConfiguration, 0x42843719,0xDB4C,0x46C2,0x8E,0x7C,0x64,0xF1,0x81,0x6E,0xFD,0x5B);

// {177F0C4A-1CD3-4DE7-A32C-71DBBB9FA36D}
DEFINE_GUID(CLSID_SetupConfiguration, 0x177F0C4A,0x1CD3,0x4DE7,0xA3,0x2C,0x71,0xDB,0xBB,0x9F,0xA3,0x6D);

typedef struct ISetupConfiguration ISetupConfiguration;

typedef struct ISetupConfigurationVtbl
{
    // IUnknown
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(ISetupConfiguration*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(ISetupConfiguration*);
    ULONG   (STDMETHODCALLTYPE* Release)(ISetupConfiguration*);

    // ISetupConfiguration
    HRESULT (STDMETHODCALLTYPE* EnumInstances)(ISetupConfiguration*, IEnumSetupInstances** ppEnumInstances);
    HRESULT (STDMETHODCALLTYPE* GetInstanceForCurrentProcess)(ISetupConfiguration*, ISetupInstance** ppInstance);
    HRESULT (STDMETHODCALLTYPE* GetInstanceForPath)(ISetupConfiguration*, LPCWSTR wzPath, ISetupInstance** ppInstance);
} ISetupConfigurationVtbl;

struct ISetupConfiguration
{
    ISetupConfigurationVtbl* lpVtbl;
};

#endif


// -------------------- Visual Studio discovery (COM + ) --------------------

static bool FindVisualStudioViaCOM(LinearAllocator* Arena, MicrosoftVisualStudioPaths* Result)
{
    xx CoInitializeEx(NULL, COINIT_MULTITHREADED);

    ISetupConfiguration* Config = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_SetupConfiguration, NULL, CLSCTX_INPROC_SERVER, &IID_ISetupConfiguration, (void**)&Config);
    if (FAILED(hr) || !Config)
    {
        return false;
    }

    IEnumSetupInstances* EnumSetupInstance = NULL;
    hr = Config->lpVtbl->EnumInstances(Config, &EnumSetupInstance);
    if (FAILED(hr) || !EnumSetupInstance)
    {
        Config->lpVtbl->Release(Config);
        return false;
    }

    bool bFound = false;
    
    while (1)
    {
        ULONG Fetched = 0;
        ISetupInstance* Inst = NULL;
        hr = EnumSetupInstance->lpVtbl->Next(EnumSetupInstance, 1, &Inst, &Fetched);
        if (hr != S_OK || !Inst)
        {
            break;
        }

        WCHAR* bpath = {0};
        hr = Inst->lpVtbl->GetInstallationPath(Inst, &bpath);
        if (SUCCEEDED(hr) && bpath)
        {
            String16 InstallPath_Wide = CStr16(bpath);
            StringLocal(InstallPath, MAX_PATH_LENGTH);
            String_ToNarrow(InstallPath_Wide, &InstallPath);

            StringLocal(ToolsTextPath, MAX_PATH_LENGTH);
            String_Append(&ToolsTextPath, InstallPath);
            String_Append(&ToolsTextPath, S("\\VC\\Auxiliary\\Build\\Microsoft.VCToolsVersion.default.txt"));

            FileHandle ToolsFileHandle = FileHandle_Null();
            if (Filesystem_Open(ToolsTextPath, FileMode_Read, &ToolsFileHandle))
            {
                StringLocal(Line, 128);
                xx Filesystem_ReadLine(ToolsFileHandle, &Line);
                if (Line.Length)
                {
                    String ToolsMSVCPath = S("\\VC\\Tools\\MSVC\\");

                    StringLocal(ToolBasePath, MAX_PATH_LENGTH);
                    String_Append(&ToolBasePath, InstallPath);
                    String_Append(&ToolBasePath, ToolsMSVCPath);
                    String_Append(&ToolBasePath, Line);

                    StringLocal(ToolIncludePath, MAX_PATH_LENGTH);
                    String_Append(&ToolIncludePath, InstallPath);
                    String_Append(&ToolIncludePath, ToolsMSVCPath);
                    String_Append(&ToolIncludePath, Line);
                    String_Append(&ToolIncludePath, S("\\include"));

                    StringLocal(ToolLibPath, MAX_PATH_LENGTH);
                    String_Append(&ToolLibPath, InstallPath);
                    String_Append(&ToolLibPath, ToolsMSVCPath);
                    String_Append(&ToolLibPath, Line);
                    String_Append(&ToolLibPath, S("\\lib\\x64"));

                    StringLocal(ToolExePath, MAX_PATH_LENGTH);
                    String_Append(&ToolExePath, InstallPath);
                    String_Append(&ToolExePath, ToolsMSVCPath);
                    String_Append(&ToolExePath, Line);
                    String_Append(&ToolExePath, S("\\bin\\Hostx64\\x64"));

                    if (Filesystem_DoesDirectoryExist(ToolLibPath))
                    {
                        Result->InstallPath   = String_Create(Arena, InstallPath);
                        Result->ToolBasePath  = String_Create(Arena, ToolBasePath);
                        Result->ExePath       = String_Create(Arena, ToolExePath);
                        Result->LibraryPath   = String_Create(Arena, ToolLibPath);
                        Result->IncludePath   = String_Create(Arena, ToolIncludePath);

                        bFound = true;

                        // i dont care man... dont wanna link this shit
                        // SysFreeString(bpath);

                        Inst->lpVtbl->Release(Inst);
                        break;
                    }
                }

                Filesystem_Close(&ToolsFileHandle);
            }
        }

        Inst->lpVtbl->Release(Inst);
    }

    EnumSetupInstance->lpVtbl->Release(EnumSetupInstance);
    Config->lpVtbl->Release(Config);
    CoUninitialize();
    return bFound;
}

static bool FindVisualStudioViaRegistry(LinearAllocator* Arena, MicrosoftVisualStudioPaths* Result)
{
    bool bFound = false;

    HKEY Key = NULL;
    LSTATUS Status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7", 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &Key);
    if (Status != ERROR_SUCCESS)
    {
        return false;
    }

    // Try versions in descending order
    const char* vers[] = { "17.0", "16.0", "15.0", "14.0", "12.0", "11.0", "10.0" };
    for EachElement(i, vers)
    {
        DWORD Length = 511;
        StringLocal(InstallPath, 512);

        Status = RegQueryValueEx(Key, vers[i], NULL, NULL, (LPBYTE)InstallPath.Data, &Length);

        bool bSuccess = Status == S_OK && Length > 0;
        if (bSuccess)
        {
            InstallPath.Length = Length-1;

            StringLocal(ToolBasePath, MAX_PATH_LENGTH);
            String_Append(&ToolBasePath, InstallPath);
            String_Append(&ToolBasePath, S("VC"));

            StringLocal(ToolExePath, MAX_PATH_LENGTH);
            String_Append(&ToolExePath, InstallPath);
            String_Append(&ToolExePath, S("VC\\bin\\amd64"));

            StringLocal(ToolLibPath, MAX_PATH_LENGTH);
            String_Append(&ToolLibPath, InstallPath);
            String_Append(&ToolLibPath, S("VC\\lib\\amd64"));

            StringLocal(ToolIncludePath, MAX_PATH_LENGTH);
            String_Append(&ToolIncludePath, InstallPath);
            String_Append(&ToolIncludePath, S("VC\\include"));
        
            if (Filesystem_DoesDirectoryExist(ToolLibPath))
            {
                Result->InstallPath   = String_Create(Arena, InstallPath);
                Result->ToolBasePath  = String_Create(Arena, ToolBasePath);
                Result->ExePath       = String_Create(Arena, ToolExePath);
                Result->LibraryPath   = String_Create(Arena, ToolLibPath);
                Result->IncludePath   = String_Create(Arena, ToolIncludePath);

                bFound = true;
                break;
            }
        }
    }

    xx RegCloseKey(Key);
    return bFound;
}

bool FindVisualStudio(LinearAllocator* Arena, MicrosoftVisualStudioPaths* Result)
{
    bool bSuccess = FindVisualStudioViaCOM(Arena, Result);

    // fallback to searching the registry if we didn't find anything using COM
    if (!bSuccess)
    {
        bSuccess = FindVisualStudioViaRegistry(Arena, Result);
    }

    return bSuccess;
}

static bool WinKitVerBest(const String FullPath, const String RelativePath, const String FileName, u64 FileSize, bool bIsDirectory, void* UserData)
{
    if (bIsDirectory)
    {
        String* VersionData = UserData;

        ECompareResult Comparison = String_CompareVersion(FileName, *VersionData);
        if (!String_IsValid(*VersionData) || Comparison == CompareResult_Greater || Comparison == CompareResult_None)
        {
            String_Copy(VersionData, FileName);
        }
    }

    return true;
}


static bool FindWindowsSDKViaRegistry(LinearAllocator* Arena, MicrosoftWindowsSDKPaths* Result)
{
    HKEY Key = NULL;
    LSTATUS Status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots", 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY | KEY_ENUMERATE_SUB_KEYS, &Key);
    if (Status != ERROR_SUCCESS)
    {
        return false;
    }

    // Find the Windows 10 Kit first, then 8.1
    const char* KitsRootNames[2] = { "KitsRoot10", "KitsRoot81" };
    for (u8 i = 0; i < SArray_Capacity(KitsRootNames); i++)
    {
        DWORD Length = 511;
        StringLocal(InstallPath, 512);

        Status = RegQueryValueEx(Key, KitsRootNames[i], NULL, NULL, (LPBYTE)InstallPath.Data, &Length);

        bool bSuccess = Status == S_OK && Length > 0;
        if (bSuccess)
        {
            InstallPath.Length = Length-1;

            // bin: pick best version directory
            StringLocal(KitsBinPath, MAX_PATH_LENGTH);
            {
                String_Append(&KitsBinPath, InstallPath);
                String_Append(&KitsBinPath, S("bin"));

                StringLocal(BestVersionName, 32);
                Filesystem_IterateDirectory_Ex(KitsBinPath, WinKitVerBest, false, &BestVersionName);
                String_Append(&KitsBinPath, S("\\"));
                String_Append(&KitsBinPath, BestVersionName);
            }

            // include: pick best version directory
            StringLocal(KitsIncludePath, MAX_PATH_LENGTH);
            {
                String_Append(&KitsIncludePath, InstallPath);
                String_Append(&KitsIncludePath, S("Include"));

                StringLocal(BestVersionName, 32);
                Filesystem_IterateDirectory_Ex(KitsIncludePath, WinKitVerBest, false, &BestVersionName);
                String_Append(&KitsIncludePath, S("\\"));
                String_Append(&KitsIncludePath, BestVersionName);
            }

            // Lib: pick best version directory
            StringLocal(KitsLibPath, MAX_PATH_LENGTH);
            StringLocal(KitsLibUcrtPath, MAX_PATH_LENGTH);
            StringLocal(KitsLibUmPath, MAX_PATH_LENGTH);
            {
                String_Append(&KitsLibPath, InstallPath);
                String_Append(&KitsLibPath, S("Lib"));

                StringLocal(BestVersionName, 32);
                Filesystem_IterateDirectory_Ex(KitsLibPath, WinKitVerBest, false, &BestVersionName);
                String_Append(&KitsLibPath, S("\\"));
                String_Append(&KitsLibPath, BestVersionName);

                String_Append(&KitsLibUcrtPath, KitsLibPath);
                String_Append(&KitsLibUcrtPath, S("\\ucrt\\x64"));

                String_Append(&KitsLibUmPath, KitsLibPath);
                String_Append(&KitsLibUmPath, S("\\um\\x64"));
            }

            if (i == 0) { Result->Version = 10; }
            if (i == 1) { Result->Version = 8; }

            Result->RootPath         = String_Create(Arena, InstallPath);
            Result->BinPath          = String_Create(Arena, KitsBinPath);
            Result->IncludePath      = String_Create(Arena, KitsIncludePath);
            Result->UM_LibraryPath   = String_Create(Arena, KitsLibUmPath);
            Result->UCRT_LibraryPath = String_Create(Arena, KitsLibUcrtPath);

            break;
        }
    }

    xx RegCloseKey(Key);

    return Result->Version != 0;
}

bool FindWindowsSDK(LinearAllocator* Arena, MicrosoftWindowsSDKPaths* Result)
{
    bool bSuccess = FindWindowsSDKViaRegistry(Arena, Result);

    return bSuccess;
}

#endif
