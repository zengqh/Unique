#pragma once
#include "../Core/CoreDefs.h"
#include "../Container/Ptr.h"
#include "SerializeTraitsBasic.h"
#include "SerializeTraitsStd.h"
	

namespace Unique
{
	template<class T>
	class SerializeTraits<Unique::SPtr<T> > : public SerializeTraitsBase<Unique::SPtr<T> >
	{
	public:
		inline static const char* GetTypeName() { return T::GetTypeStatic().c_str(); }
		inline static bool IsObject() { return true; }

		inline static bool CreateObject(Unique::SPtr<T>& obj, const char* type)
		{
			obj = Unique::StaticCast<T, Unique::Object>(Unique::Object::GetContext()->CreateObject(type));
			return true;
		}

		template<class TransferFunction>
		inline static void Transfer(value_type& data, TransferFunction& transfer)
		{
			transfer.TransferObject(data);
		}

	};

	template<class T>
	class SerializeTraits<Vector<T> > : public SerializeTraitsArray<Vector<T> >
	{
	public:
		template<class TransferFunction>
		inline static void Transfer(value_type& data, TransferFunction& transfer)
		{
			transfer.TransferArray(data);
		}
	};

	template<>
	class SerializeTraits<Vector<byte>> : public SerializeTraitsArray<Vector<byte>>
	{
	public:	
		template<class TransferFunction>
		inline static void Transfer(value_type& data, TransferFunction& transfer)
		{
			transfer.TransferBin(data);
		}
	};

	template<>
	class SerializeTraits<ByteArray> : public SerializeTraitsArray<ByteArray>
	{
	public:
		template<class TransferFunction>
		inline static void Transfer(value_type& data, TransferFunction& transfer)
		{
			transfer.TransferBin(data);
		}

	};

	template<class T>
	class SerializeTraitsEnum : public SerializeTraitsBase<T>
	{
	public:
		static int GetEnum(const char* enumNames[], int count, const Unique::String& val)
		{
			for (int i = 0; i < count; i++)
			{
				if (val.Compare(enumNames[i], false) == 0)
				{
					return i;
				}
			}

			return 0;
		}

		template<class TransferFunction, int count>
		inline static void TransferEnum(value_type& data, const char*(&enumNames)[count], TransferFunction& transfer)
		{
			if (transfer.IsReading())
			{
				Unique::String str;
				transfer.TransferPrimitive(str);
				data = (value_type)GetEnum(enumNames, count, str);
			}
			else
			{
				transfer.TransferPrimitive(Unique::String(enumNames[(int)data]));
			}
		}

	};
	
	template<class T>
	class SerializeTraitsFlags : public SerializeTraitsBase<T>
	{
	public:
		template<class TransferFunction>
		inline static void TransferFlags(value_type& data, const HashMap<String, value_type>& flags, TransferFunction& transfer)
		{
			Unique::String str;
			if (transfer.IsReading())
			{
				transfer.TransferPrimitive(str);
				auto i = flags.find(str.CString());
				if (i != flags.end())
				{
					data = i->second;
				}
				else
				{
					Vector<String> strs = str.Split('|');
					for (auto& s : strs)
					{
						auto it = flags.find(s);
						if (it != flags.end())
						{
							data |= it->second;
						}
					}
				}

			}
			else
			{
				int i = 0;
				for(auto& kvp : flags)
				{
					if (data == kvp.second)
					{
						str = kvp.first;
						break;
					}
					else
					{
						if (data & kvp.second)
						{
							if (i != 0)
							{
								str.Append('|');
							}
							str.Append(kvp.first);
							i++;
						}
					}
				
				}

				transfer.TransferPrimitive(str);
			}
		}

	};
}

#define uFlagsTraits(CLASS, ...)\
template<>\
class SerializeTraits<CLASS> : public SerializeTraitsFlags<CLASS>\
{\
public:\
	template<class TransferFunction>\
	inline static void Transfer(value_type& data, TransferFunction& transfer)\
	{\
		static HashMap<String, value_type> flags = \
		{\
			__VA_ARGS__\
		};\
		TransferFlags<TransferFunction>(data, flags, transfer); \
	}\
};

#define uEnumTraits(CLASS, ...)\
template<>\
class SerializeTraits<CLASS> : public SerializeTraitsEnum<CLASS>\
{\
public:\
	template<class TransferFunction>\
	inline static void Transfer(value_type& data, TransferFunction& transfer)\
	{\
		static const char* enumNames[] = \
		{\
				__VA_ARGS__\
		}; \
		TransferEnum<TransferFunction>(data, enumNames, transfer); \
	}\
};

#define uClassTraits(CLASS, ...)\
template<>\
class SerializeTraits<CLASS> : public SerializeTraitsBase<CLASS>\
{\
public:\
	template<class TransferFunction>\
	inline static void Transfer(value_type& self, TransferFunction& transfer)\
	{\
transfer.TransferAttributes(##__VA_ARGS__);\
	}\
};
