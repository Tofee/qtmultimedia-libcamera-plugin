/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2019, Google Inc.
 *
 * qt_event_dispatcher.h - qcam - Qt-based event dispatcher
 */
#ifndef LIBCAMERA_QT_EVENT_DISPATCHER_H
#define LIBCAMERA_QT_EVENT_DISPATCHER_H

#include <map>

#include <QObject>

#include <libcamera/event_dispatcher.h>

using namespace libcamera;

class QSocketNotifier;
class QTimerEvent;

class LibcameraQtEventDispatcher final : public EventDispatcher, public QObject
{
public:
    LibcameraQtEventDispatcher();
    ~LibcameraQtEventDispatcher();

	void registerEventNotifier(EventNotifier *notifier);
	void unregisterEventNotifier(EventNotifier *notifier);

	void registerTimer(Timer *timer);
	void unregisterTimer(Timer *timer);

	void processEvents();

	void interrupt();

protected:
	void timerEvent(QTimerEvent *event);

private:
	void readNotifierActivated(int socket);
	void writeNotifierActivated(int socket);
	void exceptionNotifierActivated(int socket);

	struct NotifierPair {
		NotifierPair()
			: notifier(nullptr), qnotifier(nullptr)
		{
		}
		EventNotifier *notifier;
		QSocketNotifier *qnotifier;
	};

	struct NotifierSet {
		NotifierPair read;
		NotifierPair write;
		NotifierPair exception;
	};

	std::map<int, NotifierSet> notifiers_;
	std::map<int, Timer *> timers_;
	std::map<Timer *, int> timerIds_;
};

#endif /* LIBCAMERA_QT_EVENT_DISPATCHER_H */
