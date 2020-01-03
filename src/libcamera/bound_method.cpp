/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2019, Google Inc.
 *
 * bound_method.cpp - Method bind and invocation
 */

#include <libcamera/bound_method.h>

#include "message.h"
#include "semaphore.h"
#include "thread.h"
#include "utils.h"

/**
 * \file bound_method.h
 * \brief Method bind and invocation
 */

namespace libcamera {

/**
 * \enum ConnectionType
 * \brief Connection type for asynchronous communication
 *
 * This enumeration describes the possible types of asynchronous communication
 * between a sender and a receiver. It applies to Signal::emit() and
 * Object::invokeMethod().
 *
 * \var ConnectionType::ConnectionTypeAuto
 * \brief If the sender and the receiver live in the same thread,
 * ConnectionTypeDirect is used. Otherwise ConnectionTypeQueued is used.
 *
 * \var ConnectionType::ConnectionTypeDirect
 * \brief The receiver is invoked immediately and synchronously in the sender's
 * thread.
 *
 * \var ConnectionType::ConnectionTypeQueued
 * \brief The receiver is invoked asynchronously in its thread when control
 * returns to the thread's event loop. The sender proceeds without waiting for
 * the invocation to complete.
 *
 * \var ConnectionType::ConnectionTypeBlocking
 * \brief The receiver is invoked asynchronously in its thread when control
 * returns to the thread's event loop. The sender blocks until the receiver
 * signals the completion of the invocation. This connection type shall not be
 * used when the sender and receiver live in the same thread, otherwise
 * deadlock will occur.
 */

/**
 * \brief Invoke the bound method with packed arguments
 * \param[in] pack Packed arguments
 * \param[in] deleteMethod True to delete \a this bound method instance when
 * method invocation completes
 *
 * The bound method stores its return value, if any, in the arguments \a pack.
 * For direct and blocking invocations, this is performed synchronously, and
 * the return value contained in the pack may be used. For queued invocations,
 * the return value is stored at an undefined point of time and shall thus not
 * be used by the caller.
 *
 * \return True if the return value contained in the \a pack may be used by the
 * caller, false otherwise
 */
bool BoundMethodBase::activatePack(std::shared_ptr<BoundMethodPackBase> pack,
				   bool deleteMethod)
{
	ConnectionType type = connectionType_;
	if (type == ConnectionTypeAuto) {
		if (Thread::current() == object_->thread())
			type = ConnectionTypeDirect;
		else
			type = ConnectionTypeQueued;
	}

	switch (type) {
	case ConnectionTypeDirect:
	default:
		invokePack(pack.get());
		if (deleteMethod)
			delete this;
		return true;

	case ConnectionTypeQueued: {
		std::unique_ptr<Message> msg =
			utils::make_unique<InvokeMessage>(this, pack, nullptr, deleteMethod);
		object_->postMessage(std::move(msg));
		return false;
	}

	case ConnectionTypeBlocking: {
		Semaphore semaphore;

		std::unique_ptr<Message> msg =
			utils::make_unique<InvokeMessage>(this, pack, &semaphore, deleteMethod);
		object_->postMessage(std::move(msg));

		semaphore.acquire();
		return true;
	}
	}
}

} /* namespace libcamera */
