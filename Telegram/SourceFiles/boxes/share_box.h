/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "boxes/abstract_box.h"
#include "base/observer.h"
#include "ui/effects/round_checkbox.h"

namespace Dialogs {
class Row;
class IndexedList;
} // namespace Dialogs

namespace Notify {
struct PeerUpdate;
} // namespace Notify

namespace Ui {
class MultiSelect;
} // namespace Ui

QString AppendShareGameScoreUrl(const QString &url, const FullMsgId &fullId);
void ShareGameScoreByHash(const QString &hash);

class ShareBox : public BoxContent, public RPCSender {
	Q_OBJECT

public:
	using CopyCallback = Fn<void()>;
	using SubmitCallback = Fn<void(const QVector<PeerData*> &)>;
	using FilterCallback = Fn<bool(PeerData*)>;
	ShareBox(QWidget*, CopyCallback &&copyCallback, SubmitCallback &&submitCallback, FilterCallback &&filterCallback);

protected:
	void prepare() override;
	void setInnerFocus() override;

	void resizeEvent(QResizeEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;

private slots:
	bool onSearchByUsername(bool searchCache = false);
	void onNeedSearchByUsername();

	void onSubmit();
	void onCopyLink();

	void onMustScrollTo(int top, int bottom);

private:
	void scrollAnimationCallback();

	void onFilterUpdate(const QString &query);
	void onSelectedChanged();
	void updateButtons();
	void createButtons();
	int getTopScrollSkip() const;
	void updateScrollSkips();

	void addPeerToMultiSelect(PeerData *peer, bool skipAnimation = false);
	void onPeerSelectedChanged(PeerData *peer, bool checked);

	void peopleReceived(
		const MTPcontacts_Found &result,
		mtpRequestId requestId);
	bool peopleFailed(const RPCError &error, mtpRequestId requestId);

	CopyCallback _copyCallback;
	SubmitCallback _submitCallback;
	FilterCallback _filterCallback;

	object_ptr<Ui::MultiSelect> _select;

	class Inner;
	QPointer<Inner> _inner;

	bool _hasSelected = false;

	object_ptr<QTimer> _searchTimer;
	QString _peopleQuery;
	bool _peopleFull = false;
	mtpRequestId _peopleRequest = 0;

	using PeopleCache = QMap<QString, MTPcontacts_Found>;
	PeopleCache _peopleCache;

	using PeopleQueries = QMap<mtpRequestId, QString>;
	PeopleQueries _peopleQueries;

	Animation _scrollAnimation;

};

// This class is hold in header because it requires Qt preprocessing.
class ShareBox::Inner : public TWidget, public RPCSender, private base::Subscriber {
	Q_OBJECT

public:
	Inner(QWidget *parent, ShareBox::FilterCallback &&filterCallback);

	void setPeerSelectedChangedCallback(Fn<void(PeerData *peer, bool selected)> callback);
	void peerUnselected(not_null<PeerData*> peer);

	QVector<PeerData*> selected() const;
	bool hasSelected() const;

	void peopleReceived(
		const QString &query,
		const QVector<MTPPeer> &my,
		const QVector<MTPPeer> &people);

	void activateSkipRow(int direction);
	void activateSkipColumn(int direction);
	void activateSkipPage(int pageHeight, int direction);
	void updateFilter(QString filter = QString());

	~Inner();

public slots:
	void onSelectActive();

signals:
	void mustScrollTo(int ymin, int ymax);
	void searchByUsername();

protected:
	void visibleTopBottomUpdated(
		int visibleTop,
		int visibleBottom) override;

	void paintEvent(QPaintEvent *e) override;
	void enterEventHook(QEvent *e) override;
	void leaveEventHook(QEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

private:
	// Observed notifications.
	void notifyPeerUpdated(const Notify::PeerUpdate &update);
	void invalidateCache();

	int displayedChatsCount() const;

	struct Chat {
		Chat(PeerData *peer, Fn<void()> updateCallback);

		PeerData *peer;
		Ui::RoundImageCheckbox checkbox;
		Text name;
		Animation nameActive;
	};
	void paintChat(Painter &p, TimeMs ms, not_null<Chat*> chat, int index);
	void updateChat(not_null<PeerData*> peer);
	void updateChatName(not_null<Chat*> chat, not_null<PeerData*> peer);
	void repaintChat(not_null<PeerData*> peer);
	int chatIndex(not_null<PeerData*> peer) const;
	void repaintChatAtIndex(int index);
	Chat *getChatAtIndex(int index);

	void loadProfilePhotos(int yFrom);
	void changeCheckState(Chat *chat);
	enum class ChangeStateWay {
		Default,
		SkipCallback,
	};
	void changePeerCheckState(
		not_null<Chat*> chat,
		bool checked,
		ChangeStateWay useCallback = ChangeStateWay::Default);

	Chat *getChat(Dialogs::Row *row);
	void setActive(int active);
	void updateUpon(const QPoint &pos);

	void refresh();

	float64 _columnSkip = 0.;
	float64 _rowWidthReal = 0.;
	int _rowsLeft = 0;
	int _rowsTop = 0;
	int _rowWidth = 0;
	int _rowHeight = 0;
	int _columnCount = 4;
	int _active = -1;
	int _upon = -1;

	ShareBox::FilterCallback _filterCallback;
	std::unique_ptr<Dialogs::IndexedList> _chatsIndexed;
	QString _filter;
	using FilteredDialogs = QVector<Dialogs::Row*>;
	FilteredDialogs _filtered;

	using DataMap = QMap<PeerData*, Chat*>;
	DataMap _dataMap;
	using SelectedChats = OrderedSet<PeerData*>;
	SelectedChats _selected;

	Fn<void(PeerData *peer, bool selected)> _peerSelectedChangedCallback;

	ChatData *data(Dialogs::Row *row);

	bool _searching = false;
	QString _lastQuery;
	std::vector<PeerData*> _byUsernameFiltered;
	std::vector<Chat*> d_byUsernameFiltered;

};
