/*++
* Copyright (c) Connor McGarr. All rights reserved.
*
* @file:      Vtl1Mon/Trace.hpp
*
* @summary:   ETW trace configuration and management definitions.
*
* @author:    Connor McGarr (@33y0re)
*
* @copyright  Use of this source code is governed by a MIT-style license that
*             can be found in the LICENSE file.
*
--*/
#pragma once
#include <Windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <initguid.h>

//
// Tracing thread handle
//
static HANDLE k_Vtl1EnterExitTracingThreadHandle = NULL;

//
// Trace handle
//
static TRACEHANDLE k_Vtl1EnterExitTraceHandle = 0;

//
// Processing trace handle
//
static PROCESSTRACE_HANDLE k_Vtl1EnterExitProcessTraceHandle = 0;

//
// Trace name
//
static const wchar_t* k_Vtl1EnterExitTraceName = L"Vtl1Trace";

//
// Tracing properties
//
static PEVENT_TRACE_PROPERTIES k_Vtl1EnterExitProperties = NULL;

//
// Trace GUID
//
DEFINE_GUID( /* 7f1e3d22-a33b-4b9c-947a-214f11f86ebb */
    k_Vtl1EnterExitTraceGuid,
    0x7f1e3d22,
    0xa33b,
    0x4b9c,
    0x94, 0x7a, 0x21, 0x4f, 0x11, 0xf8, 0x6e, 0xbb);

//
// Thread GUID (for system logger events. VTL 1 enter/exit events come in here)
//
DEFINE_GUID( /* 3d6fa8d1-fe05-11d0-9dda-00c04fd7ba7c */
    ThreadGuid,
    0x3d6fa8d1,
    0xfe05,
    0x11d0,
    0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c  );

//
// Stack walk GUID
//
DEFINE_GUID( /* def2fe46-7bd6-4b80-bd94-f57fe20d0ce3 */
    StackWalkGuid,
    0xdef2fe46,
    0x7bd6,
    0x4b80,
    0xbd, 0x94, 0xf5, 0x7f, 0xe2, 0x0d, 0x0c, 0xe3
);

DEFINE_GUID( /* 2cb15d1d-5fc1-11d2-abe1-00a0c911f518 */
    ImageLoadGuid,
    0x2cb15d1d,
    0x5fc1,
    0x11d2,
    0xab, 0xe1, 0x00, 0xa0, 0xc9, 0x11, 0xf5, 0x18);

//
// Undocumented?
// See nt!VslpEnterIumSecureMode check on nt!PerfGlobalGroupMask
// 
// It seems ONLY PERF_VTL_CHANGE does anything! A successful
// call to EnableTraceEx2 using the new "System providers" 
// cannot enable VTL 1 enter/exit!
// 
// https://learn.microsoft.com/en-us/windows/win32/etw/system-providers
// 
// Part of "Masks[5]" (group 6) in the perf group mask.
// 
// Also, for whatever reason, it is a part of the "thread" provider,
// not the "perf info" provider (which the PERF_HV_PROFILE flag is a part of)
//
#define PERF_VTL_CHANGE 0xA0000008

//
// From System Informer
//
typedef enum _EVENT_TRACE_INFORMATION_CLASS
{
    EventTraceKernelVersionInformation, // EVENT_TRACE_VERSION_INFORMATION
    EventTraceGroupMaskInformation, // EVENT_TRACE_GROUPMASK_INFORMATION
    EventTracePerformanceInformation, // EVENT_TRACE_PERFORMANCE_INFORMATION
    EventTraceTimeProfileInformation, // EVENT_TRACE_TIME_PROFILE_INFORMATION
    EventTraceSessionSecurityInformation, // EVENT_TRACE_SESSION_SECURITY_INFORMATION
    EventTraceSpinlockInformation, // EVENT_TRACE_SPINLOCK_INFORMATION
    EventTraceStackTracingInformation, // EVENT_TRACE_STACK_TRACING_INFORMATION
    EventTraceExecutiveResourceInformation, // EVENT_TRACE_EXECUTIVE_RESOURCE_INFORMATION
    EventTraceHeapTracingInformation, // EVENT_TRACE_HEAP_TRACING_INFORMATION
    EventTraceHeapSummaryTracingInformation, // EVENT_TRACE_HEAP_TRACING_INFORMATION
    EventTracePoolTagFilterInformation, // EVENT_TRACE_POOLTAG_FILTER_INFORMATION
    EventTracePebsTracingInformation, // EVENT_TRACE_PEBS_TRACING_INFORMATION
    EventTraceProfileConfigInformation, // EVENT_TRACE_PROFILE_CONFIG_INFORMATION
    EventTraceProfileSourceListInformation, // EVENT_TRACE_PROFILE_LIST_INFORMATION
    EventTraceProfileEventListInformation, // EVENT_TRACE_PROFILE_EVENT_INFORMATION
    EventTraceProfileCounterListInformation, // EVENT_TRACE_PROFILE_COUNTER_INFORMATION
    EventTraceStackCachingInformation, // EVENT_TRACE_STACK_CACHING_INFORMATION
    EventTraceObjectTypeFilterInformation, // EVENT_TRACE_OBJECT_TYPE_FILTER_INFORMATION
    EventTraceSoftRestartInformation, // EVENT_TRACE_SOFT_RESTART_INFORMATION
    EventTraceProfileSourceAddInformation, // EVENT_TRACE_PROFILE_ADD_INFORMATION // REDSTONE4
    EventTraceProfileSourceRemoveInformation, // EVENT_TRACE_PROFILE_REMOVE_INFORMATION
    EventTraceCoverageSamplerInformation, // EVENT_TRACE_COVERAGE_SAMPLER_INFORMATION
    EventTraceUnifiedStackCachingInformation, // since 21H1
    MaxEventTraceInfoClass
} EVENT_TRACE_INFORMATION_CLASS;

#define PERF_MASK_INDEX (0xe0000000)
#define PERF_MASK_GROUP (~PERF_MASK_INDEX)
#define PERF_NUM_MASKS 8

typedef struct _PERFINFO_GROUPMASK
{
    ULONG Masks[PERF_NUM_MASKS];
} PERFINFO_GROUPMASK, * PPERFINFO_GROUPMASK;

#define PERF_GET_MASK_INDEX(GM) (((GM) & PERF_MASK_INDEX) >> 29)
#define PERF_GET_MASK_GROUP(GM) ((GM) & PERF_MASK_GROUP)
#define PERFINFO_OR_GROUP_WITH_GROUPMASK(Group, pGroupMask) \
    (pGroupMask)->Masks[PERF_GET_MASK_INDEX(Group)] |= PERF_GET_MASK_GROUP(Group);

typedef struct _EVENT_TRACE_GROUPMASK_INFORMATION
{
    EVENT_TRACE_INFORMATION_CLASS EventTraceInformationClass;
    TRACEHANDLE TraceHandle;
    PERFINFO_GROUPMASK EventTraceGroupMasks;
} EVENT_TRACE_GROUPMASK_INFORMATION, * PEVENT_TRACE_GROUPMASK_INFORMATION;

//
// Function definitions
//
bool
CreateAndConfigureVtlEnterExitTrace ();

void
StopAndCleanupVtl1EnterExitTrace ();