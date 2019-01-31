/*
 * * Copyright (c) 2012-2019 Snowflake Computing Inc. All rights reserved.
 */
#include "snowflake/IJwt.hpp"
#include "snowflake/jwtWrapper.h"

using namespace Snowflake::Client::Jwt;

typedef std::shared_ptr<IClaimSet> ClaimSetPtr;
typedef std::shared_ptr<IHeader> HeaderPtr;


HEADER HDR_buildHeader()
{
    return static_cast<HEADER>(IHeader::buildHeader());
}

void HDR_setAlgorithm(HEADER cjwt_header, ALGORITHM_TYPE alg)
{
    IHeader *hdr = static_cast<IHeader *>(cjwt_header);
    hdr->setAlgorithm((AlgorithmType)alg);
}

void HDR_setCustomHeaderEntry(HEADER cjwt_header, const char *entry_type, const char *entry_value)
{
    IHeader *hdr = static_cast<IHeader *>(cjwt_header);
    hdr->setCustomHeaderEntry(std::string(entry_type), std::string(entry_value));
}

ALGORITHM_TYPE HDR_getAlgorithmType(HEADER cjwt_header)
{
    IHeader *hdr = static_cast<IHeader *>(cjwt_header);
    AlgorithmType algType = hdr->getAlgorithmType();
    return (ALGORITHM_TYPE)algType;
}

const char * HDR_getCustomHeaderEntry(HEADER cjwt_header, const char *entry_type)
{
    IHeader *hdr = static_cast<IHeader *>(cjwt_header);
    return (hdr->getCustomHeaderEntry(std::string(entry_type))).c_str();
}

void HDR_delete(HEADER cjwt_header)
{
    IHeader *hdr = static_cast<IHeader *>(cjwt_header);
    delete(hdr);
}

/* CLAIMSET WRAPPER */


CLAIMSET CSET_buildClaimset()
{
    return static_cast<CLAIMSET>(IClaimSet::buildClaimSet());
}

int CSET_containsClaimset(CLAIMSET cjwt_cset, const char *key)
{
    IClaimSet *cset = static_cast<IClaimSet *>(cjwt_cset);
    if (cset->containsClaim(std::string(key)))
    {
        return 1;
    }
    return 0;
}

void CSET_addStrClaimset(CLAIMSET cjwt_cset, const char *key, char *val)
{
    IClaimSet *cset = static_cast<IClaimSet *>(cjwt_cset);
    cset->addClaim(std::string(key), std::string(val));
}

void CSET_addIntClaimset(CLAIMSET cjwt_cset, const char *key, long value)
{
    IClaimSet *cset = static_cast<IClaimSet *>(cjwt_cset);
    cset->addClaim(std::string(key), value);
}

const char * CSET_getClaimsetString(CLAIMSET cjwt_cset, const char *key)
{
    IClaimSet *cset = static_cast<IClaimSet *>(cjwt_cset);
    return (cset->getClaimInString(std::string(key))).c_str();
}

long CSET_getClaimsetLong(CLAIMSET cjwt_cset, const char *key)
{
    IClaimSet *cset = static_cast<IClaimSet *>(cjwt_cset);
    return cset->getClaimInLong(std::string(key));
}

double CSET_getClaimsetDouble(CLAIMSET cjwt_cset, const char *key)
{
    IClaimSet *cset = static_cast<IClaimSet *>(cjwt_cset);
    return cset->getClaimInDouble(std::string(key));
}

void CSET_removeClaim(CLAIMSET cjwt_cset, char *key)
{
    IClaimSet *cset = static_cast<IClaimSet *>(cjwt_cset);
    cset->removeClaim(std::string(key));
}

void CSET_deleteClaimset(CLAIMSET cjwt_cset)
{
    IClaimSet *cset = static_cast<IClaimSet *>(cjwt_cset);
    delete(cset);
}

/* JWT Token Wrapper */


CJWT CJWT_buildCJWT()
{
    return static_cast<CJWT>(IJwt::buildIJwt());
}

CJWT CJWT_buildCJWTFromString(const char *text)
{
    std::string cjwt_text(text);
    return static_cast<CJWT>(IJwt::buildIJwt(cjwt_text));
}

const char * CJWT_serialize(CJWT cjwt_obj, EVP_PKEY *key)
{
    IJwt *ijwt_obj = static_cast<IJwt *>(cjwt_obj);

    return (ijwt_obj->serialize(key)).c_str();
}

int CJWT_verify(CJWT cjwt_obj, EVP_PKEY *key)
{
    IJwt *ijwt_obj = static_cast<IJwt *>(cjwt_obj);
    if ((ijwt_obj->verify(key, false)))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void CJWT_setClaimset(CJWT cjwt_obj, CLAIMSET cjwt_cset_obj)
{
    IClaimSet *cset_obj = NULL;
    IJwt *ijwt_obj = NULL;
    IClaimSet *temp_obj = NULL;

    if (cjwt_obj == NULL || cjwt_cset_obj == NULL)
    {
        return;
    }
    cset_obj = static_cast<IClaimSet *>(cjwt_cset_obj);
    ijwt_obj = static_cast<IJwt *>(cjwt_obj);

    ijwt_obj->setClaimSet(ClaimSetPtr(cset_obj));
}

CLAIMSET CJWT_getClaimset(CJWT cjwt_obj)
{
    IJwt *ijwt_obj = NULL;
    if (cjwt_obj == NULL)
    {
        return NULL;
    }

    ijwt_obj = static_cast<IJwt *>(cjwt_obj);
    return static_cast<CLAIMSET>((ijwt_obj->getClaimSet()).get());
}

void CJWT_setHeader(CJWT cjwt_obj, HEADER cjwt_hdr_obj)
{
    IHeader *hdr_obj = NULL;
    IJwt *ijwt_obj = NULL;

    if (cjwt_obj == NULL || cjwt_hdr_obj == NULL)
    {
        return;
    }

    hdr_obj = static_cast<IHeader *>(cjwt_hdr_obj);
    ijwt_obj = static_cast<IJwt *>(cjwt_obj);
    ijwt_obj->setHeader(HeaderPtr(hdr_obj));
}

HEADER CJWT_getHeader(CJWT cjwt_obj)
{
    /*
     * Be cautious while using this.
     * The void * returned by this function
     * is not a managed shared_ptr. You will
     * have to manage the memory of this pointer
     * yourself.
     */
    IJwt *ijwt_obj = static_cast<IJwt *>(cjwt_obj);
    return static_cast<HEADER>((ijwt_obj->getHeader()).get());
}

void CJWT_deleteToken(CJWT cjwt_obj)
{
    IJwt *ijwt_obj = static_cast<IJwt *>(cjwt_obj);
    if (ijwt_obj->getHeader() != NULL)
    {
        /* TODO LOG Header not NULL */
    }
    if (ijwt_obj->getClaimSet() != NULL)
    {
        /* TODO LOG Claimset not NULL */
    }
    delete ijwt_obj;
}
