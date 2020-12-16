// ======================================================================== //
// Copyright 2009-2019 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //


/* This is a small example tutorial how to use OSPRay in an application.
 *
 * On Linux build it in the build_directory with
 *   gcc -std=c99 ../apps/ospTutorial.c -I ../ospray/include -I .. ./libospray.so -Wl,-rpath,. -o ospTutorial
 * On Windows build it in the build_directory\$Configuration with
 *   cl ..\..\apps\ospTutorial.c -I ..\..\ospray\include -I ..\.. ospray.lib
 */

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#ifdef _WIN32
#  include <malloc.h>
#else
#  include <alloca.h>
#endif
#include "ospray/ospray_util.h"

// vtk
#include <vtkSmartPointer.h>
#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkCellTypes.h>
#include <vtkDataSet.h>
#include <vtkDoubleArray.h>
#include <vtkFloatArray.h>
#include <vtkGenericDataObjectReader.h>
#include <vtkPolyData.h>
#include <vtkXMLPolyDataReader.h>
#include <vtksys/SystemTools.hxx>


#include <chrono> 
using namespace std::chrono; 

// helper function to write the rendered image as PPM file
void writePPM(
    const char *fileName, int size_x, int size_y, const uint32_t *pixel)
{
  FILE *file = fopen(fileName, "wb");
  if (!file) {
    fprintf(stderr, "fopen('%s', 'wb') failed: %d", fileName, errno);
    return;
  }
  fprintf(file, "P6\n%i %i\n255\n", size_x, size_y);
  unsigned char *out = (unsigned char *)alloca(3 * size_x);
  for (int y = 0; y < size_y; y++) {
    const unsigned char *in =
        (const unsigned char *)&pixel[(size_y - 1 - y) * size_x];
    for (int x = 0; x < size_x; x++) {
      out[3 * x + 0] = in[4 * x + 0];
      out[3 * x + 1] = in[4 * x + 1];
      out[3 * x + 2] = in[4 * x + 2];
    }
    fwrite(out, 3 * size_x, sizeof(char), file);
  }
  fprintf(file, "\n");
  fclose(file);
}

struct vec3f { float x, y, z; };

int main(int argc, const char **argv) {
  std::vector<vec3f> vertices;
  std::vector<vec3f> overtices;
  std::vector<unsigned int> indices;

  if (argc != 3) {
    std::cerr << "expected " << argv[0] << " -[RC|PT] path/to/edges.vtp" << std::endl;
    return 1;
  }

  bool pathtrace = true;
  if (!strcmp(argv[1], "-RC")) {
    pathtrace = false;
  }
  
  auto reader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
  reader->SetFileName(argv[2]);
  reader->Update();
  vtkDataSet *dataSet = reader->GetOutput();
  int numberOfCells  = dataSet->GetNumberOfCells();
  int numberOfPoints = dataSet->GetNumberOfPoints();
  double point[3];
  for (int i = 0; i < numberOfPoints; i++) {
    dataSet->GetPoint(i, point);
    vertices.push_back({static_cast<float>(point[0]),
	static_cast<float>(point[1]),
	static_cast<float>(point[2])});
  }
  for (int i = 0; i < numberOfCells; i++) {
    vtkCell *cell = dataSet->GetCell(i);
    
    if (cell->GetCellType() == VTK_LINE) {
      int p0 = static_cast<int>(cell->GetPointId(0));
      int p1 = static_cast<int>(cell->GetPointId(1));
      if (false && i < 10)
	{
	  std::cerr << p0 << " " << p1 << std::endl;
	  std::cerr
	    << vertices[p0].x << "," << vertices[p0].y << "," << vertices[p0].z << " " 
	    << vertices[p1].x << "," << vertices[p1].y << "," << vertices[p1].z << std::endl;
	}
      overtices.push_back(vertices[p0]);
      overtices.push_back(vertices[p1]);
      indices.push_back(i * 2);
    }
  }
  std::cerr << "READ " << vertices.size() << " " << numberOfCells << " " << overtices.size() << std::endl;
  
  // image size
  int imgSize_x = 1024; // width
  int imgSize_y = 768; // height

  // camera
  float cam_pos[] = {-10.f, 10.f, 0.f};
  float cam_up[] = {1.f, 0.f, 0.f};
  float cam_view[] = {0.3f, -0.7f, 0.f};


#ifdef _WIN32
  int waitForKey = 0;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
    // detect standalone console: cursor at (0,0)?
    waitForKey = csbi.dwCursorPosition.X == 0 && csbi.dwCursorPosition.Y == 0;
  }
#endif


  // initialize OSPRay; OSPRay parses (and removes) its commandline parameters,
  // e.g. "--osp:debug"
  OSPError init_error = ospInit(&argc, argv);
  if (init_error != OSP_NO_ERROR)
    return init_error;

  // create and setup camera
  OSPCamera camera = ospNewCamera("perspective");
  ospSetFloat(camera, "aspect", imgSize_x / (float)imgSize_y);
  ospSetParam(camera, "position", OSP_VEC3F, cam_pos);
  ospSetParam(camera, "direction", OSP_VEC3F, cam_view);
  ospSetParam(camera, "up", OSP_VEC3F, cam_up);
  ospCommit(camera); // commit each object to indicate modifications are done


  // create and setup model and mesh
  OSPGeometry mesh = ospNewGeometry("curve");
  OSPData data = ospNewSharedData1D(overtices.data(), OSP_VEC3F, overtices.size());
  ospCommit(data);
  ospSetObject(mesh, "vertex.position", data);

  OSPData idata = ospNewSharedData1D(indices.data(), OSP_UINT, indices.size());
  ospCommit(idata);
  ospSetObject(mesh, "index", idata);

  ospSetInt(mesh, "type", OSP_ROUND);
  ospSetInt(mesh, "basis", OSP_LINEAR);
  ospSetFloat(mesh, "radius", .005);
  ospCommit(mesh);
  ospRelease(data); // we are done using this handle

  OSPMaterial mat = (pathtrace?ospNewMaterial("pathtracer", "obj"):ospNewMaterial("scivis", "obj"));
  ospCommit(mat);

  // put the mesh into a model
  OSPGeometricModel model = ospNewGeometricModel(mesh);
  ospSetObject(model, "material", mat);
  ospCommit(model);
  ospRelease(mesh);
  ospRelease(mat);

  // put the model into a group (collection of models)
  OSPGroup group = ospNewGroup();
  ospSetObjectAsData(group, "geometry", OSP_GEOMETRIC_MODEL, model);
  ospCommit(group);
  ospRelease(model);

  // put the group into an instance (give the group a world transform)
  OSPInstance instance = ospNewInstance(group);
  ospCommit(instance);
  ospRelease(group);

  // put the instance in the world
  OSPWorld world = ospNewWorld();
  ospSetObjectAsData(world, "instance", OSP_INSTANCE, instance);
  ospRelease(instance);

  // create and setup light for Ambient Occlusion
  OSPLight light = ospNewLight("ambient");
  ospCommit(light);
  //ospSetObjectAsData(world, "light", OSP_LIGHT, light);
  ospRelease(light);

  ospCommit(world);

  // print out world bounds
  OSPBounds worldBounds = ospGetBounds(world);

  // create renderer
  OSPRenderer renderer = (pathtrace?ospNewRenderer("pathtracer"):ospNewRenderer("scivis"));

  // complete setup of renderer
  float bgColor[] = {0.1f, 0.1f, 0.3f};
  ospSetParam(renderer, "backgroundColor", OSP_VEC3F, bgColor);
  ospCommit(renderer);

  // create and setup framebuffer
  OSPFrameBuffer framebuffer = ospNewFrameBuffer(imgSize_x,
      imgSize_y,
      OSP_FB_SRGBA,
      OSP_FB_COLOR | /*OSP_FB_DEPTH |*/ OSP_FB_ACCUM);
  ospResetAccumulation(framebuffer);


  // render one frame
  ospRenderFrameBlocking(framebuffer, renderer, camera, world);
  
  // access framebuffer and write its content as PPM file
  const uint32_t *fb = (uint32_t *)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
  writePPM("firstFrame.ppm", imgSize_x, imgSize_y, fb);
  ospUnmapFrameBuffer(fb, framebuffer);

  // render 10 more frames, which are accumulated to result in a better
  // converged image
  auto start = high_resolution_clock::now(); 
  for (int frames = 0; frames < 10; frames++)
    ospRenderFrameBlocking(framebuffer, renderer, camera, world);
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(stop - start); 
  cerr << "RENDERTIME " << duration.count() << endl;

  fb = (uint32_t *)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
  writePPM("accumulatedFrame.ppm", imgSize_x, imgSize_y, fb);
  ospUnmapFrameBuffer(fb, framebuffer);


  OSPPickResult p;
  ospPick(&p, framebuffer, renderer, camera, world, 0.5f, 0.5f);


  // cleanup pick handles (because p.hasHit was 'true')
  ospRelease(p.instance);
  ospRelease(p.model);

  // final cleanups
  ospRelease(renderer);
  ospRelease(camera);
  ospRelease(framebuffer);
  ospRelease(world);


  ospShutdown();

#ifdef _WIN32
  if (waitForKey) {
    printf("\n\tpress any key to exit");
    _getch();
  }
#endif

  return 0;

}
