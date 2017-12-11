#include "Precompiled.h"
#include "JsonDeserializer.h"
#include "rapidjson/rapidjson.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/istreamwrapper.h>

namespace Unique
{
	JsonDeserializer::JsonDeserializer() : Serializer(TransferState::Reading)
	{
	}

	JsonDeserializer::~JsonDeserializer()
	{
	}

	bool JsonDeserializer::StartObject(uint size)
	{
		return true;
	}

	void JsonDeserializer::EndObject()
	{
	}

	void JsonDeserializer::TransferBin(ByteArray& data)
	{
		const char* base64 = currentNode_->GetString();
		data = FromBase64(base64, currentNode_->GetStringLength());
	}

	bool JsonDeserializer::StartDocument(const String& fileName)
	{
		document.reset(new rapidjson::Document());
		if (document->Parse<kParseCommentsFlag | kParseTrailingCommasFlag>
			(data_.data(), data_.size()).HasParseError())
		{
			UNIQUE_LOGERROR("Could not parse JSON data from " + fileName);
			return false;
		}

		currentNode_ = document.get();
		return true;
	}

	void JsonDeserializer::EndDocument()
	{
	}

	SPtr<Object> JsonDeserializer::CreateObject()
	{
		Value::MemberIterator node = currentNode_->FindMember("Type");
		if (node == currentNode_->MemberEnd())
		{
			UNIQUE_LOGWARNING("Unkown object type.");
			return nullptr;
		}

		return GetContext()->CreateObject(node->value.GetString());
	}

	bool JsonDeserializer::StartAttribute(const String& key)
	{
		Value::MemberIterator node = currentNode_->FindMember(key.CString());
		if (node == currentNode_->MemberEnd())
		{
			return false;
		}

		parentNode_.push_back(currentNode_);
		currentNode_ = &node->value;
		return true;
	}

	void JsonDeserializer::EndAttribute()
	{
		currentNode_ = parentNode_.back();
		parentNode_.pop_back();
	}

	bool JsonDeserializer::StartArray(uint& size)
	{
		if (!currentNode_->IsArray())
		{
			assert(false);
			return false;
		}

		size = currentNode_->Size();
		parentNode_.push_back(currentNode_);
		return true;
	}

	void JsonDeserializer::SetElement(uint index)
	{
		Value* parentNode = parentNode_.back();
		auto& child = (*parentNode)[index];
		currentNode_ = &child;
	}

	void JsonDeserializer::EndArray()
	{
		currentNode_ = parentNode_.back();
		parentNode_.pop_back();
	}
	
	void JsonDeserializer::TransferPrimitive(std::string& data)
	{
		data = currentNode_->GetString();
	}

	void JsonDeserializer::TransferPrimitive(String& data)
	{
		data = currentNode_->GetString();
	}

	void JsonDeserializer::TransferPrimitive(bool& data)
	{
		assert(currentNode_->IsBool());
		data = currentNode_->GetBool();
	}

	void JsonDeserializer::TransferPrimitive(char& data)
	{
		assert(currentNode_->IsInt());
		data = currentNode_->GetInt();
	}

	void JsonDeserializer::TransferPrimitive(unsigned char& data)
	{
		assert(currentNode_->IsUint());
		data = currentNode_->GetUint();
	}

	void JsonDeserializer::TransferPrimitive(short& data)
	{
		assert(currentNode_->IsInt());
		data = currentNode_->GetInt();
	}
	
	void JsonDeserializer::TransferPrimitive(unsigned short& data)
	{
		assert(currentNode_->IsUint());
		data = currentNode_->GetUint();
	}

	void JsonDeserializer::TransferPrimitive(int& data)
	{
		assert(currentNode_->IsInt());
		data = currentNode_->GetInt();
	}
	
	void JsonDeserializer::TransferPrimitive(unsigned int& data)
	{
		assert(currentNode_->IsUint());
		data = currentNode_->GetUint();
	}

	void JsonDeserializer::TransferPrimitive(long long& data)
	{
		assert(currentNode_->IsInt64());
		data = currentNode_->GetInt64();
	}

	void JsonDeserializer::TransferPrimitive(unsigned long long& data)
	{
		assert(currentNode_->IsUint64());
		data = currentNode_->GetUint64();
	}

	void JsonDeserializer::TransferPrimitive(float& data)
	{
		assert(currentNode_->IsFloat());
		data = currentNode_->GetFloat();
	}

	void JsonDeserializer::TransferPrimitive(double& data)
	{
		assert(currentNode_->IsDouble());
		data = currentNode_->GetDouble();
	}
}