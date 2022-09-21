# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License").
# You may not use this file except in compliance with the License.
# A copy of the License is located at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# or in the "license" file accompanying this file. This file is distributed
# on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
# express or implied. See the License for the specific language governing
# permissions and limitations under the License.

import argparse
import math


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output", required=True, help="output file")
    args = parser.parse_args()
    with open(args.output, "w") as handle:
        for i in range(9):
            val = ((2.0 * math.pi) / 8.0) * i
            print(f"{val}, {math.sin(val)}",
                file=handle)


if __name__ == "__main__":
    main()
