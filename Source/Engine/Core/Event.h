#ifndef _SRC_EVENT_EVENT_HANDLER_HPP_
#define _SRC_EVENT_EVENT_HANDLER_HPP_

#include "../Container/linkedlist.h"

#include "../Container/StringID.h"

namespace Unique
{
	class Object;

	struct Event
	{
	};

	/// Internal helper class for invoking event handler functions.
	class UNIQUE_API EventHandler : public LinkedListNode
	{
	public:
		/// Construct with specified receiver and userdata.
		EventHandler(Object* receiver, void* userData = 0) :
			receiver_(receiver),
			sender_(0),
			userData_(userData)
		{
		}

		/// Destruct.
		virtual ~EventHandler() { }

		/// Set sender and event type.
		void SetSenderAndEventType(Object* sender, StringID eventType)
		{
			sender_ = sender;
			eventType_ = eventType;
		}

		/// Invoke event handler function.
		virtual void Invoke(const Event& eventData) = 0;
		/// Return a unique copy of the event handler.
		virtual EventHandler* Clone() const = 0;

		/// Return event receiver.
		Object* GetReceiver() const { return receiver_; }

		/// Return event sender. Null if the handler is non-specific.
		Object* GetSender() const { return sender_; }

		/// Return event type.
		const StringID& GetEventType() const { return eventType_; }

		/// Return userdata.
		void* GetUserData() const { return userData_; }

	protected:
		/// Event receiver.
		Object* receiver_;
		/// Event sender.
		Object* sender_;
		/// Event type.
		StringID eventType_;
		/// Userdata.
		void* userData_;
	};

	/// Template implementation of the event handler invoke helper (stores a function pointer of specific class.)
	template <class T, class E> class TEventHandler : public EventHandler
	{
	public:
		typedef void (T::*HandlerFunctionPtr)(StringID, const E&);

		/// Construct with receiver and function pointers and userdata.
		TEventHandler(T* receiver, HandlerFunctionPtr function, void* userData = 0) :
			EventHandler(receiver, userData),
			function_(function)
		{
			assert(receiver_);
			assert(function_);
		}

		/// Invoke event handler function.
		virtual void Invoke(const Event& eventData)
		{
			T* receiver = static_cast<T*>(receiver_);
			(receiver->*function_)(eventType_, (const E&)eventData);
		}

		void Invoke(const E& args)
		{
			T* receiver = static_cast<T*>(receiver_);
			(receiver->*function_)(eventType_, args);
		}

		/// Return a unique copy of the event handler.
		virtual EventHandler* Clone() const
		{
			return new TEventHandler(static_cast<T*>(receiver_), function_, userData_);
		}

	private:
		/// Class-specific pointer to handler function.
		HandlerFunctionPtr function_;
	};

	typedef void(__stdcall *EventFn)(Object* self, StringID, const void* eventData);

	class StaticEventHandler : public EventHandler
	{
	public:
		/// Construct with receiver and function pointers and userdata.
		StaticEventHandler(Object* receiver, EventFn function) :
			EventHandler(receiver, nullptr)
		{
			function_ = function;
		}

		/// Invoke event handler function.
		virtual void Invoke(const Event& eventData)
		{
			function_(receiver_, eventType_, &eventData);
		}

		/// Return a unique copy of the event handler.
		virtual EventHandler* Clone() const
		{
			return new StaticEventHandler(receiver_, function_);
		}
	private:
		/// Class-specific pointer to handler function.
		EventFn function_;
	};

	/// Describe an event's hash ID and begin a namespace in which to define its parameters.
#define UNIQUE_EVENT(eventID, eventName)\
 static const Unique::StringID eventID(#eventName);\
 struct eventName : public Event\

	/// Describe an event's parameter hash ID. Should be used inside an event namespace.
#define UNIQUE_PARAM(paramType, paramName) paramType paramName
	
	/// Convenience macro to construct an EventHandler that points to a receiver object and its member function.
#define UNIQUE_HANDLER(className, function) &className::function

}

#endif /* _SRC_EVENT_EVENT_HANDLER_HPP_ */
