////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
//      and may be used under the terms of the MIT license. See the LICENSE file for details.     //
//                        Copyright: (c) 2019 German Aerospace Center (DLR)                       //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Plugin.hpp"

#include "../../../src/cs-core/GuiManager.hpp"
#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-gui/GuiItem.hpp"
#include "../../../src/cs-utils/logger.hpp"

#include <VistaKernel/GraphicsManager/VistaTransformNode.h>
#include <VistaKernelOpenSGExt/VistaOpenSGMaterialTools.h>

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

Plugin::Plugin() {
  // Create default logger for this plugin.
  spdlog::set_default_logger(cs::utils::logger::createLogger("csp-sharad"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::init() {

  spdlog::info("Loading plugin...");

  mPluginSettings = mAllSettings->mPlugins.at("csp-sharad");

  mGuiManager->addHtmlToGui("sharad", "../share/resources/gui/sharad-template.html");

  mGuiManager->addScriptToGuiFromJS("../share/resources/gui/js/csp-sharad.js");

  mGuiManager->addPluginTabToSideBarFromHTML(
      "SHARAD Profiles", "line_style", "../share/resources/gui/sharad-tab.html");

  boost::filesystem::path               dir(mPluginSettings.mFilePath);
  boost::filesystem::directory_iterator end_iter;

  if (boost::filesystem::exists(dir) && boost::filesystem::is_directory(dir)) {
    for (boost::filesystem::directory_iterator dir_iter(dir); dir_iter != end_iter; ++dir_iter) {
      if (boost::filesystem::is_regular_file(dir_iter->status())) {
        boost::filesystem::path path(boost::filesystem::path(*dir_iter).normalize());
        std::string             file(path.stem().string());
        std::string             ext(path.extension().string());

        if (ext == ".tab") {
          std::string sName = file.substr(0, file.length() - 5);
          auto sharad = std::make_shared<Sharad>(mGraphicsEngine, mSceneGraph, "MARS", "IAU_Mars",
              mPluginSettings.mFilePath + sName + "_tiff.tif",
              mPluginSettings.mFilePath + sName + "_geom.tab");
          mSolarSystem->registerAnchor(sharad);

          auto sharadNode = mSceneGraph->NewOpenGLNode(mSceneGraph->GetRoot(), sharad.get());
          VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
              sharadNode, static_cast<int>(cs::utils::DrawOrder::ePlanets) + 2);

          mSharads.push_back(sharad);
          mSharadNodes.push_back(sharadNode);

          mGuiManager->getGui()->callJavascript(
              "CosmoScout.sharad.add", sName, sharad->getStartExistence() + 10);
        }
      }
    }
  }

  mEnabled.onChange().connect([this](bool val) {
    for (auto const& node : mSharadNodes) {
      node->SetIsEnabled(val);
    }
  });

  mEnabled.touch();

  mGuiManager->getGui()->registerCallback(
      "sharad.setEnabled", std::function([this](bool enable) { mEnabled = enable; }));

  mActiveBodyConnection = mSolarSystem->pActiveBody.onChange().connect(
      [this](std::shared_ptr<cs::scene::CelestialBody> const& body) {
        bool enabled = false;

        if (body && body->getCenterName() == "Mars") {
          enabled = true;
        }

        mGuiManager->getGui()->callJavascript(
            "CosmoScout.sidebar.setTabEnabled", "collapse-SHARAD-Profiles", enabled);
      });

  spdlog::info("Loading done.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::deInit() {
  spdlog::info("Unloading plugin...");

  for (auto const& sharad : mSharads) {
    mSolarSystem->unregisterAnchor(sharad);
  }

  for (auto const& node : mSharadNodes) {
    mSceneGraph->GetRoot()->DisconnectChild(node);
  }

  mSolarSystem->pActiveBody.onChange().disconnect(mActiveBodyConnection);
  mGuiManager->getGui()->unregisterCallback("sharad.setEnabled");
  mGuiManager->getGui()->callJavascript("CosmoScout.gui.unregisterHtml", "sharad");

  spdlog::info("Unloading done.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::sharad
