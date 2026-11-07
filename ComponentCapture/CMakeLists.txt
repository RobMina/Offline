find_package(TBB REQUIRED)
find_package(Threads REQUIRED)

set(capture_libraries
    Offline::Mu2eG4 Offline::Mu2eG4Helper Offline::MCDataProducts Offline::DataProducts Offline::ConfigTools
    Offline::GeometryService Offline::CosmicRayShieldGeom Offline::GeneralUtilities
    art::Framework_Core art::Framework_Principal art::Framework_Services_Registry
    art_root_io::TFileService art_root_io::tfile_support art::Persistency_Common
    art::Persistency_Provenance art::Utilities canvas::canvas fhiclcpp::fhiclcpp fhiclcpp::types
    cetlib::cetlib cetlib_except::cetlib_except messagefacility::MF_MessageLogger
    CLHEP::CLHEP ROOT::Core ROOT::Tree
    Geant4::G4global Geant4::G4geometry Geant4::G4materials Geant4::G4particles
    Geant4::G4track Geant4::G4processes Geant4::G4intercoms Geant4::G4run Geant4::G4graphics_reps
    TBB::tbb Threads::Threads)

cet_build_plugin(ComponentRecorder art::tool
    REG_SOURCE src/ComponentRecorder_tool.cc LIBRARIES REG ${capture_libraries})
cet_build_plugin(ConstructCaptureBox art::tool
    REG_SOURCE src/ConstructCaptureBox_tool.cc LIBRARIES REG ${capture_libraries})
cet_build_plugin(ConstructCaptureCRVModule art::tool
    REG_SOURCE src/ConstructCaptureCRVModule_tool.cc LIBRARIES REG ${capture_libraries})
cet_build_plugin(ComponentCaptureDump art::module
    REG_SOURCE src/ComponentCaptureDump_module.cc LIBRARIES REG ${capture_libraries})
cet_build_plugin(ComponentCaptureTestGate art::tool
    REG_SOURCE src/ComponentCaptureTestGate_tool.cc LIBRARIES REG ${capture_libraries})
cet_build_plugin(ComponentRecorderCallbackFixture art::module
    REG_SOURCE src/ComponentRecorderCallbackFixture_module.cc LIBRARIES REG ${capture_libraries})
cet_build_plugin(ComponentZeroStepProbe art::tool
    REG_SOURCE src/ComponentZeroStepProbe_tool.cc LIBRARIES REG ${capture_libraries})
cet_build_plugin(ComponentZeroStepDump art::module
    REG_SOURCE src/ComponentZeroStepDump_module.cc LIBRARIES REG ${capture_libraries})
cet_build_plugin(ComponentEndKinematicsFixture art::module
    REG_SOURCE src/ComponentEndKinematicsFixture_module.cc LIBRARIES REG ${capture_libraries})
cet_build_plugin(ComponentSecondaryZeroStepProbe art::tool
    REG_SOURCE src/ComponentSecondaryZeroStepProbe_tool.cc LIBRARIES REG ${capture_libraries})

install_source(SUBDIRS src)
install_fhicl(SUBDIRS fcl)
install(DIRECTORY geom DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/Offline/ComponentCapture)
