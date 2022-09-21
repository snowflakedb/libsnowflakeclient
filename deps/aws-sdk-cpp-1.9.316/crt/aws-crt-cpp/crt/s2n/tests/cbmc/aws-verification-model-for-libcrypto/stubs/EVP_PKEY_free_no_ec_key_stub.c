/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include <openssl/evp.h>
#include <stdlib.h>

/**
 * Description: EVP_PKEY_free() decrements the reference count of key and, if the reference count is zero, frees it up.
 * If key is NULL, nothing is done.
 * Use this stub when we are *certain* there is no ec_key associated with the key.
 */
void EVP_PKEY_free(EVP_PKEY *pkey) {
    if (pkey) {
        pkey->references -= 1;
        if (pkey->references == 0) {
            assert(!pkey->ec_key);
            free(pkey);
        }
    }
}
