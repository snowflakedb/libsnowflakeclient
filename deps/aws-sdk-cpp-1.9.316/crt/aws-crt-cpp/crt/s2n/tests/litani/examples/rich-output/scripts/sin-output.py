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
import jinja2


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--input", required=True, help="input csv")
    args = parser.parse_args()

    data = {}
    with open(args.input, "r") as handle:
        for line in handle:
            values = line.split(',')
            data[float(values[0].strip())] = float(values[1].strip())

    env = jinja2.Environment(
        loader=jinja2.FileSystemLoader("./templates"),
        autoescape=jinja2.select_autoescape(
            enabled_extensions=('csv'),
            default_for_string=True))
    table_template = env.get_template("sin-output.jinja.html")
    output = table_template.render(data=data)
    print(output)


if __name__ == "__main__":
    main()
