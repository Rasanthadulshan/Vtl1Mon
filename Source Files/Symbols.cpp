/*++
* Copyright (c) Connor McGarr. All rights reserved.
*
* @file:      Vtl1Mon/Symbols.cpp
*
* @summary:   Symbol-related functionality.
*
* @author:    Connor McGarr (@33y0re)
*
* @copyright  Use of this source code is governed by a MIT-style license that
*             can be found in the LICENSE file.
*
--*/
#include "Symbols.hpp"
#include "Helpers.hpp"
#include <unordered_map>
#include <Shlwapi.h>
#include <string>

//
// We need to maintain the NT base for our nt!_SKSERVICE enum symbol.
//
static bool k_NtFound = false;
static ULONG_PTR k_NtBase = 0;

//
// A mapping of all valid secure call numbers -> names (strings)
//
static std::unordered_map<ULONG, wchar_t*> k_SecureCallValues;

//
// Functionality for symbols
//
SymGetOptions_T SymGetOptions_I = NULL;
SymInitializeW_T SymInitializeW_I = NULL;
SymSetOptions_T SymSetOptions_I = NULL;
SymCleanup_T SymCleanup_I = NULL;
SymSetSearchPathW_T SymSetSearchPathW_I = NULL;
SymLoadModuleExW_T SymLoadModuleExW_I = NULL;
SymFromAddrW_T SymFromAddrW_I = NULL;
SymGetTypeFromNameW_T SymGetTypeFromNameW_I = NULL;
SymGetTypeInfo_T SymGetTypeInfo_I = NULL;

/**
*
* @brief        Initializes all symbol-related functionality.
* @return       true on success, otherwise false.
*
*/
bool
InitializeSymbols ()
{
    bool result;
    HMODULE dbgHelp;
    DWORD symOptions;
    BOOL res;
    ULONG size;
    ULONG neededSize;
    std::wstring symbolPath;
    wchar_t* currentDirectory;

    result = false;
    symOptions = 0;
    res = FALSE;
    size = 0;
    neededSize = 0;
    currentDirectory = NULL;

    neededSize = GetCurrentDirectoryW(0, NULL);
    if (neededSize == 0)
    {
        wprintf(L"[-] Error! GetCurrentDirectoryW failed in InitializeSymbols. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    size = (neededSize * sizeof(wchar_t) + sizeof(UNICODE_NULL));
    
    currentDirectory = static_cast<wchar_t*>(malloc(size));
    if (currentDirectory == NULL)
    {
        wprintf(L"[-] Error! malloc failed in InitializeSymbols. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    if (GetCurrentDirectoryW(neededSize,
                             currentDirectory) == 0)
    {
        wprintf(L"[-] Error! GetCurrentDirectoryW failed in InitializeSymbols. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    //
    // Get the relative path to dbghelp
    //
    symbolPath = std::wstring(symbolPath);
    symbolPath.append(L"..\\..\\SymbolDlls\\dbghelp.dll");

    dbgHelp = LoadLibraryW(symbolPath.c_str());
    if (dbgHelp == NULL)
    {
        wprintf(L"[-] Error! LoadLibraryW failed in InitializeSymbols. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    //
    // Resolve all of the functions. The built-in dbghelp is broken because of its implicit load
    // of symsrv.dll (which doesn't exist in System32). We ship the DLLs from the SDK and, therefore,
    // need to resolve our target functions.
    //
    SymGetOptions_I = reinterpret_cast<SymGetOptions_T>(GetProcAddress(dbgHelp, "SymGetOptions"));
    SymInitializeW_I = reinterpret_cast<SymInitializeW_T>(GetProcAddress(dbgHelp, "SymInitializeW"));
    SymSetOptions_I = reinterpret_cast<SymSetOptions_T>(GetProcAddress(dbgHelp, "SymSetOptions"));
    SymCleanup_I = reinterpret_cast<SymCleanup_T>(GetProcAddress(dbgHelp, "SymCleanup"));
    SymSetSearchPathW_I = reinterpret_cast<SymSetSearchPathW_T>(GetProcAddress(dbgHelp, "SymSetSearchPathW"));
    SymLoadModuleExW_I = reinterpret_cast<SymLoadModuleExW_T>(GetProcAddress(dbgHelp, "SymLoadModuleExW"));
    SymFromAddrW_I = reinterpret_cast<SymFromAddrW_T>(GetProcAddress(dbgHelp, "SymFromAddrW"));
    SymGetTypeFromNameW_I = reinterpret_cast<SymGetTypeFromNameW_T>(GetProcAddress(dbgHelp, "SymGetTypeFromNameW"));
    SymGetTypeInfo_I = reinterpret_cast<SymGetTypeInfo_T>(GetProcAddress(dbgHelp, "SymGetTypeInfo"));

    if ((SymGetOptions_I == NULL) ||
        (SymInitializeW_I == NULL) ||
        (SymSetOptions_I == NULL) ||
        (SymCleanup_I == NULL) ||
        (SymSetSearchPathW_I == NULL) ||
        (SymLoadModuleExW_I == NULL) ||
        (SymFromAddrW_I == NULL) ||
        (SymGetTypeFromNameW_I == NULL) ||
        (SymGetTypeInfo_I == NULL))
    {
        wprintf(L"[-] Error! GetProcAddress failed in InitializeSymbols. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    symOptions = (SymGetOptions_I() |
                  SYMOPT_AUTO_PUBLICS |
                  SYMOPT_CASE_INSENSITIVE |
                  SYMOPT_UNDNAME |
                  SYMOPT_DEFERRED_LOADS);

    SymSetOptions_I(symOptions);

    res = SymInitializeW_I(GetCurrentProcess(),
                           NULL,
                           FALSE);
    if (res == FALSE)
    {
        wprintf(L"[-] Error! SymInitializeW failed in InitializeSymbols. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    if (SymSetSearchPathW_I(GetCurrentProcess(),
                            L"srv*C:\\Symbols*http://msdl.microsoft.com/download/symbols") == FALSE)
    {
        wprintf(L"[-] Error! SymSetSearchPathW failed in InitializeSymbols. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    result = true;

Exit:
    if (currentDirectory != NULL)
    {
        free(currentDirectory);
    }

    return result;
}

/**
*
* @brief        Captures each loaded image (notified from ETW) for symbol processing.
* @param[in]    BaseAddress - The base address of the target module being loaded.
* @param[in]    ImagePath - The NT path of the loaded image.
* @param[in]    Size - The size of the loaded image.
* @return       true on success, otherwise false.
*
*/
bool
CaptureModuleForSymbols (
    _In_ ULONG_PTR BaseAddress,
    _In_ wchar_t* ImagePath,
    _In_ ULONG Size
    )
{
    bool result;
    ULONG_PTR baseAddr;
    std::wstring finalPath(ImagePath);
    std::wstring kernelPrefix(L"\\SystemRoot");
    std::wstring userPrefix(L"\\Device\\HarddiskVolume3");

    result = false;

    //
    // Handle drivers
    //
    auto pos = finalPath.find(kernelPrefix);
    if (pos != std::wstring::npos)
    {
        finalPath.replace(pos, kernelPrefix.length(), L"C:\\Windows");
    }
    else
    {
        //
        // Handle user-mode
        //
        auto pos = finalPath.find(userPrefix);
        if (pos != std::wstring::npos)
        {
            finalPath.replace(pos, userPrefix.length(), L"C:\\Windows");
        }
    }

    //
    // We need to preserve NT's base address for the secure call number
    // to nt!_SKSERVICE enum.
    //
    if (!k_NtFound)
    {
        if (wcscmp(L"C:\\Windows\\system32\\ntoskrnl.exe",
                   finalPath.c_str()) == 0)
        {
            k_NtBase = BaseAddress;
            k_NtFound = true;
        }
    }

    //
    // Track the module!
    //
    baseAddr = SymLoadModuleExW_I(GetCurrentProcess(),
                                  NULL,
                                  finalPath.c_str(),
                                  NULL,
                                  static_cast<ULONG64>(BaseAddress),
                                  Size,
                                  NULL,
                                  0);
    if (baseAddr == 0)
    {
        goto Exit;
    }

    result = true;

Exit:
    return result;
}

/**
*
* @brief        Processes a stack frame address into the appropriate (symbol or image) 
*               name and offset.
* @param[in]    TargetAddress - The target address.
* @param[in]    ImageName - The name of the image where this address is found.
*                           This is in case we cannot find an appropriate symbol.
* @return       The "string-ified" name.
*
*/
wchar_t*
ConvertAddressToFrameStringWithSymbol (
    _In_ ULONG_PTR TargetAddress,
    _In_ wchar_t* ImageName
    )
{
    PSYMBOL_INFOW symbol;
    ULONGLONG offset;
    char buffer[sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(wchar_t)] = { 0 };
    std::wstring fullFrameString;
    SIZE_T returnStringLength;
    wchar_t* returnString;
    
    symbol = reinterpret_cast<PSYMBOL_INFOW>(buffer);
    offset = 0;
    returnStringLength = 0;
    returnString = NULL;

    RtlZeroMemory(&buffer, sizeof(buffer));

    symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
    symbol->MaxNameLen = MAX_SYM_NAME;

    if (SymFromAddrW_I(GetCurrentProcess(),
                       (ULONG64)TargetAddress,
                       &offset,
                       symbol) == FALSE)
    {
        goto Exit;
    }

    //
    // Construct the frame as a symbol string.
    //
    fullFrameString.append(ImageName);
    fullFrameString.append(L"!");
    fullFrameString.append(symbol->Name);
    fullFrameString.append(L" + ");
    fullFrameString.append(std::to_wstring(offset));
    fullFrameString.append(L"|");

    //
    // Calculate the string
    //
    returnStringLength = (fullFrameString.length() * sizeof(wchar_t) + sizeof(UNICODE_NULL));

    returnString = static_cast<wchar_t*>(malloc(returnStringLength));
    if (returnString == NULL)
    {
        wprintf(L"[-] Error! malloc failed in GetSymbolNameFromAddress. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    RtlCopyMemory(returnString,
                  fullFrameString.data(),
                  returnStringLength);

Exit:
    return returnString;
}

/**
*
* @brief        Creates a mapping of secure call numbers to their nt!_SKSERVICE enum value.
*
*/
void
CreateListOfValidSecureCalls ()
{
    SYMBOL_INFOW symbol;
    ULONG childrenCount;
    TI_FINDCHILDREN_PARAMS* childrenSyms;
    SIZE_T childrenSymSize;
    wchar_t* childSymName;
    ULONG index;
    VARIANT secureCallValue;

    childrenCount = 0;
    childrenSyms = NULL;
    childrenSymSize = 0;
    childSymName = NULL;
    index = 0;

    RtlZeroMemory(&symbol, sizeof(symbol));
    RtlZeroMemory(&secureCallValue, sizeof(secureCallValue));

    symbol.SizeOfStruct = sizeof(SYMBOL_INFOW);
    symbol.MaxNameLen = 0;

    if (SymGetTypeFromNameW_I(GetCurrentProcess(),
                              k_NtBase,
                              L"_SKSERVICE",
                              &symbol) == FALSE)
    {
        wprintf(L"[-] Error! SymGetTypeFromNameW failed in GetSecureCallSymbolName. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    //
    // Preserve the index
    //
    index = symbol.TypeIndex;

    if (SymGetTypeInfo_I(GetCurrentProcess(),
                         k_NtBase,
                         index,
                         TI_GET_CHILDRENCOUNT,
                         &childrenCount) == FALSE)
    {
        wprintf(L"[-] Error! SymGetTypeInfo_I failed in GetSecureCallSymbolName. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    childrenSymSize = sizeof(TI_FINDCHILDREN_PARAMS) + childrenCount * sizeof(ULONG);
    childrenSyms = static_cast<TI_FINDCHILDREN_PARAMS*>(malloc(childrenSymSize));
    if (childrenSyms == NULL)
    {
        wprintf(L"[-] Error! malloc failed in GetSecureCallSymbolName. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    RtlZeroMemory(childrenSyms, childrenSymSize);

    childrenSyms->Count = childrenCount;
    childrenSyms->Start = 0;

    if (SymGetTypeInfo_I(GetCurrentProcess(),
                         k_NtBase,
                         index,
                         TI_FINDCHILDREN,
                         childrenSyms) == FALSE)
    {
        wprintf(L"[-] Error! SymGetTypeInfo_I failed in GetSecureCallSymbolName. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    for (ULONG i = 0; i < childrenSyms->Count; i++)
    {
        if (SymGetTypeInfo_I(GetCurrentProcess(),
                             k_NtBase,
                             childrenSyms->ChildId[i],
                             TI_GET_SYMNAME,
                             &childSymName) == FALSE)
        {
            wprintf(L"[-] Error! SymGetTypeInfo_I failed in GetSecureCallSymbolName. (GLE: %d)\n", GetLastError());
            goto Exit;
        }

        if (SymGetTypeInfo_I(GetCurrentProcess(),
                             k_NtBase,
                             childrenSyms->ChildId[i],
                             TI_GET_VALUE,
                             &secureCallValue) == FALSE)
        {
            wprintf(L"[-] Error! SymGetTypeInfo_I failed in GetSecureCallSymbolName. (GLE: %d)\n", GetLastError());
            goto Exit;
        }

        k_SecureCallValues.insert({secureCallValue.ulVal, childSymName});

        childSymName = NULL;
    }

Exit:
    if (childrenSyms != NULL)
    {
        free(childrenSyms);
    }

    return;
}

/**
*
* @brief        Retrieves the literal name for a secure call value.
* @param[in]    SecureCallValue - The target secure call value.
* @return       The associated nt!_SKSERVICE enum value.
*
*/
wchar_t*
GetSecureCallName (
    _In_ ULONG SecureCallValue
    )
{
    auto it = k_SecureCallValues.find(SecureCallValue);
    return it->second;
}

/**
*
* @brief        Tears down the secure call mapping strings. Called on Vtl1Mon exit.
*
*/
void
DestroySecureCallNameVector ()
{
    for (auto &i : k_SecureCallValues)
    {
        free(i.second);
    }
}

/**
*
* @brief        Tears down the symbol infrastructure. Called on Vtl1Mon exit.
*
*/
void
SymbolCleanup ()
{
    SymCleanup_I(GetCurrentProcess());
}