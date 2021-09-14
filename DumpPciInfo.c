//----------------------------------------------------------------------------
// Include(s)
//----------------------------------------------------------------------------
#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/ShellLib.h>
#include <Include/ShellBase.h>
#include <Library/BaseLib.h>
#include <Library/PciLib.h>
#include "PCI.h"

//----------------------------------------------------------------------------
// Constant, Macro and Type Definition(s)
//----------------------------------------------------------------------------
// Constant Definition(s)
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = NULL;
EFI_SHELL_PARAMETERS_PROTOCOL *EfiShellParametersProtocol = NULL;

typedef struct _NODE
{
  UINT8 Bus;
  UINT8 Dev;
  UINT8 Fun;
  BOOLEAN isPcie;
  BOOLEAN isBridge;
  UINT8 IndentNum;
  struct _NODE *next;
  struct _NODE *previous;
} NODE;

typedef struct _LIST_T
{
  NODE *head;
} LIST_T;

LIST_T *List = NULL;
NODE *Current = NULL;

// Macro Definition(s)
#ifndef PciReadByte
#define PciReadByte(Bus, Device, Function, Register) PciRead8(PCI_LIB_ADDRESS(Bus, Device, Function, Register))
#endif
#ifndef PciReadWord
#define PciReadWord(Bus, Device, Function, Register) PciRead16(PCI_LIB_ADDRESS(Bus, Device, Function, Register))
#endif
#ifndef PciReadDword
#define PciReadDword(Bus, Device, Function, Register) PciRead32(PCI_LIB_ADDRESS(Bus, Device, Function, Register))
#endif
// Type Definition(s)

// Function Prototype(s)
void FreeNode();
NODE *CreateNode(UINT8 Bus, UINT8 Dev, UINT8 Fun, BOOLEAN isPcie, BOOLEAN isBridge, UINT8 IndentNum, NODE *Previous);
void AppendNode(UINT8 Bus, UINT8 Dev, UINT8 Fun, BOOLEAN isPcie, BOOLEAN isBridge, UINT8 IndentNum);
UINT32 FindInitialAddress(void);
UINT32 FindCurrentDevAddress(UINT32 PciInitialAddr, UINT32 BusNum, UINT32 DevNum, UINT32 FuncNum);
EFI_STATUS ArgumentInput(EFI_SHELL_PARAMETERS_PROTOCOL **EfiShellParametersProtocol);
UINT32 ArgumentStringProcessing(CHAR16 *ArgvStr, UINT32 base);
void ErrorProcessing(UINT32 ErrorType, UINT32 WrongNum);
void InstructionGuide(void);
CHAR16 *FindPcie(UINT32 BusNum, UINT32 DevNum, UINT32 FuncNum, UINT8 RegisterNum);
void DisplayDeviceName(UINT8 BusNum, UINT8 DevNum, UINT8 FuncNum, UINT32 IndentNum);
void GenDeviceList(UINT8 BusNum, UINT8 IndentNum);
void GenerateAllRegs(UINT32 BusNum, UINT32 DevNum, UINT32 FuncNum, UINT32 ReadType, BOOLEAN isPcie, UINT32 PciInitialAddr);
void DumpRegByte(UINT8 *RegisterValue, UINT16 MaxTableColumn);
void DumpRegWord(UINT16 *RegisterValue);
void DumpRegDword(UINT32 *RegisterValue);
void DumpBar(UINT32 BusNum, UINT32 DevNum, UINT32 FuncNum, UINT8 FindBridge);
//----------------------------------------------------------------------------
// Variable and External Declaration(s)
//----------------------------------------------------------------------------
// Variable Declaration(s)

// GUID Definition(s)

// Protocol Definition(s)

// External Declaration(s)

// Function Definition(s)

/**
 * @brief 
 * 
 * @param ImageHandle 
 * @param SystemTable 
 * @return EFI_STATUS 
 */
EFI_STATUS DumpPciInfoEntryPoint(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINT32 PciInitialAddr = 0;
  UINT32 i = 0;
  UINT32 j = 0;
  UINT8 BusNum = 0;
  UINT8 DevNum = 0;
  UINT8 FuncNum = 0;
  UINT32 ReadType = 0;
  UINT8 DumpSingleDeviceRegsFlag = 0;
  UINT32 BusDevFuncRead_NumArr[4] = {0};
  UINT32 Max_BusDevFunc[3] = {0xFF, 0x1F, 0x07};
  CHAR16 BusDevFuncRead_for_Cmp[5] = L"BDFR"; //Bus, Device, Function, ReadType
  CHAR16 AllStr[4] = L"all";
  CHAR16 DumpPciTree[8] = L"pcitree";
  CHAR16 HelpInstruction[5] = L"help";
  BOOLEAN DumpPciTreeFlag = FALSE;
  BOOLEAN DumpAllDevsRegFlag = FALSE;
  UINT32 IndentNum = 0;

  ConOut = SystemTable->ConOut;
  ArgumentInput(&EfiShellParametersProtocol);

  PciInitialAddr = FindInitialAddress();

  for (i = 1; i < (EfiShellParametersProtocol->Argc); i++)
  {
    //PCI Tree
    if (!StrCmp((EfiShellParametersProtocol->Argv[i]), DumpPciTree)) //pcitree
    {
      DumpPciTreeFlag = TRUE;
    }
    //Dump All Device's Registers
    else if (!StrCmp((EfiShellParametersProtocol->Argv[i]), AllStr)) //Dump all Device's Registers
    {
      DumpAllDevsRegFlag = TRUE;
    }
    //Dump Instruction
    else if (!StrCmp((EfiShellParametersProtocol->Argv[i]), HelpInstruction))
    {
      InstructionGuide();
      return EFI_SUCCESS;
    }

    //Dump Single Device's Registers or Get Read Type when Dump All Device's Registers
    for (j = 0; j < 4; j++)
    {
      //Check if arguments Prefix match B, D, F, and R
      if (!MemCmp((EfiShellParametersProtocol->Argv[i] + 1), (BusDevFuncRead_for_Cmp + j), sizeof(CHAR16)))
      {
        //Check for Read Type, 8-Byte; 16-Word; 32-DWord
        if (j == 3)
        {
          BusDevFuncRead_NumArr[j] = ArgumentStringProcessing(EfiShellParametersProtocol->Argv[i] + 5, 10);
          if (!((BusDevFuncRead_NumArr[j] == 8) || (BusDevFuncRead_NumArr[j] == 16) || (BusDevFuncRead_NumArr[j] == 32))) //Byte, Word, Dword
          {
            ErrorProcessing(j, BusDevFuncRead_NumArr[j]);
            return EFI_SUCCESS;
          }
        }
        //Get Bus/Dev/Fun Number
        else
        {
          BusDevFuncRead_NumArr[j] = ArgumentStringProcessing(EfiShellParametersProtocol->Argv[i] + 3, 16);
          if (BusDevFuncRead_NumArr[j] > Max_BusDevFunc[j])
          {
            ErrorProcessing(j, BusDevFuncRead_NumArr[j]);
            return EFI_SUCCESS;
          }
        }
        DumpSingleDeviceRegsFlag++; //DumpSingleDeviceRegsFlag = 4 means 4 conditions "ALL" match
      }
    }
  }

  // Generate Device's List use LinkedList
  List = MallocZ(sizeof(LIST_T));
  List->head = NULL;
  IndentNum = 0;
  GenDeviceList((UINT8)BusNum, IndentNum);

  /*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  !!!!!!!Please Free Node before return!!!!!!!
  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

  // Dump Single Device's Registers
  if (DumpSingleDeviceRegsFlag == 4)
  {
    BusNum = BusDevFuncRead_NumArr[0];
    DevNum = BusDevFuncRead_NumArr[1];
    FuncNum = BusDevFuncRead_NumArr[2];
    ReadType = BusDevFuncRead_NumArr[3];
    IndentNum = 0;
    DisplayDeviceName(BusNum, DevNum, FuncNum, IndentNum);
    GenerateAllRegs(BusNum, DevNum, FuncNum, ReadType, FALSE, PciInitialAddr);
    DumpBar(BusNum, DevNum, FuncNum, FALSE);

    FreeNode();
    return EFI_SUCCESS;
  }

  // Dump All Devices' Registers
  if (DumpAllDevsRegFlag)
  {
    if (!BusDevFuncRead_NumArr[3])
    {
      FreeNode();
      ErrorProcessing(3, 0);
      return EFI_SUCCESS;
    }
    IndentNum = 0;
    ReadType = BusDevFuncRead_NumArr[3];
    while (List->head)
    {
      DisplayDeviceName(List->head->Bus, List->head->Dev, List->head->Fun, IndentNum);
      GenerateAllRegs(List->head->Bus, List->head->Dev, List->head->Fun, ReadType, List->head->isPcie, PciInitialAddr);
      DumpBar(List->head->Bus, List->head->Dev, List->head->Fun, List->head->isBridge);
      List->head = List->head->next;
    }
    FreeNode();
    return EFI_SUCCESS;
  }

  // Dump PCI Tree
  if (DumpPciTreeFlag)
  {
    while (List->head)
    {
      DisplayDeviceName(List->head->Bus, List->head->Dev, List->head->Fun, List->head->IndentNum);
      List->head = List->head->next;
    }
    FreeNode();
    return EFI_SUCCESS;
  }

  ConOut->OutputString(ConOut, L"Please type 'DumpPciInfo help' for more information.\r\n");

  //Free Pool for LinkedList
  FreeNode();

  return EFI_SUCCESS;
}

/**
 * @brief Free
 * 
 */
void FreeNode()
{
  NODE *Node = NULL;

  //Free Nodes
  while (List->head)
  {
    Node = List->head->next;
    List->head->next = NULL;
    gBS->FreePool(List->head);
    List->head = Node;
  } // free every node
  gBS->FreePool(List);

  return;
}

/**
 * @brief Create a Node object
 * 
 * @param Bus 
 * @param Dev 
 * @param Fun 
 * @param isPcie 
 * @param isBridge 
 * @param IndentNum 
 * @param Previous 
 * @return NODE* 
 */
NODE *CreateNode(UINT8 Bus, UINT8 Dev, UINT8 Fun, BOOLEAN isPcie, BOOLEAN isBridge, UINT8 IndentNum, NODE *Previous)
{
  NODE *node = MallocZ(sizeof(NODE));
  // memset(node, '\0', sizeof(NODE));
  node->Bus = Bus;
  node->Dev = Dev;
  node->Fun = Fun;
  node->isPcie = isPcie;
  node->isBridge = isBridge;
  node->IndentNum = IndentNum;
  node->next = NULL;
  node->previous = Previous;

  return node;
}

/**
 * @brief 
 * 
 * @param Bus 
 * @param Dev 
 * @param Fun 
 * @param isPcie 
 * @param isBridge 
 * @param IndentNum 
 */
void AppendNode(UINT8 Bus, UINT8 Dev, UINT8 Fun, BOOLEAN isPcie, BOOLEAN isBridge, UINT8 IndentNum)
{
  NODE *current = List->head;

  NODE *node = NULL; //create a node

  if (current)
  {
    while (current->next)
    {
      current = current->next;
    }
    node = CreateNode(Bus, Dev, Fun, isBridge, isPcie, IndentNum, current);
    current->next = node;
  }
  else //create first node
  {
    node = CreateNode(Bus, Dev, Fun, isBridge, isPcie, IndentNum, NULL);
    List->head = node;
  }

  return;
}

/**
 * @brief 
 * 
 * @return UINT32 
 */
UINT32 FindInitialAddress(void)
{
  UINT32 InitialAddress = 0;
  UINT32 PciAddr = 0;
  UINT32 PciAddressNum = 0;

  InitialAddress = PciReadDword(0, 0, 0, 0);

  for (PciAddr = 0xFEC00000; PciAddr >= 0x0; PciAddr -= 0x60000)
  {
    PciAddressNum = MmioRead32(PciAddr);
    if (PciAddressNum == InitialAddress)
    {
      break;
    }
  }

  return PciAddr;
}

/**
 * @brief 
 * 
 * @param PciInitialAddr 
 * @param BusNum 
 * @param DevNum 
 * @param FuncNum 
 * @return UINT32 
 */
UINT32 FindCurrentDevAddress(UINT32 PciInitialAddr, UINT32 BusNum, UINT32 DevNum, UINT32 FuncNum)
{
  UINT32 CurrentAddr = PciInitialAddr + (BusNum << 20) + (DevNum << 15) + (FuncNum << 12);

  return CurrentAddr;
}

/**
 * @brief 
 * 
 * @param EfiShellParametersProtocol 
 * @return EFI_STATUS 
 */
EFI_STATUS ArgumentInput(EFI_SHELL_PARAMETERS_PROTOCOL **EfiShellParametersProtocol)
{
  return gBS->OpenProtocol(TheImageHandle,
                           &gEfiShellParametersProtocolGuid,
                           (VOID **)EfiShellParametersProtocol,
                           TheImageHandle,
                           NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
}

/**
 * @brief 
 * 
 * @param ArgvStr 
 * @param base 
 * @return UINT32 
 */
UINT32 ArgumentStringProcessing(CHAR16 *ArgvStr, UINT32 base)
{
  UINT32 ArgvNum = 0;

  ArgvNum = Wcstol(ArgvStr, &ArgvStr, base);

  return ArgvNum;
}

/**
 * @brief 
 * 
 * @param ErrorType 
 * @param WrongNum 
 */
void ErrorProcessing(UINT32 ErrorType, UINT32 WrongNum)
{
  CHAR16 *ErrorString = MallocZ(100 * sizeof(CHAR16));

  if (ErrorType == 0)
  {
    Swprintf(ErrorString, L"The Bus Number %02X you type is greater than 0xFF.\r\n", WrongNum);
    ConOut->OutputString(ConOut, ErrorString);
  }
  else if (ErrorType == 1)
  {
    Swprintf(ErrorString, L"The Device Number %02X you type is greater than 0x1F.\r\n", WrongNum);
    ConOut->OutputString(ConOut, ErrorString);
  }
  else if (ErrorType == 2)
  {
    Swprintf(ErrorString, L"The input Function Number %02X was greater than 0x07.\r\n", WrongNum);
    ConOut->OutputString(ConOut, ErrorString);
  }
  else if (ErrorType == 3)
  {
    Swprintf(ErrorString, L"The input Read Type Number %d was not 8, 16 or 32.\r\n", WrongNum);
    ConOut->OutputString(ConOut, ErrorString);
  }
  else if (ErrorType == 4)
  {
    ConOut->OutputString(ConOut, L"Can not access the device of the input Bus/Dev/Func Number.\r\n");
  }

  gBS->FreePool(ErrorString);

  return;
}

/**
 * @brief 
 * 
 */
void InstructionGuide(void)
{
  ConOut->OutputString(ConOut, L"var is a variable, you should replace to a number you want to search.\r\n");
  ConOut->OutputString(ConOut, L"/B:var /D:var /F:var /Readvar   - Dump all registers in one read type of a single Device.\r\n");
  ConOut->OutputString(ConOut, L"pcitree                         - Dump PCI Tree.\r\n");
  ConOut->OutputString(ConOut, L"all /Readvar                    - Dump all registers of all Devices.(need to input Read Type too (/Read8, /Read16 or /Read32))\r\n");

  return;
}

/**
 * @brief 
 * 
 * @param BusNum 
 * @param DevNum 
 * @param FuncNum 
 * @param RegisterNum 
 * @return CHAR16* 
 */
CHAR16 *FindPcie(UINT32 BusNum, UINT32 DevNum, UINT32 FuncNum, UINT8 RegisterNum)
{
  UINT8 CapabilityID = 0;
  CHAR16 *PCIEStr = NULL;
  UINT8 TypeValue = 0;

  /*Register 0x34 is the head of capability id, the value of register 0x34 points
    to the register number of next capability id*/

  CapabilityID = PciReadByte(BusNum, DevNum, FuncNum, RegisterNum);

  if (CapabilityID == 0x0)
  {
    return L"";
  }

  RegisterNum = CapabilityID + 0x1;
  TypeValue = PciReadByte(BusNum, DevNum, FuncNum, CapabilityID);

  if (TypeValue == 0x10)
  {
    PCIEStr = MallocZ(7 * sizeof(CHAR16));
    Swprintf(PCIEStr, L"(PCIE)");
    return PCIEStr;
  }

  return FindPcie(BusNum, DevNum, FuncNum, RegisterNum);
}

/**
 * @brief 
 * 
 * @param BusNum 
 * @param DevNum 
 * @param FuncNum 
 * @param IndentNum 
 */
void DisplayDeviceName(UINT8 BusNum, UINT8 DevNum, UINT8 FuncNum, UINT32 IndentNum) //use device ID and Class code to implement
{
  CHAR16 BusDevFunc[10] = {0};
  CHAR16 *Vendorname = PciVendorName(PciReadWord(BusNum, DevNum, FuncNum, 0x00));
  CHAR16 *IsPCIE = FindPcie(BusNum, DevNum, FuncNum, 0x34);
  UINT32 i = 0;

  if (IndentNum)
  {
    for (i = 1; i <= IndentNum; i++)
    {
      ConOut->OutputString(ConOut, L"  ");
    }
  }

  //Print vendor name
  ConOut->OutputString(ConOut, Vendorname);
  if (Vendorname != L"")
  {
    ConOut->OutputString(ConOut, L" ");
  }
  //Print class name
  ConOut->OutputString(ConOut, PciClassName(PciReadDword(BusNum, DevNum, FuncNum, 0x0B)));
  ConOut->OutputString(ConOut, L" ");
  //Print type
  ConOut->OutputString(ConOut, PciTypeName(PciReadByte(BusNum, DevNum, FuncNum, 0x0B)));
  //Print if PCIe
  ConOut->OutputString(ConOut, IsPCIE);
  //Print if Bus/Device/Function number
  ConOut->OutputString(ConOut, L" (");
  Swprintf(BusDevFunc, L"Bus%02X/Dev%02X/Func%02X", BusNum, DevNum, FuncNum);
  ConOut->OutputString(ConOut, BusDevFunc);
  ConOut->OutputString(ConOut, L")");
  ConOut->OutputString(ConOut, L"\r\n");

  gBS->FreePool(IsPCIE);

  return;
}

/**
 * @brief 
 * 
 * @param BusNum 
 * @param IndentNum 
 */
void GenDeviceList(UINT8 BusNum, UINT8 IndentNum)
{
  UINT8 DevNum = 0;
  UINT8 FuncNum = 0;
  UINT8 MaxFun = 1;
  UINT8 SecondBus = 0;
  UINT32 VendorID = 0;
  UINT8 MultiFunBit = 0;
  CHAR16 *IsPcie = NULL;
  CHAR16 PciStr[7] = L"(PCIE)";
  BOOLEAN isBridgeFlag = FALSE;
  BOOLEAN isPcieFlag = FALSE;

  for (DevNum = 0; DevNum <= 0x1F; DevNum++)
  {
    for (FuncNum = 0, MaxFun = 1; FuncNum < MaxFun; FuncNum++)
    {
      VendorID = PciReadDword(BusNum, DevNum, FuncNum, 0x0);
      if (VendorID == 0xFFFFFFFF || !VendorID) //means the device is invalid
      {
        continue;
      }
      else //VendorID != 0xFFFFFFFF
      {
        MultiFunBit = PciReadByte(BusNum, DevNum, 0x0, 0x0E) & BIT7; //Whether B7 is 1 or 0 (header type)
        if (MultiFunBit)                                             //multi-func
        {
          MaxFun = 0x8;
        }
        isBridgeFlag = PciReadByte(BusNum, DevNum, FuncNum, 0x0E) & 0x1;
        IsPcie = FindPcie(BusNum, DevNum, FuncNum, 0x34);
        if (!StrCmp(IsPcie, PciStr))
        {
          isPcieFlag = TRUE;
        }
        AppendNode(BusNum, DevNum, FuncNum, isPcieFlag, isBridgeFlag, IndentNum);
        if (isBridgeFlag)
        {
          IndentNum++;
          SecondBus = PciReadByte(BusNum, DevNum, FuncNum, 0x19);
          GenDeviceList(SecondBus, IndentNum); //recursively
        }
      }
    }

    if (!BusNum)
    {
      IndentNum = 0;
    }
  }

  return;
}

/**
 * @brief 
 * 
 * @param BusNum 
 * @param DevNum 
 * @param FuncNum 
 * @param ReadType 
 * @param isPcie 
 * @param PciInitialAddr 
 */
void GenerateAllRegs(UINT32 BusNum, UINT32 DevNum, UINT32 FuncNum, UINT32 ReadType, BOOLEAN isPcie, UINT32 PciInitialAddr) //Display all registers of a function
{
  UINT32 RegNum = 0;
  UINT8 RegArrayByte[0x1000] = {0};
  UINT16 *RegArrayWord = NULL;
  UINT32 *RegArrayDWord = NULL;
  CHAR16 *DisplayStr = NULL;
  UINT32 ArraySize = 256;
  UINT32 FirstRegNum = 0;
  UINT32 RegMemAddress = 0;
  UINT16 MaxTableColumn = 0x11;
#if DUMP_VALUE_TEST
  CHAR16 testStr[100] = {0};
#endif

  DisplayStr = MallocZ(50 * sizeof(CHAR16));
  Swprintf(DisplayStr, L"Bus: %02X, Device: %02X, Function: %02X\r\n", BusNum, DevNum, FuncNum); //Display bus number, device number and function number
  ConOut->OutputString(ConOut, DisplayStr);

  if (ReadType == 8)
  {
    for (RegNum = 0; RegNum <= 0xFF; RegNum++)
    {
      RegArrayByte[RegNum] = PciReadByte(BusNum, DevNum, FuncNum, RegNum);
    }

    if (isPcie)
    {
      MaxTableColumn = 0x101;
      RegMemAddress = PciInitialAddr + (BusNum << 20) + (DevNum << 15) + (FuncNum << 12) + 0x100;
      for (RegNum = 0x100; RegNum < 0x1000; RegNum++)
      {
        RegArrayByte[RegNum] = MmioRead8(RegMemAddress);
        RegMemAddress++;
      }
    }
    DumpRegByte(RegArrayByte, MaxTableColumn);
  }
  else if (ReadType == 16)
  {
    RegArrayWord = MallocZ(256 * sizeof(UINT16));

    for (RegNum = 0; RegNum <= 0xFF; RegNum += 2)
    {
      RegArrayWord[RegNum] = PciReadWord(BusNum, DevNum, FuncNum, RegNum);
    }
    DumpRegWord(RegArrayWord);
    gBS->FreePool(RegArrayWord);
  }
  else if (ReadType == 32)
  {
    RegArrayDWord = MallocZ(256 * sizeof(UINT32));

    for (RegNum = 3; RegNum <= 0xFF; RegNum += 4)
    {
      RegArrayDWord[RegNum] = PciReadDword(BusNum, DevNum, FuncNum, RegNum);
    }
    DumpRegDword(RegArrayDWord);
    gBS->FreePool(RegArrayDWord);
  }
  else
  {
    ErrorProcessing(3, ReadType);
  }

  ConOut->OutputString(ConOut, L"\r\n");

  gBS->FreePool(DisplayStr);

  return;
}

/**
 * @brief 
 * 
 * @param RegisterValue 
 * @param MaxTableColumn 
 */
void DumpRegByte(UINT8 *RegisterValue, UINT16 MaxTableColumn)
{
  UINT32 TableColumn = 0;
  UINT32 TableRow = 0;
  CHAR16 *DisplayStr = NULL;

  DisplayStr = MallocZ(100 * sizeof(CHAR16));

  for (TableColumn = 0; TableColumn < MaxTableColumn; TableColumn++)
  {
    if (TableColumn != 0)
    {
      ConOut->SetAttribute(ConOut, EFI_LIGHTRED | EFI_BACKGROUND_BLACK);
      Swprintf(DisplayStr, L"%03X0 ", TableColumn - 1);
      ConOut->OutputString(ConOut, DisplayStr);
    }
    else
    {
      ConOut->OutputString(ConOut, L"     ");
    }
    for (TableRow = 0; TableRow <= 0x0F; TableRow++)
    {
      if (TableColumn != 0)
      {
        ConOut->SetAttribute(ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
        Swprintf(DisplayStr, L"%02X ", *(RegisterValue + ((TableColumn - 1) * 16 + TableRow)));
        ConOut->OutputString(ConOut, DisplayStr);
      }
      else
      {
        ConOut->SetAttribute(ConOut, EFI_LIGHTRED | EFI_BACKGROUND_BLACK);
        Swprintf(DisplayStr, L"%02X ", TableRow);
        ConOut->OutputString(ConOut, DisplayStr);
      }
    }
    ConOut->OutputString(ConOut, L"\r\n");
  }
  gBS->FreePool(DisplayStr);

  return;
}

/**
 * @brief 
 * 
 * @param RegisterValue 
 */
void DumpRegWord(UINT16 *RegisterValue)
{
  UINT32 TableColumn = 0;
  UINT32 TableRow = 0;
  CHAR16 *DisplayStr = NULL;

  DisplayStr = MallocZ(100 * sizeof(CHAR16));

  for (TableColumn = 0; TableColumn < 17; TableColumn++)
  {
    if (TableColumn != 0)
    {
      ConOut->SetAttribute(ConOut, EFI_LIGHTRED | EFI_BACKGROUND_BLACK);
      Swprintf(DisplayStr, L"%03X0 ", TableColumn - 1);
      ConOut->OutputString(ConOut, DisplayStr);
    }
    else
    {
      ConOut->OutputString(ConOut, L"     ");
    }
    for (TableRow = 0; TableRow <= 0x0F; TableRow += 2)
    {
      if (TableColumn != 0)
      {
        ConOut->SetAttribute(ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
        Swprintf(DisplayStr, L"%04X ", *(RegisterValue + ((TableColumn - 1) * 8 + TableRow)));
        ConOut->OutputString(ConOut, DisplayStr);
      }
      else
      {
        ConOut->SetAttribute(ConOut, EFI_LIGHTRED | EFI_BACKGROUND_BLACK);
        Swprintf(DisplayStr, L"%02X%02X ", TableRow + 1, TableRow);
        ConOut->OutputString(ConOut, DisplayStr);
      }
    }
    ConOut->OutputString(ConOut, L"\r\n");
  }
  gBS->FreePool(DisplayStr);

  return;
}

/**
 * @brief 
 * 
 * @param RegisterValue 
 */
void DumpRegDword(UINT32 *RegisterValue)
{
  UINT32 TableColumn = 0;
  UINT32 TableRow = 0;
  CHAR16 *DisplayStr = NULL;

  DisplayStr = MallocZ(100 * sizeof(CHAR16));

  for (TableColumn = 0; TableColumn < 17; TableColumn++)
  {
    if (TableColumn != 0)
    {
      ConOut->SetAttribute(ConOut, EFI_LIGHTRED | EFI_BACKGROUND_BLACK);
      Swprintf(DisplayStr, L"%03X0 ", TableColumn - 1);
      ConOut->OutputString(ConOut, DisplayStr);
    }
    else
    {
      ConOut->OutputString(ConOut, L"     ");
    }
    for (TableRow = 3; TableRow <= 0x0F; TableRow += 4)
    {
      if (TableColumn != 0)
      {
        ConOut->SetAttribute(ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
        Swprintf(DisplayStr, L"%08X ", *(RegisterValue + ((TableColumn - 1) * 4 + TableRow)));
        ConOut->OutputString(ConOut, DisplayStr);
      }
      else
      {
        ConOut->SetAttribute(ConOut, EFI_LIGHTRED | EFI_BACKGROUND_BLACK);
        Swprintf(DisplayStr, L"%02X%02X%02X%02X ", TableRow, TableRow - 1, TableRow - 2, TableRow - 3);
        ConOut->OutputString(ConOut, DisplayStr);
      }
    }
    ConOut->OutputString(ConOut, L"\r\n");
  }
  gBS->FreePool(DisplayStr);

  return;
}

/**
 * @brief 
 * 
 * @param BusNum 
 * @param DevNum 
 * @param FuncNum 
 * @param FindBridge 
 */
void DumpBar(UINT32 BusNum, UINT32 DevNum, UINT32 FuncNum, UINT8 FindBridge)
{
  UINT32 BarNum = 0;
  UINT32 BarType = 0;
  CHAR16 *DisplayStr = NULL;
  UINT32 RegNum = 0;
  UINT32 i = 0;
  UINT32 MaxRegNum = 0x24;
  UINT8 IO_Base = 0;
  UINT8 IO_Limit = 0;
  UINT16 Memory_Base = 0;
  UINT16 Memory_Limit = 0;
  UINT16 Prefetch_Memory_Base = 0;
  UINT16 Prefetch_Memory_Limit = 0;
  UINT32 Prefetch_Base_Upper = 0;
  UINT32 Prefetch_Limit_Upper = 0;
  UINT16 IO_Base_Upper = 0;
  UINT16 IO_Limit_Upper = 0;
  CHAR16 *DisplayP2P_IO_Mem = NULL;
  UINT16 Base_Limit_var = 0;

  DisplayStr = MallocZ(50 * sizeof(CHAR16));
  DisplayP2P_IO_Mem = MallocZ(70 * sizeof(CHAR16));

  if (FindBridge)
  {
    MaxRegNum = 0x14;
    IO_Base = PciReadByte(BusNum, DevNum, FuncNum, 0x1C);
    IO_Limit = PciReadByte(BusNum, DevNum, FuncNum, 0x1D);
    Swprintf(DisplayP2P_IO_Mem, L"I/O Base: %02X ; I/O Limit: %02X\r\n");
    ConOut->OutputString(ConOut, DisplayP2P_IO_Mem);

    Memory_Base = PciReadWord(BusNum, DevNum, FuncNum, 0x20);
    Memory_Limit = PciReadWord(BusNum, DevNum, FuncNum, 0x22);
    Swprintf(DisplayP2P_IO_Mem, L"Memory Base: %04X ; Memory Limit: %04X\r\n");
    ConOut->OutputString(ConOut, DisplayP2P_IO_Mem);

    Prefetch_Memory_Base = PciReadWord(BusNum, DevNum, FuncNum, 0x24);
    Prefetch_Memory_Limit = PciReadWord(BusNum, DevNum, FuncNum, 0x26);
    Swprintf(DisplayP2P_IO_Mem, L"Prefetch Memory Base: %04X ; Prefetch_Memory_Limit: %04X\r\n");
    ConOut->OutputString(ConOut, DisplayP2P_IO_Mem);

    gBS->FreePool(DisplayP2P_IO_Mem);
  }

  for (RegNum = 0x10; RegNum <= MaxRegNum; RegNum += 4)
  {
    BarNum = PciReadDword(BusNum, DevNum, FuncNum, RegNum);
    BarType = BarNum & 0x1; //see bit 0 = 0 or 1
    if (BarType)            //bit0 == 1, I/O type
    {
      BarNum = BarNum & 0xFFFFFFFC;
      Swprintf(DisplayStr, L"BAR%d: %08X, I/O Space\r\n", ((RegNum - 16) >> 2), BarNum);
      ConOut->OutputString(ConOut, DisplayStr);
    }
    else //bit0 == 0, Memory type
    {
      BarNum = BarNum & 0xFFFFFFF0;
      Swprintf(DisplayStr, L"BAR%d: %08X, Memory Space\r\n", ((RegNum - 16) >> 2), BarNum);
      ConOut->OutputString(ConOut, DisplayStr);
    }
  }

  ConOut->OutputString(ConOut, L"\r\n\r\n");
  gBS->FreePool(DisplayStr);

  return;
}
