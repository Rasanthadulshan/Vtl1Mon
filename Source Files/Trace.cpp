/*++
* Copyright (c) Connor McGarr. All rights reserved.
*
* @file:      Vtl1Mon/Trace.cpp
*
* @summary:   ETW trace configuration and management implementation.
*
* @author:    Connor McGarr (@33y0re)
*
* @copyright  Use of this source code is governed by a MIT-style license that
*             can be found in the LICENSE file.
*
--*/
#include "Trace.hpp"
#include "Helpers.hpp"
#include "Callback.hpp"
#include "Symbols.hpp"
#include <stdio.h>

//
// From Callbacks.cpp
//
extern volatile LONG g_ContinueTracing;
extern ULONGLONG g_TotalEventsSeen;

//
// Let's us know when we can enable VTL 1 enter/exit events.
//
HANDLE g_EnableVtl1EnterExitEvent = NULL;

/**
*
* @brief        Enables stack walk ETW events for VTL 1 enter/exit ETW events
*               on an existing ETW trace.
* @return       true on success, otherwise false.
*
*/
static
bool
EnableStackTracesForVtl1EnterExit ()
{
    bool result;
    ULONG error;
    CLASSIC_EVENT_ID eventIds;

    result = false;

    RtlZeroMemory(&eventIds, sizeof(eventIds));

    eventIds.EventGuid = ThreadGuid;
    eventIds.Type = VTL1_ENTER_OPCODE;

    error = TraceSetInformation(k_Vtl1EnterExitTraceHandle,
                                TraceStackTracingInfo,
                                &eventIds,
                                sizeof(eventIds));
    if (error != ERROR_SUCCESS)
    {
        wprintf(L"[-] Error! TraceSetInformation failed in EnableStackTracesForVtl1EnterExit. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    result = true;

Exit:
    return result;
}

/**
*
* @brief        Enables VTL 1 enter/exit ETW events on an existing ETW trace.
* @return       STATUS_SUCCESS on success, otherwise appropriate NTSTATUS code.
*
*/
static
NTSTATUS
EnableVtl1EnterExitEvents ()
{
    NTSTATUS status;
    EVENT_TRACE_GROUPMASK_INFORMATION groupInfo;

    status = STATUS_INVALID_PARAMETER;

    RtlZeroMemory(&groupInfo, sizeof(groupInfo));

    groupInfo.TraceHandle = k_Vtl1EnterExitTraceHandle;
    groupInfo.EventTraceInformationClass = EventTraceGroupMaskInformation;

    //
    // Query what we have (which should be load image events)
    // in order to preserve this value when we enable VTL 1 enter/exit events.
    //
    status = NtQuerySystemInformation(SystemPerformanceTraceInformation,
                                      &groupInfo,
                                      sizeof(groupInfo),
                                      NULL);
    if (!NT_SUCCESS(status))
    {
        wprintf(L"[-] Error! NtQuerySystemInformation failed in EnableVtl1EnterExitEvents. (NTSTATUS: %X)\n", status);
        goto Exit;
    }

    groupInfo.TraceHandle = k_Vtl1EnterExitTraceHandle;
    groupInfo.EventTraceInformationClass = EventTraceGroupMaskInformation;

    //
    // Now add our VTL 1 enter/exit events.
    //
    PERFINFO_OR_GROUP_WITH_GROUPMASK(PERF_VTL_CHANGE, &groupInfo.EventTraceGroupMasks);

    status = NtSetSystemInformation(SystemPerformanceTraceInformation,
                                    &groupInfo,
                                    sizeof(groupInfo));
    if (!NT_SUCCESS(status))
    {
        wprintf(L"[-] Error! NtSetSystemInformation failed in EnableVtl1EnterExitEvents. (NTSTATUS: %X)\n", status);
        goto Exit;
    }

Exit:
    return status;
}

/**
*
* @brief        Thread-entry point for the thread which processes the ETW trace.
* @param[in]    Context - Unused thread context ("thread argument").
* @return       ERROR_SUCCESS on success, otherwise appropriate error code.
*
*/
static
_Function_class_(PTHREAD_START_ROUTINE)
DWORD
ProcessVtl1EnterExitTrace (
    _In_ PVOID Context
    )
{
    ULONG error;

    error = ProcessTrace(&k_Vtl1EnterExitProcessTraceHandle,
                         1,
                         NULL,
                         NULL);
    if (error != ERROR_SUCCESS)
    {
        wprintf(L"[-] Error! ProcessTrace failed in ProcessVtl1EnterExitTrace. (GLE: %X)\n", error);
        goto Exit;
    }

Exit:
    return error;
}

/**
*
* @brief        Creates the thread which calls ProcessTrace (ETW "tracing thread").
* @return       true on success, otherwise false.
*
*/
static
bool
CreateTracingThreadAndStartTracing ()
{
    bool result;
    EVENT_TRACE_LOGFILEW logFile;

    result = false;

    RtlZeroMemory(&logFile, sizeof(logFile));

    logFile.ProcessTraceMode = (PROCESS_TRACE_MODE_REAL_TIME |
                                PROCESS_TRACE_MODE_EVENT_RECORD |
                                PROCESS_TRACE_MODE_RAW_TIMESTAMP);

    logFile.LoggerName = const_cast<LPWSTR>(k_Vtl1EnterExitTraceName);
    logFile.EventRecordCallback = EtwEventCallback;
    logFile.BufferCallback = EtwBufferCallback;

    k_Vtl1EnterExitProcessTraceHandle = OpenTraceW(&logFile);
    if (k_Vtl1EnterExitProcessTraceHandle == INVALID_PROCESSTRACE_HANDLE)
    {
        wprintf(L"[-] Error! OpenTraceW failed in CreateTracingThread. (GLE: %X)\n", GetLastError());
        goto Exit;
    }

    k_Vtl1EnterExitTracingThreadHandle = CreateThread(NULL,
                                                      0,
                                                      ProcessVtl1EnterExitTrace,
                                                      NULL,
                                                      0,
                                                      NULL);
    if (k_Vtl1EnterExitTracingThreadHandle == NULL)
    {
        wprintf(L"[-] Error! CreateThread failed in CreateTracingThread. (GLE: %X)\n", GetLastError());
        goto Exit;
    }

    result = true;

Exit:
    return result;
}

/**
*
* @brief        Creates and configures the ETW trace.
* @return       true on success, otherwise false.
*
*/
bool
CreateAndConfigureVtlEnterExitTrace ()
{
    bool result;
    NTSTATUS status;
    SYSTEM_INFO sysInfo;
    SIZE_T size;
    PEVENT_TRACE_PROPERTIES traceProps;
    ULONG error;

    result = false;
    status = STATUS_INVALID_PARAMETER;
    size = (sizeof(EVENT_TRACE_PROPERTIES) + (wcslen(k_Vtl1EnterExitTraceName) * sizeof(wchar_t) + sizeof(UNICODE_NULL)));
    traceProps = NULL;
    error = ERROR_SUCCESS;

    RtlZeroMemory(&sysInfo, sizeof(sysInfo));

    //
    // Before we enable VTL 1 enter/exit events, we first
    // will do a loaded image rundown so we can be aware of
    // all the images which were loaded at the time of consuming
    // the first VTL 1 enter/exit events. This is specifically
    // for symbol resolution.
    //
    g_EnableVtl1EnterExitEvent = CreateEventW(NULL,
                                              FALSE,
                                              FALSE,
                                              NULL);
    if (g_EnableVtl1EnterExitEvent == NULL)
    {
        wprintf(L"[-] Error! malloc CreateEventW in CreateAndConfigureVtlEnterExitTrace. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    //
    // For the number of processors present.
    //
    GetSystemInfo(&sysInfo);

    traceProps = static_cast<PEVENT_TRACE_PROPERTIES>(malloc(size));
    if (traceProps == NULL)
    {
        wprintf(L"[-] Error! malloc failed in CreateAndConfigureVtlEnterExitTrace. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    RtlZeroMemory(traceProps, size);

    //
    // Wnode
    //
    traceProps->Wnode.BufferSize = static_cast<ULONG>(size);
    traceProps->Wnode.ClientContext = 1;    // QPC
    traceProps->Wnode.Flags = WNODE_FLAG_TRACED_GUID;

    RtlCopyMemory(&traceProps->Wnode.Guid,
                  &k_Vtl1EnterExitTraceGuid,
                  sizeof(k_Vtl1EnterExitTraceGuid));

    //
    // Other config
    //
    traceProps->BufferSize = 200;
    traceProps->MinimumBuffers = 4;
    traceProps->MaximumBuffers = max((4 * 2), (sysInfo.dwNumberOfProcessors * 2));
    traceProps->LogFileMode = (EVENT_TRACE_REAL_TIME_MODE |
                               EVENT_TRACE_SYSTEM_LOGGER_MODE |
                               EVENT_TRACE_NO_PER_PROCESSOR_BUFFERING);
    traceProps->LogFileNameOffset = 0;
    traceProps->FlushTimer = 1;
    traceProps->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    traceProps->EnableFlags = EVENT_TRACE_FLAG_IMAGE_LOAD;

    //
    // Enable
    //
    error = StartTraceW(&k_Vtl1EnterExitTraceHandle,
                        k_Vtl1EnterExitTraceName,
                        traceProps);
    if (error != ERROR_SUCCESS)
    {
        wprintf(L"[-] Error! StartTraceW failed in CreateAndConfigureVtlEnterExitTrace. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    //
    // Enable stack traces. We don't enable VTL 1 events until
    // we receive all image load rundown events.
    //
    if (!EnableStackTracesForVtl1EnterExit())
    {
        goto Exit;
    }

    //
    // This will start processing events on another thread.
    //
    if (!CreateTracingThreadAndStartTracing())
    {
        goto Exit;
    }

    //
    // Wait until it is okay to enable VTL 1 enter/exit events.
    // We wait so that call stacks -> module names cache
    // is populated with currently-loaded images.
    //
    WaitForSingleObject(g_EnableVtl1EnterExitEvent,
                        INFINITE);

    //
    // Now we can enable VTL 1 enter/exit events!
    //
    status = EnableVtl1EnterExitEvents();
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    k_Vtl1EnterExitProperties = traceProps;
    result = true;

Exit:
    if (g_EnableVtl1EnterExitEvent != NULL)
    {
        CloseHandle(g_EnableVtl1EnterExitEvent);
    }

    return result;
}

/**
*
* @brief        Stops and cleans up the ETW trace and associated resources.
*
*/
void
StopAndCleanupVtl1EnterExitTrace ()
{
    ULONG error;

    //
    // Stop more events from coming in.
    //
    _InterlockedExchange(&g_ContinueTracing, FALSE);

    //
    // Stop the trace.
    //
    error = ControlTraceW(k_Vtl1EnterExitProcessTraceHandle,
                          k_Vtl1EnterExitTraceName,
                          k_Vtl1EnterExitProperties,
                          EVENT_TRACE_CONTROL_STOP);
    if (error != ERROR_SUCCESS)
    {
        wprintf(L"[-] Error! ControlTraceW failed in StopAndCleanupVtl1EnterExitTrace. (GLE: %d)\n", GetLastError());
        goto Exit;
    }

    //
    // Close the handles
    //
    CloseHandle(k_Vtl1EnterExitTracingThreadHandle);
    CloseTrace(k_Vtl1EnterExitProcessTraceHandle);

    wprintf(L"[+] %s trace statistics:\n", k_Vtl1EnterExitTraceName);
    wprintf(L"  [>] Events dropped: %d\n", k_Vtl1EnterExitProperties->EventsLost);
    wprintf(L"  [>] Events seen: %llu\n", g_TotalEventsSeen);

    free(k_Vtl1EnterExitProperties);

Exit:
    return;
}