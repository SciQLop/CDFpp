/* How tests/resources/sparse_records.cdf was made, with NASA's CDF library 3.9.2:
   gcc make_sparse_records.c -lcdf && ./a.out. One zVariable per rule for records missing from the file. */
/* Writes sparse_records.cdf with NASA's CDF library: one zVariable per missing-record rule. */
#include <stdio.h>
#include <string.h>
#include "cdf.h"

static CDFid id;
static long fillval_attr;

static void check(CDFstatus s, const char* what)
{
    if (s < CDF_WARN) { char msg[CDF_STATUSTEXT_LEN + 1]; CDFgetStatusText(s, msg); fprintf(stderr, "%s: %s\n", what, msg); }
}

static long make_var(const char* name, long type, long elems, long sparse, long comp)
{
    long num, dims[1] = { 0 }, varys[1] = { VARY };
    check(CDFcreatezVar(id, name, type, elems, 0L, dims, VARY, varys, &num), name);
    if (sparse != NO_SPARSERECORDS) check(CDFsetzVarSparseRecords(id, num, sparse), "sparse");
    if (comp) { long parms[CDF_MAX_PARMS] = { 6 }; check(CDFsetzVarCompression(id, num, GZIP_COMPRESSION, parms), "comp"); }
    return num;
}

static void put(long num, long rec, void* value) { check(CDFputzVarRecordData(id, num, rec, value), "put"); }

int main(void)
{
    remove("sparse_records.cdf");
    check(CDFcreateCDF("sparse_records", &id), "create");
    check(CDFcreateAttr(id, "FILLVAL", VARIABLE_SCOPE, &fillval_attr), "attr");
    float f; double d; int i4; long long tt; char c3[3];

    /* pad-missing, explicit pad value, no FILLVAL: records 0, 3, 5 */
    long v = make_var("pad_explicit", CDF_FLOAT, 1, PAD_SPARSERECORDS, 0);
    f = 1.5f; check(CDFsetzVarPadValue(id, v, &f), "pad");
    f = 10; put(v, 0, &f); f = 30; put(v, 3, &f); f = 50; put(v, 5, &f);

    /* pad-missing, explicit pad value, and a FILLVAL: records 0, 3, 5 */
    v = make_var("pad_with_fillval", CDF_FLOAT, 1, PAD_SPARSERECORDS, 0);
    f = 1.5f; check(CDFsetzVarPadValue(id, v, &f), "pad");
    f = -1e31f; check(CDFputAttrzEntry(id, fillval_attr, v, CDF_FLOAT, 1, &f), "fillval");
    f = 10; put(v, 0, &f); f = 30; put(v, 3, &f); f = 50; put(v, 5, &f);

    /* previous-missing, first record missing: records 2, 4 */
    v = make_var("prev", CDF_INT4, 1, PREV_SPARSERECORDS, 0);
    i4 = 7; check(CDFsetzVarPadValue(id, v, &i4), "pad");
    i4 = 22; put(v, 2, &i4); i4 = 44; put(v, 4, &i4);

    /* previous-missing with a FILLVAL: records 2, 4 */
    v = make_var("prev_with_fillval", CDF_INT4, 1, PREV_SPARSERECORDS, 0);
    i4 = -99; check(CDFputAttrzEntry(id, fillval_attr, v, CDF_INT4, 1, &i4), "fillval");
    i4 = 22; put(v, 2, &i4); i4 = 44; put(v, 4, &i4);

    /* default pad values, one per type family: records 0, 2 */
    v = make_var("default_double", CDF_DOUBLE, 1, PAD_SPARSERECORDS, 0);
    d = 1; put(v, 0, &d); d = 3; put(v, 2, &d);
    v = make_var("default_uint2", CDF_UINT2, 1, PAD_SPARSERECORDS, 0);
    unsigned short u2 = 1; put(v, 0, &u2); u2 = 3; put(v, 2, &u2);
    v = make_var("default_char", CDF_CHAR, 3, PAD_SPARSERECORDS, 0);
    memcpy(c3, "abc", 3); put(v, 0, c3); memcpy(c3, "xyz", 3); put(v, 2, c3);
    v = make_var("default_tt2000", CDF_TIME_TT2000, 1, PAD_SPARSERECORDS, 0);
    tt = 0; put(v, 0, &tt); tt = 2; put(v, 2, &tt);
    v = make_var("default_epoch", CDF_EPOCH, 1, PAD_SPARSERECORDS, 0);
    d = 63745056000000.0; put(v, 0, &d); put(v, 2, &d);

    /* gzip-compressed, pad-missing: records 0, 3 */
    v = make_var("compressed_pad", CDF_FLOAT, 1, PAD_SPARSERECORDS, 1);
    f = 2.5f; check(CDFsetzVarPadValue(id, v, &f), "pad");
    f = 10; put(v, 0, &f); f = 30; put(v, 3, &f);

    /* no sparse records, written with a gap: records 0, 3 (the library pads 1 and 2 itself) */
    v = make_var("no_sparse_gap", CDF_FLOAT, 1, NO_SPARSERECORDS, 0);
    f = 10; put(v, 0, &f); f = 30; put(v, 3, &f);

    check(CDFcloseCDF(id), "close");
    return 0;
}
