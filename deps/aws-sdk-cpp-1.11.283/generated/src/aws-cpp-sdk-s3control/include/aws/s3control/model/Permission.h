﻿/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/s3control/S3Control_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>

namespace Aws
{
namespace S3Control
{
namespace Model
{
  enum class Permission
  {
    NOT_SET,
    READ,
    WRITE,
    READWRITE
  };

namespace PermissionMapper
{
AWS_S3CONTROL_API Permission GetPermissionForName(const Aws::String& name);

AWS_S3CONTROL_API Aws::String GetNameForPermission(Permission value);
} // namespace PermissionMapper
} // namespace Model
} // namespace S3Control
} // namespace Aws
