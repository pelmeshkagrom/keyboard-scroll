#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/CCMouseDispatcher.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

#include <string>
#include <vector>
#include <string_view>

using namespace geode::prelude;

enum class ScrollDirection {
	Right,
	Left,
	Up,
	Down
};

static bool pageMouseScrollToggle = true;
static std::string mouseScrollModifier = "Shift";
static double kbdScrollSensitivity = 4.5;
static bool invertPageMouseScroll = false;
static bool disablePageScrollSpam = false;

static std::vector<geode::Keybind> pageRightKeys;
static std::vector<geode::Keybind> pageLeftKeys;
static bool rightArrowKeyBound = false;
static bool leftArrowKeyBound = false;

static bool isHoldingUp = false;
static bool isHoldingDown = false;
static bool isSchedulerScheduled = false;

// Node IDs for page switching buttons
static const std::vector<std::string_view> rightIDs = {
	"next-page-button",
	"next-button",
	"right-button",
	"right-arrow-button",
	"next-previous-button", // on FRequestProfilePage
	"right"
};
static const std::vector<std::string_view> leftIDs = {
	"prev-page-button",
	"prev-button",
	"left-button",
	"left-arrow-button",
	"previous-page-button",
	"left"
};

static void checkArrowKeybinds() {
	rightArrowKeyBound = false;
	leftArrowKeyBound = false;

	for(int i = 0; i < pageRightKeys.size(); i++) {
		if(pageRightKeys[i].key == cocos2d::enumKeyCodes::KEY_Right || pageRightKeys[i].key == cocos2d::enumKeyCodes::KEY_ArrowRight) {
			rightArrowKeyBound = true;
		}
		if(pageRightKeys[i].key == cocos2d::enumKeyCodes::KEY_Left || pageRightKeys[i].key == cocos2d::enumKeyCodes::KEY_ArrowLeft) {
			leftArrowKeyBound = true;
		}
	}

	for(int i = 0; i < pageLeftKeys.size(); i++) {
		if(pageLeftKeys[i].key == cocos2d::enumKeyCodes::KEY_Left || pageLeftKeys[i].key == cocos2d::enumKeyCodes::KEY_ArrowLeft) {
			leftArrowKeyBound = true;
		}
		if(pageLeftKeys[i].key == cocos2d::enumKeyCodes::KEY_Right || pageLeftKeys[i].key == cocos2d::enumKeyCodes::KEY_ArrowRight) {
			rightArrowKeyBound = true;
		}
	}
}

// Switch to the next/previous page
static void activatePageBtn(ScrollDirection direction) {
	auto scene = CCDirector::sharedDirector()->getRunningScene();
	if(!scene) return;

	const auto& targetIDs = (direction == ScrollDirection::Right) ? rightIDs : leftIDs;

	auto children = scene->getChildren();
	if(!children || children->count() == 0) return;

	CCNode* topmostChild = nullptr;
	bool shouldUseRecursive = false;

	for(int i = children->count() - 1; i >= 0; --i) {
		auto child = typeinfo_cast<CCNode*>(children->objectAtIndex(i));
		if(child && child->isVisible()) {
			topmostChild = child;
			break;
		}
	}

	if(!topmostChild) return;

	if(typeinfo_cast<FLAlertLayer*>(topmostChild)) {
		shouldUseRecursive = true;
	}
	
	auto subChildren = topmostChild->getChildren();
	if(subChildren && subChildren->count() > 0) {
		for(int k = subChildren->count() - 1; k >= 0; --k) {
			auto subChild = typeinfo_cast<CCNode*>(subChildren->objectAtIndex(k));
			if(subChild && subChild->isVisible() && typeinfo_cast<FLAlertLayer*>(subChild)) {
				topmostChild = subChild;
				shouldUseRecursive = true;
				break;
			}
		}
	}

	if(!topmostChild) return;

	if(!shouldUseRecursive) {
		auto subChildren = topmostChild->getChildren();
		if(subChildren && subChildren->count() > 0) {
			for(int j = subChildren->count() - 1; j >= 0; --j) {
				auto menu = typeinfo_cast<CCMenu*>(subChildren->objectAtIndex(j));
				if(menu && menu->isEnabled() && menu->isVisible()) {
					for(int l = 0; l < targetIDs.size(); l++) {
						auto btn = typeinfo_cast<CCMenuItemSpriteExtra*>(menu->getChildByID(targetIDs[l]));
						if(btn) {
							if(btn->isEnabled() && btn->isVisible()) {
								btn->activate();
								return;
							}
						}
					}
				}
			}
		}
	} else {
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
}

class $modify(MyMouseDispatcher, CCMouseDispatcher) {
	static void onModify(auto& self) {
		(void) self.setHookPriorityBeforePre("cocos2d::CCMouseDispatcher::dispatchScrollMSG", "prevter.smooth-scroll");
	}
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
		if(!pageMouseScrollToggle) {
			return CCMouseDispatcher::dispatchScrollMSG(y, x);
		}

		auto kbd = CCKeyboardDispatcher::get();
		bool activeModifier = false;

		if(!isSchedulerScheduled && !isHoldingUp && !isHoldingDown) {
			if(mouseScrollModifier == "Shift") {
				activeModifier = kbd->getShiftKeyPressed();
			} else if(mouseScrollModifier == "Ctrl") {
				activeModifier = kbd->getControlKeyPressed();
			} else if(mouseScrollModifier == "Alt") {
				activeModifier = kbd->getAltKeyPressed();
			}
		}

		if(!activeModifier) {
			return CCMouseDispatcher::dispatchScrollMSG(y, x);
		}

		if(y < 0.0f) {
			activatePageBtn(!invertPageMouseScroll ? ScrollDirection::Right : ScrollDirection::Left);
			return true;
		} else if(y > 0.0f) {
			activatePageBtn(!invertPageMouseScroll ? ScrollDirection::Left : ScrollDirection::Right);
			return true;
		}

		return CCMouseDispatcher::dispatchScrollMSG(y, x);
	}
};

class $modify(MyKeyboardDispatcher, CCKeyboardDispatcher) {
	bool dispatchKeyboardMSG(cocos2d::enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double timestamp) {
		if(!rightArrowKeyBound && !leftArrowKeyBound) return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, timestamp);

		if(isKeyDown) {
			if(key == cocos2d::enumKeyCodes::KEY_Right || key == cocos2d::enumKeyCodes::KEY_ArrowRight) {
				if(rightArrowKeyBound) {
					return true;
				}
			} else if(key == cocos2d::enumKeyCodes::KEY_Left || key == cocos2d::enumKeyCodes::KEY_ArrowLeft) {
				if(leftArrowKeyBound) {
					return true;
				}
			}
		}

		return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, timestamp);
	}
};

static void kbScroll(ScrollDirection direction, bool isHolding) {
	if(direction == ScrollDirection::Up) isHoldingUp = isHolding;
	if(direction == ScrollDirection::Down) isHoldingDown = isHolding;

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
			kbScroll(ScrollDirection::Up, down);
		}
	});
	listenForKeybindSettingPresses("scroll-down", [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
		if(!repeat) {
			kbScroll(ScrollDirection::Down, down);
		}
	});
	listenForKeybindSettingPresses("page-right", [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
		if (down) {
			if(disablePageScrollSpam && repeat) return;
			activatePageBtn(ScrollDirection::Right);
		}
	});
	listenForKeybindSettingPresses("page-left", [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
		if (down) {
			if(disablePageScrollSpam && repeat) return;
			activatePageBtn(ScrollDirection::Left);
		}
	});
}

$on_mod(Loaded) {
	pageMouseScrollToggle = Mod::get()->getSettingValue<bool>("page-mouse-scroll-toggle");
	listenForSettingChanges<bool>("page-mouse-scroll-toggle", [](bool value) {
		pageMouseScrollToggle = value;
	});
	mouseScrollModifier = Mod::get()->getSettingValue<std::string>("page-mouse-scroll-key");
	listenForSettingChanges<std::string>("page-mouse-scroll-key", [](std::string value) {
		mouseScrollModifier = value;
	});
	kbdScrollSensitivity = Mod::get()->getSettingValue<double>("scroll-sensitivity");
	listenForSettingChanges<double>("scroll-sensitivity", [](double value) {
		kbdScrollSensitivity = value;
	});
	invertPageMouseScroll = Mod::get()->getSettingValue<bool>("invert-page-mouse-scroll");
	listenForSettingChanges<bool>("invert-page-mouse-scroll", [](bool value) {
		invertPageMouseScroll = value;
	});
	disablePageScrollSpam = Mod::get()->getSettingValue<bool>("disable-page-scroll-spam");
	listenForSettingChanges<bool>("disable-page-scroll-spam", [](bool value) {
		disablePageScrollSpam = value;
	});

	pageRightKeys = Mod::get()->getSettingValue<std::vector<geode::Keybind>>("page-right");
	listenForSettingChanges<std::vector<geode::Keybind>>("page-right", [](std::vector<geode::Keybind> value) {
		pageRightKeys = value;
		checkArrowKeybinds();
	});
	pageLeftKeys = Mod::get()->getSettingValue<std::vector<geode::Keybind>>("page-left");
	listenForSettingChanges<std::vector<geode::Keybind>>("page-left", [](std::vector<geode::Keybind> value) {
		pageLeftKeys = value;
		checkArrowKeybinds();
	});

	checkArrowKeybinds();
}