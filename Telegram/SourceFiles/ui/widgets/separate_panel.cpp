/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/widgets/separate_panel.h"

#include "window/main_window.h"
#include "platform/platform_specific.h"
#include "ui/widgets/shadow.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/fade_wrap.h"
#include "ui/toast/toast.h"
#include "ui/widgets/tooltip.h"
#include "window/layer_widget.h"
#include "window/themes/window_theme.h"
#include "messenger.h"
#include "styles/style_widgets.h"
#include "styles/style_info.h"


namespace Ui {

SeparatePanel::SeparatePanel()
: _close(this, st::separatePanelClose)
, _back(this, object_ptr<Ui::IconButton>(this, st::separatePanelBack))
, _body(this) {
	setMouseTracking(true);
	setWindowIcon(Window::CreateIcon());
	initControls();
	initLayout();
}

void SeparatePanel::setTitle(rpl::producer<QString> title) {
	_title.create(this, std::move(title), st::separatePanelTitle);
	_title->setAttribute(Qt::WA_TransparentForMouseEvents);
	_title->show();
	updateTitleGeometry(width());
}

void SeparatePanel::initControls() {
	widthValue(
	) | rpl::start_with_next([=](int width) {
		_back->moveToLeft(_padding.left(), _padding.top());
		_close->moveToRight(_padding.right(), _padding.top());
		if (_title) {
			updateTitleGeometry(width);
		}
	}, lifetime());

	_back->toggledValue(
	) | rpl::start_with_next([=](bool toggled) {
		_titleLeft.start(
			[=] { updateTitlePosition(); },
			toggled ? 0. : 1.,
			toggled ? 1. : 0.,
			st::fadeWrapDuration);
	}, _back->lifetime());
	_back->hide(anim::type::instant);
	_titleLeft.finish();
}

void SeparatePanel::updateTitleGeometry(int newWidth) {
	_title->resizeToWidth(newWidth
		- _padding.left() - _back->width()
		- _padding.right() - _close->width());
	updateTitlePosition();
}

void SeparatePanel::updateTitlePosition() {
	if (!_title) {
		return;
	}
	const auto progress = _titleLeft.current(_back->toggled() ? 1. : 0.);
	const auto left = anim::interpolate(
		st::separatePanelTitleLeft,
		_back->width() + st::separatePanelTitleSkip,
		progress);
	_title->moveToLeft(
		_padding.left() + left,
		_padding.top() + st::separatePanelTitleTop);
}

rpl::producer<> SeparatePanel::backRequests() const {
	return rpl::merge(
		_back->entity()->clicks(
		) | rpl::map([] { return rpl::empty_value(); }),
		_synteticBackRequests.events());
}

rpl::producer<> SeparatePanel::closeRequests() const {
	return rpl::merge(
		_close->clicks(
		) | rpl::map([] { return rpl::empty_value(); }),
		_userCloseRequests.events());
}

rpl::producer<> SeparatePanel::closeEvents() const {
	return _closeEvents.events();
}

void SeparatePanel::setBackAllowed(bool allowed) {
	if (allowed != _back->toggled()) {
		_back->toggle(allowed, anim::type::normal);
	}
}

void SeparatePanel::setHideOnDeactivate(bool hideOnDeactivate) {
	_hideOnDeactivate = hideOnDeactivate;
	if (!_hideOnDeactivate) {
		showAndActivate();
	} else if (!isActiveWindow()) {
		LOG(("Export Info: Panel Hide On Inactive Change."));
		hideGetDuration();
	}
}

void SeparatePanel::showAndActivate() {
	toggleOpacityAnimation(true);
	raise();
	setWindowState(windowState() | Qt::WindowActive);
	activateWindow();
	setFocus();
}

void SeparatePanel::keyPressEvent(QKeyEvent *e) {
	if (e->key() == Qt::Key_Escape && _back->toggled()) {
		_synteticBackRequests.fire({});
	}
	return RpWidget::keyPressEvent(e);
}

bool SeparatePanel::eventHook(QEvent *e) {
	if (e->type() == QEvent::WindowDeactivate && _hideOnDeactivate) {
		LOG(("Export Info: Panel Hide On Inactive Window."));
		hideGetDuration();
	}
	return RpWidget::eventHook(e);
}

void SeparatePanel::initLayout() {
	setWindowFlags(Qt::WindowFlags(Qt::FramelessWindowHint)
		| Qt::WindowStaysOnTopHint
		| Qt::NoDropShadowWindowHint
		| Qt::Dialog);
	setAttribute(Qt::WA_MacAlwaysShowToolWindow);
	setAttribute(Qt::WA_NoSystemBackground, true);
	setAttribute(Qt::WA_TranslucentBackground, true);

	createBorderImage();
	subscribe(Window::Theme::Background(), [=](
			const Window::Theme::BackgroundUpdate &update) {
		if (update.paletteChanged()) {
			createBorderImage();
			Ui::ForceFullRepaint(this);
		}
	});

	Platform::InitOnTopPanel(this);
}

void SeparatePanel::createBorderImage() {
}

void SeparatePanel::toggleOpacityAnimation(bool visible) {
	if (_visible == visible) {
		return;
	}
}

void SeparatePanel::opacityCallback() {
	update();
	if (!_visible && !_opacityAnimation.animating()) {
		finishAnimating();
	}
}

void SeparatePanel::finishAnimating() {
	_animationCache = QPixmap();
	if (_visible) {
		showControls();
		if (_inner) {
			_inner->setFocus();
		}
	} else {
		finishClose();
	}
}

void SeparatePanel::showControls() {
	showChildren();
	if (!_back->toggled()) {
		_back->setVisible(false);
	}
}

void SeparatePanel::finishClose() {
	hide();
	crl::on_main(this, [=] {
		if (isHidden() && !_visible && !_opacityAnimation.animating()) {
			LOG(("Export Info: Panel Closed."));
			_closeEvents.fire({});
		}
	});
}

int SeparatePanel::hideGetDuration() {
	return 0;
}

void SeparatePanel::showBox(
		object_ptr<BoxContent> box,
		LayerOptions options,
		anim::type animated) {
	ensureLayerCreated();
	_layer->showBox(std::move(box), options, animated);
}

void SeparatePanel::showToast(const QString &text) {
	auto toast = Ui::Toast::Config();
	toast.text = text;
	Ui::Toast::Show(this, toast);
}

void SeparatePanel::ensureLayerCreated() {
	if (_layer) {
		return;
	}
	_layer.create(_body);
	_layer->setHideByBackgroundClick(false);
	_layer->move(0, 0);
	_body->sizeValue(
	) | rpl::start_with_next([=](QSize size) {
		_layer->resize(size);
	}, _layer->lifetime());
	_layer->hideFinishEvents(
	) | rpl::start_with_next([=, pointer = _layer.data()]{
		if (_layer != pointer) {
			return;
		}
		auto saved = std::exchange(_layer, nullptr);
		if (Ui::InFocusChain(saved)) {
			setFocus();
		}
		saved.destroyDelayed();
	}, _layer->lifetime());
}

void SeparatePanel::showInner(base::unique_qptr<Ui::RpWidget> inner) {
	Expects(!size().isEmpty());

	_inner = std::move(inner);
	_inner->setParent(_body);
	_inner->move(0, 0);
	_body->sizeValue(
	) | rpl::start_with_next([=](QSize size) {
		_inner->resize(size);
	}, _inner->lifetime());
	_inner->show();

	if (_layer) {
		_layer->raise();
	}

	showAndActivate();
}

void SeparatePanel::focusInEvent(QFocusEvent *e) {
	crl::on_main(this, [=] {
		if (_layer) {
			_layer->setInnerFocus();
		} else if (_inner && !_inner->isHidden()) {
			_inner->setFocus();
		}
	});
}

void SeparatePanel::setInnerSize(QSize size) {
	Expects(!size.isEmpty());

	if (rect().isEmpty()) {
		initGeometry(size);
	} else {
		updateGeometry(size);
	}
}

void SeparatePanel::initGeometry(QSize size) {
	
}

void SeparatePanel::updateGeometry(QSize size) {
	setGeometry(
		x(),
		y(),
		_padding.left() + size.width() + _padding.right(),
		_padding.top() + size.height() + _padding.bottom());
	updateControlsGeometry();
	update();
}

void SeparatePanel::resizeEvent(QResizeEvent *e) {
	updateControlsGeometry();
}

void SeparatePanel::updateControlsGeometry() {
	const auto top = _padding.top() + st::separatePanelTitleHeight;
	_body->setGeometry(
		_padding.left(),
		top,
		width() - _padding.left() - _padding.right(),
		height() - top - _padding.bottom());
}

void SeparatePanel::paintEvent(QPaintEvent *e) {
	Painter p(this);
	if (!_animationCache.isNull()) {
		auto opacity = _opacityAnimation.current(
			getms(),
			_visible ? 1. : 0.);
		if (!_opacityAnimation.animating()) {
			finishAnimating();
			if (isHidden()) return;
		} else {
			Platform::StartTranslucentPaint(p, e);
			p.setOpacity(opacity);

			PainterHighQualityEnabler hq(p);
			auto marginRatio = (1. - opacity) / 5;
			auto marginWidth = qRound(width() * marginRatio);
			auto marginHeight = qRound(height() * marginRatio);
			p.drawPixmap(
				rect().marginsRemoved(
					QMargins(
						marginWidth,
						marginHeight,
						marginWidth,
						marginHeight)),
				_animationCache,
				QRect(QPoint(0, 0), _animationCache.size()));
			return;
		}
	}

	if (_useTransparency) {
		Platform::StartTranslucentPaint(p, e);
		paintShadowBorder(p);
	} else {
		paintOpaqueBorder(p);
	}
}

void SeparatePanel::paintShadowBorder(Painter &p) const {
	
}

void SeparatePanel::paintOpaqueBorder(Painter &p) const {
	const auto border = st::windowShadowFgFallback;
	p.fillRect(0, 0, width(), _padding.top(), border);
	p.fillRect(
		myrtlrect(
			0,
			_padding.top(),
			_padding.left(),
			height() - _padding.top()),
		border);
	p.fillRect(
		myrtlrect(
			width() - _padding.right(),
			_padding.top(),
			_padding.right(),
			height() - _padding.top()),
		border);
	p.fillRect(
		_padding.left(),
		height() - _padding.bottom(),
		width() - _padding.left() - _padding.right(),
		_padding.bottom(),
		border);

	p.fillRect(
		_padding.left(),
		_padding.top(),
		width() - _padding.left() - _padding.right(),
		height() - _padding.top() - _padding.bottom(),
		st::windowBg);
}

void SeparatePanel::closeEvent(QCloseEvent *e) {
	e->ignore();
	_userCloseRequests.fire({});
}

void SeparatePanel::mousePressEvent(QMouseEvent *e) {
	auto dragArea = myrtlrect(
		_padding.left(),
		_padding.top(),
		width() - _padding.left() - _padding.right(),
		st::separatePanelTitleHeight);
	if (e->button() == Qt::LeftButton) {
		if (dragArea.contains(e->pos())) {
			_dragging = true;
			_dragStartMousePosition = e->globalPos();
			_dragStartMyPosition = QPoint(x(), y());
		} else if (!rect().contains(e->pos()) && _hideOnDeactivate) {
			LOG(("Export Info: Panel Hide On Click."));
			hideGetDuration();
		}
	}
}

void SeparatePanel::mouseMoveEvent(QMouseEvent *e) {
	if (_dragging) {
		if (!(e->buttons() & Qt::LeftButton)) {
			_dragging = false;
		} else {
			move(_dragStartMyPosition
				+ (e->globalPos() - _dragStartMousePosition));
		}
	}
}

void SeparatePanel::mouseReleaseEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
		_dragging = false;
	}
}

void SeparatePanel::leaveEventHook(QEvent *e) {
	Ui::Tooltip::Hide();
}

void SeparatePanel::leaveToChildEvent(QEvent *e, QWidget *child) {
	Ui::Tooltip::Hide();
}

} // namespace Ui
