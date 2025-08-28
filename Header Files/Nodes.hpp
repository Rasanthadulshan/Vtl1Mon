/*++
* Copyright (c) Connor McGarr. All rights reserved.
*
* @file:      Vtl1Mon/Nodes.hpp
*
* @summary:   Structured data definitions for correlating events.
*
* @author:    Connor McGarr (@33y0re)
*
* @copyright  Use of this source code is governed by a MIT-style license that
*             can be found in the LICENSE file.
*
--*/
#pragma once
#include <Windows.h>
#include <unordered_map>

//
// Image data structure
//
typedef struct _IMAGE_NODE
{
    ULONG_PTR ImageBase;
    wchar_t* ImageName;
    ULONG ImageSize;
} IMAGE_NODE, * PIMAGE_NODE;

//
// Node containing the VTL 1 enter/exit data
//
typedef struct _VTL1_ENTER_NODE
{
    LONGLONG Vtl1EnterTime;
    ULONG ProcessId;
    ULONG ThreadId;
    unsigned __int16 SecureCallNumber;
} VTL1_ENTER_NODE, *PVTL1_ENTER_NODE;

//
// Image map
//
static std::unordered_map<ULONG_PTR, IMAGE_NODE> k_ImageMap;

//
// Completed VTL 1 enter map
//
static std::unordered_map<ULONGLONG, VTL1_ENTER_NODE> k_VtlEnterMap;

//
// Function definitions
//
bool
InsertImage (
    _In_ ULONG_PTR ImageBase,
    _In_ ULONG ImageSize,
    _In_ wchar_t* ImageName
    );

bool
GetImageDataFromAddress (
    _In_ ULONG_PTR TargetAddress,
    _Out_ PIMAGE_NODE ImageNode
    );

void
InsertVtl1EnterEventData (
    _In_ ULONGLONG TimeStamp,
    _In_ ULONG ProcessId,
    _In_ ULONG ThreadId,
    _In_ unsigned __int16 SecureCallNumber
    );

void
CorrelateVtl1EnterCallStack (
    _In_ ULONGLONG TimeStamp,
    _In_ ULONG_PTR* CallStack,
    _In_ ULONG NumberOfFrames
    );