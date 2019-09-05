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

const std::string Sharad::VERT = R"(
#version 330

uniform mat4 uMatModelView;
uniform mat4 uMatProjection;
uniform float uHeightScale;
uniform float uRadius;

// inputs
layout(location = 0) in vec3  iPosition;
layout(location = 1) in vec2  iTexCoords;
layout(location = 2) in float iTime;

// outputs
out vec3  vPosition;
out vec2  vTexCoords;
out float vTime;

void main()
{
    vTexCoords = iTexCoords;
    vTime      = iTime;

    float height = vTexCoords.y < 0.5 ? 
                        uRadius + 10000 * uHeightScale : 
                        uRadius - 10100 * uHeightScale ;

    vPosition   = (uMatModelView * vec4(iPosition * height, 1.0)).xyz;
    gl_Position =  uMatProjection * vec4(vPosition, 1);
}
)";

////////////////////////////////////////////////////////////////////////////////////////////////////

const std::string Sharad::FRAG = R"(
#version 330

uniform sampler2DRect uDepthBuffer;
uniform sampler2D uSharadTexture;
uniform float uAmbientBrightness;
uniform float uTime;
uniform float uSceneScale;
uniform float uFarClip;
uniform vec2 uViewportPos;

// inputs
in vec3  vPosition;
in vec2  vTexCoords;
in float vTime;

// outputs
layout(location = 0) out vec4 oColor;

void main()
{
    if (vTime > uTime)
    {
        discard;
    }

    float sharadDistance  = length(vPosition);
    float surfaceDistance = texture(uDepthBuffer, gl_FragCoord.xy - uViewportPos).r * uFarClip;
    
    if (sharadDistance < surfaceDistance)
    {
        discard;
    }

    float val = texture(uSharadTexture, vTexCoords).r;
    val = mix(1, val, clamp((uTime - vTime), 0, 1));

    oColor.r = pow(val,  0.5);
    oColor.g = pow(val,  2.0);
    oColor.b = pow(val, 10.0);
    oColor.a = 1.0 - clamp((sharadDistance - surfaceDistance) * uSceneScale / 30000, 0.1, 1.0);

    gl_FragDepth = sharadDistance / uFarClip;
}
)";

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

  // create sphere shader ----------------------------------------------------
  mShader.InitVertexShaderFromString(VERT);
  mShader.InitFragmentShaderFromString(FRAG);
  mShader.Link();
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

bool Sharad::Do() {
  if (getIsInExistence()) {
    cs::utils::FrameTimings::ScopedTimer timer("Sharad");

    mShader.Bind();

    // get modelview and projection matrices
    GLfloat glMatMV[16], glMatP[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, &glMatMV[0]);
    glGetFloatv(GL_PROJECTION_MATRIX, &glMatP[0]);
    auto matMV = glm::make_mat4x4(glMatMV) * glm::mat4(getWorldTransform());
    glUniformMatrix4fv(
        mShader.GetUniformLocation("uMatModelView"), 1, GL_FALSE, glm::value_ptr(matMV));
    glUniformMatrix4fv(mShader.GetUniformLocation("uMatProjection"), 1, GL_FALSE, glMatP);

    GLint iViewport[4];
    glGetIntegerv(GL_VIEWPORT, iViewport);
    mShader.SetUniform(
        mShader.GetUniformLocation("uViewportPos"), (float)iViewport[0], (float)iViewport[1]);

    mShader.SetUniform(mShader.GetUniformLocation("uSharadTexture"), 0);
    mShader.SetUniform(mShader.GetUniformLocation("uDepthBuffer"), 1);
    mShader.SetUniform(mShader.GetUniformLocation("uSceneScale"), (float)mSceneScale);
    mShader.SetUniform(
        mShader.GetUniformLocation("uHeightScale"), mGraphicsEngine->pHeightScale.get());
    mShader.SetUniform(mShader.GetUniformLocation("uRadius"),
        (float)cs::core::SolarSystem::getRadii(getCenterName())[0]);
    mShader.SetUniform(mShader.GetUniformLocation("uTime"), (float)(mCurrTime - mStartExistence));
    mShader.SetUniform(
        mShader.GetUniformLocation("uFarClip"), cs::utils::getCurrentFarClipDistance());

    mTexture->Bind(GL_TEXTURE0);
    mDepthBuffer->Bind(GL_TEXTURE1);

    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // glDepthFunc(GL_GEQUAL);
    // glDepthMask(false);
    glDisable(GL_DEPTH_TEST);

    // draw --------------------------------------------------------------------
    mVAO.Bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, mSamples * 2);
    mVAO.Release();

    // clean up ----------------------------------------------------------------
    mTexture->Unbind(GL_TEXTURE0);

    glPopAttrib();

    mShader.Release();
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Sharad::GetBoundingBox(VistaBoundingBox& bb) {
  return false;
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
