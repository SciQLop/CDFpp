#pragma author Alexis Jeandet
#pragma description Common Data Format File Format
#pragma MIME application / x - cdf

#include "std/core.pat"
#include "std/mem.pat"
#include "std/ptr.pat"
import std.io;

std::core::set_endian(std::mem::Endian::Big);

u32 Magic1 @0;
u32 Magic2 @4;

enum CDF_Types : u32
{
    CDF_NONE = 0,
    CDF_INT1 = 1,
    CDF_INT2 = 2,
    CDF_INT4 = 4,
    CDF_INT8 = 8,
    CDF_UINT1 = 11,
    CDF_UINT2 = 12,
    CDF_UINT4 = 14,
    CDF_BYTE = 41,
    CDF_REAL4 = 21,
    CDF_REAL8 = 22,
    CDF_FLOAT = 44,
    CDF_DOUBLE = 45,
    CDF_EPOCH = 31,
    CDF_EPOCH16 = 32,
    CDF_TIME_TT2000 = 33,
    CDF_CHAR = 51,
    CDF_UCHAR = 52
};

enum cdf_encoding : u32
{
    network = 1,
    SUN = 2,
    VAX = 3,
    decstation = 4,
    SGi = 5,
    IBMPC = 6,
    IBMRS = 7,
    PPC = 9,
    HP = 11,
    NeXT = 12,
    ALPHAOSF1 = 13,
    ALPHAVMSd = 14,
    ALPHAVMSg = 15,
    ALPHAVMSi = 16,
    ARM_LITTLE = 17,
    ARM_BIG = 18,
    IA64VMSi = 19,
    IA64VMSd = 20,
    IA64VMSg = 21
};

enum cdf_record_type : u32
{
    CDR = 1,
    GDR = 2,
    rVDR = 3,
    ADR = 4,
    AgrEDR = 5,
    VXR = 6,
    VVR = 7,
    zVDR = 8,
    AzEDR = 9,
    CCR = 10,
    CPR = 11,
    SPR = 12,
    CVVR = 13,
    UIR = -1,
};

fn is_cdf_v3()
{
    return Magic1 == 0xCDF30001;
};

struct offset_t
{
    if (is_cdf_v3())
        be u64 value;
    else
        be u32 value;
};

struct ptr_t<Pointee_t>
{
    if (is_cdf_v3())
        std::ptr::NullablePtr<Pointee_t, u64> ptr;
    else
        std::ptr::NullablePtr<Pointee_t, u32> ptr;
};


struct Header
{
    offset_t RecordSize;
    cdf_record_type RecordType;
};

struct CDF_Record : Header
{
};


using GDR;

struct CDR : CDF_Record
{
    ptr_t<GDR> GDROffset;
    u32 Version;
    u32 Release;
    u32 Encoding;
    u32 Flags;
    u32 rfuA;
    u32 rfuB;
    u32 Increment;
    u32 Identifier;
    u32 rfuE;
    char Copyright[256];
};

struct GDR : CDF_Record
{
    offset_t rVDRhead;
    offset_t zVDRhead;
    offset_t ADRhead;
    offset_t eof;
    u32 NrVars;
    u32 NumAttr;
    u32 rMaxRec;
    u32 rNumDims;
    u32 NzVars;
    offset_t UIRhead;
    u32 rfuC;
    u32 LeapSecondLastUpdated;
    u32 rfuE;
};

struct VDR : CDF_Record
{
    offset_t VDRnext;
    CDF_Types DataType;
    u32 MaxRec;
    offset_t VXRhead;
    offset_t VXRtail;
    u32 Flags;
    u32 SRecords;
    u32 rfuB;
    u32 rfuC;
    u32 rfuF;
    u32 NumElems;
    u32 Num;
    offset_t CPRorSPRoffset;
    u32 BlockingFactor;
    char Name[256];
    if (RecordType == cdf_record_type::zVDR)
    {
        u32 zNumDims;
        if (zNumDims > 0)
        {
            u32 zDimSizes[zNumDims];
            u32 DimVarys[zNumDims];
        }
    }
    else
    {
        u32 DimVarys[gdr.rNumDims];
    }
    if (Flags & 2 == 2)
    {
        u8 pad_values[RecordSize.value - ($ - addressof(parent))];
    }
};

struct VXR : CDF_Record
{
    offset_t VXRnext;
    s32 Nentries;
    std::print("{}", Nentries);
    u32 NusedEntries;
    u32 First[Nentries];
    u32 Last[Nentries];
    offset_t Offsets[Nentries];
};

struct UhandledRecord : CDF_Record
{
    u8 data[RecordSize.value - sizeof(Header)];
};


struct PolymorphicRecord
{
    match(std::mem::read_unsigned($ + 8, 4, std::mem::Endian::Big))
    {
        (cdf_record_type::GDR) : GDR gdr;
        (cdf_record_type::CDR) : CDR cdr;
        (cdf_record_type::zVDR) : VDR zvdr;
        (cdf_record_type::VXR) : VXR vxr;
        (_) : UhandledRecord _r;
    }
};


struct CDF
{
    u32 Magic1;
    u32 Magic2;
    PolymorphicRecord records[while (!std::mem::eof())];
};


CDF cdf @0;
