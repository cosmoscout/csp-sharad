////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Plugin.hpp"

#include "Sharad.hpp"
#include "SharadRenderer.hpp"

#include "../../../src/cs-core/GuiManager.hpp"
#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-core/TimeControl.hpp"

#include <VistaKernel/GraphicsManager/VistaTransformNode.h>
#include <VistaKernelOpenSGExt/VistaOpenSGMaterialTools.h>
#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN cs::core::PluginBase* create() {
  return new csp::sharad::Plugin;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN void destroy(cs::core::PluginBase* pluginBase) {
  delete pluginBase;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace csp::sharad {

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(const nlohmann::json& j, Plugin::Settings& o) {
  cs::core::parseSection(
      "csp-sharad", [&] { o.mFilePath = cs::core::parseProperty<std::string>("filePath", j); });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::init() {
  std::cout << "Loading: CosmoScout VR Plugin Sharad" << std::endl;

  mPluginSettings = mAllSettings->mPlugins.at("csp-sharad");

  mGuiManager->addPluginTabToSideBarFromHTML(
      "SHARAD Profiles", "line_style", "../share/resources/gui/sharad-tab.html");

  mGuiManager->addScriptToSideBarFromJS("../share/resources/gui/js/sharad-tab.js");

  boost::filesystem::path               dir(mPluginSettings.mFilePath);
  boost::filesystem::directory_iterator end_iter;

  if (boost::filesystem::exists(dir) && boost::filesystem::is_directory(dir)) {
    for (boost::filesystem::directory_iterator dir_iter(dir); dir_iter != end_iter; ++dir_iter) {
      if (boost::filesystem::is_regular_file(dir_iter->status())) {
        boost::filesystem::path path(boost::filesystem::path(*dir_iter).normalize());
        std::string             file(path.stem().string());
        std::string             ext(path.extension().string());

        if (ext == ".tab") {
          std::string sName  = file.substr(0, file.length() - 5);
          auto        sharad = std::make_shared<Sharad>("MARS", "IAU_Mars",
              mPluginSettings.mFilePath + sName + "_tiff.tif",
              mPluginSettings.mFilePath + sName + "_geom.tab");
          mSolarSystem->registerAnchor(sharad);
          mSharads.push_back(sharad);

          mGuiManager->getSideBar()->callJavascript(
              "add_sharad", sName, sharad->getStartExistence() + 10);
        }
      }
    }
  }

  mSharadRenderer = std::make_unique<SharadRenderer>(mGraphicsEngine);
  mSharadRenderer->setSharads(mSharads);

  mSharadNode = mSceneGraph->NewOpenGLNode(mSceneGraph->GetRoot(), mSharadRenderer.get());
  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mSharadNode, static_cast<int>(cs::utils::DrawOrder::ePlanets) + 2);

  mEnabled.onChange().connect([this](bool val) { mSharadNode->SetIsEnabled(val); });

  mEnabled.touch();

  mGuiManager->getSideBar()->registerCallback<bool>(
      "set_enable_sharad", ([this](bool enable) { mEnabled = enable; }));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::deInit() {
  for (auto const& sharad : mSharads) {
    mSolarSystem->unregisterAnchor(sharad);
  }

  mSceneGraph->GetRoot()->DisconnectChild(mSharadNode);

  mGuiManager->getSideBar()->unregisterCallback("set_enable_sharad");
}

void Plugin::update() {
  PluginBase::update();

  mSharadRenderer->setCurrentTime(mTimeControl->pSimulationTime.get());
  mSharadRenderer->setSceneScale(mSolarSystem->getObserver().getAnchorScale());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::sharad
