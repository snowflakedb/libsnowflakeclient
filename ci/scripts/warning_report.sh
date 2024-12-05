#!/bin/bash

if [ ! -f ci/scripts/warnings_baseline.json ]; then
  echo "Baseline file does not exist."
  echo "[]" > ci/scripts/warnings_baseline.json
fi

python3 ci/scripts/generate_warning_report.py --build-log build.log --load-warnings ci/scripts/warnings_baseline.json --dump-warnings warnings.json --report report.md

if [[ -n "${GITHUB_STEP_SUMMARY}" ]];
then
  cat report.md >> "$GITHUB_STEP_SUMMARY"
fi

if [[ "$(head -n 1 report.md)" == "### Failed" ]];
then
  if [[ "${BRANCH_NAME}" == "master" ]]
  then
    echo "Failures are ignored on master since there is no base branch to reference."
    exit 0
  else
    echo "Warnings check failed. Check workflow summary to find out the details."
    exit 1
  fi
fi
