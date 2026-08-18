/** @file
  PCI Segment Group aware configuration space access.
**/
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/PciLib.h>
#include <Guid/Acpi.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include "PciConfigAccess.h"

typedef EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE MCFG_ENTRY;

VOID
BuildSegmentTable (
  IN  EFI_SYSTEM_TABLE   *SystemTable,
  OUT PCI_SEGMENT_TABLE  *Table
  )
{
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER                     *Rsdp;
  EFI_ACPI_DESCRIPTION_HEADER                                      *Xsdt;
  EFI_ACPI_DESCRIPTION_HEADER                                      *AcpiTable;
  EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER   *Mcfg;
  MCFG_ENTRY                                                       *McfgEntry;
  UINT64                                                           *XsdtEntry;
  UINTN                                                            XsdtEntryCount;
  UINTN                                                            McfgEntryCount;
  UINTN                                                            Index;

  Table->Segments = NULL;
  Table->Count    = 0;

  Rsdp = NULL;
  for (Index = 0; Index < SystemTable->NumberOfTableEntries; Index++) {
    if (CompareGuid (&(SystemTable->ConfigurationTable[Index].VendorGuid), &gEfiAcpi20TableGuid) ||
        CompareGuid (&(SystemTable->ConfigurationTable[Index].VendorGuid), &gEfiAcpiTableGuid))
    {
      Rsdp = (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)SystemTable->ConfigurationTable[Index].VendorTable;
      break;
    }
  }

  if ((Rsdp == NULL) || (Rsdp->XsdtAddress == 0)) {
    return;
  }

  Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->XsdtAddress;
  if (Xsdt->Length < sizeof (EFI_ACPI_DESCRIPTION_HEADER)) {
    // Corrupted table header - bail out rather than underflow the entry count below.
    return;
  }

  XsdtEntry      = (UINT64 *)(Xsdt + 1);
  XsdtEntryCount = (Xsdt->Length - sizeof (EFI_ACPI_DESCRIPTION_HEADER)) / sizeof (UINT64);

  Mcfg = NULL;
  for (Index = 0; Index < XsdtEntryCount; Index++) {
    AcpiTable = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)XsdtEntry[Index];
    if (AcpiTable->Signature == SIGNATURE_32 ('M', 'C', 'F', 'G')) {
      Mcfg = (EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER *)AcpiTable;
      break;
    }
  }

  if ((Mcfg == NULL) ||
      (Mcfg->Header.Length < sizeof (EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER) + sizeof (MCFG_ENTRY)))
  {
    // No MCFG table, or it is too short to contain even one base address allocation entry.
    return;
  }

  // Base address allocation structures immediately follow the MCFG header, one per segment group.
  McfgEntryCount = (Mcfg->Header.Length - sizeof (EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER)) / sizeof (MCFG_ENTRY);
  McfgEntry      = (MCFG_ENTRY *)(Mcfg + 1);

  Table->Segments = AllocateZeroPool (McfgEntryCount * sizeof (PCI_SEGMENT_INFO));
  if (Table->Segments == NULL) {
    return;
  }

  for (Index = 0; Index < McfgEntryCount; Index++) {
    Table->Segments[Index].Seg      = McfgEntry[Index].PciSegmentGroupNumber;
    Table->Segments[Index].EcamBase = McfgEntry[Index].BaseAddress;
    Table->Segments[Index].StartBus = McfgEntry[Index].StartBusNumber;
    Table->Segments[Index].EndBus   = McfgEntry[Index].EndBusNumber;
  }

  Table->Count = McfgEntryCount;
}

VOID
FreeSegmentTable (
  IN OUT PCI_SEGMENT_TABLE  *Table
  )
{
  if (Table->Segments != NULL) {
    gBS->FreePool (Table->Segments);
  }

  Table->Segments = NULL;
  Table->Count    = 0;
}

UINT64
FindEcamBaseForSegment (
  IN CONST PCI_SEGMENT_TABLE  *Table,
  IN UINT16                   Seg
  )
{
  UINTN  Index;

  for (Index = 0; Index < Table->Count; Index++) {
    if (Table->Segments[Index].Seg == Seg) {
      return Table->Segments[Index].EcamBase;
    }
  }

  return 0;
}

/**
  Compute the ECAM address for a Bus/Dev/Fun/Register, or return 0 if this
  segment has no known ECAM base.
**/
STATIC
UINT64
GetEcamAddress (
  IN CONST PCI_SEGMENT_TABLE  *Table,
  IN UINT16                   Seg,
  IN UINT8                    Bus,
  IN UINT8                    Dev,
  IN UINT8                    Fun,
  IN UINT16                   Register
  )
{
  UINT64  Base;

  Base = FindEcamBaseForSegment (Table, Seg);
  if (Base == 0) {
    return 0;
  }

  return Base + ((UINT64)Bus << 20) + ((UINT64)Dev << 15) + ((UINT64)Fun << 12) + Register;
}

UINT8
PciCfgRead8 (
  IN CONST PCI_SEGMENT_TABLE  *Table,
  IN UINT16                   Seg,
  IN UINT8                    Bus,
  IN UINT8                    Dev,
  IN UINT8                    Fun,
  IN UINT16                   Register
  )
{
  UINT64  EcamAddress;

  if ((Seg == 0) && (Register < 0x100)) {
    return PciRead8 (PCI_LIB_ADDRESS (Bus, Dev, Fun, Register));
  }

  EcamAddress = GetEcamAddress (Table, Seg, Bus, Dev, Fun, Register);
  if (EcamAddress == 0) {
    return 0xFF;
  }

  return MmioRead8 ((UINTN)EcamAddress);
}

UINT16
PciCfgRead16 (
  IN CONST PCI_SEGMENT_TABLE  *Table,
  IN UINT16                   Seg,
  IN UINT8                    Bus,
  IN UINT8                    Dev,
  IN UINT8                    Fun,
  IN UINT16                   Register
  )
{
  UINT64  EcamAddress;

  if ((Seg == 0) && (Register < 0x100)) {
    return PciRead16 (PCI_LIB_ADDRESS (Bus, Dev, Fun, Register));
  }

  EcamAddress = GetEcamAddress (Table, Seg, Bus, Dev, Fun, Register);
  if (EcamAddress == 0) {
    return 0xFFFF;
  }

  return MmioRead16 ((UINTN)EcamAddress);
}

UINT32
PciCfgRead32 (
  IN CONST PCI_SEGMENT_TABLE  *Table,
  IN UINT16                   Seg,
  IN UINT8                    Bus,
  IN UINT8                    Dev,
  IN UINT8                    Fun,
  IN UINT16                   Register
  )
{
  UINT64  EcamAddress;

  if ((Seg == 0) && (Register < 0x100)) {
    return PciRead32 (PCI_LIB_ADDRESS (Bus, Dev, Fun, Register));
  }

  EcamAddress = GetEcamAddress (Table, Seg, Bus, Dev, Fun, Register);
  if (EcamAddress == 0) {
    return 0xFFFFFFFF;
  }

  return MmioRead32 ((UINTN)EcamAddress);
}
