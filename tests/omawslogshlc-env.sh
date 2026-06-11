#!/bin/bash

omawslogshlc_require_env() {
	if [[ -z "${AWS_CWL_BEARER_TOKEN}" ]]; then
		echo "SKIP: AWS_CWL_BEARER_TOKEN environment variable not set - SKIPPING"
		exit 77
	fi
	if [[ -z "${AWS_CWL_REGION}" ]]; then
		echo "SKIP: AWS_CWL_REGION environment variable not set - SKIPPING"
		exit 77
	fi
	if [[ -z "${AWS_CWL_LOG_GROUP}" ]]; then
		echo "SKIP: AWS_CWL_LOG_GROUP environment variable not set - SKIPPING"
		exit 77
	fi
	if [[ -z "${AWS_CWL_LOG_STREAM}" ]]; then
		echo "SKIP: AWS_CWL_LOG_STREAM environment variable not set - SKIPPING"
		exit 77
	fi
}
