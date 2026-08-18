/** @file
  Printing PCI device names and dumping their configuration space / BARs.
**/
#ifndef _REGISTER_DUMP_H_
#define _REGISTER_DUMP_H_

#include <Uefi.h>
#include "PciConfigAccess.h"

/**
  Print one line describing a device: vendor name, class/type, PCIe tag and
  Seg/Bus/Dev/Fun, indented to IndentNum levels.

  @param  IsPcie     Whether the device implements the PCI Express Capability.
**/
VOID
DisplayDeviceName (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN UINT16                   Seg,
  IN UINT8                    Bus,
  IN UINT8                    Dev,
  IN UINT8                    Fun,
  IN UINT32                   IndentNum,
  IN BOOLEAN                  IsPcie
  );

/**
  Dump all configuration registers of one function in the requested width
  (8/16/32-bit), printed as a hex table. For PCIe devices, when ExtendSpace
  is TRUE and reading 8-bit registers, the extended configuration space
  (0x100-0xFFF) is also dumped via ECAM - this requires the segment to have
  a known ECAM base; if it doesn't, the extended range reads back as all-FF.

  @param  ReadType     8, 16, or 32.
  @param  IsPcie       Whether the device implements the PCI Express Capability.
  @param  ExtendSpace  Whether to also dump the extended configuration space (the "/Extend" flag).
**/
VOID
DumpDeviceRegisters (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN UINT16                   Seg,
  IN UINT8                    Bus,
  IN UINT8                    Dev,
  IN UINT8                    Fun,
  IN UINT32                   ReadType,
  IN BOOLEAN                  IsPcie,
  IN BOOLEAN                  ExtendSpace
  );

/**
  Print the device's BARs, and for bridges, the I/O/Memory/Prefetchable
  windows forwarded to its secondary bus.

  @param  IsBridge  Whether this device is a PCI-to-PCI bridge.
**/
VOID
DumpBar (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN UINT16                   Seg,
  IN UINT8                    Bus,
  IN UINT8                    Dev,
  IN UINT8                    Fun,
  IN BOOLEAN                  IsBridge
  );

#endif
