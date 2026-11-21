#include <Geode/Geode.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>

using namespace geode::prelude;

extern bool actionBlocked;

class $modify(EditorPauseLayer) {
	bool init(LevelEditorLayer* layer) {
		if(!EditorPauseLayer::init(layer)) return false;

		actionBlocked = false;

		return true;
	}

	void onResume(CCObject* sender) {
		EditorPauseLayer::onResume(sender);
		actionBlocked = true;
	}
};