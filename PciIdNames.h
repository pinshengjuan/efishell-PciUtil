typedef struct _PCIVENDOR_NAME
{
    UINT16  Id;
    CHAR16  *Name;
}
PCIVENDOR_NAME;

typedef struct _PCICLASS_NAME
{
    UINT8   BaseClass;
    UINT8   SubClass;
    UINT8   Interface;
    CHAR16  *Name;
}
PCICLASS_NAME;

CHAR16 *PciVendorName(UINT16 Vendor);
CHAR16 *PciClassName(UINT32 ClassCode);
CHAR16 *PciTypeName(UINT8 BaseClass);