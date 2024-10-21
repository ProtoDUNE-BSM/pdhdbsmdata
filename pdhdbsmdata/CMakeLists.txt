# basic source code CMakeLists.txt

art_make(BASENAME_ONLY
  MODULE_LIBRARIES
  larcore::ServiceUtil  
  larcore::Geometry_Geometry_service
  larcorealg::Geometry
  larcoreobj::SummaryData
  larcoreobj::SimpleTypesAndConstants
  larcoreobj::SummaryData
  lardataobj::RecoBase
  lardataobj::Simulation
  larsim::Simulation
  nusimdata_SimulationBase
  art::Framework_Core
  art::Framework_Principal
  art::Framework_Services_Registry
  art::Persistency_Common
  lardataobj::RawData
  artdaq_core::artdaq-core_Data
  artdaq_core::artdaq-core_Utilities
  canvas::canvas
  messagefacility::MF_MessageLogger
  hep_concurrency
  fhiclcpp::fhiclcpp
  fhiclcpp::types
  cetlib::cetlib
  cetlib_except::cetlib_except
  ifdh::ifdh 
  art_root_io::tfile_support
  ROOT::Core
  ROOT::Hist
  ROOT::Physics
  ROOT::Tree
  art_root_io::TFileService_service
)


install_headers()
install_source()
install_fhicl()
