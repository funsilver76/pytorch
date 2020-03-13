#!/bin/bash

if [ -z "$GBENCH_REPORT_FILE" ]
then
  echo "Please set GBENCH_REPORT_FILE"
  exit 1
fi

set -eux -o pipefail
WORKDIR=.

echo "GBENCH_REPORT_FILE:$GBENCH_REPORT_FILE"
echo "WORKDIR:$WORKDIR"

DEVICE_DIR=/storage/emulated/0/Android/data/org.pytorch.testapp.bench/files/
DEVICE_FILE_PATH=$DEVICE_DIR/$GBENCH_REPORT_FILE
REPORT_FILE_PATH=$WORKDIR/$GBENCH_REPORT_FILE

REPORT_FILE_PDF_PATH="${WORKDIR}/${GBENCH_REPORT_FILE}.pdf"

echo "Device DEVICE_FILE_PATH:${DEVICE_FILE_PATH}"
echo "Host   REPORT_FILE_PATH:${REPORT_FILE_PATH}"

adb shell cat $DEVICE_FILE_PATH > $REPORT_FILE_PATH

echo "PDF REPORT_FILE_PATH:${REPORT_FILE_PDF_PATH}"

python bench_reportfile_process.py $REPORT_FILE_PATH $REPORT_FILE_PDF_PATH && open $REPORT_FILE_PDF_PATH
