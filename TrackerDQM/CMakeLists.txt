cet_make_library(
    SOURCE
      src/TrackerDigiDQM.cc
    LIBRARIES PUBLIC
      Offline::DQMHelpers
      Offline::RecoDataProducts
      Offline::DataProducts
      ROOT::Hist
)

install_source(SUBDIRS src)
install_headers(USE_PROJECT_NAME SUBDIRS inc)
install_fhicl(SUBDIRS fcl SUBDIRNAME Offline/TrackerDQM/fcl)
