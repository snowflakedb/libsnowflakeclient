/*
* Copyright 2010-2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include <aws/core/utils/DNS.h>
#include <ctype.h>

namespace Aws
{
    namespace Utils
    {
        bool IsValidDnsLabel(const Aws::String& label)
        {
            // Valid DNS hostnames are composed of valid DNS labels separated by a period.
            // Valid DNS labels are characterized by the following:
            // 1- Only contains alphanumeric characters and/or dashes
            // 2- Cannot start or end with a dash
            // 3- Have a maximum length of 63 characters (the entirety of the domain name should be less than 255 bytes)

            if (label.empty())
                return false;

            if (label.size() > 63)
                return false;

            if (!isalnum(label.front()))
                return false; // '-' is not acceptable as the first character

            if (!isalnum(label.back()))
                return false; // '-' is not acceptable as the last  character

            for (size_t i = 1, e = label.size() - 1; i < e; ++i)
            {
                auto c = label[i];
                if (c != '-' && !isalnum(c))
                    return false;
            }

            return true;
        }
    }
}
