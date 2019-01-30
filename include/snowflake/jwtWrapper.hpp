#ifndef JWT_WRAPPER_H
#define JWT_WRAPPER_H
typedef void * HEADER;
typedef void * CLAIMSET;
typedef void * CJWT;

#ifdef __cplusplus
extern "C" {
#endif
    /*
     * Any updates to this enum should
     * be reflected in IJwt header to
     * maintain interoperability.
     */
    typedef enum
    {
        HS256, HS384, HS512,
        RS256, RS384, RS512,
        ES256, ES384, ES512,
        UNKNOWN,
    }ALGORITHM_TYPE;

    HEADER HDR_buildHeader();
    void HDR_setAlgorithm(HEADER cjwt_header, ALGORITHM_TYPE alg);
    void HDR_setCustomHeaderEntry(HEADER cjwt_header, char *entry_type, char *entry_value);
    ALGORITHM_TYPE HDR_getAlgorithmType(HEADER cjwt_header);
    const char * HDR_getCustomHeaderEntry(HEADER cjwt_header, const char *entry_type);

    CLAIMSET  CSET_buildClaimset();
    void CSET_parseClaimset(CLAIMSET cjwt_cset, char *text);
    int CSET_containsClaimset(CLAIMSET cjwt_cset, char *key);
    void CSET_addStrClaim(CLAIMSET cjwt_cset, char *key, char *value);
    void CSET_addIntClaim(CLAIMSET cjwt_cset, char *key, long value);
    const char * CSET_getClaimsetString(CLAIMSET cjwt_cset, const char *key);
    long CSET_getClaimsetLong(CLAIMSET cjwt_cset, const char *key);
    double CSET_getClaimsetDouble(CLAIMSET cjwt_cset, const char *key);

    CJWT CJWT_buildCJWT();
    CJWT CJWT_buildCJWTFromString(char *text);
    void CJWT_delete_cjwt(CJWT c_jwt_token);
    const char *CJWT_serialize(CJWT cjwt_obj, EVP_PKEY *key);
    int CJWT_verify(CJWT c_jwt_token, EVP_PKEY *key);
    void CJWT_setClaimset(CJWT c_jwt_token, CLAIMSET cjwt_cset);
    CLAIMSET CJWT_getClaimset(CJWT c_jwt_token);
    void CJWT_setHeader(CJWT c_jwt_token, HEADER cjwt_hdr);
    HEADER CJWT_getHeader(CJWT C_jwt_token);


#ifdef __cplusplus
}
#endif
#endif