#!/bin/bash

:<<'EOF'

To use this jobscript to process 5 files from the dataset fardet-hd__fd_mc_2023a_reco2__full-reconstructed__v09_81_00d02__standard_reco2_dune10kt_nu_1x2x6__prodgenie_nu_dune10kt_1x2x6__out1__validation
data and put the output in the $USER namespace (MetaCat) and saves the output in /scratch
Use this command to create the workflow:

EOF

WORKDIR=${PWD}
USERF=${USER}/ProtoDUNEBSM/PDHDBSMData
FNALURL='https://fndcadoor.fnal.gov:2880/dune/scratch/users'
MQL_QUERY="files from dune:all where core.runs in (29424) and core.run_type=hd-protodune and core.data_tier=raw and core.file_type=detector limit 1"

echo "Setting INPUT_TAR_DIR_LOCAL = $INPUT_TAR_DIR_LOCAL"

justin simple-workflow --mql "${MQL_QUERY}" \
  --env INPUT_TAR_DIR_LOCAL="$INPUT_TAR_DIR_LOCAL" \
  --jobscript ${WORKDIR}/srcs/pdhdbsmdata/test/submit_pdhdbsm_jobscript.jobscript \
  --rss-mb 8000 --env NUM_EVENTS=1 \
  --scope usertests --lifetime-days 1 \
  --output-pattern "*_pdhdreco2_*.root:${FNALURL}/${USERF}"

#justin-test-jobscript \
#  --mql ${MQL_QUERY} \
#  --jobscript ${WORKDIR}/srcs/pdhdbsmdata/test/submit_pdhdbsm_jobscript.jobscript \
#  --env NUM_EVENTS=1
  #--output-pattern '*_pdhdreco2_*.root:$FNALURL/$USERF' 
