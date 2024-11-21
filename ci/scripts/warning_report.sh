#!/bin/bash

python3 ci/scripts/generate_warning_report.py --build-log build.log --load-warnings ci/scripts/warnings_baseline.json --dump-warnings warnings.json --report report.txt
