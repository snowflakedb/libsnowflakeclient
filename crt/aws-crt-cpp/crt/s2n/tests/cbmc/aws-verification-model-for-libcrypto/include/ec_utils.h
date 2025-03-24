/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#ifndef EC_UTILS_H
#define EC_UTILS_H

#include <openssl/ec.h>
#include <stdbool.h>
#include <stdlib.h>

int ec_key_get_reference_count(EC_KEY *key);

bool ec_key_is_valid(EC_KEY *key);

EC_KEY *ec_key_nondet_alloc();

void ec_key_unconditional_free(EC_KEY *key);

/**
 * Initializes a fixed nondeterministic value meant to represent the maximum
 * possible amount of decrypted data to be written to the output buffer
 * (see EVP_PKEY_encrypt for an example of its use).
 */
void initialize_max_decryption_size();

void initialize_max_derivation_size();

/**
 * Initializes a fixed nondeterministic value meant to represent the maximum
 * possible amount of encrypted data to be written to the output buffer
 * (see EVP_PKEY_decrypt for an example of its use).
 */
void initialize_max_encryption_size();

void initialize_max_signature_size();

size_t max_decryption_size();

size_t max_derivation_size();

size_t max_encryption_size();

size_t max_signature_size();

void write_unconstrained_data(unsigned char *out, size_t len);

unsigned char nondet_unsigned_char();

#endif
