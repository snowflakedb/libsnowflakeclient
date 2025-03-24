/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include <openssl/evp.h>

/*
 * Checks whether EVP_MD_CTX is a valid object.
 * Use this stub when we are certain there is no pkey
 * associated with the digest context.
 */
bool evp_md_ctx_is_valid(EVP_MD_CTX *ctx) {
    if (!ctx) return false;
    /* We must be sure there is no pkey associated with the digest context. */
    assert(ctx->pkey == NULL);
    return ctx->is_initialized && ctx->digest_size <= EVP_MAX_MD_SIZE;
}
