/** @file
  PCI bus tree scanning and the device list used to remember what was found.
**/
#ifndef _PCI_TREE_H_
#define _PCI_TREE_H_

#include <Uefi.h>
#include "PciConfigAccess.h"

typedef struct _NODE {
  UINT16          Segment;
  UINT8           Bus;
  UINT8           Dev;
  UINT8           Fun;
  BOOLEAN         IsPcie;
  BOOLEAN         IsBridge;
  UINT8           IndentNum;
  struct _NODE    *Next;
} NODE;

typedef struct {
  NODE    *Head;
  NODE    *Tail;
} PCI_DEVICE_LIST;

/**
  Check whether Status Register bit4 (Capabilities List) is set and, if so,
  walk the capability list looking for the PCI Express Capability (0x10).

  @retval TRUE   The device implements the PCI Express Capability.
  @retval FALSE  It does not (or has no capability list at all).
**/
BOOLEAN
IsPcieDevice (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN UINT16                   Segment,
  IN UINT8                    BusNum,
  IN UINT8                    DevNum,
  IN UINT8                    FuncNum
  );

/**
  Recursively scan a PCI bus (and any bus reachable through a bridge on it)
  within one segment, appending one NODE per device/function found to List.

  @param  SegmentTable  Segment/ECAM info, used for reads beyond legacy CF8 range.
  @param  List          The list to append discovered devices to.
  @param  Segment       The PCI Segment Group to scan.
  @param  BusNum        The bus number to scan.
  @param  IndentNum     Tree-display indent level for devices found on this bus.
**/
VOID
ScanPciBus (
  IN     CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN OUT PCI_DEVICE_LIST          *List,
  IN     UINT16                   Segment,
  IN     UINT8                    BusNum,
  IN     UINT8                    IndentNum
  );

/**
  Scan every segment in SegmentTable (starting at each segment's StartBus).
  If SegmentTable is empty (no MCFG table was found), falls back to scanning
  Segment 0 / Bus 0 alone via legacy CF8 access, matching pre-MCFG behavior.
**/
VOID
ScanAllSegments (
  IN     CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN OUT PCI_DEVICE_LIST          *List
  );

/**
  Free every node in List. List is reset to empty.
**/
VOID
FreeDeviceList (
  IN OUT PCI_DEVICE_LIST  *List
  );

#endif
