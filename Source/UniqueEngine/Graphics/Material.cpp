#include "Precompiled.h"
#include "Material.h"
#include <ResourceMapping.h>

namespace Unique
{
	uObject(Material)
	{
		uFactory("Graphics");
		uAttribute("Shader", shaderRes_);
		uAttribute("ShaderDefines", shaderDefines_);
		uAttribute("test", test[0]);
	}

	Material::Material()
	{
	}

	Material::~Material()
	{
	}

	void Material::SetShader(const ResourceRef& shader)
	{
		shaderRes_ = shader;
		shader_ = GetSubsystem<ResourceCache>().GetResource<Shader>(shaderRes_.name_);
	}

}
