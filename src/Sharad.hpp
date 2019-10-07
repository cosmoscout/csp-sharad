////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef CSP_SHARAD_HPP
#define CSP_SHARAD_HPP

#include "../../../src/cs-scene/CelestialObject.hpp"

#include <VistaOGLExt/VistaBufferObject.h>
#include <VistaOGLExt/VistaVertexArrayObject.h>

class VistaTexture;

namespace csp::sharad {

class SharadRenderer;

/// Renders a single SHARAD image.
class Sharad : public cs::scene::CelestialObject {
  friend SharadRenderer;

 public:
  Sharad(std::string const& sCenterName, std::string const& sFrameName,
      std::string const& sTiffFile, std::string const& sTabFile);
  virtual ~Sharad() = default;

 private:
  std::shared_ptr<VistaTexture> mTexture;

  VistaVertexArrayObject mVAO;
  VistaBufferObject      mVBO;

  int    mSamples;
};

} // namespace csp::sharad

#endif // CSP_SHARAD_HPP
