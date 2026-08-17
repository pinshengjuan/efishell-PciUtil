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
  Create one NODE and append it to the tail of List in O(1).
**/
STATIC
VOID
AppendNode (
  IN OUT PCI_DEVICE_LIST  *List,
  IN     UINT16           Segment,
  IN     UINT8            Bus,
  IN     UINT8            Dev,
  IN     UINT8            Fun,
  IN     BOOLEAN          IsPcie,
  IN     BOOLEAN          IsBridge,
  IN     UINT8            IndentNum
  )
{
  NODE  *Node;

  Node = AllocateZeroPool (sizeof (NODE));
  if (Node == NULL) {
    return;
  }

  Node->Segment   = Segment;
  Node->Bus       = Bus;
  Node->Dev       = Dev;
  Node->Fun       = Fun;
  Node->IsPcie    = IsPcie;
  Node->IsBridge  = IsBridge;
  Node->IndentNum = IndentNum;
  Node->Next      = NULL;

  if (List->Tail == NULL) {
    List->Head = Node;
  } else {
    List->Tail->Next = Node;
  }

  List->Tail = Node;
}

VOID
ScanPciBus (
  IN     CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN OUT PCI_DEVICE_LIST          *List,
  IN     UINT16                   Segment,
  IN     UINT8                    BusNum,
  IN     UINT8                    IndentNum
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
      AppendNode (List, Segment, BusNum, DevNum, FuncNum, IsPcie, IsBridgeFlag, IndentNum);

      if (IsBridgeFlag) {
        SecondBus = PciCfgRead8 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x19);
        ScanPciBus (SegmentTable, List, Segment, SecondBus, IndentNum + 1);
      }
    }
  }
}

VOID
ScanAllSegments (
  IN     CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN OUT PCI_DEVICE_LIST          *List
  )
{
  UINTN  Index;

  if (SegmentTable->Count == 0) {
    // No MCFG table - assume Segment 0 only, reachable via legacy CF8 access.
    ScanPciBus (SegmentTable, List, 0, 0, 0);
    return;
  }

  for (Index = 0; Index < SegmentTable->Count; Index++) {
    ScanPciBus (SegmentTable, List, SegmentTable->Segments[Index].Segment, SegmentTable->Segments[Index].StartBus, 0);
  }
}

VOID
FreeDeviceList (
  IN OUT PCI_DEVICE_LIST  *List
  )
{
  NODE  *Node;

  while (List->Head != NULL) {
    Node       = List->Head;
    List->Head = Node->Next;
    gBS->FreePool (Node);
  }

  List->Tail = NULL;
}
