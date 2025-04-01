#!/bin/bash

# Bash script to make the tarball that is given to the FermiGrid production job

DMWORKDIR=${PWD}
LOCALPRODDIR=${DMWORKDIR}/localProducts_*

if [ -z "${LOCALPRODDIR}" ]; then
  echo "[ERROR]: not source localproducts is not set up, cannot tar up required binaries."
  exit 1
fi

mkdir tar_state; cd tar_state
mkdir protodunedm
cp -r ${LOCALPRODDIR} ./protodunedm/
mkdir protodunedm/srcs
cp -r ${DMWORKDIR}/srcs/pdhdbsmdata ./protodunedm/srcs

tar --exclude '.git' -zcvf LocalProdNeutrinoNP04.Blob.tar.gz ./*

cd ..

mv tar_state/LocalProdNeutrinoNP04.Blob.tar.gz .
rm -rf tar_state

