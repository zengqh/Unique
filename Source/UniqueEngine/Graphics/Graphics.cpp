#include "Precompiled.h"
#include "Graphics.h"
#include "Texture.h"
#include "Buffers/VertexBuffer.h"
#include "Buffers/IndexBuffer.h"
#include "GraphicsContext.h"
#include <iostream>

namespace Unique
{
	uObject(Graphics)
	{
		uFactory("Graphics")
	}
	
	IRenderDevice* renderDevice = nullptr;
	IDeviceContext* deviceContext = nullptr;

	Graphics::Graphics()
	{
	}

	Graphics::~Graphics()
	{
	}

	bool Graphics::Initialize(const std::string& rendererModule, const IntVector2& size)
	{


		GraphicsContext::FrameNoRenderWait();

		return true;
	}

	void Graphics::Resize(const IntVector2& size)
	{
		auto fn = [=]()
		{/*
			auto videoMode = renderContext->GetVideoMode();

			// Update video mode
			videoMode.resolution = (LLGL::Size&)size;
			renderContext->SetVideoMode(videoMode);

			SetRenderTarget(nullptr);

			SetViewport(Viewport(0, 0, (float)size.x_, (float)size.y_));

			// Update scissor
			SetScissor({ 0, 0, videoMode.resolution.x, videoMode.resolution.y });*/
		};

		if (Thread::IsMainThread())
		{
			GraphicsContext::AddCommand(fn);
		}
		else
		{
			fn();
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
	
	void Graphics::BeginFrame()
	{
		GraphicsContext::BeginFrame();
	}

	void Graphics::EndFrame()
	{
		GraphicsContext::EndFrame();
	}

	void Graphics::BeginRender()
	{
		GraphicsContext::BeginRender();
	}

	void Graphics::EndRender()
	{
	//	renderContext->Present();
		
		GraphicsContext::EndRender();
	}

	void Graphics::Clear(uint flags)
	{
	//	commands->Clear(flags);
	}
	/*
	void Graphics::SetRenderTarget(RenderTarget* renderTarget)
	{
		if (renderTarget)
		{
			commands->SetRenderTarget(*renderTarget);
		}
		else
		{
			commands->SetRenderTarget(*renderContext);
		}
	}

	void Graphics::SetViewport(const Viewport& viewport)
	{
		commands->SetViewport(viewport);
	}

	void Graphics::SetScissor(const Scissor& scissor)
	{
		commands->SetScissor(scissor);
	}

	void Graphics::SetGraphicsPipeline(GraphicsPipeline* graphicsPipeline)
	{
		commands->SetGraphicsPipeline(*graphicsPipeline);
	}

	void Graphics::SetComputePipeline(ComputePipeline* computePipeline)
	{
		commands->SetComputePipeline(*computePipeline);
	}

	void Graphics::SetVertexBuffer(VertexBuffer* buffer)
	{
		commands->SetVertexBuffer(*buffer);
	}

	void Graphics::SetVertexBuffers(BufferArray* buffers)
	{
		commands->SetVertexBufferArray(*buffers);
	}

	void Graphics::SetIndexBuffer(IndexBuffer* buffer)
	{
		commands->SetIndexBuffer(*buffer);
	}

	void Graphics::SetTexture(Texture* texture, uint slot, long shaderStageFlags)
	{
	//	commands->SetTexture(*texture, slot, shaderStageFlags);
	//	commands->SetSampler(texture->sampler(), slot);
	}*/

	void Graphics::Draw(unsigned int numVertices, unsigned int firstVertex)
	{
	//	commands->Draw(numVertices, firstVertex);
	}

	void Graphics::DrawIndexed(unsigned int numIndexes, unsigned int firstIndex)
	{
	//	commands->DrawIndexed(numIndexes, firstIndex);
	}

	void Graphics::DrawIndexed(unsigned int numIndexes, unsigned int firstIndex, int vertexOffset)
	{
	//	commands->DrawIndexed(numIndexes, firstIndex, vertexOffset);
	}

	void Graphics::DrawInstanced(unsigned int numVertices, unsigned int firstVertex, unsigned int numInstances)
	{
	//	commands->DrawInstanced(numVertices, firstVertex, numInstances);
	}

	void Graphics::DrawInstanced(unsigned int numVertices, unsigned int firstVertex, unsigned int numInstances, unsigned int instanceOffset)
	{
	//	commands->DrawInstanced(numVertices, firstVertex, numInstances, instanceOffset);
	}

	void Graphics::DrawIndexedInstanced(unsigned int numVerticesPerInstance, unsigned int numInstances, unsigned int firstIndex)
	{
	//	commands->DrawIndexedInstanced(numVerticesPerInstance, numInstances, firstIndex);
	}

	void Graphics::DrawIndexedInstanced(unsigned int numVertices, unsigned int numInstances, unsigned int firstIndex, int vertexOffset)
	{
	//	commands->DrawIndexedInstanced(numVertices, numInstances, firstIndex, vertexOffset);
	}

	void Graphics::DrawIndexedInstanced(unsigned int numVertices, unsigned int numInstances, unsigned int firstIndex, int vertexOffset, unsigned int instanceOffset)
	{
	//	commands->DrawIndexedInstanced(numVertices, numInstances, firstIndex, vertexOffset, instanceOffset);
	}

	void Graphics::Dispatch(unsigned int groupSizeX, unsigned int groupSizeY, unsigned int groupSizeZ)
	{
	//	commands->Dispatch(groupSizeX, groupSizeY, groupSizeZ);
	}

	void Graphics::Close()
	{
		GraphicsContext::Stop();
	}

}

