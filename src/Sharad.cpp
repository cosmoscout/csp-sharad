////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Sharad.hpp"

#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-graphics/TextureLoader.hpp"
#include "../../../src/cs-scene/CelestialObserver.hpp"
#include "../../../src/cs-utils/FrameTimings.hpp"
#include "../../../src/cs-utils/convert.hpp"
#include "../../../src/cs-utils/utils.hpp"

#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>
#include <VistaKernel/VistaSystem.h>
#include <VistaKernelOpenSGExt/VistaOpenSGMaterialTools.h>

#include <glm/gtc/type_ptr.hpp>

namespace csp::sharad {

////////////////////////////////////////////////////////////////////////////////////////////////////

VistaTexture*                Sharad::mDepthBuffer     = nullptr;
Sharad::FramebufferCallback* Sharad::mPreCallback     = nullptr;
VistaOpenGLNode*             Sharad::mPreCallbackNode = nullptr;
int                          Sharad::mInstanceCount   = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////

struct ProfileRadarData {
  unsigned int Number;
  unsigned int Year;
  unsigned int Month;
  unsigned int Day;
  unsigned int Hour;
  unsigned int Minute;
  unsigned int Second;
  unsigned int Millisecond;
  float        Latitude;
  float        Longitude;
  float        SurfaceAltitude;
  float        MROAltitude;
  float        c;
  float        d;
  float        e;
  float        f;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

Sharad::Sharad(std::shared_ptr<cs::core::GraphicsEngine> graphicsEngine,
    VistaSceneGraph* sceneGraph, std::string const& sCenterName, std::string const& sFrameName,
    std::string const& sTiffFile, std::string const& sTabFile)
    : cs::scene::CelestialObject(sCenterName, sFrameName, 0, 0)
    , mGraphicsEngine(graphicsEngine)
    , mTexture(cs::graphics::TextureLoader::loadFromFile(sTiffFile)) {
  // arbitray date in future
  mEndExistence = cs::utils::convert::toSpiceTime("2040-01-01 00:00:00.000");

  if (mInstanceCount == 0) {
    mDepthBuffer = new VistaTexture(GL_TEXTURE_RECTANGLE);
    mDepthBuffer->Bind();
    mDepthBuffer->SetWrapS(GL_CLAMP);
    mDepthBuffer->SetWrapT(GL_CLAMP);
    mDepthBuffer->SetMinFilter(GL_NEAREST);
    mDepthBuffer->SetMagFilter(GL_NEAREST);
    mDepthBuffer->Unbind();

    mPreCallback = new FramebufferCallback(mDepthBuffer);

    mPreCallbackNode = sceneGraph->NewOpenGLNode(sceneGraph->GetRoot(), mPreCallback);

    VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
        mPreCallbackNode, static_cast<int>(cs::utils::DrawOrder::ePlanets) + 1);
  }

  ++mInstanceCount;

  // load metadata -----------------------------------------------------------
  FILE* pFile = fopen(sTabFile.c_str(), "r");

  if (pFile == nullptr) {
    std::cout << "Failed to open Shard file " << sTabFile << std::endl;
    return;
  }

  std::vector<ProfileRadarData> meta;

  while (fgetc(pFile) != EOF) {
    ProfileRadarData dataElement;

    // Scan the File, this is specific to the one SHARAD we currently have
    fscanf(pFile, "%d,%d-%d-%dT%d:%d:%d.%d, %f,%f,%f,%f, %f,%f,%f,%f", &dataElement.Number,
        &dataElement.Year, &dataElement.Month, &dataElement.Day, &dataElement.Hour,
        &dataElement.Minute, &dataElement.Second, &dataElement.Millisecond, &dataElement.Latitude,
        &dataElement.Longitude, &dataElement.SurfaceAltitude, &dataElement.MROAltitude,
        &dataElement.c, &dataElement.d, &dataElement.e, &dataElement.f);
    meta.push_back(dataElement);
  }

  mSamples = (int)meta.size();

  fclose(pFile);

  // create geometry ---------------------------------------------------------
  struct Vertex {
    glm::vec3 pos;
    glm::vec2 tc;
    float     time;
  };

  std::vector<Vertex> vertices(mSamples * 2);

  for (int i = 0; i < mSamples; ++i) {
    double tTime = cs::utils::convert::toSpiceTime(
        boost::posix_time::ptime(boost::gregorian::date(meta[i].Year, meta[i].Month, meta[i].Day),
            boost::posix_time::hours(meta[i].Hour) + boost::posix_time::minutes(meta[i].Minute) +
                boost::posix_time::seconds(meta[i].Second) +
                boost::posix_time::milliseconds(meta[i].Millisecond)));

    if (i == 0) {
      mStartExistence = tTime;
    }

    glm::dvec2 lngLat(
        cs::utils::convert::toRadians(glm::dvec2(meta[i].Longitude, meta[i].Latitude)));
    glm::dvec3 point = cs::utils::convert::toCartesian(lngLat, 1.0, 1.0);

    float x    = 1.f * i / (mSamples - 1.f);
    auto  time = (float)(tTime - mStartExistence);

    vertices[i * 2 + 0].pos  = point;
    vertices[i * 2 + 0].tc   = glm::vec2(x, 1.f);
    vertices[i * 2 + 0].time = time;
    vertices[i * 2 + 1].pos  = point;
    vertices[i * 2 + 1].tc   = glm::vec2(x, 0.f);
    vertices[i * 2 + 1].time = time;
  }

  mVBO.Bind(GL_ARRAY_BUFFER);
  mVBO.BufferData(vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
  mVBO.Release();

  mVAO.EnableAttributeArray(0);
  mVAO.SpecifyAttributeArrayFloat(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0, &mVBO);

  mVAO.EnableAttributeArray(1);
  mVAO.SpecifyAttributeArrayFloat(
      1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLuint)offsetof(Vertex, tc), &mVBO);

  mVAO.EnableAttributeArray(2);
  mVAO.SpecifyAttributeArrayFloat(
      2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLuint)offsetof(Vertex, time), &mVBO);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Sharad::~Sharad() {
  --mInstanceCount;

  if (mInstanceCount == 0) {
    delete mPreCallback;
    delete mDepthBuffer;
    delete mPreCallbackNode;
    mPreCallback     = nullptr;
    mDepthBuffer     = nullptr;
    mPreCallbackNode = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Sharad::update(double tTime, cs::scene::CelestialObserver const& oObs) {
  cs::scene::CelestialObject::update(tTime, oObs);

  mCurrTime   = tTime;
  mSceneScale = oObs.getAnchorScale();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Sharad::FramebufferCallback::FramebufferCallback(VistaTexture* pDepthBuffer)
    : mDepthBuffer(pDepthBuffer) {
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Sharad::FramebufferCallback::Do() {
  GLint iViewport[4];
  glGetIntegerv(GL_VIEWPORT, iViewport);
  mDepthBuffer->Bind();
  glCopyTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_DEPTH_COMPONENT, iViewport[0], iViewport[1],
      iViewport[2], iViewport[3], 0);
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::sharad
