/** @file
  PCI bus tree scanning and the device list used to remember what was found.
**/
#ifndef _PCI_TREE_H_
#define _PCI_TREE_H_

#include <Uefi.h>
#include "PciConfigAccess.h"

typedef struct _NODE {
  UINT16          Seg;
  UINT8           Bus;
  UINT8           Dev;
  UINT8           Fun;
  BOOLEAN         IsPcie;
  BOOLEAN         IsBridge;
  struct _NODE    *FirstChild;   // First device behind this node (bridges only).
  struct _NODE    *NextSibling;  // Next device at the same tree level.
} NODE;

typedef struct {
  NODE    *Root;   // First top-level sibling (a root-bus device of some segment).
} PCI_DEVICE_LIST;

/**
  Called once per node by PciTreeWalk, in the same order a depth-first
  pre-order traversal visits them (a node before its children, children
  before its next sibling).

  @param  Depth  How many bridge hops deep Node is (0 = a root-bus device).
**/
typedef VOID (EFIAPI *PCI_NODE_VISITOR)(
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN NODE                     *Node,
  IN UINT8                    Depth,
  IN VOID                     *Context OPTIONAL
  );

/**
  Recursively walk Node and its FirstChild/NextSibling tree in pre-order,
  invoking Visitor on each node.
**/
VOID
PciTreeWalk (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN NODE                     *Node,
  IN UINT8                    Depth,
  IN PCI_NODE_VISITOR         Visitor,
  IN VOID                     *Context OPTIONAL
  );

/**
  Check whether Status Register bit4 (Capabilities List) is set and, if so,
  walk the capability list looking for the PCI Express Capability (0x10).

  @retval TRUE   The device implements the PCI Express Capability.
  @retval FALSE  It does not (or has no capability list at all).
**/
BOOLEAN
IsPcieDevice (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN UINT16                   Seg,
  IN UINT8                    Bus,
  IN UINT8                    Dev,
  IN UINT8                    Fun
  );

/**
  Recursively scan a PCI bus (and any bus reachable through a bridge on it)
  within one segment, building a NODE tree (FirstChild/NextSibling) for every
  device/function found.

  @param  SegmentTable  Segment/ECAM info, used for reads beyond legacy CF8 range.
  @param  SubtreeHead   On return, the head of the NextSibling chain of devices
                         found directly on Bus (their descendants, if any,
                         hang off each node's FirstChild).
  @param  Seg           The PCI Segment Group to scan.
  @param  Bus           The bus number to scan.
**/
VOID
ScanPciBus (
  IN     CONST PCI_SEGMENT_TABLE  *SegmentTable,
  OUT    NODE                     **SubtreeHead,
  IN     UINT16                   Seg,
  IN     UINT8                    Bus
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
