/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "window/window_main_menu.h"

#include "styles/style_window.h"
#include "styles/style_dialogs.h"
#include "window/themes/window_theme.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu.h"
#include "ui/toast/toast.h"
#include "ui/special_buttons.h"
#include "ui/empty_userpic.h"
#include "mainwindow.h"
#include "storage/localstorage.h"
#include "support/support_templates.h"
#include "boxes/about_box.h"
#include "boxes/peer_list_controllers.h"
#include "calls/calls_box_controller.h"
#include "lang/lang_keys.h"
#include "core/click_handler_types.h"
#include "observer_peer.h"
#include "auth_session.h"
#include "mainwidget.h"

namespace Window {

MainMenu::MainMenu(
	QWidget *parent,
	not_null<Controller*> controller)
: TWidget(parent)
, _controller(controller)
, _menu(this, st::mainMenu)
, _telegram(this, st::mainMenuTelegramLabel)
, _version(this, st::mainMenuVersionLabel) {
	setAttribute(Qt::WA_OpaquePaintEvent);

	auto showSelfChat = [] {
		App::main()->choosePeer(Auth().userPeerId(), ShowAtUnreadMsgId);
	};
	_userpicButton.create(
		this,
		_controller,
		Auth().user(),
		Ui::UserpicButton::Role::Custom,
		st::mainMenuUserpic);
	_userpicButton->setClickedCallback(showSelfChat);
	_userpicButton->show();
	_cloudButton.create(this, st::mainMenuCloudButton);
	_cloudButton->setClickedCallback(showSelfChat);
	_cloudButton->show();

	_nightThemeSwitch.setCallback([this] {
		if (const auto action = *_nightThemeAction) {
			const auto nightMode = Window::Theme::IsNightMode();
			if (action->isChecked() != nightMode) {
				Window::Theme::ToggleNightMode();
				Window::Theme::KeepApplied();
			}
		}
	});

	resize(st::mainMenuWidth, parentWidget()->height());
	_menu->setTriggeredCallback([](QAction *action, int actionTop, Ui::Menu::TriggeredSource source) {
		emit action->triggered();
	});
	refreshMenu();

	_telegram->setRichText(textcmdLink(1, qsl("Telegram Desktop")));
	_telegram->setLink(1, std::make_shared<UrlClickHandler>(qsl("https://desktop.telegram.org")));
	_version->setRichText(textcmdLink(1, lng_settings_current_version(lt_version, currentVersionText())) + QChar(' ') + QChar(8211) + QChar(' ') + textcmdLink(2, lang(lng_menu_about)));
	_version->setLink(1, std::make_shared<UrlClickHandler>(qsl("https://desktop.telegram.org/changelog")));
	_version->setLink(2, std::make_shared<LambdaClickHandler>([] { Ui::show(Box<AboutBox>()); }));

	subscribe(Auth().downloaderTaskFinished(), [this] { update(); });
	subscribe(Auth().downloaderTaskFinished(), [this] { update(); });
	subscribe(Notify::PeerUpdated(), Notify::PeerUpdatedHandler(Notify::PeerUpdate::Flag::UserPhoneChanged, [this](const Notify::PeerUpdate &update) {
		if (update.peer->isSelf()) {
			updatePhone();
		}
	}));
	subscribe(Global::RefPhoneCallsEnabledChanged(), [this] { refreshMenu(); });
	subscribe(Window::Theme::Background(), [this](const Window::Theme::BackgroundUpdate &update) {
		if (update.type == Window::Theme::BackgroundUpdate::Type::ApplyingTheme) {
			refreshMenu();
		}
	});
	updatePhone();
}

void MainMenu::refreshMenu() {
	_menu->clearActions();
	if (!Auth().supportMode()) {
		_menu->addAction(lang(lng_create_group_title), [] {
			App::wnd()->onShowNewGroup();
		}, &st::mainMenuNewGroup, &st::mainMenuNewGroupOver);
		_menu->addAction(lang(lng_create_channel_title), [] {
			App::wnd()->onShowNewChannel();
		}, &st::mainMenuNewChannel, &st::mainMenuNewChannelOver);
		_menu->addAction(lang(lng_menu_contacts), [] {
			Ui::show(Box<PeerListBox>(std::make_unique<ContactsBoxController>(), [](not_null<PeerListBox*> box) {
				box->addButton(langFactory(lng_close), [box] { box->closeBox(); });
				box->addLeftButton(langFactory(lng_profile_add_contact), [] { App::wnd()->onShowAddContact(); });
			}));
		}, &st::mainMenuContacts, &st::mainMenuContactsOver);
		if (Global::PhoneCallsEnabled()) {
			_menu->addAction(lang(lng_menu_calls), [] {
				Ui::show(Box<PeerListBox>(std::make_unique<Calls::BoxController>(), [](not_null<PeerListBox*> box) {
					box->addButton(langFactory(lng_close), [box] { box->closeBox(); });
				}));
			}, &st::mainMenuCalls, &st::mainMenuCallsOver);
		}
	} else {
		_menu->addAction(lang(lng_profile_add_contact), [] {
			App::wnd()->onShowAddContact();
		}, &st::mainMenuContacts, &st::mainMenuContactsOver);

		const auto fix = std::make_shared<QPointer<QAction>>();
		*fix = _menu->addAction(qsl("Fix chats order"), [=] {
			(*fix)->setChecked(!(*fix)->isChecked());
			Auth().settings().setSupportFixChatsOrder((*fix)->isChecked());
			Local::writeUserSettings();
		}, &st::mainMenuFixOrder, &st::mainMenuFixOrderOver);
		(*fix)->setCheckable(true);
		(*fix)->setChecked(Auth().settings().supportFixChatsOrder());

		const auto subscription = Ui::AttachAsChild(_menu, rpl::lifetime());
		_menu->addAction(qsl("Reload templates"), [=] {
			*subscription = Auth().supportTemplates()->errors(
			) | rpl::start_with_next([=](QStringList errors) {
				Ui::Toast::Show(errors.isEmpty()
					? "Templates reloaded!"
					: ("Errors:\n\n" + errors.join("\n\n")));
			});
			Auth().supportTemplates()->reload();
		}, &st::mainMenuReload, &st::mainMenuReloadOver);
	}
	_menu->addAction(lang(lng_menu_settings), [] {
		App::wnd()->showSettings();
	}, &st::mainMenuSettings, &st::mainMenuSettingsOver);

	_nightThemeAction = std::make_shared<QPointer<QAction>>();
	auto action = _menu->addAction(lang(lng_menu_night_mode), [=] {
		if (auto action = *_nightThemeAction) {
			action->setChecked(!action->isChecked());
			_nightThemeSwitch.callOnce(st::mainMenu.itemToggle.duration);
		}
	}, &st::mainMenuNightMode, &st::mainMenuNightModeOver);
	*_nightThemeAction = action;
	action->setCheckable(true);
	action->setChecked(Window::Theme::IsNightMode());
	_menu->finishAnimating();

	updatePhone();
}

void MainMenu::resizeEvent(QResizeEvent *e) {
	_menu->setForceWidth(width());
	updateControlsGeometry();
}

void MainMenu::updateControlsGeometry() {
	if (_userpicButton) {
		_userpicButton->moveToLeft(st::mainMenuUserpicLeft, st::mainMenuUserpicTop);
	}
	if (_cloudButton) {
		_cloudButton->moveToRight(0, st::mainMenuCoverHeight - _cloudButton->height());
	}
	_menu->moveToLeft(0, st::mainMenuCoverHeight + st::mainMenuSkip);
	_telegram->moveToLeft(st::mainMenuFooterLeft, height() - st::mainMenuTelegramBottom - _telegram->height());
	_version->moveToLeft(st::mainMenuFooterLeft, height() - st::mainMenuVersionBottom - _version->height());
}

void MainMenu::updatePhone() {
	_phoneText = App::formatPhone(Auth().user()->phone());
	update();
}

void MainMenu::paintEvent(QPaintEvent *e) {
	Painter p(this);
	auto clip = e->rect();
	auto cover = QRect(0, 0, width(), st::mainMenuCoverHeight).intersected(clip);
	if (!cover.isEmpty()) {
		p.fillRect(cover, st::mainMenuCoverBg);
		p.setPen(st::mainMenuCoverFg);
		p.setFont(st::semiboldFont);
		Auth().user()->nameText.drawLeftElided(
			p,
			st::mainMenuCoverTextLeft,
			st::mainMenuCoverNameTop,
			width() - 2 * st::mainMenuCoverTextLeft,
			width());
		p.setFont(st::normalFont);
		p.drawTextLeft(st::mainMenuCoverTextLeft, st::mainMenuCoverStatusTop, width(), _phoneText);
		if (_cloudButton) {
			Ui::EmptyUserpic::PaintSavedMessages(
				p,
				_cloudButton->x() + (_cloudButton->width() - st::mainMenuCloudSize) / 2,
				_cloudButton->y() + (_cloudButton->height() - st::mainMenuCloudSize) / 2,
				width(),
				st::mainMenuCloudSize,
				st::mainMenuCloudBg,
				st::mainMenuCloudFg);
			//PainterHighQualityEnabler hq(p);
			//p.setPen(Qt::NoPen);
			//p.setBrush(st::mainMenuCloudBg);
			//auto cloudBg = QRect(
			//	_cloudButton->x() + (_cloudButton->width() - st::mainMenuCloudSize) / 2,
			//	_cloudButton->y() + (_cloudButton->height() - st::mainMenuCloudSize) / 2,
			//	st::mainMenuCloudSize,
			//	st::mainMenuCloudSize);
			//p.drawEllipse(cloudBg);
		}
	}
	auto other = QRect(0, st::mainMenuCoverHeight, width(), height() - st::mainMenuCoverHeight).intersected(clip);
	if (!other.isEmpty()) {
		p.fillRect(other, st::mainMenuBg);
	}
}

} // namespace Window
