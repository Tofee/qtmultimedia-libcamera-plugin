/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2019, Google Inc.
 *
 * qt_event_dispatcher.cpp - qcam - Qt-based event dispatcher
 */

#include <chrono>
#include <iostream>

#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTimerEvent>

#include <libcamera/event_notifier.h>
#include <libcamera/timer.h>

#include "libcameraqteventdispatcher.h"

using namespace libcamera;

LibcameraQtEventDispatcher::LibcameraQtEventDispatcher()
{
}

LibcameraQtEventDispatcher::~LibcameraQtEventDispatcher()
{
	for (auto &it : notifiers_) {
		NotifierSet &set = it.second;
		delete set.read.qnotifier;
		delete set.write.qnotifier;
		delete set.exception.qnotifier;
	}
}

void LibcameraQtEventDispatcher::registerEventNotifier(EventNotifier *notifier)
{
	NotifierSet &set = notifiers_[notifier->fd()];
	QSocketNotifier::Type qtype;
    void (LibcameraQtEventDispatcher::*method)(int);
	NotifierPair *pair;

	switch (notifier->type()) {
	case EventNotifier::Read:
		qtype = QSocketNotifier::Read;
        method = &LibcameraQtEventDispatcher::readNotifierActivated;
		pair = &set.read;
		break;

	case EventNotifier::Write:
		qtype = QSocketNotifier::Write;
        method = &LibcameraQtEventDispatcher::writeNotifierActivated;
		pair = &set.write;
		break;

	case EventNotifier::Exception:
		qtype = QSocketNotifier::Exception;
        method = &LibcameraQtEventDispatcher::exceptionNotifierActivated;
		pair = &set.exception;
		break;
	}

	QSocketNotifier *qnotifier = new QSocketNotifier(notifier->fd(), qtype);
	connect(qnotifier, &QSocketNotifier::activated, this, method);
	pair->notifier = notifier;
	pair->qnotifier = qnotifier;
}

void LibcameraQtEventDispatcher::unregisterEventNotifier(EventNotifier *notifier)
{
	NotifierSet &set = notifiers_[notifier->fd()];
	NotifierPair *pair;

	switch (notifier->type()) {
	case EventNotifier::Read:
		pair = &set.read;
		break;

	case EventNotifier::Write:
		pair = &set.write;
		break;

	case EventNotifier::Exception:
		pair = &set.exception;
		break;
	}

	delete pair->qnotifier;
	pair->qnotifier = nullptr;
	pair->notifier = nullptr;
}

void LibcameraQtEventDispatcher::readNotifierActivated(int socket)
{
	EventNotifier *notifier = notifiers_[socket].read.notifier;
	notifier->activated.emit(notifier);
}

void LibcameraQtEventDispatcher::writeNotifierActivated(int socket)
{
	EventNotifier *notifier = notifiers_[socket].write.notifier;
	notifier->activated.emit(notifier);
}

void LibcameraQtEventDispatcher::exceptionNotifierActivated(int socket)
{
	EventNotifier *notifier = notifiers_[socket].exception.notifier;
	notifier->activated.emit(notifier);
}

void LibcameraQtEventDispatcher::registerTimer(Timer *timer)
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	std::chrono::steady_clock::duration duration = timer->deadline() - now;
	std::chrono::milliseconds msec =
		std::chrono::duration_cast<std::chrono::milliseconds>(duration);
	int timerId = startTimer(msec.count());
	timers_[timerId] = timer;
	timerIds_[timer] = timerId;
}

void LibcameraQtEventDispatcher::unregisterTimer(Timer *timer)
{
	auto it = timerIds_.find(timer);
	if (it == timerIds_.end())
		return;

	timers_.erase(it->second);
	killTimer(it->second);
	timerIds_.erase(it);
}

void LibcameraQtEventDispatcher::timerEvent(QTimerEvent *event)
{
	Timer *timer = timers_[event->timerId()];
	timer->stop();
	timer->timeout.emit(timer);
}

void LibcameraQtEventDispatcher::processEvents()
{
    std::cout << "LibcameraQtEventDispatcher::processEvents() should not be called"
		  << std::endl;
}

void LibcameraQtEventDispatcher::interrupt()
{
	QCoreApplication::eventDispatcher()->interrupt();
}
