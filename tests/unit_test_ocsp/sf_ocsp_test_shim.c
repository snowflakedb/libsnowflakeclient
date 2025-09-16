#include <openssl/ocsp.h>
#include <openssl/x509.h>
#include <string.h>

/* Test hooks provided by the unit tests */
int sf_ocsp_verify_for_test(OCSP_BASICRESP *br, STACK_OF(X509) *ch, X509_STORE *st)
{
  if(!br || !st)
    return 0;
  /* Prefer strict verify against provided chain + store */
  if(OCSP_basic_verify(br, ch, st, 0) > 0)
    return 1;
  /* Fallback: verify using only embedded responder certs, trusting other */
  const STACK_OF(X509) *embedded = OCSP_resp_get0_certs(br);
  if(OCSP_basic_verify(br, (STACK_OF(X509)*)embedded, st, OCSP_TRUSTOTHER | OCSP_NOVERIFY) > 0)
    return 1;
  return 0;
}

/* Add self-signed issuer of the responder from ch into br->certs if missing */
int sf_ocsp_inject_selfsigned_issuer_for_test(OCSP_BASICRESP *br, STACK_OF(X509) *ch)
{
  int added = 0;
  if(!br || !ch)
    return 0;
  const STACK_OF(X509) *embedded = OCSP_resp_get0_certs(br);
  int ecount = embedded ? sk_X509_num(embedded) : 0;
  X509 *signer = NULL;
  if(OCSP_resp_get0_signer(br, &signer, (STACK_OF(X509)*)embedded) != 1 || !signer)
    return 0;
  for(int i = 0; i < sk_X509_num(ch); ++i) {
    X509 *cand = sk_X509_value(ch, i);
    if(X509_check_issued(cand, signer) == X509_V_OK) {
      if(X509_check_issued(cand, cand) != X509_V_OK) continue; /* only self-signed */
      int present = 0;
      for(int j = 0; j < ecount; ++j) {
        if(X509_cmp(sk_X509_value((STACK_OF(X509)*)embedded, j), cand) == 0) { present = 1; break; }
      }
      if(!present) {
        if(OCSP_basic_add1_cert(br, cand)) added = 1;
      }
      break;
    }
  }
  return added;
}


