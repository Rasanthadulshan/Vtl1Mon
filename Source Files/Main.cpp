/*++
* Copyright (c) Connor McGarr. All rights reserved.
*
* @file:      Vtl1Mon/Main.cpp
*
* @summary:   Vtl1Mon entry point.
*
* @author:    Connor McGarr (@33y0re)
*
* @copyright  Use of this source code is governed by a MIT-style license that
*             can be found in the LICENSE file.
*
--*/
#include "Trace.hpp"
#include "Symbols.hpp"
#include "Helpers.hpp"
#include <stdio.h>

/**
*
* @brief        Vtl1Mon entry point.
* @param[in]    argc - Number of arguments.
* @param[in]	argv - Argument array.
* @return       ERROR_SUCCESS on success, otherwise appropriate error code.
*
*/
int
wmain (
    _In_ int argc,
    _In_ wchar_t** argv
    )
{
    ULONG error;

    error = ERROR_SUCCESS;

    if (argc != 2)
    {
        wprintf(L"[+] Usage: .\\Vtl1Mon.exe C:\\Path\\To\\Output\\File.csv\n");
        goto Exit;
    }

    if (!CreateOutputFile(argv[1]))
    {
        error = ERROR_GEN_FAILURE;
        goto Exit;
    }

    wprintf(L"[+] Target output file: %s\n", argv[1]);
    wprintf(L"[+] Configuring the trace! Please wait!\n");

    if (!InitializeSymbols())
    {
        error = ERROR_GEN_FAILURE;
        goto Exit;
    }

    //
    // Create and start tracing!
    //
    CreateAndConfigureVtlEnterExitTrace();

    wprintf(L"[+] Press ENTER to terminate the trace!\n");
    getchar();

    CleanupVtl1MonResources();

Exit:
    return error;
}