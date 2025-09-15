#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/ocsp.h>
#include <openssl/pem.h>

/* Test hooks from sf_ocsp.c */
extern int sf_ocsp_verify_for_test(OCSP_BASICRESP *br, STACK_OF(X509) *ch, X509_STORE *st);
extern int sf_ocsp_inject_selfsigned_issuer_for_test(OCSP_BASICRESP *br, STACK_OF(X509) *ch);

/* Helpers to build a minimal self-signed root and an intermediate (responder) */
static EVP_PKEY* make_key(void) {
  EVP_PKEY *pkey = EVP_PKEY_new();
  RSA *rsa = RSA_new();
  BIGNUM *e = BN_new();
  BN_set_word(e, RSA_F4);
  RSA_generate_key_ex(rsa, 1024, e, NULL);
  EVP_PKEY_assign_RSA(pkey, rsa);
  BN_free(e);
  return pkey;
}

static X509* make_self_signed(const char *cn, EVP_PKEY *pkey, long not_after_s) {
  X509 *x = X509_new();
  X509_set_version(x, 2);
  ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
  X509_gmtime_adj(X509_get_notBefore(x), -60);
  X509_gmtime_adj(X509_get_notAfter(x), not_after_s);
  X509_set_pubkey(x, pkey);
  X509_NAME *name = X509_get_subject_name(x);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*)cn, -1, -1, 0);
  X509_set_issuer_name(x, name);
  X509_sign(x, pkey, EVP_sha256());
  return x;
}

static X509* make_cert_signed(const char *cn, EVP_PKEY *subject_key, X509 *issuer_cert, EVP_PKEY *issuer_key, long not_after_s) {
  X509 *x = X509_new();
  X509_set_version(x, 2);
  ASN1_INTEGER_set(X509_get_serialNumber(x), 2);
  X509_gmtime_adj(X509_get_notBefore(x), -60);
  X509_gmtime_adj(X509_get_notAfter(x), not_after_s);
  X509_set_pubkey(x, subject_key);
  X509_NAME *subj = X509_get_subject_name(x);
  X509_NAME_add_entry_by_txt(subj, "CN", MBSTRING_ASC, (const unsigned char*)cn, -1, -1, 0);
  X509_set_issuer_name(x, X509_get_subject_name(issuer_cert));
  X509_sign(x, issuer_key, EVP_sha256());
  return x;
}

int main(void) {
  /* Chain: ch[0]=EE (dummy), ch[1]=responder (issued by root), ch[2]=root (self-signed) */
  STACK_OF(X509) *ch = sk_X509_new_null();
  EVP_PKEY *root_key = make_key();
  X509 *root = make_self_signed("Test Root", root_key, 3600);
  EVP_PKEY *resp_key = make_key();
  X509 *resp = make_cert_signed("Test Responder", resp_key, root, root_key, 3600);
  EVP_PKEY *ee_key = make_key();
  X509 *ee = make_self_signed("Test EE", ee_key, 3600);
  sk_X509_push(ch, ee);
  sk_X509_push(ch, resp);
  sk_X509_push(ch, root);

  /* Build OCSP basic response with signer only (no issuer). */
  OCSP_BASICRESP *br = OCSP_BASICRESP_new();
  /* Sign the basic response with responder key; embed signer */
  if(!OCSP_basic_sign(br, resp, resp_key, EVP_sha256(), NULL, 0)) {
    fprintf(stderr, "OCSP_basic_sign failed\n");
    return 2;
  }

  /* Trust store with root only */
  X509_STORE *st = X509_STORE_new();
  X509_STORE_add_cert(st, root);

  /* Expect that injection adds an issuer and verify succeeds using our test hook. */
  (void)sf_ocsp_inject_selfsigned_issuer_for_test(br, ch);
  int ok = sf_ocsp_verify_for_test(br, ch, st);
  if(ok <= 0) {
    fprintf(stderr, "OCSP verify via test hook FAILED (%d)\n", ok);
    return 1;
  }
  fprintf(stderr, "OK\n");
  return 0;
}


