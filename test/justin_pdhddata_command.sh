#!/bin/bash

:<<'EOF'

Submit jobs with justin using metacat query to find np04 BSM trigger files

make the tarball and upload it to cvmfs by running:

source srcs/pdhdbsmdata/test/tarball_protodunedm.sh
htgettoken -a htvaultprod.fnal.gov -i dune
INPUT_TAR_DIR_LOCAL=`justin-cvmfs-upload LocalProdNeutrinoNP04.Blob.tar.gz`

EOF

WORKDIR=${PWD}
USERF=${USER}/ProtoDUNEBSM/PDHDBSMData
FNALURL='https://fndcadoor.fnal.gov:2880/dune/scratch/users'
MQL_QUERY="files from dune:all where core.runs in (29425) and core.run_type=hd-protodune and core.data_tier=raw and core.file_type=detector limit 1"

echo "Setting INPUT_TAR_DIR_LOCAL = $INPUT_TAR_DIR_LOCAL"

justin simple-workflow --mql "${MQL_QUERY}" \
  --env INPUT_TAR_DIR_LOCAL="$INPUT_TAR_DIR_LOCAL" \
  --jobscript ${WORKDIR}/srcs/pdhdbsmdata/test/submit_pdhdbsm_jobscript.jobscript \
  --rss-mb 8000 --env SPS_DATA="spillrun029425.csv" \
  --scope usertests --lifetime-days 1 \
  --output-pattern "*_pdhdreco2_*.root:${FNALURL}/${USERF}"

:<<'EOF'
justin-test-jobscript --mql "${MQL_QUERY}" \
  --env INPUT_TAR_DIR_LOCAL="$INPUT_TAR_DIR_LOCAL" \
  --jobscript ${WORKDIR}/srcs/pdhdbsmdata/test/submit_pdhdbsm_jobscript.jobscript \
  --env SPS_DATA="spillrun029425.csv" \
  #--env NUM_EVENTS=1
EOF
