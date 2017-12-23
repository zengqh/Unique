#pragma once
#include "../Core/CoreDefs.h"
#include "../Container/refcounted.h"
#include "../Serialize/Serializer.h"
#include "AttributeTraits.h"
#include <type_traits>

namespace Unique
{

	class Attribute : public RefCounted
	{
	public:
		Attribute(const String& name, AttributeFlag flag)
			: name_(name), flag_(flag)
		{
		}

		virtual void Visit(Serializer& serializer, void* obj) {}
		virtual void Get(const void* ptr, void* dest) const = 0;
		virtual void Set(void* ptr, const void* value) = 0;

		/// Name.
		String name_;
		AttributeFlag flag_;
		uint type_;
	};

	template<class T>
	class TAttribute : public Attribute
	{
	public:
		using RawType = typename remove_reference<T>::type;

		TAttribute(const String& name, size_t offset, AttributeFlag flag)
			: Attribute(name, flag), offset_(offset)
		{
		}

		virtual void Visit(Serializer& serializer, void* obj)
		{
			VisitImpl(serializer, obj);
		}

		virtual void Get(const void* ptr, void* dest) const
		{
			const void* src = reinterpret_cast<const unsigned char*>(ptr) + offset_;
			std::memcpy(dest, src, sizeof(RawType));
		}

		virtual void Set(void* ptr, const void* value)
		{
			void* dest = reinterpret_cast<unsigned char*>(ptr) + offset_;
			std::memcpy(dest, value, sizeof(RawType));
		}

	protected:
		template<class Visitor>
		void VisitImpl(Visitor& visitor, void* ptr)
		{
			RawType* dest = (RawType*)(reinterpret_cast<unsigned char*>(ptr) + offset_);
			visitor.TransferAttribute(name_.CString(), *dest, flag_);
		}

		size_t offset_;
		RawType defaultVal_;
	};
	
	/// Template implementation of the attribute accessor invoke helper class.
	template <typename T, typename U, typename Trait> class AttributeAccessorImpl : public Attribute
	{
	public:
		typedef typename Trait::ReturnType(T::*GetFunctionPtr)() const;
		typedef void (T::*SetFunctionPtr)(typename Trait::ParameterType);

		/// Construct with function pointers.
		AttributeAccessorImpl(const String& name, GetFunctionPtr getFunction, SetFunctionPtr setFunction, AttributeFlag flag) :
			getFunction_(getFunction),
			setFunction_(setFunction),
			Attribute(name, flag)
		{
			assert(getFunction_);
			assert(setFunction_);
		}

		virtual void Visit(Serializer& serializer, void* obj)
		{
			VisitImpl(serializer, obj);
		}

		/// Invoke getter function.
		virtual void Get(const void* ptr, void* dest) const
		{
			assert(ptr);
			const T* classPtr = static_cast<const T*>(ptr);
			*(U*)dest = (classPtr->*getFunction_)();
		}

		/// Invoke setter function.
		virtual void Set(void* ptr, const void* value)
		{
			assert(ptr);
			T* classPtr = static_cast<T*>(ptr);
			(classPtr->*setFunction_)(*(U*)value);
		}
	protected:
		template<class Visitor>
		void VisitImpl(Visitor& visitor, void* ptr)
		{
			assert(ptr);
			T* classPtr = static_cast<T*>(ptr);

			if (visitor.IsReading())
			{
				U value;
				visitor.TransferAttribute(name_.CString(), value, flag_);
				(classPtr->*setFunction_)(value);
			}

			if (visitor.IsWriting())
			{
				U value = (classPtr->*getFunction_)();
				visitor.TransferAttribute(name_.CString(), value, flag_);
			}
		}

		GetFunctionPtr getFunction_;
		SetFunctionPtr setFunction_;
		typename remove_reference<U>::type defaultVal_;
	};


}
