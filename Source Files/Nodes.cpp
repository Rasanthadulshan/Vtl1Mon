/*++
* Copyright (c) Connor McGarr. All rights reserved.
*
* @file:      Vtl1Mon/Nodes.cpp
*
* @summary:   Structured data implementation for correlating events.
*
* @author:    Connor McGarr (@33y0re)
*
* @copyright  Use of this source code is governed by a MIT-style license that
*             can be found in the LICENSE file.
*
--*/
#include "Nodes.hpp"
#include "Helpers.hpp"
#include "Symbols.hpp"

/**
*
* @brief        Inserts a loaded image (notified via ETW) into the image map.
* @param[in]    ImageBase - The base address of the target module being loaded.
* @param[in]    ImageSize - The size of the loaded image.
* @param[in]    ImagePath - The NT path of the loaded image.
* @return       true on success, otherwise false.
*
*/
bool
InsertImage (
    _In_ ULONG_PTR ImageBase,
    _In_ ULONG ImageSize,
    _In_ wchar_t* ImageName
    )
{
    bool doNotIgnore;
    wchar_t* imageNameCopy;
    SIZE_T imageNameLength;
    IMAGE_NODE imageNode;

    doNotIgnore = true;
    imageNameCopy = nullptr;
    imageNameLength = 0;

    RtlZeroMemory(&imageNode, sizeof(imageNode));

    //
    // Ignore duplicates
    //
    auto it = k_ImageMap.find(ImageBase);
    if (it != k_ImageMap.end())
    {
        doNotIgnore = false;
        goto Exit;
    }

    imageNameLength = (wcslen(ImageName) * sizeof(wchar_t) + sizeof(UNICODE_NULL));
    imageNameCopy = static_cast<wchar_t*>(malloc(imageNameLength));
    if (imageNameCopy == NULL)
    {
        goto Exit;
    }

    RtlCopyMemory(imageNameCopy,
                  ImageName,
                  imageNameLength);

    //
    // Fill out the structure to insert.
    //
    imageNode.ImageBase = ImageBase;
    imageNode.ImageName = imageNameCopy;
    imageNode.ImageSize = ImageSize;

    //
    // Insert
    //
    k_ImageMap.insert({ ImageBase, imageNode });

Exit:
    return doNotIgnore;
}

/**
*
* @brief        Retrieves, from the image map, the image housing a target address.
* @param[in]    TargetAddress - The target address.
* @param[out]   ImageNode - The image node from the map.
* @return       true on success, otherwise false.
*
*/
bool
GetImageDataFromAddress (
    _In_ ULONG_PTR TargetAddress,
    _Out_ PIMAGE_NODE ImageNode
    )
{
    bool result;
    ULONG_PTR baseAddress;
    ULONG_PTR endAddress;

    result = false;
    baseAddress = 0;
    endAddress = 0;

    RtlZeroMemory(ImageNode, sizeof(IMAGE_NODE));

    //
    // Try to locate an associated image!
    //
    for (const auto& i : k_ImageMap)
    {
        baseAddress = i.first;
        endAddress = (baseAddress + i.second.ImageSize);
        if ((TargetAddress >= baseAddress) &&
            (TargetAddress <= endAddress))
        {
            //
            // We found the target node!
            //
            RtlCopyMemory(ImageNode,
                          &i.second,
                          sizeof(IMAGE_NODE));

            result = true;
            break;
        }
    }

    return result;
}

/**
*
* @brief        Inserts the "primal" VTL 1 enter event into the VTL 1 event map.
* @param[in]    TimeStamp - The event timestamp.
* @param[in]    ProcessId - The target process ID.
* @param[in]    ThreadId - The target thread ID.
* @param[in]    SecureCallNumber - The target secure call value.
*
*/
void
InsertVtl1EnterEventData (
    _In_ ULONGLONG TimeStamp,
    _In_ ULONG ProcessId,
    _In_ ULONG ThreadId,
    _In_ unsigned __int16 SecureCallNumber
    )
{
    VTL1_ENTER_NODE vtl1Node;

    RtlZeroMemory(&vtl1Node, sizeof(vtl1Node));

    vtl1Node.Vtl1EnterTime = TimeStamp;
    vtl1Node.ProcessId = ProcessId;
    vtl1Node.ThreadId = ThreadId;
    vtl1Node.SecureCallNumber = SecureCallNumber;

    k_VtlEnterMap.insert({ TimeStamp , vtl1Node });
}

/**
*
* @brief        Correlates a given stack walk event with a VTL 1 enter node.
* @param[in]    TimeStamp - The  stack walk event timestamp.
* @param[in]    CallStack - The call stack.
* @param[in]    NumberOfFrames - The number of frames in the call stack.
*
*/
void
CorrelateVtl1EnterCallStack (
    _In_ ULONGLONG TimeStamp,
    _In_ ULONG_PTR* CallStack,
    _In_ ULONG NumberOfFrames
    )
{
    auto it = k_VtlEnterMap.find(TimeStamp);
    if (it == k_VtlEnterMap.end())
    {
        goto Exit;
    }

    //
    // This will:
    //   1. Resolve symbols associated with the call stack
    //   2. Create a "call stack string"
    //   3. Write the associated VTL 1 enter data and the call stack to the output file
    //
    ConstructCallStackStringAndPublishData(&it->second,
                                           CallStack,
                                           NumberOfFrames);

    //
    // Done!
    //
    k_VtlEnterMap.erase(it);

Exit:
    return;
}