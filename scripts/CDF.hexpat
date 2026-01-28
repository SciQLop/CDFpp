#pragma author Alexis Jeandet
#pragma description Common Data Format File Format
#pragma MIME application / x - cdf
#pragma eval_depth -1

#include "std/core.pat"
#include "std/mem.pat"
#include "std/ptr.pat"
import std.io;

std::core::set_endian(std::mem::Endian::Big);

u32 Magic1 @0;
u32 Magic2 @4;

using PolymorphicRecord;

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

struct offset_ptr_t<T> {
    if (is_cdf_v3())
    {
        if (std::mem::read_signed($, 8, std::mem::Endian::Big)==-1)
             u64 value;
        else
            std::ptr::NullablePtr<T, u64> [[inline, name("Offset")]];
    }
    else
    {
        if (std::mem::read_signed($, 4, std::mem::Endian::Big)==-1)
            u32 value;
        else
            std::ptr::NullablePtr<T, u32>[[inline, name("Offset")]];
    }
};

struct offset_t {
    if (is_cdf_v3())
        u64 value;
    else
        u32 value;
} [[inline]];

struct ptr_t<Pointee_t>
{
    if (is_cdf_v3())
        std::ptr::NullablePtr<Pointee_t, u64> ptr;
    else
        std::ptr::NullablePtr<Pointee_t, u32> ptr;
};


struct Header
{
    if (is_cdf_v3())
        u64 RecordSize;
    else
        u32 RecordSize;
    cdf_record_type RecordType;
}[[inline]];

struct CDF_Record : Header
{
    std::print("{} @0x{:X}", RecordType, addressof(RecordSize));
};


using GDR;

struct CDR : CDF_Record
{
    offset_ptr_t<PolymorphicRecord> GDROffset;
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
    offset_ptr_t<PolymorphicRecord> rVDRhead;
    offset_ptr_t<PolymorphicRecord> zVDRhead;
    offset_ptr_t<PolymorphicRecord> ADRhead;
    offset_t eof;
    u32 NrVars;
    u32 NumAttr;
    u32 rMaxRec;
    u32 rNumDims;
    u32 NzVars;
    offset_ptr_t<PolymorphicRecord> UIRhead;
    u32 rfuC;
    u32 LeapSecondLastUpdated;
    u32 rfuE;
};

struct AgrEDR: CDF_Record
{
    offset_ptr_t<PolymorphicRecord> AEDRnext;
    s32 AttrNum;
    CDF_Types DataType;
    s32 Num;
    s32 NumElements;
    s32 NumStrings;
    u32 rfuB;
    u32 rfuC;
    u32 rfuD;
    u32 rfuE;
    u8 data[RecordSize - ($-addressof(RecordSize))];
};

bitfield VDR_Flags {
    bool record_variance :1;
    bool has_pad_value :1;
    bool compressed :1;
    padding :29[[hidden]];
};

struct VDR : CDF_Record
{
    offset_ptr_t<PolymorphicRecord> VDRnext;
    CDF_Types DataType;
    u32 MaxRec;
    offset_ptr_t<PolymorphicRecord> VXRhead;
    offset_ptr_t<PolymorphicRecord> VXRtail;
    VDR_Flags Flags;
    u32 SRecords;
    u32 rfuB;
    u32 rfuC;
    u32 rfuF;
    u32 NumElems;
    u32 Num;
    offset_ptr_t<PolymorphicRecord> CPRorSPRoffset;
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
    if (Flags.has_pad_value)
    {
        u8 pad_values[RecordSize - ($ - addressof(parent))];
    }
};

struct VXR : CDF_Record
{
    offset_ptr_t<PolymorphicRecord> VXRnext;
    s32 Nentries;
    u32 NusedEntries;
    u32 First[Nentries];
    u32 Last[Nentries];
    offset_ptr_t<PolymorphicRecord> Offsets[Nentries];
};

struct UhandledRecord : CDF_Record
{
    u8 data[RecordSize - sizeof(Header)];
};


struct PolymorphicRecord
{
    match(std::mem::read_unsigned($ + 8, 4, std::mem::Endian::Big))
    {
        (cdf_record_type::GDR) : GDR gdr;
        (cdf_record_type::CDR) : CDR cdr;
        (cdf_record_type::zVDR) : VDR zvdr;
        (cdf_record_type::VXR) : VXR vxr;
        (cdf_record_type::AgrEDR) : AgrEDR agedr;
        (_) : UhandledRecord _r;
    }
};


struct CDF
{
    u32 Magic1;
    u32 Magic2;
    CDR cdr;
    // PolymorphicRecord  records[while (!std::mem::eof())];
};


CDF cdf @0;
