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
  NODE                 *Node;

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

      IsPcie = IsPcieDevice (&SegmentTable, Options.Segment, Options.Bus, Options.Dev, Options.Func);
      DisplayDeviceName (&SegmentTable, Options.Segment, Options.Bus, Options.Dev, Options.Func, 0, IsPcie);
      DumpDeviceRegisters (&SegmentTable, Options.Segment, Options.Bus, Options.Dev, Options.Func, Options.ReadType, IsPcie, Options.ExtendFlag);
      DumpBar (&SegmentTable, Options.Segment, Options.Bus, Options.Dev, Options.Func, FALSE);
      break;
    }

    case PciCmdAll:
      for (Node = List.Head; Node != NULL; Node = Node->Next) {
        DisplayDeviceName (&SegmentTable, Node->Segment, Node->Bus, Node->Dev, Node->Fun, 0, Node->IsPcie);
        DumpDeviceRegisters (&SegmentTable, Node->Segment, Node->Bus, Node->Dev, Node->Fun, Options.ReadType, Node->IsPcie, Options.ExtendFlag);
        DumpBar (&SegmentTable, Node->Segment, Node->Bus, Node->Dev, Node->Fun, Node->IsBridge);
      }

      break;

    case PciCmdTree:
      for (Node = List.Head; Node != NULL; Node = Node->Next) {
        DisplayDeviceName (&SegmentTable, Node->Segment, Node->Bus, Node->Dev, Node->Fun, Node->IndentNum, Node->IsPcie);
      }

      break;

    default:
      break;
  }

  FreeDeviceList (&List);
  FreeSegmentTable (&SegmentTable);

  return EFI_SUCCESS;
}
