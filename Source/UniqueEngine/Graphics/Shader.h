#pragma once
#include "Resource/Resource.h"
#include "GraphicsDefs.h"
#include "ShaderVariation.h"
#include "ShaderVariable.h"
#include <DepthStencilState.h>
#include <BlendState.h>
#include <RasterizerState.h>

namespace Unique
{
	using BlendFactor = BLEND_FACTOR;
	using BlendOperation = BLEND_OPERATION;

	class Shader;
	class PipelineState;

	class Pass
	{
	public:
		Pass(const String& name = "");
		~Pass();

		uint GetMask(Shader* shader, const String& defs);

		PipelineState* GetPipeline(Shader* shader, const String & defs);

		PipelineState* GetPipeline(Shader* shader, unsigned defMask);

		bool Prepare();

		String					name_;
		uint					passIndex_ = 0;
		BlendStateDesc			blendState_;
		DepthStencilStateDesc	depthState_;
		RasterizerStateDesc		rasterizerState_;
		Vector<LayoutElement>	inputLayout_;
		ShaderStage				shaderStage_[6];

	private:
		
		Vector<String>			allDefs_;
		uint					allMask_ = 0;
		HashMap<uint, SPtr<PipelineState>> cachedPass_;

		friend class Shader;
		friend class ShaderVariation;
		friend class PipelineState;
	};

	class Shader : public Resource
	{
		uRTTI(Shader, Resource)
	public:
		Shader();
		~Shader();

		virtual bool Prepare();
		
		virtual bool Create();

		Pass* AddPass(const String& name);

		Pass* GetShaderPass(const String & pass);
		Pass* GetShaderPass(unsigned passIndex);
		uint GetMask(const String& passName, const String& defs);

		PipelineState* GetPipeline(const String& passName, uint defMask);

		PipelineState* GetPipeline(const String& passName, const String & defs);

		static unsigned GetPassIndex(const String& passName);

		static Vector<String> SplitDef(const String& defs);

		/// Index for base pass. Initialized once GetPassIndex() has been called for the first time.
		static unsigned basePassIndex;
		/// Index for alpha pass. Initialized once GetPassIndex() has been called for the first time.
		static unsigned alphaPassIndex;
		/// Index for prepass material pass. Initialized once GetPassIndex() has been called for the first time.
		static unsigned materialPassIndex;
		/// Index for deferred G-buffer pass. Initialized once GetPassIndex() has been called for the first time.
		static unsigned deferredPassIndex;
		/// Index for per-pixel light pass. Initialized once GetPassIndex() has been called for the first time.
		static unsigned lightPassIndex;
		/// Index for lit base pass. Initialized once GetPassIndex() has been called for the first time.
		static unsigned litBasePassIndex;
		/// Index for lit alpha pass. Initialized once GetPassIndex() has been called for the first time.
		static unsigned litAlphaPassIndex;
		/// Index for shadow pass. Initialized once GetPassIndex() has been called for the first time.
		static unsigned shadowPassIndex;

	private:
		String shaderName_;
		Vector<ShaderVariable> shaderVaribles_;
		Vector<Pass> passes_;

		/// Pass index assignments.
		static HashMap<String, unsigned> passIndices;
	};

}
