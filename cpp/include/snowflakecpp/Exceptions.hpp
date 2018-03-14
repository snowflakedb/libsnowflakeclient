/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#ifndef SNOWFLAKECLIENT_EXCEPTIONS_HPP
#define SNOWFLAKECLIENT_EXCEPTIONS_HPP

#include <exception>
#include "Include.hpp"

class SnowflakeException: public std::exception {
public:
    SnowflakeException(Snowflake::CAPI::SF_ERROR_STRUCT *error);

    const char * what() const throw();

    Snowflake::CAPI::SF_STATUS code();

    const char *sqlstate();

    const char *msg();

    const char *sfqid();

    const char *file();

    int line();

protected:
    Snowflake::CAPI::SF_ERROR_STRUCT *error;
};

class GeneralException: public SnowflakeException {
public:
    GeneralException(Snowflake::CAPI::SF_ERROR_STRUCT *error) : SnowflakeException(error) {};
};

class

#endif //SNOWFLAKECLIENT_EXCEPTIONS_HPP
