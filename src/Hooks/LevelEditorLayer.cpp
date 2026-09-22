#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>

using namespace geode::prelude;

extern bool actionBlocked;

class $modify(LevelEditorLayer) {
	struct Fields {
		~Fields() {
			actionBlocked = false;
		}
	};

	bool init(GJGameLevel* level, bool noUI) {
		if(!LevelEditorLayer::init(level, noUI)) return false;

		m_fields.self();
		actionBlocked = true;

		return true;
	}
};