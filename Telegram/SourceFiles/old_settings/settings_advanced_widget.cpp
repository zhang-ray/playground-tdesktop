/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "old_settings/settings_advanced_widget.h"

#include "styles/style_old_settings.h"
#include "lang/lang_keys.h"
#include "boxes/connection_box.h"
#include "boxes/confirm_box.h"
#include "boxes/about_box.h"
#include "boxes/local_storage_box.h"
#include "data/data_session.h"
#include "mainwindow.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/slide_wrap.h"
#include "storage/localstorage.h"
#include "window/themes/window_theme.h"

namespace OldSettings {

AdvancedWidget::AdvancedWidget(QWidget *parent, UserData *self) : BlockWidget(parent, self, lang(lng_settings_section_advanced_settings)) {
	createControls();
#ifndef TDESKTOP_DISABLE_NETWORK_PROXY
	subscribe(Global::RefConnectionTypeChanged(), [this]() {
		connectionTypeUpdated();
	});
#endif // !TDESKTOP_DISABLE_NETWORK_PROXY
	if (!self) {
		subscribe(Window::Theme::Background(), [this](const Window::Theme::BackgroundUpdate &update) {
			if (update.type == Window::Theme::BackgroundUpdate::Type::ApplyingTheme) {
				checkNonDefaultTheme();
			}
		});
	}
}

void AdvancedWidget::createControls() {
	style::margins marginSmall(0, 0, 0, st::settingsSmallSkip);
	style::margins marginLarge(0, 0, 0, st::settingsLargeSkip);

	style::margins marginLocalStorage = [&] {
#ifndef TDESKTOP_DISABLE_NETWORK_PROXY
		return marginSmall;
#else // !TDESKTOP_DISABLE_NETWORK_PROXY
		return marginLarge;
#endif // TDESKTOP_DISABLE_NETWORK_PROXY
	}();
	if (self()) {
		createChildRow(_manageLocalStorage, marginLocalStorage, lang(lng_settings_manage_local_storage), SLOT(onManageLocalStorage()));
	}

#ifndef TDESKTOP_DISABLE_NETWORK_PROXY
	createChildRow(_connectionType, marginLarge, lang(lng_connection_type), lang(lng_connection_auto_connecting), LabeledLink::Type::Primary, SLOT(onConnectionType()));
	connectionTypeUpdated();
#endif // !TDESKTOP_DISABLE_NETWORK_PROXY

	if (self()) {
		createChildRow(_askQuestion, marginSmall, lang(lng_settings_ask_question), SLOT(onAskQuestion()));
	} else {
		style::margins slidedPadding(0, marginLarge.bottom() / 2, 0, marginLarge.bottom() - (marginLarge.bottom() / 2));
		createChildRow(_useDefaultTheme, marginLarge, slidedPadding, lang(lng_settings_bg_use_default), SLOT(onUseDefaultTheme()));
		if (!Window::Theme::SuggestThemeReset()) {
			_useDefaultTheme->hide(anim::type::instant);
		}
		createChildRow(_toggleNightTheme, marginLarge, getNightThemeToggleText(), SLOT(onToggleNightTheme()));
	}
	createChildRow(_telegramFAQ, marginLarge, lang(lng_settings_faq), SLOT(onTelegramFAQ()));
	if (self()) {
		style::margins marginLogout(0, 0, 0, 2 * st::settingsLargeSkip);
		createChildRow(_logOut, marginLogout, lang(lng_settings_logout), SLOT(onLogOut()));
	}
}

void AdvancedWidget::checkNonDefaultTheme() {
	if (self()) {
		return;
	}
	_useDefaultTheme->toggle(
		Window::Theme::SuggestThemeReset(),
		anim::type::normal);
	_toggleNightTheme->setText(getNightThemeToggleText());
}

void AdvancedWidget::onManageLocalStorage() {
	LocalStorageBox::Show(&Auth().data().cache());
}

#ifndef TDESKTOP_DISABLE_NETWORK_PROXY
void AdvancedWidget::connectionTypeUpdated() {
	const auto connection = [] {
		const auto transport = MTP::dctransport();
		if (!Global::UseProxy()) {
			return transport.isEmpty()
				? lang(lng_connection_auto_connecting)
				: lng_connection_auto(lt_transport, transport);
		} else {
			return transport.isEmpty()
				? lang(lng_connection_proxy_connecting)
				: lng_connection_proxy(lt_transport, transport);
		}
	}();
	_connectionType->link()->setText(connection);
	resizeToWidth(width());
}

void AdvancedWidget::onConnectionType() {
	Ui::show(ProxiesBoxController::CreateOwningBox());
}
#endif // !TDESKTOP_DISABLE_NETWORK_PROXY

void AdvancedWidget::onUseDefaultTheme() {
	Window::Theme::ApplyDefault();
}

void AdvancedWidget::onToggleNightTheme() {
	Window::Theme::ToggleNightMode();
}

void AdvancedWidget::onAskQuestion() {
	auto box = Box<ConfirmBox>(lang(lng_settings_ask_sure), lang(lng_settings_ask_ok), lang(lng_settings_faq_button), crl::guard(this, [this] {
		onAskQuestionSure();
	}), crl::guard(this, [this] {
		onTelegramFAQ();
	}));
	box->setStrictCancel(true);
	Ui::show(std::move(box));
}

void AdvancedWidget::onAskQuestionSure() {
	if (_supportGetRequest) return;
	_supportGetRequest = MTP::send(MTPhelp_GetSupport(), rpcDone(&AdvancedWidget::supportGot));
}

void AdvancedWidget::supportGot(const MTPhelp_Support &support) {
	if (!App::main()) return;

	if (support.type() == mtpc_help_support) {
		if (auto user = App::feedUsers(MTP_vector<MTPUser>(1, support.c_help_support().vuser))) {
			Ui::showPeerHistory(user, ShowAtUnreadMsgId);
		}
	}
}

QString AdvancedWidget::getNightThemeToggleText() const {
	return lang(Window::Theme::IsNightMode()
		? lng_settings_disable_night_theme
		: lng_settings_enable_night_theme);
}

void AdvancedWidget::onTelegramFAQ() {
	QDesktopServices::openUrl(telegramFaqLink());
}

void AdvancedWidget::onLogOut() {
	App::wnd()->onLogout();
}

} // namespace Settings
