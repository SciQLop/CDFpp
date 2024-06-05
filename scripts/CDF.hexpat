#pragma author Alexis Jeandet
#pragma description Common Data Format File Format
#pragma MIME application/x-cdf

#include <std/core.pat>
import std.io;
import std.mem;

enum CDF_Types: u32{
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

enum cdf_encoding: u32{
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


struct Header {
 be u64 RecordSize;
 be cdf_record_type RecordType;
};

struct CDR{
 be u64 GDROffset;
 be u32 Version;
 be u32 Release;
 be u32 Encoding;
 be u32 Flags;
 be u32 rfuA;
 be u32 rfuB;
 be u32 Increment;
 be u32 Identifier;
 be u32 rfuE;
 char Copyright[256];
};

struct GDR{
 be u64 rVDRhead;
 be u64 zVDRhead;
 be u64 ADRhead;
 be u64 eof;
 be u32 NrVars;
 be u32 NumAttr;
 be u32 rMaxRec;
 be u32 rNumDims;
 be u32 NzVars;
 be u64 UIRhead;
 be u32 rfuC;
 be u32 LeapSecondLastUpdated;
 be u32 rfuE;
};

struct VDR{
 be u64 VDRnext;
 be CDF_Types DataType;
 be u32 MaxRec;
 be u64 VXRhead;
 be u64 VXRtail;
 be u32 Flags;
 be u32 SRecords;
 be u32 rfuB;
 be u32 rfuC;
 be u32 rfuF;
 be u32 NumElems;
 be u32 Num;
 be u64 CPRorSPRoffset;
 be u32 BlockingFactor;
 char Name[256];
 if (parent.header.RecordType == cdf_record_type::zVDR)
 {
    be u32 zNumDims;
    be u32 zDimSizes[zNumDims];
    be u32 DimVarys[zNumDims];
 }
 else
 {
    be u32 DimVarys[gdr.rNumDims];
 }
 
};

struct CDF_Record{
    Header header;
    match(header.RecordType)
    {
    (cdf_record_type::GDR): GDR gdr;
    (cdf_record_type::CDR): CDR cdr;
    (cdf_record_type::zVDR): VDR zvdr;
    (_): u8 data[header.RecordSize - sizeof(header)];
    }
};

struct CDF{
    be u32 Magic1;
    be u32 Magic2;
    CDF_Record records [while(!std::mem::eof())];
};


CDF cdf @ 0;
