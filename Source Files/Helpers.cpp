/*++
* Copyright (c) Connor McGarr. All rights reserved.
*
* @file:      Vtl1Mon/Helpers.cpp
*
* @summary:   Various helper routines.
*
* @author:    Connor McGarr (@33y0re)
*
* @copyright  Use of this source code is governed by a MIT-style license that
*             can be found in the LICENSE file.
*
--*/
#include "Helpers.hpp"
#include "Nodes.hpp"
#include "Symbols.hpp"
#include "Trace.hpp"
#include <string>

/**
*
* @brief        Creates the "large" string of data to write to the CSV
*               containing the correlated event data and sends it to be written to disk.
* @param[in]    Vtl1Data - The "primal" VTL 1 enter event data.
* @param[in]    CallStack - The raw list of stack frame addresses.
* @param[in]    NumberOfFrames - The number of stack frames to process.
*
*/
void
ConstructCallStackStringAndPublishData (
    _In_ PVTL1_ENTER_NODE Vtl1Data,
    _In_ ULONG_PTR* CallStack,
    _In_ ULONG NumberOfFrames
    )
{
    IMAGE_NODE imageNode;
    ULONG_PTR offset;
    std::wstring stackAsString;
    wchar_t* frameString;

    offset = 0;
    frameString = NULL;

    RtlZeroMemory(&imageNode, sizeof(imageNode));

    for (ULONG i = 0; i < NumberOfFrames; i++)
    {
        if (!GetImageDataFromAddress(CallStack[i], &imageNode))
        {
            //
            // Unknown
            //
            stackAsString += std::to_wstring(CallStack[i]);
            stackAsString += L"|";
            continue;
        }

        frameString = ConvertAddressToFrameStringWithSymbol(CallStack[i],
                                                            imageNode.ImageName);
        if (frameString == NULL)
        {
            //
            // Unknown
            // We do not have symbols, but we _do_ have image data!
            //
            offset = (CallStack[i] - imageNode.ImageBase);

            //
            // Compute the string.
            //
            stackAsString += imageNode.ImageName;
            stackAsString += L" + ";
            stackAsString += std::to_wstring(offset);
            stackAsString += L"|";
            continue;
        }

        //
        // Immediately append the string so we can free the return
        // value from ConvertAddressToFrameStringWithSymbol.
        //
        stackAsString.append(frameString);

        free(frameString);

        //
        // Reset the current in-scope frame string and offset.
        //
        frameString = NULL;
        offset = 0;
    }

    //
    // Write it to the file
    //
    WriteVtl1DataAndCallStackToFile(Vtl1Data,
                                    stackAsString.c_str());

    return;
}

/**
*
* @brief        Creates the CSV output file.
* @param[in]    FilePath - The user-provided path.
* @return       true on success, otherwise false.
* 
*/
bool
CreateOutputFile (
    _In_ const wchar_t* FilePath
    )
{
    bool result;
    const wchar_t csvHeadings[] = L"TIMESTAMP,SECURE CALL NUMBER,PROCESS ID,THREAD ID, CALL STACK\n";

    result = false;

    k_OutputFileHandle = CreateFileW(FilePath,
                                     GENERIC_READ | GENERIC_WRITE,
                                     0,
                                     NULL,
                                     CREATE_ALWAYS,
                                     0,
                                     NULL);
    if (k_OutputFileHandle == INVALID_HANDLE_VALUE)
    {
        wprintf(L"[-] Error! CreateFileW failed in CreateOutputFile. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    //
    // Write the headings.
    //
    if (WriteFile(k_OutputFileHandle,
                  csvHeadings,
                  sizeof(csvHeadings),
                  NULL,
                  NULL) == FALSE)
    {
        wprintf(L"[-] Error! WriteFile failed in CreateOutputFile. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    result = true;

Exit:
    return result;
}

/**
*
* @brief        Write the final correlated event to the user-specified CSV file.
* @param[in]    Vtl1Data - The "primal" event data.
* @param[in]    CallStack - The "string-ified" call stack.
*
*/
void
WriteVtl1DataAndCallStackToFile (
    _In_ PVTL1_ENTER_NODE Vtl1Data,
    _In_ const wchar_t* CallStack
    )
{
    std::wstring csvString;
    SIZE_T stringSize;
    wchar_t secureCallName[MAX_SYM_NAME];

    RtlZeroMemory(&secureCallName, sizeof(secureCallName));

    if (_InterlockedCompareExchange(&k_CanWriteToFile, TRUE, TRUE) == FALSE)
    {
        goto Exit;
    }

    //
    // First, convert the secure call value to the appropriate nt!_SKSERVICE
    // enum value.
    //
    if (!k_SecureCallNamesInitialized)
    {
        CreateListOfValidSecureCalls();
        k_SecureCallNamesInitialized = true;
    }

    csvString.append(std::to_wstring(Vtl1Data->Vtl1EnterTime));
    csvString.append(L",");
    csvString.append(GetSecureCallName(Vtl1Data->SecureCallNumber));
    csvString.append(L" (");
    csvString.append(std::to_wstring(Vtl1Data->SecureCallNumber));
    csvString.append(L")");
    csvString.append(L",");
    csvString.append(std::to_wstring(Vtl1Data->ProcessId));
    csvString.append(L",");
    csvString.append(std::to_wstring(Vtl1Data->ThreadId));
    csvString.append(L",");
    csvString.append(CallStack);
    csvString.append(L"\n");

    stringSize = (csvString.length() * sizeof(wchar_t) + sizeof(UNICODE_NULL));

    if (WriteFile(k_OutputFileHandle,
                  csvString.data(),
                  static_cast<DWORD>(stringSize),
                  NULL,
                  NULL) == FALSE)
    {
        wprintf(L"[-] Error! WriteFile failed in WriteVtl1DataAndCallStackToFile. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

Exit:
    return;
}

/**
*
* @brief        Cleans up all Vtl1Mon resources on program exit.
*
*/
void
CleanupVtl1MonResources ()
{
    //
    // Stop and cleanup the trace
    //
    StopAndCleanupVtl1EnterExitTrace();

    //
    // Stop writing.
    //
    _InterlockedExchange(&k_CanWriteToFile, FALSE);

    //
    // Close the file handle
    //
    CloseHandle(k_OutputFileHandle);

    //
    // Destroy the vector of secure call names
    //
    DestroySecureCallNameVector();

    //
    // Symbol cleanup
    //
    SymbolCleanup();
}