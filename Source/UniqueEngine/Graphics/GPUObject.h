#pragma once
#include "GraphicsDefs.h"

namespace Unique
{
	class GPUObject
	{
	public:
		enum class State
		{
			None,
			Creating,
			Created,
			Dying,
			Dead
		};

		virtual bool Create();
		virtual void Release();
		//call in render thread
		virtual void UpdateBuffer();

		bool IsValid() const { return handle_ != nullptr; }

		template<class T>
		operator T*() { return (T*)handle_; }
		
	protected:	
		virtual bool CreateImpl();
		virtual void ReleaseImpl();

		State state_ = State::None;
		IDeviceObject* handle_ = nullptr;
	};
	

}
