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

#ifndef HEADER_ENGINE_H
#define HEADER_ENGINE_H

/* These flags are used to control combinations of algorithm (methods)
 * by bitwise "OR"ing. */
#define ENGINE_METHOD_RAND (unsigned int)0x0008

/* This flag if for an ENGINE that does not want its methods registered as
 * part of ENGINE_register_all_complete() for example if the methods are
 * not usable as default methods. */
#define ENGINE_FLAGS_NO_REGISTER_ALL (int)0x0008

#endif
