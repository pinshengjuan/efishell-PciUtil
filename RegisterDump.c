/** @file
  Printing PCI device names and dumping their configuration space / BARs.
**/
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include "RegisterDump.h"
#include "PciIdNames.h"

/**
  Print a table of ElementCount elements (each ElementSizeBytes wide), 16
  bytes worth of elements per row, prefixed with a column-index header row
  and a byte-offset label at the start of each data row.
**/
STATIC
VOID
DumpRegisterTable (
  IN CONST UINT8  *RegisterValues,
  IN UINTN        ElementSizeBytes,
  IN UINTN        ElementCount
  )
{
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *ConOut;
  UINTN                            ElementsPerRow;
  UINTN                            RowCount;
  UINTN                            Row;
  UINTN                            Col;
  UINTN                            Offset;
  INT32                            SavedAttribute;

  ConOut         = gST->ConOut;
  SavedAttribute = ConOut->Mode->Attribute;
  ElementsPerRow = 16 / ElementSizeBytes;
  RowCount       = (ElementCount + ElementsPerRow - 1) / ElementsPerRow;

  for (Row = 0; Row <= RowCount; Row++) {
    if (Row == 0) {
      Print (L"     ");
    } else {
      ConOut->SetAttribute (ConOut, EFI_LIGHTRED | EFI_BACKGROUND_BLACK);
      Print (L"%03X0 ", Row - 1);
    }

    for (Col = 0; Col < ElementsPerRow; Col++) {
      if (Row == 0) {
        ConOut->SetAttribute (ConOut, EFI_LIGHTRED | EFI_BACKGROUND_BLACK);
        switch (ElementSizeBytes) {
          case 1:
            Print (L"%02X ", Col);
            break;
          case 2:
            Print (L"%04X ", Col * 2);
            break;
          default:
            Print (L"%08X ", Col * 4);
            break;
        }

        continue;
      }

      Offset = (Row - 1) * ElementsPerRow + Col;
      if (Offset >= ElementCount) {
        continue;
      }

      ConOut->SetAttribute (ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
      switch (ElementSizeBytes) {
        case 1:
          Print (L"%02X ", RegisterValues[Offset]);
          break;
        case 2:
          Print (L"%04X ", ((CONST UINT16 *)RegisterValues)[Offset]);
          break;
        default:
          Print (L"%08X ", ((CONST UINT32 *)RegisterValues)[Offset]);
          break;
      }
    }

    Print (L"\r\n");
  }

  ConOut->SetAttribute (ConOut, SavedAttribute);
}

VOID
DisplayDeviceName (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN UINT16                   Segment,
  IN UINT8                    BusNum,
  IN UINT8                    DevNum,
  IN UINT8                    FuncNum,
  IN UINT32                   IndentNum,
  IN BOOLEAN                  IsPcie
  )
{
  CHAR16  *Vendorname;
  UINT32  i;

  Vendorname = PciVendorName (PciCfgRead16 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x00));

  for (i = 0; i < IndentNum; i++) {
    Print (L"  ");
  }

  Print (L"%s", Vendorname);
  if ((Vendorname != NULL) && (StrCmp (Vendorname, L"") != 0)) {
    Print (L" ");
  }

  // Class Code is the DWORD at offset 0x08 (Revision ID, ProgIf, SubClass, BaseClass).
  Print (
    L"%s %s",
    PciClassName (PciCfgRead32 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x08)),
    PciTypeName (PciCfgRead8 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x0B))
    );

  if (IsPcie) {
    Print (L"(PCIE)");
  }

  Print (L" (Seg%04X/Bus%02X/Dev%02X/Func%02X)\r\n", Segment, BusNum, DevNum, FuncNum);
}

VOID
DumpDeviceRegisters (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN UINT16                   Segment,
  IN UINT8                    BusNum,
  IN UINT8                    DevNum,
  IN UINT8                    FuncNum,
  IN UINT32                   ReadType,
  IN BOOLEAN                  IsPcie,
  IN BOOLEAN                  ExtendSpace
  )
{
  UINT32  RegNum;
  UINT8   *RegArrayByte;
  UINT16  *RegArrayWord;
  UINT32  *RegArrayDWord;
  UINTN   ByteCount;
  BOOLEAN DumpExtended;

  Print (L"Segment: %04X, Bus: %02X, Device: %02X, Function: %02X\r\n", Segment, BusNum, DevNum, FuncNum);

  switch (ReadType) {
    case 8:
      DumpExtended = ExtendSpace && IsPcie;
      ByteCount    = DumpExtended ? 0x1000 : 0x100;

      RegArrayByte = AllocateZeroPool (ByteCount);
      if (RegArrayByte == NULL) {
        break;
      }

      for (RegNum = 0; RegNum <= 0xFF; RegNum++) {
        RegArrayByte[RegNum] = PciCfgRead8 (SegmentTable, Segment, BusNum, DevNum, FuncNum, (UINT16)RegNum);
      }

      if (DumpExtended) {
        for (RegNum = 0x100; RegNum < 0x1000; RegNum++) {
          RegArrayByte[RegNum] = PciCfgRead8 (SegmentTable, Segment, BusNum, DevNum, FuncNum, (UINT16)RegNum);
        }
      }

      DumpRegisterTable (RegArrayByte, 1, ByteCount);
      gBS->FreePool (RegArrayByte);
      break;

    case 16:
      RegArrayWord = AllocateZeroPool (128 * sizeof (UINT16));
      if (RegArrayWord == NULL) {
        break;
      }

      for (RegNum = 0; RegNum <= 0xFF; RegNum += 2) {
        RegArrayWord[RegNum / 2] = PciCfgRead16 (SegmentTable, Segment, BusNum, DevNum, FuncNum, (UINT16)RegNum);
      }

      DumpRegisterTable ((UINT8 *)RegArrayWord, 2, 128);
      gBS->FreePool (RegArrayWord);
      break;

    case 32:
      RegArrayDWord = AllocateZeroPool (64 * sizeof (UINT32));
      if (RegArrayDWord == NULL) {
        break;
      }

      for (RegNum = 0; RegNum <= 0xFF; RegNum += 4) {
        RegArrayDWord[RegNum / 4] = PciCfgRead32 (SegmentTable, Segment, BusNum, DevNum, FuncNum, (UINT16)RegNum);
      }

      DumpRegisterTable ((UINT8 *)RegArrayDWord, 4, 64);
      gBS->FreePool (RegArrayDWord);
      break;

    default:
      break;
  }

  Print (L"\r\n");
}

VOID
DumpBar (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN UINT16                   Segment,
  IN UINT8                    BusNum,
  IN UINT8                    DevNum,
  IN UINT8                    FuncNum,
  IN BOOLEAN                  IsBridge
  )
{
  UINT32  BarNum;
  UINT32  RegNum;
  UINT32  MaxRegNum;
  UINT8   IoBase;
  UINT8   IoLimit;
  UINT16  MemoryBase;
  UINT16  MemoryLimit;
  UINT16  PrefetchMemoryBase;
  UINT16  PrefetchMemoryLimit;

  MaxRegNum = 0x24;

  if (IsBridge) {
    MaxRegNum = 0x14;

    IoBase  = PciCfgRead8 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x1C);
    IoLimit = PciCfgRead8 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x1D);
    Print (L"I/O Base: %02X ; I/O Limit: %02X\r\n", IoBase, IoLimit);

    MemoryBase  = PciCfgRead16 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x20);
    MemoryLimit = PciCfgRead16 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x22);
    Print (L"Memory Base: %04X ; Memory Limit: %04X\r\n", MemoryBase, MemoryLimit);

    PrefetchMemoryBase  = PciCfgRead16 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x24);
    PrefetchMemoryLimit = PciCfgRead16 (SegmentTable, Segment, BusNum, DevNum, FuncNum, 0x26);
    Print (L"Prefetch Memory Base: %04X ; Prefetch_Memory_Limit: %04X\r\n", PrefetchMemoryBase, PrefetchMemoryLimit);
  }

  for (RegNum = 0x10; RegNum <= MaxRegNum; RegNum += 4) {
    BarNum = PciCfgRead32 (SegmentTable, Segment, BusNum, DevNum, FuncNum, (UINT16)RegNum);
    if (BarNum & 0x1) {
      // Bit 0 = 1: I/O Space
      BarNum &= 0xFFFFFFFC;
      Print (L"BAR%d: %08X, I/O Space\r\n", (RegNum - 16) >> 2, BarNum);
    } else {
      // Bit 0 = 0: Memory Space
      BarNum &= 0xFFFFFFF0;
      Print (L"BAR%d: %08X, Memory Space\r\n", (RegNum - 16) >> 2, BarNum);
    }
  }

  Print (L"\r\n\r\n");
}
