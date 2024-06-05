#pragma endian big

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


struct Header {
 be u64 RecordSize;
 be u32 RecordType;
};

struct CDR{
 Header;
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
 u8 Copyright[256];
};

struct GDR{
 Header;
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
 Header;
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
 u8 Name[256];
 be u32 zNumDims;
 be u32 zDimSizes;
 be u32 DimVarys;
};

be u32 Magic1 @ 0;
be u32 Magic2 @ 4;

CDR cdr @ 0x08;

GDR gdr @ cdr.GDROffset;

VDR vdr1 @ gdr.zVDRhead;
