/** @file
  PCI bus tree scanning and the device list used to remember what was found.
**/
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include "PciTree.h"

BOOLEAN
IsPcieDevice (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN UINT16                   Segment,
  IN UINT8                    BusNum,
  IN UINT8                    DevNum,
  IN UINT8                    FuncNum
  )
{
  UINT16  Status;
  UINT8   CapPtr;
  UINT8   CapId;
  UINTN   MaxHops;

  // Status Register (offset 0x06) bit 4 = Capabilities List present.
  Status = PciCfgRead16 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x06);
  if ((Status & BIT4) == 0) {
    return FALSE;
  }

  // First capability pointer (offset 0x34).
  CapPtr  = PciCfgRead8 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x34) & 0xFC;
  MaxHops = 48; // Guard against a corrupted/cyclic capability chain.

  while ((CapPtr >= 0x40) && (MaxHops-- > 0)) {
    CapId = PciCfgRead8 (SegmentTable, Segment, BusNum, DevNum, FuncNum, CapPtr);
    if (CapId == 0x10) { // PCI Express Capability
      return TRUE;
    }

    CapPtr = PciCfgRead8 (SegmentTable, Segment, BusNum, DevNum, FuncNum, CapPtr + 1) & 0xFC;
  }

  return FALSE;
}

/**
  Allocate and initialize one NODE. Returns NULL on allocation failure.
**/
STATIC
NODE *
CreateNode (
  IN UINT16   Segment,
  IN UINT8    Bus,
  IN UINT8    Dev,
  IN UINT8    Fun,
  IN BOOLEAN  IsPcie,
  IN BOOLEAN  IsBridge
  )
{
  NODE  *Node;

  Node = AllocateZeroPool (sizeof (NODE));
  if (Node == NULL) {
    return NULL;
  }

  Node->Segment = Segment;
  Node->Bus     = Bus;
  Node->Dev     = Dev;
  Node->Fun     = Fun;
  Node->IsPcie  = IsPcie;
  Node->IsBridge = IsBridge;
  return Node;
}

/**
  Walk a NextSibling chain to its last node. Returns NULL if Node is NULL.
**/
STATIC
NODE *
LastSibling (
  IN NODE  *Node
  )
{
  if (Node == NULL) {
    return NULL;
  }

  while (Node->NextSibling != NULL) {
    Node = Node->NextSibling;
  }

  return Node;
}

VOID
ScanPciBus (
  IN     CONST PCI_SEGMENT_TABLE  *SegmentTable,
  OUT    NODE                     **SubtreeHead,
  IN     UINT16                   Segment,
  IN     UINT8                    BusNum
  )
{
  UINT8    DevNum;
  UINT8    FuncNum;
  UINT8    MaxFun;
  UINT8    SecondBus;
  UINT32   VendorID;
  UINT8    MultiFunBit;
  BOOLEAN  IsBridgeFlag;
  BOOLEAN  IsPcie;
  NODE     *Head;
  NODE     *Tail;
  NODE     *NewNode;

  Head = NULL;
  Tail = NULL;

  for (DevNum = 0; DevNum <= 0x1F; DevNum++) {
    MultiFunBit = 0;

    for (FuncNum = 0, MaxFun = 1; FuncNum < MaxFun; FuncNum++) {
      VendorID = PciCfgRead32 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x0);
      if ((VendorID == 0xFFFFFFFF) || (VendorID == 0)) {
        // No device at this Bus/Dev/Func.
        continue;
      }

      if (FuncNum == 0) {
        // The multi-function bit is only meaningful in function 0's header type.
        MultiFunBit = PciCfgRead8 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x0E) & BIT7;
        if (MultiFunBit) {
          MaxFun = 0x8;
        }
      }

      IsBridgeFlag = (PciCfgRead8 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x0E) & 0x1) != 0;
      IsPcie       = IsPcieDevice (SegmentTable, Segment, BusNum, DevNum, FuncNum);

      NewNode = CreateNode (Segment, BusNum, DevNum, FuncNum, IsPcie, IsBridgeFlag);
      if (NewNode == NULL) {
        continue;
      }

      if (Tail == NULL) {
        Head = NewNode;
      } else {
        Tail->NextSibling = NewNode;
      }

      Tail = NewNode;

      if (IsBridgeFlag) {
        SecondBus = PciCfgRead8 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x19);
        ScanPciBus (SegmentTable, &NewNode->FirstChild, Segment, SecondBus);
      }
    }
  }

  *SubtreeHead = Head;
}

VOID
ScanAllSegments (
  IN     CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN OUT PCI_DEVICE_LIST          *List
  )
{
  UINTN  Index;
  NODE   *SegRoot;
  NODE   *Tail;

  List->Root = NULL;

  if (SegmentTable->Count == 0) {
    // No MCFG table - assume Segment 0 only, reachable via legacy CF8 access.
    ScanPciBus (SegmentTable, &List->Root, 0, 0);
    return;
  }

  Tail = NULL;
  for (Index = 0; Index < SegmentTable->Count; Index++) {
    ScanPciBus (SegmentTable, &SegRoot, SegmentTable->Segments[Index].Segment, SegmentTable->Segments[Index].StartBus);
    if (SegRoot == NULL) {
      continue;
    }

    if (Tail == NULL) {
      List->Root = SegRoot;
    } else {
      Tail->NextSibling = SegRoot;
    }

    Tail = LastSibling (SegRoot);
  }
}

VOID
PciTreeWalk (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN NODE                     *Node,
  IN UINT8                    Depth,
  IN PCI_NODE_VISITOR         Visitor,
  IN VOID                     *Context
  )
{
  while (Node != NULL) {
    Visitor (SegmentTable, Node, Depth, Context);
    if (Node->FirstChild != NULL) {
      PciTreeWalk (SegmentTable, Node->FirstChild, Depth + 1, Visitor, Context);
    }

    Node = Node->NextSibling;
  }
}

/**
  Recursively free Node, its FirstChild subtree, and its NextSibling chain.
**/
STATIC
VOID
FreeSubtree (
  IN NODE  *Node
  )
{
  NODE  *NextSib;

  while (Node != NULL) {
    NextSib = Node->NextSibling;
    if (Node->FirstChild != NULL) {
      FreeSubtree (Node->FirstChild);
    }

    gBS->FreePool (Node);
    Node = NextSib;
  }
}

VOID
FreeDeviceList (
  IN OUT PCI_DEVICE_LIST  *List
  )
{
  FreeSubtree (List->Root);
  List->Root = NULL;
}
