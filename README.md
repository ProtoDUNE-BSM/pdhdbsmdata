# LArSoftDataTools

LArSoft product to handle data processing of PD-HD data from hdf5 format to reconstructed art::ROOT files.

## Apptainer Container

You will need an SL7 container to run LArSoft. This can be done on gpvm and lxplus.

Run the following command when on gpvm:

```bash
/cvmfs/oasis.opensciencegrid.org/mis/apptainer/current/bin/apptainer shell --shell=/bin/bash -B /cvmfs,/exp,/nashome,/pnfs/dune,/opt,/run/user,/etc/hostname,/etc/hosts,/etc/krb5.conf --ipc --pid /cvmfs/singularity.opensciencegrid.org/fermilab/fnal-dev-sl7:latest
```

And this command when on lxplus (they are the same command), just different directories need to be made accessible:

```bash
/cvmfs/oasis.opensciencegrid.org/mis/apptainer/current/bin/apptainer shell --shell=/bin/bash -B /cvmfs,/eos,/afs/cern.ch/work,/opt,/run/user,/etc/hostname,/etc/hosts,/etc/krb5.conf --ipc --pid /cvmfs/singularity.opensciencegrid.org/fermilab/fnal-dev-sl7:latest
```

## Setting up Code and Development Area

You need to set up LArSoft, dunesw, pull the code from GitHub and create your development area. Follow this script:

```bash
VERSION=v10_01_03d00
QUALS=e26:prof
DIRECTORY=protodunedm_data_analysis
export WORKDIR=/exp/dune/app/users/$USER/ # or on lxplus /afs/cern.ch/work/c/${USER}/public/

source /cvmfs/dune.opensciencegrid.org/products/dune/setup_dune.sh
export UPS_OVERRIDE="-H Linux64bit+3.10-2.17"
setup dunesw -v ${VERSION} -q ${QUALS}

cd ${WORKDIR}
mkdir -p ${DIRECTORY}
cd ${DIRECTORY}

# Create a new development area using mrb
mrb newDev -v ${VERSION} -q ${QUALS}
source ${WORKDIR}/${DIRECTORY}/localProducts*/setup

# Clone LArSoftDataTools from ProtoDUNE-BSM GitHub
mrb g https://github.com/ProtoDUNE-BSM/pdhdbsmdata.git

cd ${MRB_BUILDDIR}

mrbsetenv

mrb i -j4
mrbslp
```

Once the development area has been created you can set up the environment withe following script:

```bash
VERSION=v10_01_03d00
QUALS=e26:prof

source /cvmfs/dune.opensciencegrid.org/products/dune/setup_dune.sh
setup dunesw ${VERSION} -q ${QUALS}
echo $DUNESW_DIR

source localProducts_*/setup
mrbslp
```

## Certificates and Finding Files

You may want to use tools such as metacat and rucio to find raw data files. To set up metacat run

```bash
setup metacat
export METACAT_AUTH_SERVER_URL=https://metacat.fnal.gov:8143/auth/dune
export METACAT_SERVER_URL=https://metacat.fnal.gov:9443/dune_meta_prod/app
```

Then rucio

```bash
setup rucio
```

Not sure this is relevant outside of the FNAL machines, but if you are on gpvm you will need a grid proxy:

```bash
kx509
voms-proxy-init --noregen -rfc -voms dune:/dune/Role=Analysis
```

## Running the Decoder on Raw Data

The LArSoft decoder takes raw hdf5 format files and generates TPC waveform and trigger information in an art::ROOT file format. Using this package you can run the usual LArSoft decoder modules for the TPC and trigger infromation as well as apply additional filters that are relevant for PD-HD-BSM studies.

There is an example of a fcl file that runs the decoder in `example/protodunehd_dm_decoder_modularfilter.fcl` which also include 3 filters. To run this use the following command:

```bash
LD_PRELOAD=$XROOTD_LIB/libXrdPosixPreload.so lar -c .src/pdhdbsmdata/example/protodunehd_dm_decoder_modularfilter.fcl root://eospublic.cern.ch:1094//eos/experiment/neutplatform/protodune/dune/hd-protodune/75/9a/np04hd_raw_run029425_0887_dataflow0_datawriter_0_20241006T161230.hdf5
```

where an example protodune file is given.

## Producers and Filters

The fcl file `protodunehd_dm_decoder_modularfilter.fcl` contains a series of filters and producers that takes a hdf5 file and produces `art::ROOT` files whilst removing events that are not of interest. This section of `protodunehd_dm_decoder_modularfilter.fcl` shows how the filters and producers are ordered.

```fcl
  producers:
  {
    # Raw decoding
    tpcrawdecoder: @local::PDHDTPCReaderDefaults
    triggerrawdecoder: @local::PDHDTriggerReader3Defaults 
    timingrawdecoder: @local::PDHDTimingRawDecoder
    pdhddaphne: @local::DAPHNEReaderPDHD
  }

  filters:
  {
    filterspillon: @local::pdhdfilter_spillon
    triggertypefilter: @local::pdhdtriggertypefilter
    extmuonfilter: @local::pdhdextmuonfilter
  }

  produce: [
    filterspillon,
    triggerrawdecoder,
    triggertypefilter,
    tpcrawdecoder,
    timingrawdecoder,
    extmuonfilter
    # Don't run pdhddaphne in this version of dunesw as there is an issue
  ]
```

The producer modules are referenced in `producers` and the filters in `filters`. The order in which the modules are called and applied to the data is given in `produce`.

It can be seen that the filter `filterspillon` is called first. The module is defined in `PDHDSPSSpillFilter_module.cc`. This determines whether the event occurred when the SPS beam spill was ON or OFF. The user can decide if they want a spill ON or spill OFF sample by altering a config parameter. Also, the option to remove events by using the PoT (Protons on Target) threshold is now available ($1 \times 10^{12}$).

The second filter, `triggertypefilter`, comes after trigger decoder. The module is defined in `PDHDTriggerTypeFilter_module.cc`. This uses the trigger information to determine whether the event was a ground shake type, in which case the event is removed.

The third filter is still in development. It is the `extmuonfilter` module that comes at the end of the process and is defined in `PDHDExtMuonFilter_module.cc`. The filter aims to remove events where the shower that caused the trigger is aligned in drift time with a muon entering the front of the TPC. This is a major source of background and filtering a large of them out at the decoder level would be useful.
