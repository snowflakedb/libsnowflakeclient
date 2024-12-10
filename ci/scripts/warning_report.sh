#!/bin/bash

python3 ci/scripts/generate_warning_report.py --build-log build.log --load-warnings ci/scripts/warnings_baseline.json --dump-warnings warnings.json --report report.md

if [[ -n "${GITHUB_STEP_SUMMARY}" ]];
then
  cat report.md >> "$GITHUB_STEP_SUMMARY"
fi

if [[ "$(head -n 1 report.md)" == "### Failed" ]];
then
  exit 1
fi
