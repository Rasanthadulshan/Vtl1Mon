/*++
* Copyright (c) Connor McGarr. All rights reserved.
*
* @file:      Vtl1Mon/Callback.cpp
*
* @summary:   ETW callback implementation.
*
* @author:    Connor McGarr (@33y0re)
*
* @copyright  Use of this source code is governed by a MIT-style license that
*             can be found in the LICENSE file.
*
--*/
#include "Callback.hpp"
#include "Nodes.hpp"
#include "Helpers.hpp"
#include "Symbols.hpp"
#include <stdio.h>

//
// From Trace.cpp
//
extern HANDLE g_EnableVtl1EnterExitEvent;

//
// Determines if we should continue to process events.
//
volatile LONG g_ContinueTracing = TRUE;

//
// Total events seen
//
ULONGLONG g_TotalEventsSeen = 0;

/**
*
* @brief        VTL 1 enter/exit ETW callback.
* @param[in]    EventRecord - Associated ETW event record.
*
*/
static
_Function_class_(PEVENT_RECORD_CALLBACK)
void
HandleVtl1EnterExitEvents (
    _In_ PEVENT_RECORD EventRecord
    )
{
    PSECURE_CALL_EVENT_DATA secureCallEventData;

    secureCallEventData = NULL;

    if (EventRecord->EventHeader.ProcessId == GetCurrentProcessId())
    {
        goto Exit;
    }

    if (EventRecord->UserDataLength != VTL1_ENTER_EXIT_EVENT_SIZE)
    {
        goto Exit;
    }

    secureCallEventData = reinterpret_cast<PSECURE_CALL_EVENT_DATA>(EventRecord->UserData);
    if (secureCallEventData == NULL)
    {
        goto Exit;
    }

    if (EventRecord->EventHeader.EventDescriptor.Opcode != VTL1_ENTER_OPCODE)
    {
        goto Exit;
    }

    g_TotalEventsSeen++;

    InsertVtl1EnterEventData(static_cast<ULONGLONG>(EventRecord->EventHeader.TimeStamp.QuadPart),
                             EventRecord->EventHeader.ProcessId,
                             EventRecord->EventHeader.ThreadId,
                             secureCallEventData->SecureCallNumber);

Exit:
    return;
}

/**
*
* @brief        VTL 1 enter/exit stack walk ETW callback.
* @param[in]    EventRecord - Associated ETW event record.
*
*/
static
_Function_class_(PEVENT_RECORD_CALLBACK)
void
HandleStackWalkEvents (
    _In_ PEVENT_RECORD EventRecord
    )
{
    PSTACK_WALK_EVENT_DATA stackWalkEvent;
    ULONG_PTR* stack;
    ULONG numberOfFrames;

    stackWalkEvent = reinterpret_cast<PSTACK_WALK_EVENT_DATA>(EventRecord->UserData);
    numberOfFrames = ((EventRecord->UserDataLength -
                       FIELD_OFFSET(STACK_WALK_EVENT_DATA, Stack)) / sizeof(ULONG_PTR));

    if (numberOfFrames == 0)
    {
        goto Exit;
    }

    if (stackWalkEvent->StackProcess == GetCurrentProcessId())
    {
        goto Exit;
    }

    stack = &stackWalkEvent->Stack;

    CorrelateVtl1EnterCallStack(stackWalkEvent->EventTimeStamp,
                                stack,
                                numberOfFrames);

Exit:
    return;
}

/**
*
* @brief        Image load ETW callback.
* @param[in]    EventRecord - Associated ETW event record.
*
*/
static
_Function_class_(PEVENT_RECORD_CALLBACK)
void
HandleImageLoadEvents (
    _In_ PEVENT_RECORD EventRecord
    )
{
    PIMAGE_LOAD_EVENT_DATA imageLoadEvent;
    UCHAR opcode;

    imageLoadEvent = reinterpret_cast<PIMAGE_LOAD_EVENT_DATA>(EventRecord->UserData);

    opcode = EventRecord->EventHeader.EventDescriptor.Opcode;
    if ((opcode != IMAGE_LOADED_RUNDOWN_OPCODE) &&
        (opcode != IMAGE_LOADED_OPCODE))
    {
        //
        // We do not get a rundown "end" event with image loads.
        // So we use a "hack" here. The first image unload event
        // we get indicates that the rundown must be over.
        //
        if (opcode == IMAGE_LOADED_UNLOAD)
        {
            //
            // One-time init
            //
            if (!k_ImageRunDownComplete)
            {
                k_ImageRunDownComplete = true;

                //
                // Set the event
                //
                SetEvent(g_EnableVtl1EnterExitEvent);
            }
        }

        goto Exit;
    }

    //
    // Insert the image
    //
    if (!InsertImage(imageLoadEvent->ImageBase,
                     static_cast<ULONG>(imageLoadEvent->ImageSize),
                     &imageLoadEvent->FileName))
    {
        //
        // We had an error or a duplicate. Do not send to symbols.
        //
        goto Exit;
    }

    //
    // Capture the symbols
    //
    if (!CaptureModuleForSymbols(imageLoadEvent->ImageBase,
                                 &imageLoadEvent->FileName,
                                 static_cast<ULONG>(imageLoadEvent->ImageSize)))
    {
        goto Exit;
    }

Exit:
    return;
}

/**
*
* @brief        Main ETW callback. Invokes "lower-level callbacks" based on
*               provider GUID.
* @param[in]    EventRecord - Associated ETW event record.
*
*/
_Function_class_(PEVENT_RECORD_CALLBACK)
void
EtwEventCallback (
    _In_ PEVENT_RECORD EventRecord
    )
{
    //
    // VTL 1 enter/exit events
    // 
    // For whatever reason these come in on the Thread GUID
    // and not the PerfInfo GUID!
    //
    if (IsEqualGUID(ThreadGuid,
                    EventRecord->EventHeader.ProviderId) == TRUE)
    {
        HandleVtl1EnterExitEvents(EventRecord);
    }

    //
    // Stack walks
    //
    else if (IsEqualGUID(StackWalkGuid,
                         EventRecord->EventHeader.ProviderId) == TRUE)
    {
        HandleStackWalkEvents(EventRecord);
    }

    //
    // Image load events
    //
    else if (IsEqualGUID(ImageLoadGuid,
                         EventRecord->EventHeader.ProviderId) == TRUE)
    {
        HandleImageLoadEvents(EventRecord);
    }

    return;
}

/**
*
* @brief        ETW buffer callback.
* @param[in]    Logfile - Associated trace data source structure.
* @return       TRUE to continue receiving ETW events, FALSE otherwise.
*
*/
_Function_class_(PEVENT_TRACE_BUFFER_CALLBACKW)
ULONG
EtwBufferCallback (
    _In_ PEVENT_TRACE_LOGFILEW Logfile
    )
{
    return g_ContinueTracing;
}