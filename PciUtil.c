/** @file
  PciUtil - a UEFI Shell application that dumps PCI device information:
  a tree of enumerated devices, per-device configuration space, and BARs.
**/
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include "CliParser.h"
#include "PciTree.h"
#include "PciConfigAccess.h"
#include "RegisterDump.h"

/**
  PciTreeWalk visitor for "all": dumps full config space + BARs for every
  device, flat (no indentation).
**/
STATIC
VOID
EFIAPI
VisitAllDevice (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN NODE                     *Node,
  IN UINT8                    Depth,
  IN VOID                     *Context
  )
{
  PCI_CLI_OPTIONS  *Options;

  Options = (PCI_CLI_OPTIONS *)Context;

  DisplayDeviceName (SegmentTable, Node->Seg, Node->Bus, Node->Dev, Node->Fun, 0, Node->IsPcie);
  DumpDeviceRegisters (SegmentTable, Node->Seg, Node->Bus, Node->Dev, Node->Fun, Options->ReadType, Node->IsPcie, Options->ExtendFlag);
  DumpBar (SegmentTable, Node->Seg, Node->Bus, Node->Dev, Node->Fun, Node->IsBridge);
}

/**
  PciTreeWalk visitor for "pcitree": prints one indented line per device.
**/
STATIC
VOID
EFIAPI
VisitTreeDevice (
  IN CONST PCI_SEGMENT_TABLE  *SegmentTable,
  IN NODE                     *Node,
  IN UINT8                    Depth,
  IN VOID                     *Context
  )
{
  DisplayDeviceName (SegmentTable, Node->Seg, Node->Bus, Node->Dev, Node->Fun, Depth, Node->IsPcie);
}

EFI_STATUS
EFIAPI
PciUtilEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  PCI_CLI_OPTIONS     Options;
  PCI_SEGMENT_TABLE   SegmentTable;
  PCI_DEVICE_LIST     List;

  switch (ParseArguments (&Options)) {
    case PciParseHelp:
    case PciParseError:
      return EFI_SUCCESS;
    default:
      break;
  }

  if (Options.Mode == PciCmdNone) {
    Print (L"Please type '%s -h' for more information.\r\n", GetProgramName ());
    return EFI_SUCCESS;
  }

  BuildSegmentTable (SystemTable, &SegmentTable);

  ZeroMem (&List, sizeof (List));
  ScanAllSegments (&SegmentTable, &List);

  switch (Options.Mode) {
    case PciCmdSingle:
    {
      BOOLEAN  IsPcie;

      IsPcie = IsPcieDevice (&SegmentTable, Options.Seg, Options.Bus, Options.Dev, Options.Fun);
      DisplayDeviceName (&SegmentTable, Options.Seg, Options.Bus, Options.Dev, Options.Fun, 0, IsPcie);
      DumpDeviceRegisters (&SegmentTable, Options.Seg, Options.Bus, Options.Dev, Options.Fun, Options.ReadType, IsPcie, Options.ExtendFlag);
      DumpBar (&SegmentTable, Options.Seg, Options.Bus, Options.Dev, Options.Fun, FALSE);
      break;
    }

    case PciCmdAll:
      PciTreeWalk (&SegmentTable, List.Root, 0, VisitAllDevice, &Options);
      break;

    case PciCmdTree:
      PciTreeWalk (&SegmentTable, List.Root, 0, VisitTreeDevice, NULL);
      break;

    default:
      break;
  }

  FreeDeviceList (&List);
  FreeSegmentTable (&SegmentTable);

  return EFI_SUCCESS;
}
