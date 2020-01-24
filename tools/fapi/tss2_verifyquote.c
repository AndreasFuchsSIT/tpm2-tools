/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tools/fapi/tss2_template.h"

/* needed by tpm2_util and tpm2_option functions */
bool output_enabled = false;

/* Context struct used to store passed command line parameters */
static struct cxt {
    char *publicKeyPath;
    char const *qualifyingData;
    char const *quoteInfo;
    char const *signature;
    char const *pcrEventLog;
} ctx;

/* Parse command line parameters */
static bool on_option(char key, char *value) {
    switch (key) {
    case 'q':
        ctx.qualifyingData = value;
        break;
    case 'l':
        ctx.pcrEventLog = value;
        break;
    case 'i':
        ctx.quoteInfo = value;
        break;
    case 'p':
        ctx.publicKeyPath = value;
        break;
    case 's':
        ctx.signature = value;
        break;
    }
    return true;
}

/* Define possible command line parameters */
bool tss2_tool_onstart(tpm2_options **opts) {
    struct option topts[] = {
        {"publicKeyPath",   required_argument, NULL, 'p'},
        {"qualifyingData",  required_argument, NULL, 'q'},
        {"quoteInfo",       required_argument, NULL, 'i'},
        {"signature",       required_argument, NULL, 's'},
        {"pcrLog",          optional_argument, NULL, 'l'}
    };
    return (*opts = tpm2_options_new ("q:l:i:p:s:", ARRAY_LEN(topts), topts,
                                      on_option, NULL, 0)) != NULL;
}

/* Execute specific tool */
int tss2_tool_onrun (FAPI_CONTEXT *fctx) {
    /* Check availability of required parameters */
    if (!ctx.qualifyingData) {
        fprintf (stderr, "qualifying data parameter not provided, use "\
            "--qualifyingData=\n");
        return -1;
    }
    if (!ctx.quoteInfo) {
        fprintf (stderr, "quote info parameter not provided, use "\
            "--quoteInfo=\n");
        return -1;
    }
    if (!ctx.publicKeyPath) {
        fprintf (stderr, "publicKeyPath parameter not provided, use "\
            "--publicKeyPath=\n");
        return -1;
    }
    if (!ctx.signature) {
        fprintf (stderr, "signature parameter not provided, use"\
            " --signature=\n");
        return -1;
    }
    if (!ctx.pcrEventLog) {
        fprintf (stderr, "pcrLog parameter not provided, use"\
            " --pcrLog=\n");
        return -1;
    }
    if (!strcmp (ctx.signature, "-") + !strcmp(ctx.qualifyingData, "-") +
        !strcmp(ctx.quoteInfo, "-") + !strcmp(ctx.pcrEventLog, "-") > 1) {
            fprintf (stderr, "At most one of --signature, "\
                "--qualifyingData, --quoteInfo and --pcrLog can "\
                "be '-' (standard output)\n");
            return -1;
    }

    /* Read qualifyingData, signature, quoteInfo and pcrEventLog from file */
    uint8_t *qualifyingData;
    size_t qualifyingDataSize;
    TSS2_RC r = open_read_and_close (ctx.qualifyingData,
        (void**)&qualifyingData, &qualifyingDataSize);
    if (r){
        LOG_PERR ("open_read_and_close qualifyingData", r);
        return -1;
    }
    uint8_t *signature;
    size_t signatureSize;
    r = open_read_and_close (ctx.signature, (void**)&signature, &signatureSize);
    if (r) {
        LOG_PERR ("open_read_and_close signature", r);
        free (qualifyingData);
        return -1;
    }
    char *quoteInfo, *pcrEventLog = NULL;
    r = open_read_and_close (ctx.quoteInfo, (void**)&quoteInfo, NULL);
    if (r) {
        LOG_PERR ("open_read_and_close quoteInfo", r);
        free (qualifyingData);
        free (signature);
        return -1;
    }
    if (pcrEventLog){
        r = open_read_and_close (ctx.pcrEventLog, (void**)&pcrEventLog, NULL);
        if (r) {
            LOG_PERR ("open_read_and_close pcrEventLog", r);
            free (qualifyingData);
            free (signature);
            free (quoteInfo);
            return -1;
        }
    }

    /* Execute FAPI command with passed arguments */
    r = Fapi_VerifyQuote (fctx, ctx.publicKeyPath, qualifyingData,
        qualifyingDataSize, quoteInfo, signature, signatureSize,
        pcrEventLog);
    if (r != TSS2_RC_SUCCESS){
        LOG_PERR ("Fapi_VerifyQuote", r);
        return 1;
    }

    free (qualifyingData);
    free (signature);
    free (quoteInfo);
    free (pcrEventLog);

    return 0;
}
