/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include <openssl/evp.h>

/**
 * Helper function for CBMC proofs: checks if EVP_PKEY is valid.
 * Use this stub when we are *certain* there is no ec_key associated with the key.
 */
bool evp_pkey_is_valid(EVP_PKEY *pkey) {
    if (!pkey) return false;
    /* We must be sure there is no ec_key associated with the key. */
    assert(pkey->ec_key == NULL);
    return (pkey->references > 0);
}
