#include "Tbx/PCH.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventSync.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Debug/IPrintable.h"
#include <Tbx/Memory/Refs.h>
#include <unordered_map>

namespace Tbx
{
	//////////// Event Suppressor ///////////////

	std::atomic_int EventSuppressor::_suppressCount = 0;

	EventSuppressor::EventSuppressor()
	{
		Suppress();
	}

	EventSuppressor::~EventSuppressor()
	{
		Unsuppress();
	}

	bool EventSuppressor::IsSuppressing()
	{
		return _suppressCount.load(std::memory_order_relaxed) > 0;
	}

	void EventSuppressor::Suppress()
	{
		_suppressCount.fetch_add(1, std::memory_order_relaxed);
	}

	void EventSuppressor::Unsuppress()
	{
		_suppressCount.fetch_sub(1, std::memory_order_relaxed);
	}

	//////////// Event Bus ///////////////

	Ref<EventBus> EventBus::Global = CreateGlobal();
	bool EventBus::_creatingGlobal = false;

	EventBus::EventBus(Ref<EventBus> parent)
		: Parent(parent != nullptr && !_creatingGlobal
			? parent
			: Global)
	{
	}

	EventBus::~EventBus()
	{
		EventSync sync;
		while (!EventQueue.empty())
		{
			EventQueue.pop();
		}
		Subscriptions.clear();
		SubscriptionIndex.clear();
	}

	void EventBus::Flush()
	{
		std::queue<ExclusiveRef<Event>> localQueue;
		{
			EventSync sync;
			localQueue.swap(EventQueue);
		}

		while (!localQueue.empty())
		{
			auto event = std::move(localQueue.front());
			localQueue.pop();

			if (!event)
			{
				continue;
			}

			// If suppressed globally, skip processing of queued events as well.
			if (EventSuppressor::IsSuppressing())
			{
				TBX_TRACE_WARNING("EventBus: Queued event \"{}\" suppressed", event->ToString());
				continue;
			}

			std::unordered_map<Uid, EventCallback> callbacks;
			const auto hashCode = Memory::Hash(*event);
			CollectCallbacks(hashCode, callbacks);
			if (callbacks.empty())
			{
				continue;
			}

			for (auto& [id, callback] : callbacks)
			{
				if (EventSuppressor::IsSuppressing())
				{
					TBX_TRACE_WARNING("EventBus: The event \"{}\" is suppressed during flush", event->ToString());
					break;
				}

				callback(*event);
			}
		}
	}

	Ref<EventBus> EventBus::CreateGlobal()
	{
		_creatingGlobal = true;
		auto bus = MakeRef<EventBus>();
		_creatingGlobal = false;
		return bus;
	}

	void EventBus::CollectCallbacks(EventHash eventKey, std::unordered_map<Uid, EventCallback>& callbacks) const
	{
		Ref<EventBus> parentCopy = nullptr;

		{
			EventSync sync;
			auto it = Subscriptions.find(eventKey);
			if (it != Subscriptions.end())
			{
				callbacks.insert(it->second.begin(), it->second.end());
			}

			parentCopy = Parent;
		}

		if (parentCopy)
		{
			parentCopy->CollectCallbacks(eventKey, callbacks);
		}
	}
}
