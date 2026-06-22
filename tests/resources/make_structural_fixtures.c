/* Generates CDF fixtures that CDFpp's own saver cannot produce, for testing
 * Variable.is_zvariable and Variable.is_contiguous():
 *   - rvariable.cdf   : a single rVariable (legacy)            -> is_zvariable == false
 *   - fragmented.cdf  : a zVariable whose contiguous records are stored in two
 *                       separate VVR blocks (allocated separately) -> is_contiguous() == false
 *
 * Build & run with the NASA CDF toolkit installed (see project memory):
 *   source /usr/local/cdf/bin/definitions.B
 *   gcc make_structural_fixtures.c -I$CDF_INC -L$CDF_LIB -lcdf -o /tmp/mkfix && /tmp/mkfix
 */
#include "cdf.h"
#include <stdio.h>
#include <stdlib.h>

static void chk(CDFstatus s, const char* what)
{
    if (s < CDF_OK)
    {
        char msg[CDF_STATUSTEXT_LEN + 1];
        CDFgetStatusText(s, msg);
        fprintf(stderr, "%s failed: %s\n", what, msg);
        exit(1);
    }
}

static void make_rvariable(void)
{
    CDFid id;
    long dimSizes[1] = { 1 };
    long varNum;
    remove("rvariable.cdf");
    chk(CDFcreate("rvariable", 0L, dimSizes, NETWORK_ENCODING, ROW_MAJOR, &id), "CDFcreate(rvariable)");
    chk(CDFcreaterVar(id, "legacy_rvar", CDF_INT4, 1L, VARY, NULL, &varNum), "CDFcreaterVar");
    for (long rec = 0; rec < 4; rec++)
    {
        int value = (int)(rec * 10);
        chk(CDFputrVarRecordData(id, varNum, rec, &value), "CDFputrVarRecordData");
    }
    chk(CDFcloseCDF(id), "CDFclose(rvariable)");
}

static void put_int(CDFid id, long varNum, long rec)
{
    int value = (int)rec;
    chk(CDFputzVarRecordData(id, varNum, rec, &value), "CDFputzVarRecordData");
}

static void make_fragmented(void)
{
    CDFid id;
    long dimSizes[1] = { 1 };
    long splitVar, fillerVar;
    long dimVarys[1] = { NOVARY };
    remove("fragmented.cdf");
    chk(CDFcreate("fragmented", 0L, dimSizes, NETWORK_ENCODING, ROW_MAJOR, &id), "CDFcreate(fragmented)");
    chk(CDFcreatezVar(id, "split_zvar", CDF_INT4, 1L, 0L, dimSizes, VARY, dimVarys, &splitVar),
        "CDFcreatezVar(split_zvar)");
    chk(CDFcreatezVar(id, "filler", CDF_INT4, 1L, 0L, dimSizes, VARY, dimVarys, &fillerVar),
        "CDFcreatezVar(filler)");
    /* Force two VVR blocks for split_zvar WITHOUT a record gap (CDFpp does not support
     * sparse records). Disable over-allocation, then interleave writes: split_zvar 0..4,
     * then filler (physically separating split_zvar's block), then split_zvar 5..9 — which
     * cannot extend the first VVR and lands in a second one. Records 0..9 stay contiguous. */
    chk(CDFsetzVarBlockingFactor(id, splitVar, 5L), "blocking factor split");
    for (long rec = 0; rec < 5; rec++)
        put_int(id, splitVar, rec);
    for (long rec = 0; rec < 5; rec++)
        put_int(id, fillerVar, rec);
    for (long rec = 5; rec < 10; rec++)
        put_int(id, splitVar, rec);
    chk(CDFcloseCDF(id), "CDFclose(fragmented)");
}

static void make_contiguous(void)
{
    CDFid id;
    long dimSizes[1] = { 1 };
    long varNum;
    long dimVarys[1] = { NOVARY };
    remove("contiguous.cdf");
    chk(CDFcreate("contiguous", 0L, dimSizes, NETWORK_ENCODING, ROW_MAJOR, &id), "CDFcreate(contiguous)");
    chk(CDFcreatezVar(id, "whole_zvar", CDF_INT4, 1L, 0L, dimSizes, VARY, dimVarys, &varNum),
        "CDFcreatezVar(whole_zvar)");
    /* Written in a single pass -> a single VVR block (the normal, contiguous case). */
    for (long rec = 0; rec < 10; rec++)
        put_int(id, varNum, rec);
    chk(CDFcloseCDF(id), "CDFclose(contiguous)");
}

int main(void)
{
    make_rvariable();
    make_fragmented();
    make_contiguous();
    printf("wrote rvariable.cdf, fragmented.cdf and contiguous.cdf\n");
    return 0;
}
