// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

const fs = require('fs');
const path = require('path');
const glob = require('glob');

const config = [];
const filenames = glob.sync(path.resolve(__dirname, `../test/data/tables/`, `*.arrow`));

countBys = {
    "tracks": ['origin', 'destination']
}
counts = {
    "tracks": [
        {col: 'lat',    test: 'gt', value: 0        },
        {col: 'lng',    test: 'gt', value: 0        },
        {col: 'origin', test: 'eq', value: 'Seattle'},
    ]
}

for (const filename of filenames) {
    const { name } = path.parse(filename);
    if (name in counts) {
        config.push({
            name,
            buffers: [fs.readFileSync(filename)],
            countBys: countBys[name],
            counts: counts[name],
        });
    }
}

module.exports = config;
