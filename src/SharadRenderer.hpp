////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CSP_SHARAD_SHARAD_RENDERER_HPP
#define CSP_SHARAD_SHARAD_RENDERER_HPP

#include <VistaKernel/GraphicsManager/VistaOpenGLDraw.h>
#include <VistaOGLExt/VistaGLSLShader.h>
#include <VistaOGLExt/VistaTexture.h>

#include "../../../src/cs-core/GraphicsEngine.hpp"
#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>
#include <memory>
#include <vector>

namespace csp::sharad {

class Sharad;

class SharadRenderer : public IVistaOpenGLDraw {
 public:
  explicit SharadRenderer(std::shared_ptr<cs::core::GraphicsEngine> const& graphicsEngine);

  bool Do() override;
  bool GetBoundingBox(VistaBoundingBox& bb) override;

  void setSharads(std::vector<std::shared_ptr<Sharad>> const& sharads);

 private:
  VistaGLSLShader mShader;

  struct {
    uint32_t matModelView;
    uint32_t matProjection;
    uint32_t viewportPos;
    uint32_t sharadTexture;
    uint32_t depthBuffer;
    uint32_t sceneScale;
    uint32_t heightScale;
    uint32_t radius;
    uint32_t time;
    uint32_t farClip;
  } mUniforms{};

  std::vector<std::shared_ptr<Sharad>> mSharads;

  std::shared_ptr<cs::core::GraphicsEngine> mGraphicsEngine;

  class FramebufferCallback : public IVistaOpenGLDraw {
   public:
    FramebufferCallback(VistaTexture* pDepthBuffer);

    bool Do() override;

    bool GetBoundingBox(VistaBoundingBox& bb) override {
      return true;
    }

   private:
    VistaTexture* mDepthBuffer;
  };

  std::unique_ptr<VistaTexture>        mDepthBuffer;
  std::unique_ptr<FramebufferCallback> mPreCallback;
  std::unique_ptr<VistaOpenGLNode>     mPreCallbackNode;
};

} // namespace csp::sharad

#endif // CSP_SHARAD_SHARAD_RENDERER_HPP
