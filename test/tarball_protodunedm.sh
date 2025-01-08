#!/bin/bash

# Bash script to make the tarball that is given to the FermiGrid production job

DMWORKDIR=/exp/dune/app/users/chasnip/CERN_Fellowship/protodunedm_mc_simulation
LOCALPROD=localProducts_larsoft_v10_01_03_e26_prof

LOCALPRODDIR=${DMWORKDIR}/${LOCALPROD}

if [ -z "${LOCALPRODDIR}" ]; then
  echo "[ERROR]: not source localproducts is not set up, cannot tar up required binaries."
  exit 1
fi

mkdir tar_state; cd tar_state
mkdir protodunedm_mc_simulation
cp -r ${LOCALPRODDIR} ./protodunedm_mc_simulation/
mkdir protodunedm_mc_simulation/srcs
cp -r ${DMWORKDIR}/srcs/pdhdbsmsimulation ./protodunedm_mc_simulation/srcs
cp -r ${DMWORKDIR}/srcs/pdhdbsmdata ./protodunedm_mc_simulation/srcs
cp -r ${DMWORKDIR}/srcs/dunetrigger ./protodunedm_mc_simulation/srcs

tar --exclude '.git' -zcvf LocalProdNumu.Blob.tar.gz ./*

cd ..

mv tar_state/LocalProdNumu.Blob.tar.gz .
rm -rf tar_state

