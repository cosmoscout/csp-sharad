////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SharadRenderer.hpp"
#include <glm/gtc/type_ptr.hpp>

#include "Sharad.hpp"

#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-utils/FrameTimings.hpp"
#include "../../../src/cs-utils/filesystem.hpp"
#include "../../../src/cs-utils/utils.hpp"

#include <VistaOGLExt/VistaTexture.h>

namespace csp::sharad {

////////////////////////////////////////////////////////////////////////////////////////////////////

SharadRenderer::SharadRenderer(std::shared_ptr<cs::core::GraphicsEngine> const& graphicsEngine)
    : mGraphicsEngine(graphicsEngine) {
  // create shader ----------------------------------------------------
  mShader.InitVertexShaderFromString(
      cs::utils::filesystem::loadToString("../share/resources/shaders/Sharad.vert.glsl"));
  mShader.InitFragmentShaderFromString(
      cs::utils::filesystem::loadToString("../share/resources/shaders/Sharad.frag.glsl"));
  mShader.Link();

  mUniforms.matModelView  = mShader.GetUniformLocation("uMatModelView");
  mUniforms.matProjection = mShader.GetUniformLocation("uMatProjection");
  mUniforms.viewportPos   = mShader.GetUniformLocation("uViewportPos");
  mUniforms.sharadTexture = mShader.GetUniformLocation("uSharadTexture");
  mUniforms.depthBuffer   = mShader.GetUniformLocation("uDepthBuffer");
  mUniforms.sceneScale    = mShader.GetUniformLocation("uSceneScale");
  mUniforms.heightScale   = mShader.GetUniformLocation("uHeightScale");
  mUniforms.radius        = mShader.GetUniformLocation("uRadius");
  mUniforms.time          = mShader.GetUniformLocation("uTime");
  mUniforms.farClip       = mShader.GetUniformLocation("uFarClip");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SharadRenderer::Do() {
  cs::utils::FrameTimings::ScopedTimer timer("Sharad");

  mShader.Bind();

  glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  // glDepthFunc(GL_GEQUAL);
  // glDepthMask(false);
  glDisable(GL_DEPTH_TEST);

  // get modelview and projection matrices
  GLfloat glMatMV[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, &glMatMV[0]);

  GLfloat glMatP[16];
  glGetFloatv(GL_PROJECTION_MATRIX, &glMatP[0]);
  glUniformMatrix4fv(mUniforms.matProjection, 1, GL_FALSE, glMatP);

  GLint iViewport[4];
  glGetIntegerv(GL_VIEWPORT, iViewport);
  mShader.SetUniform(mUniforms.viewportPos, (float)iViewport[0], (float)iViewport[1]);

  mShader.SetUniform(mUniforms.sharadTexture, 0);
  mShader.SetUniform(mUniforms.depthBuffer, 1);

  mShader.SetUniform(mUniforms.heightScale, mGraphicsEngine->pHeightScale.get());

  mShader.SetUniform(mUniforms.farClip, cs::utils::getCurrentFarClipDistance());

  for (const auto& sharad : mSharads) {
    if (sharad->getIsInExistence()) {

      auto matMV = glm::make_mat4x4(glMatMV) * glm::mat4(sharad->getWorldTransform());
      glUniformMatrix4fv(mUniforms.matModelView, 1, GL_FALSE, glm::value_ptr(matMV));

      mShader.SetUniform(mUniforms.sceneScale, (float)sharad->mSceneScale);
      mShader.SetUniform(
          mUniforms.radius, (float)cs::core::SolarSystem::getRadii(sharad->getCenterName())[0]);
      mShader.SetUniform(mUniforms.time, (float)(sharad->mCurrTime - sharad->mStartExistence));

      sharad->mTexture->Bind(GL_TEXTURE0);
      sharad->mDepthBuffer->Bind(GL_TEXTURE1);

      // draw --------------------------------------------------------------------
      sharad->mVAO.Bind();
      glDrawArrays(GL_TRIANGLE_STRIP, 0, sharad->mSamples * 2);
      sharad->mVAO.Release();

      // clean up ----------------------------------------------------------------
      sharad->mTexture->Unbind(GL_TEXTURE0);
    }
  }

  glPopAttrib();

  mShader.Release();

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SharadRenderer::GetBoundingBox(VistaBoundingBox& bb) {
  return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SharadRenderer::setSharads(std::vector<std::shared_ptr<Sharad>> const& sharads) {
  mSharads = sharads;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::sharad