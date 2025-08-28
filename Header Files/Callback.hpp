/*++
* Copyright (c) Connor McGarr. All rights reserved.
*
* @file:      Vtl1Mon/Callback.hpp
*
* @summary:   ETW callback definitions.
*
* @author:    Connor McGarr (@33y0re)
*
* @copyright  Use of this source code is governed by a MIT-style license that
*             can be found in the LICENSE file.
*
--*/
#pragma once
#include "Trace.hpp"

//
// Image load events do not seem to give a DCEnd event.
// Because of this, when we see the first "real" image load event
// we consider our rundown done.
//
static bool k_ImageRunDownComplete = false;

//
// VTl 1 enter/exit
//

//
// OpCodes ("event IDs") for VTL 1 enter and exit
//
#define VTL1_ENTER_OPCODE 0x49
#define VTL1_EXIT_OPCODE 0x4A

//
// Data for the secure system call event
//
typedef struct _SECURE_CALL_EVENT_DATA
{
    unsigned __int16 Operation;
    unsigned __int16 SecureCallNumber;
} SECURE_CALL_EVENT_DATA, *PSECURE_CALL_EVENT_DATA;

#define VTL1_ENTER_EXIT_EVENT_SIZE sizeof(SECURE_CALL_EVENT_DATA)

//
// Stack walks
//
#define STACK_WALK_OPCODE 32

typedef struct _STACK_WALK_EVENT_DATA
{
    ULONGLONG EventTimeStamp;
    ULONG StackProcess;
    ULONG StackThread;

    //
    // The stack is _in_ the event,
    // we do not get a pointer to it.
    //
    ULONG_PTR Stack;
} STACK_WALK_EVENT_DATA, *PSTACK_WALK_EVENT_DATA;

#define STACK_WALK_EVENT_SIZE sizeof(STACK_WALK_EVENT_DATA)

//
// Image loads
//
#define IMAGE_LOADED_OPCODE 10
#define IMAGE_LOADED_UNLOAD 2
#define IMAGE_LOADED_RUNDOWN_OPCODE 3

typedef struct _IMAGE_LOAD_EVENT_DATA
{
    ULONG_PTR ImageBase;
    ULONG_PTR ImageSize;
    ULONG ProcessId;
    ULONG ImageChecksum;
    ULONG TimeDateStamp;
    ULONG Reserved0;
    ULONG_PTR DefaultBase;
    ULONG Reserved1;
    ULONG Reserved2;
    ULONG Reserved3;
    ULONG Reserved4;

    //
    // The name is _in_ the event,
    // we do not get a pointer to it.
    //
    wchar_t FileName;
} IMAGE_LOAD_EVENT_DATA, *PIMAGE_LOAD_EVENT_DATA;


//
// Function definitions
//
_Function_class_(PEVENT_RECORD_CALLBACK)
void
EtwEventCallback (
    _In_ PEVENT_RECORD EventRecord
    );

_Function_class_(PEVENT_TRACE_BUFFER_CALLBACKW)
ULONG
EtwBufferCallback (
    _In_ PEVENT_TRACE_LOGFILEW Logfile
    );