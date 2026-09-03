#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/CCMouseDispatcher.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

static bool isHoldingUp = false;
static bool isHoldingDown = false;
static bool isSchedulerScheduled = false;

static double kbdScrollSensitivity = 4.5;

// Node IDs for page switching buttons
static const std::vector<std::string> rightIDs = {
	"next-page-button",
	"next-button",
	"right-button",
	"right-arrow-button",
	"page-next-button",
	"next-previous-button", // on FRequestProfilePage
	"right"
};
static const std::vector<std::string> leftIDs = {
	"prev-page-button",
	"prev-button",
	"left-button",
	"left-arrow-button",
	"page-previous-button",
	"previous-page-button",
	"left"
};

// Switch to the next/previous page
static void activatePageBtn(std::string direction) {
	auto scene = CCDirector::sharedDirector()->getRunningScene();
	if(!scene) return;

	const std::vector<std::string>& targetIDs = (direction == "right") ? rightIDs : leftIDs;

	auto children = scene->getChildren();
	if(!children || children->count() == 0) return;

	CCNode* topmostChild = nullptr;

	for(int i = children->count() - 1; i >= 0; --i) {
		auto child = typeinfo_cast<CCNode*>(children->objectAtIndex(i));
		if(child && child->isVisible()) {
			topmostChild = child;
			break;
		}
	}

	if(!topmostChild) return;

	auto subChildren = topmostChild->getChildren();
	if(subChildren && subChildren->count() > 0) {
		for(int k = subChildren->count() - 1; k >= 0; --k) {
			auto subChild = typeinfo_cast<CCNode*>(subChildren->objectAtIndex(k));
			if(subChild && subChild->isVisible() && typeinfo_cast<FLAlertLayer*>(subChild)) {
				topmostChild = subChild;
				break;
			}
		}
	}

	if(!topmostChild) return;

	for(int j = 0; j < targetIDs.size(); j++) {
		auto btn = typeinfo_cast<CCMenuItemSpriteExtra*>(topmostChild->getChildByIDRecursive(targetIDs[j]));
		if(btn) {
			if(btn->isEnabled() && btn->isVisible()) {
				btn->activate();
				return;
			}
		}
	}
}

class $modify(MyMouseDispatcher, CCMouseDispatcher) {
	// Keyboard vertical scroll
	void updateScroll(float dt) {
		if(!isHoldingUp && !isHoldingDown) return;

		double currentSensitivity = kbdScrollSensitivity * (dt * 60.0);

		if(isHoldingUp && !isHoldingDown) {
			this->dispatchScrollMSG(-currentSensitivity, 0.0f);
		}
		if(isHoldingDown && !isHoldingUp) {
			this->dispatchScrollMSG(currentSensitivity, 0.0f);
		}
	}
	void startSchedule() {
		if(isSchedulerScheduled) return;
		isSchedulerScheduled = true;
		CCScheduler::get()->scheduleSelector(
			schedule_selector(MyMouseDispatcher::updateScroll),
			this,
			0,
			false
		);
	}
	void stopSchedule() {
		if(!isSchedulerScheduled) return;
		isSchedulerScheduled = false;
		CCScheduler::get()->unscheduleSelector(
			schedule_selector(MyMouseDispatcher::updateScroll),
			this
		);
	}

	// Horizontal MWheelUp/MWheelDown scroll
	bool dispatchScrollMSG(float y, float x) {
		if(!Mod::get()->getSettingValue<bool>("page-mouse-scroll-toggle")) {
			return CCMouseDispatcher::dispatchScrollMSG(y, x);
		}

		auto kbd = CCKeyboardDispatcher::get();
		auto mouseScrollModifier = Mod::get()->getSettingValue<std::string>("page-mouse-scroll-key");

		bool activeModifier = false;
		if(mouseScrollModifier == "Shift") {
			activeModifier = kbd->getShiftKeyPressed();
		} else if(mouseScrollModifier == "Ctrl") {
			activeModifier = kbd->getControlKeyPressed();
		} else if(mouseScrollModifier == "Alt") {
			activeModifier = kbd->getAltKeyPressed();
		}

		if(!activeModifier) {
			return CCMouseDispatcher::dispatchScrollMSG(y, x);
		}
		
		if(y < 0.0f) {
			activatePageBtn("right");
			return true;
		} else if(y > 0.0f) {
			activatePageBtn("left");
			return true;
		}

		return CCMouseDispatcher::dispatchScrollMSG(y, x);
	}
};

static void kbScroll(std::string direction, bool isHolding) {
	if(direction == "up") isHoldingUp = isHolding;
	if(direction == "down") isHoldingDown = isHolding;

	auto md = static_cast<MyMouseDispatcher*>(CCDirector::sharedDirector()->getMouseDispatcher());
	if(md) {
		if(isHoldingUp || isHoldingDown) {
			md->startSchedule();
		} else {
			md->stopSchedule();
		}
	}
}

// Keybind actions
$on_game(Loaded) {
	listenForKeybindSettingPresses("scroll-up", [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
		if(!repeat) {
			kbScroll("up", down);
		}
    });
	listenForKeybindSettingPresses("scroll-down", [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
		if(!repeat) {
			kbScroll("down", down);
		}
    });
	listenForKeybindSettingPresses("page-right", [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
        if (down && !repeat) {
			activatePageBtn("right");
        }
    });
	listenForKeybindSettingPresses("page-left", [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
		if (down && !repeat) {
			activatePageBtn("left");
        }
    });
}

$on_mod(Loaded) {
	kbdScrollSensitivity = Mod::get()->getSettingValue<double>("scroll-sensitivity");
	listenForSettingChanges<double>("scroll-sensitivity", [](double value) {
		kbdScrollSensitivity = value;
	});
}