/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include <openssl/evp.h>
#include <stdlib.h>

/*
 * Frees up the space allocated to ctx context.
 * Use this stub when we are *certain* there is no pkey associated with the digest context.
 */
void EVP_MD_CTX_free(EVP_MD_CTX *ctx) {
    if (ctx) {
        assert(ctx->pkey == NULL);
        free(ctx);
    }
}
