//
// Created by smirzaei on 5/5/21.
//

#ifndef SNOWFLAKECLIENT_SFLOGGERCWRAPPER_H
#define SNOWFLAKECLIENT_SFLOGGERCWRAPPER_H

#include "snowflake/logger.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* C wrapper method for SFLogger::getExternalLogger()
*/
void *getExternalLogger();

/**
 * Method used internally in SFLogger.cpp, which call logLineVA
 */
void externalLogger_logLineVA(SF_LOG_LEVEL logLevel,
                              const char *fileName,
                              const char *msgFmt, va_list args);


/**
 * C wrapper method to initializes the external logger through SFLogger::init
 */
void setExternalLogger(void *logger);

#ifdef __cplusplus
}
#endif
#endif //SNOWFLAKECLIENT_SFLOGGERCWRAPPER_H
