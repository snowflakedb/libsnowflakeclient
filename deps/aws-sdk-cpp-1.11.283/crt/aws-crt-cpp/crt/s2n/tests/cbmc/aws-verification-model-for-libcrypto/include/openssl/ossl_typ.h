/*
 * Changes to OpenSSL version 1.1.1. copyright Amazon.com, Inc. All Rights Reserved.
 * Copyright 1995-2017 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifndef HEADER_OPENSSL_TYPES_H
#define HEADER_OPENSSL_TYPES_H

/* This is the base type that holds just about everything :-) */
struct asn1_string_st {
    int length;
    int type;
    unsigned char *data;
    /*
     * The value of the following field depends on the type being held.  It
     * is mostly being used for BIT_STRING so if the input data has a
     * non-zero 'unused bits' value, it will be handled correctly
     */
    long flags;
};

#ifdef NO_ASN1_TYPEDEFS
#    define ASN1_INTEGER ASN1_STRING
#    define ASN1_ENUMERATED ASN1_STRING
#    define ASN1_BIT_STRING ASN1_STRING
#    define ASN1_OCTET_STRING ASN1_STRING
#    define ASN1_PRINTABLESTRING ASN1_STRING
#    define ASN1_T61STRING ASN1_STRING
#    define ASN1_IA5STRING ASN1_STRING
#    define ASN1_UTCTIME ASN1_STRING
#    define ASN1_GENERALIZEDTIME ASN1_STRING
#    define ASN1_TIME ASN1_STRING
#    define ASN1_GENERALSTRING ASN1_STRING
#    define ASN1_UNIVERSALSTRING ASN1_STRING
#    define ASN1_BMPSTRING ASN1_STRING
#    define ASN1_VISIBLESTRING ASN1_STRING
#    define ASN1_UTF8STRING ASN1_STRING
#    define ASN1_BOOLEAN int
#    define ASN1_NULL int
#else
typedef struct asn1_string_st ASN1_INTEGER;
typedef struct asn1_string_st ASN1_ENUMERATED;
typedef struct asn1_string_st ASN1_BIT_STRING;
typedef struct asn1_string_st ASN1_OCTET_STRING;
typedef struct asn1_string_st ASN1_PRINTABLESTRING;
typedef struct asn1_string_st ASN1_T61STRING;
typedef struct asn1_string_st ASN1_IA5STRING;
typedef struct asn1_string_st ASN1_GENERALSTRING;
typedef struct asn1_string_st ASN1_UNIVERSALSTRING;
typedef struct asn1_string_st ASN1_BMPSTRING;
typedef struct asn1_string_st ASN1_UTCTIME;
typedef struct asn1_string_st ASN1_TIME;
typedef struct asn1_string_st ASN1_GENERALIZEDTIME;
typedef struct asn1_string_st ASN1_VISIBLESTRING;
typedef struct asn1_string_st ASN1_UTF8STRING;
typedef struct asn1_string_st ASN1_STRING;
typedef int ASN1_BOOLEAN;
typedef int ASN1_NULL;
#endif

#ifdef BIGNUM
#    undef BIGNUM
#endif
typedef struct bio_st BIO;
typedef struct bignum_st BIGNUM;

typedef struct dh_st DH;
typedef struct dh_method DH_METHOD;

typedef struct ec_key_st EC_KEY;

typedef struct evp_pkey_ctx_st EVP_PKEY_CTX;
typedef struct hmac_ctx_st HMAC_CTX;

typedef struct evp_cipher_st EVP_CIPHER;
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;
typedef struct evp_md_st EVP_MD;
typedef struct evp_md_ctx_st EVP_MD_CTX;
typedef struct evp_pkey_st EVP_PKEY;

typedef struct engine_st ENGINE;
typedef struct rand_meth_st RAND_METHOD;

#define ASN1_GENERALIZEDTIME ASN1_STRING

#endif
