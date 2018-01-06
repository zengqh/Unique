
#ifdef COMPILEVS

// Vertex shader uniforms
cbuffer FrameVS
{
    float cDeltaTime;
    float cElapsedTime;
}

cbuffer CameraVS
{  
	float4x3 cView;
    float4x3 cViewInv;
    float4x4 cViewProj;
    float3 cCameraPos;
    float cNearClip;
    float cFarClip;
    float3 cFrustumSize;
    float4 cDepthMode;
    float4 cGBufferOffsets;
    float4 cClipPlane;
}

cbuffer ZoneVS
{
    float3 cAmbientStartColor;
    float3 cAmbientEndColor;
    float4x3 cZone;
}

cbuffer LightVS
{
    float4 cLightPos;
    float3 cLightDir;
    float4 cNormalOffsetScale;
#ifdef NUMVERTEXLIGHTS
    float4 cVertexLights[4 * 3];
#else
    float4x4 cLightMatrices[4];
#endif
}

#ifndef CUSTOM_MATERIAL_CBUFFER
cbuffer MaterialVS
{
    float4 cUOffset;
    float4 cVOffset;
}
#endif

cbuffer ObjectVS
{
    float4x3 cModel;
}

cbuffer SkinnedVS
{
    uniform float4x3 cSkinMatrices[MAXBONES];
}

cbuffer BillboardVS
{
	float4x3 world_;
	float3x3 billboardRot;
}

#endif

#ifdef COMPILEPS

// Pixel shader uniforms
cbuffer FramePS
{
    float cDeltaTimePS;
    float cElapsedTimePS;
}

cbuffer CameraPS
{
    float3 cCameraPosPS;
    float4 cDepthReconstruct;
    float2 cGBufferInvSize;
    float cNearClipPS;
    float cFarClipPS;
}

cbuffer ZonePS
{
    float4 cAmbientColor;
    float4 cFogParams;
    float3 cFogColor;
    float3 cZoneMin;
    float3 cZoneMax;
}

cbuffer LightPS
{
    float4 cLightColor;
    float4 cLightPosPS;
    float3 cLightDirPS;
    float4 cNormalOffsetScalePS;
    float4 cShadowCubeAdjust;
    float4 cShadowDepthFade;
    float2 cShadowIntensity;
    float2 cShadowMapInvSize;
    float4 cShadowSplits;
    float2 cVSMShadowParams;
    float4x4 cLightMatricesPS[4];
    #ifdef PBR
        float cLightRad;
        float cLightLength;
    #endif
}

#ifndef CUSTOM_MATERIAL_CBUFFER
cbuffer MaterialPS
{
    float4 cMatDiffColor;
    float3 cMatEmissiveColor;
    float3 cMatEnvMapColor;
    float4 cMatSpecColor;
	float cRoughness;
	float cMetallic;
}
#endif

#endif

