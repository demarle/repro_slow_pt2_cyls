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
#include "ospray/ospray.h"

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
void writePPM(const char *fileName,
              const osp::vec2i *size,
              const uint32_t *pixel)
{
  FILE *file = fopen(fileName, "wb");
  if (!file) {
    fprintf(stderr, "fopen('%s', 'wb') failed: %d", fileName, errno);
    return;
  }
  fprintf(file, "P6\n%i %i\n255\n", size->x, size->y);
  unsigned char *out = (unsigned char *)alloca(3*size->x);
  for (int y = 0; y < size->y; y++) {
    const unsigned char *in = (const unsigned char *)&pixel[(size->y-1-y)*size->x];
    for (int x = 0; x < size->x; x++) {
      out[3*x + 0] = in[4*x + 0];
      out[3*x + 1] = in[4*x + 1];
      out[3*x + 2] = in[4*x + 2];
    }
    fwrite(out, 3*size->x, sizeof(char), file);
  }
  fprintf(file, "\n");
  fclose(file);
}


int main(int argc, const char **argv) {
  std::vector<osp::vec3f> vertices;
  std::vector<float> overtices;
  std::vector<osp::vec2i> indices;

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
      overtices.push_back(vertices[p0].x);
      overtices.push_back(vertices[p0].y);
      overtices.push_back(vertices[p0].z);
      overtices.push_back(vertices[p1].x);
      overtices.push_back(vertices[p1].y);
      overtices.push_back(vertices[p1].z);
	//indices.push_back({static_cast<int>(cell->GetPointId(0)),
	//  static_cast<int>(cell->GetPointId(1))});
    }
  }
  std::cerr << "READ " << vertices.size() << " " << numberOfCells << " " << overtices.size() << std::endl;
  
  // image size
  osp::vec2i imgSize = {1024, 768};

  // camera
  float cam_pos[] = {-10.f, 10.f, 0.f};
  float cam_up [] = {1.f, 0.f, 0.f};
  float cam_view [] = {0.3f, -0.7f, 0.f};

  // initialize OSPRay; OSPRay parses (and removes) its commandline parameters, e.g. "--osp:debug"
  OSPError init_error = ospInit(&argc, argv);
  if (init_error != OSP_NO_ERROR)
    return init_error;

  // create and setup camera
  OSPCamera camera = ospNewCamera("perspective");
  ospSetf(camera, "aspect", imgSize.x/(float)imgSize.y);
  ospSet3fv(camera, "pos", cam_pos);
  ospSet3fv(camera, "up",  cam_up);
  ospSet3fv(camera, "dir", cam_view);
  ospCommit(camera); // commit each object to indicate modifications are done


  // create and setup model and mesh
  OSPGeometry mesh = ospNewGeometry("cylinders");
  OSPData data = ospNewData(overtices.size(), OSP_FLOAT, overtices.data());
  ospCommit(data);
  ospSetData(mesh, "cylinders", data);
  ospRelease(data); // we are done using this handle

  //data = ospNewData(numberOfCells, OSP_INT2, indices.data(), 0);
  //ospCommit(data);
  //ospSetData(mesh, "index", data);
  //ospRelease(data); // we are done using this handle

  ospSet1i(mesh, "bytes_per_cylinder", 6*sizeof(float));
  ospSet1i(mesh, "offset_v0", 0);
  ospSet1i(mesh, "offset_v1", 3*sizeof(float));
  ospSet1f(mesh, "radius", .005);
  ospCommit(mesh);


  OSPModel world = ospNewModel();
  ospAddGeometry(world, mesh);
  ospRelease(mesh); // we are done using this handle
  ospCommit(world);


  // create renderer
  OSPRenderer renderer = (pathtrace?ospNewRenderer("pathtracer"):ospNewRenderer("scivis"));

  // create and setup light for Ambient Occlusion
  OSPLight light = ospNewLight3("ambient");
  ospCommit(light);
  OSPData lights = ospNewData(1, OSP_LIGHT, &light, 0);
  ospCommit(lights);

  // complete setup of renderer
  ospSet1i(renderer, "aoSamples", 1);
  ospSet3f(renderer, "bgColor", 0.1f, 0.1f, 0.3f); // white, transparent
  ospSetObject(renderer, "model",  world);
  ospSetObject(renderer, "camera", camera);
  ospSetObject(renderer, "lights", lights);
  ospCommit(renderer);

  // create and setup framebuffer
  OSPFrameBuffer framebuffer = ospNewFrameBuffer(imgSize, OSP_FB_SRGBA, OSP_FB_COLOR | /*OSP_FB_DEPTH |*/ OSP_FB_ACCUM);
  ospFrameBufferClear(framebuffer, OSP_FB_COLOR | OSP_FB_ACCUM);

  // render one frame
  auto start = high_resolution_clock::now(); 
  ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR | OSP_FB_ACCUM);
  auto stop = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(stop - start); 
  cerr << "RENDERTIME " << duration.count() << endl;

  // access framebuffer and write its content as PPM file
  const uint32_t * fb = (uint32_t*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
  writePPM("firstFrame.ppm", &imgSize, fb);
  ospUnmapFrameBuffer(fb, framebuffer);

  // render 10 more frames, which are accumulated to result in a better converged image
  for (int frames = 0; frames < 10; frames++)
    ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR | OSP_FB_ACCUM);

  fb = (uint32_t*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
  writePPM("accumulatedFrame.ppm", &imgSize, fb);
  ospUnmapFrameBuffer(fb, framebuffer);

  // final cleanups
  ospRelease(renderer);
  ospRelease(camera);
  ospRelease(lights);
  ospRelease(light);
  ospRelease(framebuffer);
  ospRelease(world);

  ospShutdown();

  return 0;
}
