#include "Precompiled.h"
#include "Graphics.h"
#include "Texture.h"
#include <iostream>
#include <Errors.h>
#include <SDL/SDL.h>

using namespace Diligent;

namespace Unique
{
	uObject(Graphics)
	{
		uFactory("Graphics")
	}
	
	IRenderDevice* renderDevice = nullptr;
	IDeviceContext* deviceContext = nullptr;
	ThreadID Graphics::renderThreadID = 0;
	int Graphics::currentContext_ = 0;
	int Graphics::currentFrame_ = 0;
	bool Graphics::singleThreaded_ = false;
	Semaphore Graphics::renderSem_;
	Semaphore Graphics::mainSem_;
	long long Graphics::waitSubmit_ = 0;
	long long Graphics::waitRender_ = 0;
	CommandQueue Graphics::comands_;

	extern void CreateDevice(SDL_Window* window, DeviceType DevType, const SwapChainDesc& SCDesc, IRenderDevice **ppRenderDevice, IDeviceContext **ppImmediateContext, ISwapChain **ppSwapChain);
	
	Graphics::Graphics() : title_("Unique Engine")
	{
		if (!GetContext()->RequireSDL(SDL_INIT_VIDEO))
		{
			UNIQUE_LOGERRORF("Couldn't initialize SDL: %s\n", SDL_GetError());
			return;
		}

		SetRenderThread();
	}

	Graphics::~Graphics()
	{
		GetContext()->ReleaseSDL();
	}

	bool Graphics::Initialize(const IntVector2& size, DeviceType deviceType)
	{
		resolution_ = size;
		deviceType_ = deviceType;

		window_ = SDL_CreateWindow(title_.CString(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			size.x_, size.y_, SDL_WINDOW_RESIZABLE);
		
		if (!window_)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s\n", SDL_GetError());
			SDL_Quit();
			return false;
		}

		SwapChainDesc SCDesc;
		SCDesc.SamplesCount = 1/*multiSampling_*/;
		SCDesc.ColorBufferFormat = srgb_ ? TEX_FORMAT_RGBA8_UNORM_SRGB : TEX_FORMAT_RGBA8_UNORM;
		SCDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
		CreateDevice(window_, deviceType, SCDesc, &renderDevice_, &deviceContext_, &swapChain_);
		
		renderDevice = renderDevice_;

		deviceContext = deviceContext_;

		renderDevice->CreateResourceMapping(ResourceMappingDesc(), &resourceMapping_);

		FrameNoRenderWait();
		
		return true;
	}

	void Graphics::Resize(const IntVector2& size)
	{
		resolution_ = size;

		ScreenMode eventData;
		eventData.width_ = size.x_;
		eventData.height_ = size.y_;
// 			eventData.fullscreen_ = fullscreen_;
// 			eventData.resizable_ = resizable_;
// 			eventData.borderless_ = borderless_;
// 			eventData.highDPI_ = highDPI_;
		SendEvent(eventData);

		uCall
		(
			swapChain_->Resize(size.x_, size.y_);
			deviceContext_->Flush();
		);
		
	}

	void Graphics::SetTitle(const String& title)
	{
		title_ = title;

		if (window_)
		{
			SDL_SetWindowTitle(window_, title_.CString());
		}
	}

	DeviceType Graphics::GetDeviceType() const
	{
		return deviceType_;
	}

	void Graphics::SetDebug(bool val)
	{
		debugger_ = val;
	}

	const IntVector2& Graphics::GetResolution() const
	{
		return resolution_;
	}

	bool Graphics::IsDirect3D() const
	{
		return (deviceType_ == DeviceType::D3D11 || deviceType_ == DeviceType::D3D12);
	}

	bool Graphics::IsOpenGL() const
	{
		return (deviceType_ == DeviceType::OpenGL || deviceType_ == DeviceType::OpenGLES);
	}

	void Graphics::AddResource(const char *Name, GPUObject* pObject, bool bIsUnique)
	{
		uCall
		(
			resourceMapping_->AddResource(Name, *pObject, bIsUnique);
		);
	}

	void Graphics::AddResource(const char *Name, IDeviceObject *pObject, bool bIsUnique)
	{
		uCall
		(
			resourceMapping_->AddResource(Name, pObject, bIsUnique);
		);
	}

	void Graphics::AddResourceArray(const char *Name, uint StartIndex, IDeviceObject* const* ppObjects, uint NumElements, bool bIsUnique)
	{
		uCall
		(
			resourceMapping_->AddResourceArray(Name, StartIndex, ppObjects, NumElements, bIsUnique);
		);
	}

	void Graphics::RemoveResourceByName(const char *Name, uint ArrayIndex)
	{
		uCall
		(
			resourceMapping_->RemoveResourceByName(Name, ArrayIndex);
		);
	}

	void Graphics::BindShaderResources(IPipelineState* pipelineState, uint flags)
	{
		pipelineState->BindShaderResources(resourceMapping_, flags);
	}

	void Graphics::BindResources(IShaderResourceBinding* shaderResourceBinding, uint shaderFlags, uint flags)
	{
		shaderResourceBinding->BindResources(shaderFlags, resourceMapping_, flags);
	}

	void Graphics::Frame()
	{
		RenderSemWait();

		//LOG_INFO_MESSAGE("Update");
		FrameNoRenderWait();
	}

	bool Graphics::IsRenderThread()
	{
		return renderThreadID == Thread::GetCurrentThreadID();
	}
	
	void Graphics::SetRenderThread()
	{
		renderThreadID = Thread::GetCurrentThreadID();
	}

	void Graphics::CreateBuffer(const BufferDesc& buffDesc, const BufferData& buffData, GraphicsBuffer& buffer)
	{
		renderDevice->CreateBuffer(buffDesc, buffData, buffer);
	}

	void Graphics::CreateShader(const ShaderCreationAttribs &creationAttribs, Diligent::IShader** shader)
	{
		renderDevice->CreateShader(creationAttribs, shader);
	}
	
	void Graphics::CreateTexture(const TextureDesc& texDesc, const TextureData &data, Texture& texture)
	{
		renderDevice->CreateTexture(texDesc, data, texture);
	}
	
	void Graphics::CreateSampler(const SamplerDesc& samDesc, ISampler **ppSampler)
	{
		renderDevice->CreateSampler(samDesc, ppSampler);
	}
	
	void Graphics::CreateResourceMapping(const ResourceMappingDesc &mappingDesc, IResourceMapping **ppMapping)
	{
		renderDevice->CreateResourceMapping(mappingDesc, ppMapping);
	}
	
	void Graphics::CreatePipelineState(const PipelineStateDesc &pipelineDesc, IPipelineState** pipelineState)
	{
		renderDevice->CreatePipelineState(pipelineDesc, pipelineState);
	}
	
	void Graphics::BeginRender()
	{
		GPUObject::UpdateBuffers();
	}

	void Graphics::EndRender()
	{
		swapChain_->Present();

		if (MainSemWait())
		{
			{
				UNIQUE_PROFILE(ExecCommands);
				ExecuteCommands(comands_);
			}

			//LOG_INFO_MESSAGE("Render");

			SwapContext();

			RenderSemPost();
		}
	}

	void Graphics::Close()
	{
		MainSemWait();
		RenderSemPost();
	}

	void Graphics::AddCommand(const std::function<void()>& cmd)
	{
		assert(Thread::IsMainThread());
		comands_.push_back(cmd);
	}

	void Graphics::ExecuteCommands(CommandQueue& cmds)
	{
		if (!cmds.empty())
		{
			for (auto& fn : cmds)
			{
				fn();
			}

			cmds.clear();
		}
	}

	
	void Graphics::SwapContext()
	{
		currentFrame_++;
		currentContext_ = 1 - currentContext_;
	//	LOG_INFO_MESSAGE("===============SwapContext : ", currentContext_);
	}

	void Graphics::FrameNoRenderWait()
	{
		//SwapContext();
		// release render thread
		MainSemPost();
	}

	void Graphics::MainSemPost()
	{
		if (!singleThreaded_)
		{
			mainSem_.post();
		}
	}

	bool Graphics::MainSemWait(int _msecs)
	{
		if (singleThreaded_)
		{
			return true;
		}

		UNIQUE_PROFILE(MainThreadWait);
		HiresTimer timer;
		bool ok = mainSem_.wait(_msecs);
		if (ok)
		{
			waitSubmit_ = timer.GetUSec(false);
			return true;
		}

		return false;
	}

	void Graphics::RenderSemPost()
	{
		if (!singleThreaded_)
		{
			renderSem_.post();
		}
	}

	void Graphics::RenderSemWait()
	{
		if (!singleThreaded_)
		{
			UNIQUE_PROFILE(RenderThreadWait);
			HiresTimer timer;
			bool ok = renderSem_.wait();
			assert(ok);
			waitRender_ = timer.GetUSec(false);
		}
	}
}

