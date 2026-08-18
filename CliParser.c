/** @file
  Command-line argument parsing for PciUtil, built on ShellCommandLineParseEx.
**/
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellLib.h>
#include "CliParser.h"

STATIC CONST SHELL_PARAM_ITEM  mParamList[] = {
  { L"-r", TypeValue },
  { L"-x", TypeFlag  },
  { L"-h", TypeFlag  },
  { NULL,  TypeMax   }
};

CONST CHAR16 *
GetProgramName (
  VOID
  )
{
  if ((gEfiShellParametersProtocol != NULL) &&
      (gEfiShellParametersProtocol->Argc > 0) &&
      (gEfiShellParametersProtocol->Argv[0] != NULL))
  {
    return gEfiShellParametersProtocol->Argv[0];
  }

  return L"PciUtil";
}

STATIC
VOID
PrintUsage (
  VOID
  )
{
  CONST CHAR16  *Name;

  Name = GetProgramName ();

  Print (L"%s <segment:bus:dev.func> -r 8|16|32 [-x]\r\n", Name);
  Print (L"%s all -r 8|16|32 [-x]\r\n", Name);
  Print (L"%s pcitree\r\n", Name);
  Print (L"\r\n");
  Print (L"  segment:bus:dev.func   Device address, e.g. \"00:1f.0\" or \"0000:00:1f.0\".\r\n");
  Print (L"                         \"segment:\" is optional and defaults to 0000.\r\n");
  Print (L"  all                    Dump every discovered device's registers and BARs.\r\n");
  Print (L"  pcitree                Dump the PCI device tree.\r\n");
  Print (L"  -r 8|16|32             Register read width (required for a device or \"all\").\r\n");
  Print (L"  -x                     Also dump 0x100-0xFFF for a PCIe device (with -r 8).\r\n");
  Print (L"  -h                     Show this help.\r\n");
}

/**
  Parse one hex field starting at *Cursor, requiring it to be terminated by
  Terminator (or the NUL terminator, if Terminator is CHAR_NULL). On success,
  *Cursor is advanced past the field and its terminator.
**/
STATIC
BOOLEAN
ParseHexField (
  IN OUT CONST CHAR16  **Cursor,
  IN     CHAR16         Terminator,
  IN     UINT64         MaxValue,
  OUT    UINT64         *Value
  )
{
  CHAR16  *End;

  StrHexToUint64S (*Cursor, &End, Value);
  if ((End == *Cursor) || (*End != Terminator) || (*Value > MaxValue)) {
    return FALSE;
  }

  *Cursor = (Terminator == CHAR_NULL) ? End : End + 1;
  return TRUE;
}

/**
  Parse a "[segment:]bus:dev.func" device address, e.g. "00:1f.0" or "0000:00:1f.0".
**/
STATIC
BOOLEAN
ParseBdf (
  IN  CONST CHAR16  *Str,
  OUT UINT16        *Seg,
  OUT UINT8         *Bus,
  OUT UINT8         *Dev,
  OUT UINT8         *Fun
  )
{
  CONST CHAR16  *Cursor;
  UINT64        Value;
  UINTN         ColonCount;

  *Seg = 0;

  ColonCount = 0;
  for (Cursor = Str; *Cursor != CHAR_NULL; Cursor++) {
    if (*Cursor == L':') {
      ColonCount++;
    }
  }

  if ((ColonCount != 1) && (ColonCount != 2)) {
    return FALSE;
  }

  Cursor = Str;

  if (ColonCount == 2) {
    if (!ParseHexField (&Cursor, L':', 0xFFFF, &Value)) {
      return FALSE;
    }

    *Seg = (UINT16)Value;
  }

  if (!ParseHexField (&Cursor, L':', 0xFF, &Value)) {
    return FALSE;
  }

  *Bus = (UINT8)Value;

  if (!ParseHexField (&Cursor, L'.', 0x1F, &Value)) {
    return FALSE;
  }

  *Dev = (UINT8)Value;

  if (!ParseHexField (&Cursor, CHAR_NULL, 0x07, &Value)) {
    return FALSE;
  }

  *Fun = (UINT8)Value;

  return TRUE;
}

STATIC
BOOLEAN
ParseReadType (
  IN  CONST CHAR16  *Str,
  OUT UINT32        *ReadType
  )
{
  CHAR16  *End;
  UINT64  Value;

  if (Str == NULL) {
    return FALSE;
  }

  StrDecimalToUint64S (Str, &End, &Value);
  if ((End == Str) || (*End != CHAR_NULL)) {
    return FALSE;
  }

  if ((Value != 8) && (Value != 16) && (Value != 32)) {
    return FALSE;
  }

  *ReadType = (UINT32)Value;
  return TRUE;
}

PCI_PARSE_RESULT
ParseArguments (
  OUT PCI_CLI_OPTIONS  *Options
  )
{
  EFI_STATUS        Status;
  LIST_ENTRY         *Package;
  CHAR16              *ProblemParam;
  CONST CHAR16        *Param1;
  CONST CHAR16        *ReadTypeStr;
  PCI_PARSE_RESULT    Result;

  ZeroMem (Options, sizeof (*Options));

  ProblemParam = NULL;
  Status        = ShellCommandLineParseEx (mParamList, &Package, &ProblemParam, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    if (ProblemParam != NULL) {
      Print (L"%s: unrecognized or invalid argument \"%s\".\r\n", GetProgramName (), ProblemParam);
      FreePool (ProblemParam);
    } else {
      Print (L"%s: unable to parse command line - %r\r\n", GetProgramName (), Status);
    }

    return PciParseError;
  }

  if (ShellCommandLineGetFlag (Package, L"-h") || ShellCommandLineGetFlag (Package, L"-?")) {
    PrintUsage ();
    ShellCommandLineFreeVarList (Package);
    return PciParseHelp;
  }

  Result       = PciParseOk;
  Param1       = ShellCommandLineGetRawValue (Package, 1);
  Options->ExtendFlag = ShellCommandLineGetFlag (Package, L"-x");

  if (Param1 == NULL) {
    Options->Mode = PciCmdNone;
  } else if (StrCmp (Param1, L"pcitree") == 0) {
    Options->Mode = PciCmdTree;
  } else if (StrCmp (Param1, L"all") == 0) {
    Options->Mode = PciCmdAll;

    ReadTypeStr = ShellCommandLineGetValue (Package, L"-r");
    if (!ParseReadType (ReadTypeStr, &Options->ReadType)) {
      Print (L"%s: \"all\" requires -r 8, -r 16, or -r 32.\r\n", GetProgramName ());
      Result = PciParseError;
    }
  } else if (ParseBdf (Param1, &Options->Seg, &Options->Bus, &Options->Dev, &Options->Fun)) {
    Options->Mode = PciCmdSingle;

    ReadTypeStr = ShellCommandLineGetValue (Package, L"-r");
    if (!ParseReadType (ReadTypeStr, &Options->ReadType)) {
      Print (L"%s: a device address requires -r 8, -r 16, or -r 32.\r\n", GetProgramName ());
      Result = PciParseError;
    }
  } else {
    Print (L"%s: \"%s\" is not \"all\", \"pcitree\", or a valid segment:bus:dev.func address.\r\n", GetProgramName (), Param1);
    Result = PciParseError;
  }

  ShellCommandLineFreeVarList (Package);

  return Result;
}
