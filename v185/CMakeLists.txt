find_package(VTK COMPONENTS REQUIRED
  vtkIOLegacy
  vtkIOXML
  vtkIOXMLParser
  vtkIOExport
  vtksys
)

include(${VTK_USE_FILE})

find_package(ospray REQUIRED)

add_executable(viewEdges viewEdges.cpp)
target_link_libraries(viewEdges PRIVATE ${OSPRAY_LIBRARIES} ${VTK_LIBRARIES})
