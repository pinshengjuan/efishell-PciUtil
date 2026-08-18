/** @file
  PCI Segment Group aware configuration space access.

  Segment 0 registers below 0x100 use the legacy CF8/CFC I/O port mechanism
  (via PciLib) and work even without an ACPI MCFG table. Everything else -
  other segments, and the extended configuration space (0x100-0xFFF) of any
  segment - requires ECAM and is only reachable if the platform publishes an
  MCFG table entry for that segment.
**/
#ifndef _PCI_CONFIG_ACCESS_H_
#define _PCI_CONFIG_ACCESS_H_

#include <Uefi.h>

typedef struct {
  UINT16    Seg;
  UINT64    EcamBase;
  UINT8     StartBus;
  UINT8     EndBus;
} PCI_SEGMENT_INFO;

typedef struct {
  PCI_SEGMENT_INFO    *Segments;
  UINTN               Count;
} PCI_SEGMENT_TABLE;

/**
  Build the segment table by parsing the ACPI MCFG table (RSDP -> XSDT -> MCFG).
  If no MCFG table can be found/parsed, Table is set to an empty (Count == 0)
  table - callers should treat that as "Segment 0 only, legacy CF8 access".

  @param  SystemTable  The UEFI System Table, used to reach the ACPI tables.
  @param  Table         Filled in. Free with FreeSegmentTable() when done.
**/
VOID
BuildSegmentTable (
  IN  EFI_SYSTEM_TABLE   *SystemTable,
  OUT PCI_SEGMENT_TABLE  *Table
  );

/**
  Free the array allocated by BuildSegmentTable().
**/
VOID
FreeSegmentTable (
  IN OUT PCI_SEGMENT_TABLE  *Table
  );

/**
  Look up the ECAM base address for a given segment.

  @return The ECAM base address, or 0 if that segment has no known ECAM range.
**/
UINT64
FindEcamBaseForSegment (
  IN CONST PCI_SEGMENT_TABLE  *Table,
  IN UINT16                   Seg
  );

//
// Segment-aware configuration space accessors. Segment 0 / Register < 0x100
// go through the legacy CF8/CFC mechanism; everything else goes through
// ECAM. If ECAM is required but unavailable for the requested segment, these
// return an all-ones sentinel (the same value real hardware returns for an
// unimplemented/absent device), so callers never need a separate presence
// check before calling them.
//
UINT8
PciCfgRead8 (
  IN CONST PCI_SEGMENT_TABLE  *Table,
  IN UINT16                   Seg,
  IN UINT8                    Bus,
  IN UINT8                    Dev,
  IN UINT8                    Fun,
  IN UINT16                   Register
  );

UINT16
PciCfgRead16 (
  IN CONST PCI_SEGMENT_TABLE  *Table,
  IN UINT16                   Seg,
  IN UINT8                    Bus,
  IN UINT8                    Dev,
  IN UINT8                    Fun,
  IN UINT16                   Register
  );

UINT32
PciCfgRead32 (
  IN CONST PCI_SEGMENT_TABLE  *Table,
  IN UINT16                   Seg,
  IN UINT8                    Bus,
  IN UINT8                    Dev,
  IN UINT8                    Fun,
  IN UINT16                   Register
  );

#endif
