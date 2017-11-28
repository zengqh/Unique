#pragma once

enum class AttributeFlag
{
	Editable = 1,
	FileRead = 2,
	FileWrite = 4,
	Default = 7,
};

#define uTransferField(NAME, FIELD, ...) transfer.TransferAttribute (NAME, FIELD, ##__VA_ARGS__);

#define uTransferAccessor(NAME, GET, SET, TYPE, ...)\
{\
	TYPE tmp;\
	if (transfer.IsWriting ())\
		tmp = GET(); \
	transfer.TransferAttribute(NAME, tmp, ##__VA_ARGS__); \
	if (transfer.IsReading ())\
		SET (tmp);\
}

#define uClass(...) \
	inline static bool AllowTransferOptimization ()	{ return false; } \
	template<class TransferFunction> \
	void Transfer(TransferFunction& transfer)\
{\
transfer.TransferAttributes(##__VA_ARGS__);\
}


#define DEFINE_GET_TYPESTRING_CONTAINER(x)						\
inline static bool IsBasicType() { return SerializeTraits<T>::IsBasicType(); }


namespace Unique
{
	template<class T>
	class SerializeTraitsBase
	{
	public:
		typedef T value_type;

		static int GetByteSize() { return sizeof(value_type); }

		inline static bool IsBasicType() { return false; }
		inline static bool IsObject() { return false; }
		inline static bool CreateObject(T& obj, const char* type) { return false; }

	};

	template<class T>
	class SerializeTraits : public SerializeTraitsBase<T>
	{
	public:
		typedef T value_type;

		template<class TransferFunction>
		inline static void Transfer(value_type& data, TransferFunction& transfer)
		{
			transfer.Transfer(data);
		}

	};

	template<class T>
	class SerializeTraitsPrimitive : public SerializeTraitsBase<T>
	{
	public:
		typedef T value_type;

		inline static bool IsBasicType() { return true; }

		template<class TransferFunction>
		inline static void Transfer(value_type& data, TransferFunction& transfer)
		{
			transfer.TransferPrimitive(data);
		}

	};

	template<class T>
	struct NonConstContainerValueType
	{
		typedef typename T::value_type value_type;
	};

}