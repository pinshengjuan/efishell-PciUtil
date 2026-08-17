/** @file
  Command-line argument parsing for PciUtil.

  Syntax:
    PciUtil <segment:bus:dev.func> -r 8|16|32 [-x]
    PciUtil all -r 8|16|32 [-x]
    PciUtil pcitree
    PciUtil -h

  Note: use "-h", not "-?", for help. The interactive UEFI Shell intercepts
  any argument starting with "-?" and redirects it to its own "help" command
  (see DoHelpUpdate() in ShellPkg/Application/Shell/Shell.c) before this
  application ever runs, so "-?" never reaches ParseArguments() when typed
  at the prompt.
**/
#ifndef _CLI_PARSER_H_
#define _CLI_PARSER_H_

#include <Uefi.h>

typedef enum {
  PciCmdNone = 0,   // Nothing recognized on the command line.
  PciCmdSingle,     // A <segment:bus:dev.func> device address was given.
  PciCmdAll,        // "all" was given.
  PciCmdTree        // "pcitree" was given.
} PCI_COMMAND_MODE;

typedef struct {
  PCI_COMMAND_MODE    Mode;
  BOOLEAN             ExtendFlag;   // "-x" was given - dump 0x100-0xFFF for PCIe devices too
  UINT16              Segment;      // defaults to 0 if the address omitted "segment:"
  UINT8               Bus;
  UINT8               Dev;
  UINT8               Func;
  UINT32              ReadType;     // 8, 16 or 32 (only meaningful for PciCmdSingle/PciCmdAll)
} PCI_CLI_OPTIONS;

typedef enum {
  PciParseOk,      // Options is filled in; caller acts on Options.Mode.
  PciParseHelp,    // "-h" (or "-?") was given; the usage text has already been printed.
  PciParseError    // A bad argument was given; an error has already been printed.
} PCI_PARSE_RESULT;

/**
  Parse the shell command line (via ShellCommandLineParseEx) into Options.

  @param  Options  Filled in on PciParseOk.

  @return PciParseOk, PciParseHelp, or PciParseError.
**/
PCI_PARSE_RESULT
ParseArguments (
  OUT PCI_CLI_OPTIONS  *Options
  );

/**
  The name this program was actually invoked as (Argv[0]), so messages stay
  correct even if the .efi file is renamed. Falls back to "PciUtil" if
  Argv[0] is unavailable for some reason.
**/
CONST CHAR16 *
GetProgramName (
  VOID
  );

#endif
