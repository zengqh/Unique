#include "UniquePCH.h"
#include "Core/WorkQueue.h"
#include "../Scene/Scene.h"
#include "../Graphics/GraphicsBuffer.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/Octree.h"
#include "../Graphics/OctreeQuery.h"
#include "../Graphics/Camera.h"
#include "../Graphics/DebugRenderer.h"
#include "Viewport.h"
#include "View.h"
#include "Batch.h"
#include "RenderPath.h"
#include <Errors.h>

namespace Unique
{
	static const Vector3* directions[] =
	{
		&Vector3::RIGHT,
		&Vector3::LEFT,
		&Vector3::UP,
		&Vector3::DOWN,
		&Vector3::FORWARD,
		&Vector3::BACK
	};

	/// %Frustum octree query for shadowcasters.
	class ShadowCasterOctreeQuery : public FrustumOctreeQuery
	{
	public:
		/// Construct with frustum and query parameters.
		ShadowCasterOctreeQuery(PODVector<Drawable*>& result, const Frustum& frustum, unsigned char drawableFlags = DRAWABLE_ANY,
			unsigned viewMask = DEFAULT_VIEWMASK) :
			FrustumOctreeQuery(result, frustum, drawableFlags, viewMask)
		{
		}

		/// Intersection test for drawables.
		virtual void TestDrawables(Drawable** start, Drawable** end, bool inside)
		{
			while (start != end)
			{
				Drawable* drawable = *start++;

				if (drawable->GetCastShadows() && (drawable->GetDrawableFlags() & drawableFlags_) &&
					(drawable->GetViewMask() & viewMask_))
				{
					if (inside || frustum_.IsInsideFast(drawable->GetWorldBoundingBox()))
						result_.push_back(drawable);
				}
			}
		}
	};
	
	/// %Frustum octree query for zones and occluders.
	class ZoneOctreeQuery : public FrustumOctreeQuery
	{
	public:
		/// Construct with frustum and query parameters.
		ZoneOctreeQuery(View* view,
			PODVector<Drawable*>& result, const Frustum& frustum, unsigned char drawableFlags = DRAWABLE_ANY,
			unsigned viewMask = DEFAULT_VIEWMASK) :
			FrustumOctreeQuery(result, frustum, drawableFlags, viewMask), view_(view)
		{
			Node* cameraNode = view_->cullCamera_->GetNode();
			cameraPos = cameraNode->GetWorldPosition();
		}

		/// Intersection test for drawables.
		virtual void TestDrawables(Drawable** start, Drawable** end, bool inside) override
		{
			while (start != end)
			{
				Drawable* drawable = *start++;

				if(drawable->GetViewMask() & viewMask_)
				{
					unsigned char flags = drawable->GetDrawableFlags();

					if (flags == DRAWABLE_ZONE)
					{
						if (inside || frustum_.IsInsideFast(drawable->GetWorldBoundingBox()))
						{	
							Zone* zone = (Zone*)drawable;
							view_->zones_.push_back(zone);
							int priority = zone->GetPriority();
							if (priority > view_->highestZonePriority_)
								view_->highestZonePriority_ = priority;
							if (priority > bestPriority && zone->IsInside(cameraPos))
							{
								view_->cameraZone_ = zone;
								bestPriority = priority;
							}
						}

						  
					}
					else if (flags & drawableFlags_)
					{
						if (inside || frustum_.IsInsideFast(drawable->GetWorldBoundingBox()))
							result_.push_back(drawable);
					}
				}

			}
		}
		
		/// Zone vector reference.
		View* view_;
		Vector3 cameraPos;
		int bestPriority = M_MIN_INT;
	};

	void CheckVisibilityWork(const WorkItem* item, unsigned threadIndex)
	{
		View* view = reinterpret_cast<View*>(item->aux_);
		Drawable** start = reinterpret_cast<Drawable**>(item->start_);
		Drawable** end = reinterpret_cast<Drawable**>(item->end_);
		const Matrix3x4& viewMatrix = view->cullCamera_->GetView();
		Vector3 viewZ = Vector3(viewMatrix.m20_, viewMatrix.m21_, viewMatrix.m22_);
		Vector3 absViewZ = viewZ.Abs();
		unsigned cameraViewMask = view->cullCamera_->GetViewMask();
		bool cameraZoneOverride = view->cameraZoneOverride_;
		PerThreadSceneResult& result = view->sceneResults_[threadIndex];

		while (start != end)
		{
			Drawable* drawable = *start++;

			//if (!drawable->IsOccludee())
			{
				drawable->UpdateBatches(view->frame_);
				// If draw distance non-zero, update and check it
				float maxDistance = drawable->GetDrawDistance();
				if (maxDistance > 0.0f)
				{
					if (drawable->GetDistance() > maxDistance)
						continue;
				}

				drawable->MarkInView(view->frame_);

				// For geometries, find zone, clear lights and calculate view space Z range
				if (drawable->GetDrawableFlags() & DRAWABLE_GEOMETRY)
				{             
					Zone* drawableZone = drawable->GetZone();
					if (!cameraZoneOverride &&
						(drawable->IsZoneDirty() || !drawableZone || (drawableZone->GetViewMask() & cameraViewMask) == 0))
						view->FindZone(drawable);

					const BoundingBox& geomBox = drawable->GetWorldBoundingBox();
					Vector3 center = geomBox.Center();
					Vector3 edge = geomBox.Size() * 0.5f;

					// Do not add "infinite" objects like skybox to prevent shadow map focusing behaving erroneously
					if (edge.LengthSquared() < M_LARGE_VALUE * M_LARGE_VALUE)
					{
						float viewCenterZ = viewZ.DotProduct(center) + viewMatrix.m23_;
						float viewEdgeZ = absViewZ.DotProduct(edge);
						float minZ = viewCenterZ - viewEdgeZ;
						float maxZ = viewCenterZ + viewEdgeZ;
						drawable->SetMinMaxZ(viewCenterZ - viewEdgeZ, viewCenterZ + viewEdgeZ);
						result.minZ_ = Min(result.minZ_, minZ);
						result.maxZ_ = Max(result.maxZ_, maxZ);
					}
					else
						drawable->SetMinMaxZ(M_LARGE_VALUE, M_LARGE_VALUE);

					result.geometries_.push_back(drawable);
				}
				else if (drawable->GetDrawableFlags() & DRAWABLE_LIGHT)
				{
					Light* light = static_cast<Light*>(drawable);
					// Skip lights with zero brightness or black color
					if (!light->GetEffectiveColor().Equals(Color::BLACK))
					{
						if(light->GetLightType() == LIGHT_DIRECTIONAL)
							result.dirLights_.push_back(light);
						if(light->GetLightType() == LIGHT_POINT)
							result.pointLights_.push_back(light);
						if(light->GetLightType() == LIGHT_SPOT)
							result.spotLights_.push_back(light);
					}
				}
			}
		}
	}

	void ProcessLightWork(const WorkItem* item, unsigned threadIndex)
	{
		View* view = reinterpret_cast<View*>(item->aux_);
		LightQueryResult* query = reinterpret_cast<LightQueryResult*>(item->start_);

		view->ProcessLight(*query, threadIndex);
	}

	void UpdateDrawableGeometriesWork(const WorkItem* item, unsigned threadIndex)
	{
		const FrameInfo& frame = *(reinterpret_cast<FrameInfo*>(item->aux_));
		Drawable** start = reinterpret_cast<Drawable**>(item->start_);
		Drawable** end = reinterpret_cast<Drawable**>(item->end_);

		while (start != end)
		{
			Drawable* drawable = *start++;
			// We may leave null pointer holes in the queue if a drawable is found out to require a main thread update
			if (drawable)
				drawable->UpdateGeometry(frame);
		}
	}

	void SortBatchQueueFrontToBackWork(const WorkItem* item, unsigned threadIndex)
	{
		BatchQueue* queue = reinterpret_cast<BatchQueue*>(item->start_);

		queue->SortFrontToBack();
	}

	void SortBatchQueueBackToFrontWork(const WorkItem* item, unsigned threadIndex)
	{
		BatchQueue* queue = reinterpret_cast<BatchQueue*>(item->start_);

		queue->SortBackToFront();
	}

	void SortLightQueueWork(const WorkItem* item, unsigned threadIndex)
	{
		LightBatchQueue* start = reinterpret_cast<LightBatchQueue*>(item->start_);
		start->litBaseBatches_.SortFrontToBack();
		start->litBatches_.SortFrontToBack();
	}

	void SortShadowQueueWork(const WorkItem* item, unsigned threadIndex)
	{
		LightBatchQueue* start = reinterpret_cast<LightBatchQueue*>(item->start_);
		for (unsigned i = 0; i < start->shadowSplits_.size(); ++i)
			start->shadowSplits_[i].shadowBatches_.SortFrontToBack();
	}

	View::View()
		: graphics_(GetSubsystem<Graphics>()),
		renderer_(GetSubsystem<Renderer>()), 
		geometriesUpdated_(false),
		minInstances_(2),
		cameraZone_(nullptr),
		farClipZone_(nullptr)
	{
		unsigned numThreads = GetSubsystem<WorkQueue>().GetNumThreads() + 1; // Worker threads + main thread
		tempDrawables_.resize(numThreads);
		sceneResults_.resize(numThreads);
		frame_.camera_ = nullptr;
		
		frameUniform_ = graphics_.AddUniform<FrameParameter>();
		cameraVS_ = graphics_.AddUniform<CameraVS>();
		objectVS_ = graphics_.AddUniform<ObjectVS>();
		skinnedVS_ = graphics_.AddUniform<SkinnedVS>();
		billboardVS_ = graphics_.AddUniform<BillboardVS>();
		materialVS_ = graphics_.AddUniform<MaterialVS>();

		cameraPS_ = graphics_.AddUniform<CameraPS>();
		zonePS_ = graphics_.AddUniform<ZonePS>();
		lightPS_ = graphics_.AddUniform<LightPS>();
		materialPS_ = graphics_.AddUniform<MaterialPS>();

		tempDrawables_.resize(1);

		batchMatrics_[0].reserve(2048);
		batchMatrics_[1].reserve(2048);
	}

	View::~View()
	{
	}

	bool View::Define(TextureView* renderTarget, Unique::Viewport* viewport)
	{
 		renderPath_ = viewport->GetRenderPath();
 		if (!renderPath_)
 			return false;

		renderTarget_ = renderTarget;
		drawDebug_ = viewport->GetDrawDebug();

		// Validate the rect and calculate size. If zero rect, use whole rendertarget size
		int rtWidth = renderTarget ? renderTarget->GetWidth() : graphics_.GetWidth();
		int rtHeight = renderTarget ? renderTarget->GetHeight() : graphics_.GetHeight();
		const IntRect& rect = viewport->GetRect();

		if (rect != IntRect::ZERO)
		{
			viewRect_.left_ = Clamp(rect.left_, 0, rtWidth - 1);
			viewRect_.top_ = Clamp(rect.top_, 0, rtHeight - 1);
			viewRect_.right_ = Clamp(rect.right_, viewRect_.left_ + 1, rtWidth);
			viewRect_.bottom_ = Clamp(rect.bottom_, viewRect_.top_ + 1, rtHeight);
		}
		else
			viewRect_ = IntRect(0, 0, rtWidth, rtHeight);

		viewSize_ = viewRect_.Size();
		rtSize_ = IntVector2(rtWidth, rtHeight);

		// On OpenGL flip the viewport if rendering to a texture for consistent UV addressing with Direct3D9
		if (graphics_.IsOpenGL())
		{
			if (renderTarget_)
			{
				viewRect_.bottom_ = rtHeight - viewRect_.top_;
				viewRect_.top_ = viewRect_.bottom_ - viewSize_.y_;
			}
		}

		scene_ = viewport->GetScene();
		cullCamera_ = viewport->GetCullCamera();
		camera_ = viewport->GetCamera();
		if (!cullCamera_)
			cullCamera_ = camera_;

		auto& scenePasses = MainContext(scenePasses_);
		hasScenePasses_ = false;
		scenePasses.clear();
		geometriesUpdated_ = false;
		
		auto& batchQueues = MainContext(batchQueues_);

		auto& renderPass = MainContext(renderPasses_);
		// Make sure that all necessary batch queues exist
		for (size_t i = 0; i < renderPath_->commands_.size(); ++i)
		{
			RenderPass& command = *renderPath_->commands_[i];
			if (!command.enabled_)
				continue;

			if (command.type_ == PASS_SCENE)
			{
				hasScenePasses_ = true;

				ScenePassInfo info;
				info.passIndex_ = command.passIndex_ = ShaderUtil::GetPassIndex(command.pass_);
				info.allowInstancing_ = command.sortMode_ != SORT_BACKTOFRONT;
				info.sortMode_ = command.sortMode_;

				auto j = batchQueues.find(info.passIndex_);
				if (j == batchQueues.end())
				{
					auto k = batchQueues.insert(Pair<unsigned, BatchQueue>(info.passIndex_, BatchQueue()));
					info.batchQueue_ = &(k.first->second);
				}
				else
				{
					info.batchQueue_ = &j->second;
				}

				scenePasses.push_back(info);

			}

			renderPass.push_back(renderPath_->commands_[i]);
		}

		MainContext(batchMatrics_).clear();
		matricsToOffset_.clear();

		octree_ = nullptr;
		// Get default zone first in case we do not have zones defined
		cameraZone_ = farClipZone_ = renderer_.GetDefaultZone();

		if (hasScenePasses_)
		{
			if (!scene_ || !camera_ || !camera_->IsEnabledEffective())
				return false;
			
			octree_ = scene_->GetComponent<Octree>();
			if (!octree_)
				return false;

			if (!camera_->IsProjectionValid())
				return false;
		}

		return true;
	}

	void View::Update(const FrameInfo& frame)
	{
		frame_.camera_ = camera_;
		frame_.timeStep_ = frame.timeStep_;
		frame_.frameNumber_ = frame.frameNumber_;
		frame_.viewSize_ = viewSize_;

		int maxSortedInstances = renderer_.GetMaxSortedInstances();

	//	renderTargets_.Clear();
		geometries_.clear();
		dirLights_.clear();
		pointLights_.clear();
		spotLights_.clear();
		zones_.clear();
		
		auto& batchQueues = MainContext(batchQueues_);

		for (auto i = batchQueues.begin(); i != batchQueues.end(); ++i)
			i->second.Clear(maxSortedInstances);

		if (hasScenePasses_ && (!camera_ || !octree_))
		{
		//	SendViewEvent(E_ENDVIEWUPDATE);
			return;
		}

		if (camera_ && camera_->GetAutoAspectRatio())
			camera_->SetAspectRatioInternal((float)frame_.viewSize_.x_ / (float)frame_.viewSize_.y_);
				
		GetDrawables();

		GetBatches();

 		LOG_RENDER("Update : ", Graphics::currentContext_);

	}

	void View::PostUpdate()
	{
		UpdateGeometries();

		PrepareInstancingBuffer();

		SetGlobalShaderParameters();

		SetCameraShaderParameters(camera_);

	}

	void View::DrawDebug()
	{
		// Draw the associated debug geometry now if enabled
		if (drawDebug_ && octree_ && camera_)
		{
			DebugRenderer* debug = octree_->GetComponent<DebugRenderer>();
			if (debug && debug->IsEnabledEffective() && debug->HasContent())
			{
				debug->UpdateBatches(frame_);

				const Vector<SourceBatch>& batches = debug->GetBatches();
				bool vertexLightsProcessed = false;

				auto& scenePasses = MainContext(scenePasses_);

				for (unsigned j = 0; j < batches.size(); ++j)
				{
					const SourceBatch& srcBatch = batches[j];
					Shader* shader = srcBatch.material_->GetShader();
					if (!srcBatch.geometry_ || !srcBatch.numWorldTransforms_ || !shader)
						continue;

					// Check each of the scene passes
					for (unsigned k = 0; k < scenePasses.size(); ++k)
					{
						ScenePassInfo& info = scenePasses[k];
						// Skip forward base pass if the corresponding litbase pass already exists
						//if (info.passIndex_ == basePassIndex_ && j < 32 && drawable->HasBasePass(j))
						//	continue;

						Pass* pass = srcBatch.material_->GetPass(info.passIndex_);
						if (!pass)
							continue;

						Batch destBatch(srcBatch);
						destBatch.pass_ = info.passIndex_;
						AddBatchToQueue(*info.batchQueue_, destBatch, info.passIndex_, info.allowInstancing_);
					}
				}

			}
		}

	}

	void View::Render()
	{		
		auto& passes = RenderContext(renderPasses_);

		ClearParameterSources();

		for(int i = 0; i < passes.size(); i++)
		{
			passes[i]->Render(this);
		}

		passes.clear();

		LOG_RENDER("Render : ", Graphics::GetRenderContext());
	}

	
	bool View::NeedParameterUpdate(ShaderParameterGroup group, const void* source)
	{
		if ((unsigned)(size_t)shaderParameterSources_[group] == M_MAX_UNSIGNED || shaderParameterSources_[group] != source)
		{
			shaderParameterSources_[group] = source;
			return true;
		}
		else
			return false;
	}

	void View::ClearParameterSource(ShaderParameterGroup group)
	{
		shaderParameterSources_[group] = (const void*)(uint64)M_MAX_UNSIGNED;
	}

	void View::ClearParameterSources()
	{
		for (unsigned i = 0; i < MAX_SHADER_PARAMETER_GROUPS; ++i)
			shaderParameterSources_[i] = (const void*)(uint64)M_MAX_UNSIGNED;
	}

	void View::ClearTransformSources()
	{
		shaderParameterSources_[SP_CAMERA] = (const void*)(uint64)M_MAX_UNSIGNED;
		shaderParameterSources_[SP_OBJECT] = (const void*)(uint64)M_MAX_UNSIGNED;
	}

	void View::SetGlobalShaderParameters()
	{
		FrameParameter frameParam;
		frameParam.deltaTime_ = frame_.timeStep_;

		if (scene_)
		{
			float elapsedTime = scene_->GetElapsedTime();
			frameParam.elapsedTime_ = elapsedTime;
		}

		frameUniform_->SetData(frameParam);
		
		if(dirLights_.size() > 0)
		{
			Light* light = dirLights_[0];
			Node* lightNode = light->GetNode();
            float atten = 1.0f / Max(light->GetRange(), M_EPSILON);
            Vector3 lightDir(lightNode->GetWorldRotation() * Vector3::BACK);
            Vector4 lightPos(lightNode->GetWorldPosition(), atten);
			       
			float fade = 1.0f;
            float fadeEnd = light->GetDrawDistance();
            float fadeStart = light->GetFadeDistance();
            // Do fade calculation for light if both fade & draw distance defined
            if (light->GetLightType() != LIGHT_DIRECTIONAL && fadeEnd > 0.0f && fadeStart > 0.0f && fadeStart < fadeEnd)
                fade = Min(1.0f - (light->GetDistance() - fadeStart) / (fadeEnd - fadeStart), 1.0f);
			
			Color lightColor = Color(light->GetEffectiveColor().Abs(), light->GetEffectiveSpecularIntensity()) * fade;
			LightPS* lightPS = (LightPS*)lightPS_->Lock();
			lightPS->LightColor = (float4&)lightColor;
			lightPS->LightDirPS = lightDir;
			lightPS->LightPosPS = lightPos;
			lightPS_->Unlock();
		}
	}

	void View::SetCameraShaderParameters(Camera* camera)
	{
		if (!camera)
			return;

		float nearClip = camera->GetNearClip();
		float farClip = camera->GetFarClip();
		Matrix3x4 cameraEffectiveTransform = camera->GetEffectiveWorldTransform();

		{
			Vector4 depthMode = Vector4::ZERO;
			if (camera->IsOrthographic())
			{
				depthMode.x_ = 1.0f;
				if (graphics_.IsOpenGL())
				{
					depthMode.z_ = 0.5f;
					depthMode.w_ = 0.5f;
				}
				else
				{
					depthMode.z_ = 1.0f;
				}

			}
			else
				depthMode.w_ = 1.0f / camera->GetFarClip();

			Vector3 nearVector, farVector;
			camera->GetFrustumSize(nearVector, farVector);

			Matrix4 projection = camera->GetGPUProjection();

			if (graphics_.IsOpenGL())
			{
				// Add constant depth bias manually to the projection matrix due to glPolygonOffset() inconsistency
				//float constantBias = 2.0f * graphics_->GetDepthConstantBias();
				//projection.m22_ += projection.m32_ * constantBias;
				//projection.m23_ += projection.m33_ * constantBias;
			}
			
			CameraVS* cameraParam = (CameraVS*)cameraVS_->Lock();
			cameraParam->cameraPos_ = cameraEffectiveTransform.Translation();
			cameraParam->nearClip_ = nearClip;
			cameraParam->farClip_ = farClip;
			cameraParam->depthMode_ = depthMode;
			cameraParam->frustumSize_ = farVector;
			cameraParam->view_ = camera->GetView();
			cameraParam->viewInv_ = cameraEffectiveTransform;
			cameraParam->viewProj_ = projection * camera->GetView();

			cameraVS_->Unlock();
		}

		{
			CameraPS* cameraParam = (CameraPS*)cameraPS_->Lock();
			cameraParam->cameraPosPS_ = cameraEffectiveTransform.Translation();
			cameraParam->nearClipPS_ = nearClip;
			cameraParam->farClipPS_ = farClip;

			Vector4 depthReconstruct
			(farClip / (farClip - nearClip), -nearClip / (farClip - nearClip), camera->IsOrthographic() ? 1.0f : 0.0f,
				camera->IsOrthographic() ? 0.0f : 1.0f);
			cameraParam->depthReconstruct_ = depthReconstruct;
			cameraPS_->Unlock();
		}

		{
			ObjectVS* data = (ObjectVS*)objectVS_->Lock();
			data->world_ = Matrix3x4::IDENTITY;
			objectVS_->Unlock();
		}

		{
			MaterialPS* data = (MaterialPS*)materialPS_->Lock();
			data->matDiffColor = float4(1,1,1,1);
			materialPS_->Unlock();
		}
	}

	void View::SetZoneShaderParameters(Zone* zone)
	{
		const BoundingBox& box = zone->GetBoundingBox();
		Vector3 boxSize = box.Size();
		Matrix3x4 adjust(Matrix3x4::IDENTITY);
		adjust.SetScale(Vector3(1.0f / boxSize.x_, 1.0f / boxSize.y_, 1.0f / boxSize.z_));
		adjust.SetTranslation(Vector3(0.5f, 0.5f, 0.5f));
		Matrix3x4 zoneTransform = adjust * zone->GetInverseWorldTransform();
		//	graphics->SetShaderParameter(VSP_ZONE, zoneTransform);

		ZonePS* zonePS = (ZonePS*)zonePS_->Map();
		zonePS->ambientColor = zone->GetAmbientColor();	
		zonePS->fogColor = /*overrideFogColorToBlack ? Color::BLACK :*/ zone->GetFogColor();
		zonePS->zoneMin = zone->GetBoundingBox().min_;
		zonePS->zoneMax = zone->GetBoundingBox().max_;
	
		float farClip = camera_->GetFarClip();
		float fogStart = Min(zone->GetFogStart(), farClip);
		float fogEnd = Min(zone->GetFogEnd(), farClip);
		if (fogStart >= fogEnd * (1.0f - M_LARGE_EPSILON))
			fogStart = fogEnd * (1.0f - M_LARGE_EPSILON);
		float fogRange = Max(fogEnd - fogStart, M_EPSILON);
		Vector4 fogParams(fogEnd / farClip, farClip / fogRange, 0.0f, 0.0f);

		Node* zoneNode = zone->GetNode();
		if (zone->GetHeightFog() && zoneNode)
		{
			Vector3 worldFogHeightVec = zoneNode->GetWorldTransform() * Vector3(0.0f, zone->GetFogHeight(), 0.0f);
			fogParams.z_ = worldFogHeightVec.y_;
			fogParams.w_ = zone->GetFogHeightScale() / Max(zoneNode->GetWorldScale().y_, M_EPSILON);
		}

		zonePS->fogParams = fogParams;
		zonePS_->UnMap();
		//	graphics->SetShaderParameter(PSP_FOGPARAMS, fogParams);
	}

	void View::GetDrawables()
	{
		if (!octree_ || !cullCamera_)
			return;

		UNIQUE_PROFILE(GetDrawables);

		auto& queue = GetSubsystem<WorkQueue>();
		
		highestZonePriority_ = M_MIN_INT;
		int bestPriority = M_MIN_INT;
		Node* cameraNode = cullCamera_->GetNode();
		Vector3 cameraPos = cameraNode->GetWorldPosition();

		PODVector<Drawable*>& tempDrawables = tempDrawables_[0];
		ZoneOctreeQuery query(this, tempDrawables, cullCamera_->GetFrustum(), DRAWABLE_ZONE | DRAWABLE_GEOMETRY | DRAWABLE_LIGHT, cullCamera_->GetViewMask());
		octree_->GetDrawables(query);

		// Determine the zone at far clip distance. If not found, or camera zone has override mode, use camera zone
		cameraZoneOverride_ = cameraZone_->GetOverride();
		if (!cameraZoneOverride_)
		{
			Vector3 farClipPos = cameraPos + cameraNode->GetWorldDirection() * Vector3(0.0f, 0.0f, cullCamera_->GetFarClip());
			bestPriority = M_MIN_INT;

			for (auto i = zones_.begin(); i != zones_.end(); ++i)
			{
				int priority = (*i)->GetPriority();
				if (priority > bestPriority && (*i)->IsInside(farClipPos))
				{
					farClipZone_ = *i;
					bestPriority = priority;
				}
			}
		}
		
		if (farClipZone_ == renderer_.GetDefaultZone())
			farClipZone_ = cameraZone_;
		
		// Check drawable occlusion, find zones for moved drawables and collect geometries & lights in worker threads
		if(tempDrawables.size() > 0)
		{
			for (unsigned i = 0; i < sceneResults_.size(); ++i)
			{
				PerThreadSceneResult& result = sceneResults_[i];
				result.geometries_.clear();
				result.dirLights_.clear();
				result.pointLights_.clear();
				result.spotLights_.clear();
				result.minZ_ = M_INFINITY;
				result.maxZ_ = 0.0f;
			}

			int numWorkItems = queue.GetNumThreads() + 1; // Worker threads + main thread
			int drawablesPerItem = (int)tempDrawables.size() / numWorkItems;

			auto start = &tempDrawables.front();
			// Create a work item for each thread
			for (int i = 0; i < numWorkItems; ++i)
			{
				SPtr<WorkItem> item = queue.GetFreeItem();
				item->priority_ = M_MAX_UNSIGNED;
				item->workFunction_ = CheckVisibilityWork;
				item->aux_ = this;

				auto end = &tempDrawables.back() + 1;
				if (i < numWorkItems - 1 && end - start > drawablesPerItem)
					end = start + drawablesPerItem;

				item->start_ = start;
				item->end_ = end;
				queue.AddWorkItem(item);

				start = end;
			}

			queue.Complete(M_MAX_UNSIGNED);
		}

		// Combine lights, geometries & scene Z range from the threads
		geometries_.clear();
		dirLights_.clear();
		pointLights_.clear();
		spotLights_.clear();
		minZ_ = M_INFINITY;
		maxZ_ = 0.0f;

		if (sceneResults_.size() > 1)
		{
			for (unsigned i = 0; i < sceneResults_.size(); ++i)
			{
				PerThreadSceneResult& result = sceneResults_[i];
				
				geometries_.insert(geometries_.end(), result.geometries_.begin(), result.geometries_.end());
				
				if(!result.dirLights_.empty())
					dirLights_.insert(dirLights_.end(), result.dirLights_.begin(), result.dirLights_.end());
				
				if(!result.pointLights_.empty())
					pointLights_.insert(pointLights_.end(), result.pointLights_.begin(), result.pointLights_.end());
				
				if(!result.spotLights_.empty())
					spotLights_.insert(spotLights_.end(), result.spotLights_.begin(), result.spotLights_.end());

				minZ_ = Min(minZ_, result.minZ_);
				maxZ_ = Max(maxZ_, result.maxZ_);
			}
		}
		else
		{
			// If just 1 thread, copy the results directly
			PerThreadSceneResult& result = sceneResults_[0];
			minZ_ = result.minZ_;
			maxZ_ = result.maxZ_;
			Swap(geometries_, result.geometries_);
			
			if(!result.dirLights_.empty())
				Swap(dirLights_, result.dirLights_);

			if(!result.pointLights_.empty())
				Swap(pointLights_, result.pointLights_);

			if(!result.spotLights_.empty())
				Swap(spotLights_, result.spotLights_);
		}

		if (minZ_ == M_INFINITY)
			minZ_ = 0.0f;
	}

	void View::GetBatches()
	{
		if (!octree_ || !cullCamera_)
			return;

		nonThreadedGeometries_.clear();
		threadedGeometries_.clear();

// 		ProcessLights();
// 		GetLightBatches();

 		GetBaseBatches();
	}

	void View::GetBaseBatches()
	{
		UNIQUE_PROFILE(GetBaseBatches);

		for (auto i = geometries_.begin(); i != geometries_.end(); ++i)
		{
			Drawable* drawable = *i;
			UpdateGeometryType type = drawable->GetUpdateGeometryType();
			if (type == UPDATE_MAIN_THREAD)
				nonThreadedGeometries_.push_back(drawable);
			else if (type == UPDATE_WORKER_THREAD)
				threadedGeometries_.push_back(drawable);

			const Vector<SourceBatch>& batches = drawable->GetBatches();
			bool vertexLightsProcessed = false;

			auto& scenePasses = MainContext(scenePasses_);

			for (unsigned j = 0; j < batches.size(); ++j)
			{
				const SourceBatch& srcBatch = batches[j];

				if (!srcBatch.geometry_ || !srcBatch.numWorldTransforms_)
					continue;
						
				Material* material = srcBatch.material_;
				if(material == nullptr)
				{
					material = renderer_.GetDefaultMaterial();
				}

				Shader* shader = material->GetShader();
				// Check each of the scene passes
				for (unsigned k = 0; k < scenePasses.size(); ++k)
				{
					ScenePassInfo& info = scenePasses[k];
					Pass* pass = shader->GetPass(info.passIndex_);
					if (!pass)
						continue;

					Batch destBatch(srcBatch);
					destBatch.pass_ = info.passIndex_;
					destBatch.zone_ = GetZone(drawable);
					//destBatch.lightMask_ = (unsigned char)GetLightMask(drawable);
					//destBatch.lightQueue_ = 0;

					AddBatchToQueue(*info.batchQueue_, destBatch, info.passIndex_, info.allowInstancing_);
				}
			}
		}

	}
	
	void View::AddBatchToQueue(BatchQueue& queue, Batch& batch, uint passIndex, bool allowInstancing, bool allowShadows)
	{
		if (!batch.material_)
			batch.material_ = renderer_.GetDefaultMaterial();

		// Convert to instanced if possible
		if (allowInstancing && batch.geometryType_ == GEOM_STATIC && batch.geometry_->GetIndexBuffer())
			batch.geometryType_ = GEOM_INSTANCED;

		if (batch.geometryType_ == GEOM_INSTANCED)
		{
			BatchGroupKey key(batch);

			auto i = queue.batchGroups_.find(key);
			if (i == queue.batchGroups_.end())
			{
				// Create a new group based on the batch
				// In case the group remains below the instancing limit, do not enable instancing shaders yet
				BatchGroup newGroup(batch);
				newGroup.geometryType_ = minInstances_ <= 1 ? GEOM_INSTANCED : GEOM_STATIC;
				renderer_.SetBatchShaders(newGroup, passIndex, allowShadows, queue);
				newGroup.CalculateSortKey();
				newGroup.AddTransforms(batch);

				queue.batchGroups_.insert(std::make_pair(key, newGroup));

			}
			else
			{
				size_t oldSize = i->second.instances_.size();
				i->second.AddTransforms(batch);
				// Convert to using instancing shaders when the instancing limit is reached
				if (oldSize < minInstances_ && (int)i->second.instances_.size() >= minInstances_)
				{
					i->second.geometryType_ = GEOM_INSTANCED;
					renderer_.SetBatchShaders(i->second, passIndex, allowShadows, queue);
					i->second.CalculateSortKey();
				}
			}
		}
		else
		{
			renderer_.SetBatchShaders(batch, passIndex, allowShadows, queue);
			batch.CalculateSortKey();

			// If batch is static with multiple world transforms and cannot instance, we must push copies of the batch individually
			if (batch.geometryType_ == GEOM_STATIC && batch.numWorldTransforms_ > 1)
			{
				unsigned numTransforms = batch.numWorldTransforms_;
				batch.numWorldTransforms_ = 1;
				for (unsigned i = 0; i < numTransforms; ++i)
				{
					// Move the transform pointer to generate copies of the batch which only refer to 1 world transform
					queue.batches_.push_back(batch);
					auto& b = queue.batches_.back();
					b.transformOffset_ = GetMatrics(&b.worldTransform_[i], 1);
					++batch.worldTransform_;
				}
			}
			else
			{
				queue.batches_.push_back(batch);	
				auto& b = queue.batches_.back();
				b.transformOffset_ = GetMatrics(b.worldTransform_, b.numWorldTransforms_);
			}
		}
	}

	size_t View::GetMatrics(const Matrix3x4* transform, uint num)
	{
		auto it = matricsToOffset_.find(transform);
		if (it != matricsToOffset_.end())
		{
			return it->second;
		}

		auto& batchMatrics = MainContext(batchMatrics_);
		size_t offset = batchMatrics.size();
		size_t newSize = offset + num;
		size_t cap = batchMatrics.capacity();
		if (newSize > cap)
		{
			batchMatrics.reserve(newSize);
		}

		batchMatrics.resize(newSize);
		std::memcpy(&batchMatrics[offset], transform, num * sizeof(Matrix3x4));
		matricsToOffset_[transform] = offset;
		return offset;
	}

	void View::SetWorldTransform(size_t offset)
	{
		auto& batchMatrics = RenderContext(batchMatrics_);
		const Matrix3x4* mat  = &batchMatrics[offset];
		void* data = objectVS_->Map(MapFlags::DISCARD);
		std::memcpy(data, mat, sizeof(Matrix3x4));
		objectVS_->UnMap();
	}

	void View::SetSkinedTransform(size_t offset, uint count)
	{
		auto& batchMatrics = RenderContext(batchMatrics_);
		const Matrix3x4* mat = &batchMatrics[offset];
		void* data = skinnedVS_->Map(MapFlags::DISCARD);
		std::memcpy(data, mat, sizeof(Matrix3x4) * count);
		skinnedVS_->UnMap();
	}

	void View::PrepareInstancingBuffer()
	{
		UNIQUE_PROFILE(PrepareInstancingBuffer);

		size_t totalInstances = 0;

		auto& batchQueues = MainContext(batchQueues_);

		for (auto i = batchQueues.begin(); i != batchQueues.end(); ++i)
			totalInstances += i->second.GetNumInstances(this);
		/*
		for (auto i = lightQueues_.begin(); i != lightQueues_.end(); ++i)
		{
			for (unsigned j = 0; j < i->shadowSplits_.size(); ++j)
				totalInstances += i->shadowSplits_[j].shadowBatches_.GetNumInstances();
			totalInstances += i->litBaseBatches_.GetNumInstances();
			totalInstances += i->litBatches_.GetNumInstances();
		}*/

		if (!totalInstances || !renderer_.ResizeInstancingBuffer((uint)totalInstances))
			return;

		VertexBuffer* instancingBuffer = renderer_.GetInstancingBuffer();
		unsigned freeIndex = 0;
		void* dest = instancingBuffer->Lock(0, (uint)totalInstances);
		if (!dest)
			return;

		const unsigned stride = instancingBuffer->GetStride();
		for (auto i = batchQueues.begin(); i != batchQueues.end(); ++i)
		{
			i->second.SetInstancingData(dest, stride, freeIndex);
		}

		/*
		for (auto i = lightQueues_.begin(); i != lightQueues_.end(); ++i)
		{
			for (unsigned j = 0; j < i->shadowSplits_.size(); ++j)
				i->shadowSplits_[j].shadowBatches_.SetInstancingData(dest, stride, freeIndex);
			i->litBaseBatches_.SetInstancingData(dest, stride, freeIndex);
			i->litBatches_.SetInstancingData(dest, stride, freeIndex);
		}*/

		instancingBuffer->Unlock();
	}

	void View::UpdateGeometries()
	{
		UNIQUE_PROFILE(SortAndUpdateGeometry);

		WorkQueue& queue = GetSubsystem<WorkQueue>();

		// Sort batches
		{
			auto& renderPasses = MainContext(scenePasses_);
			for (unsigned i = 0; i < renderPasses.size(); ++i)
			{
				const auto& command = renderPasses[i];
				//if (!IsNecessary(command))
				//	continue;

				SPtr<WorkItem> item = queue.GetFreeItem();
				item->priority_ = M_MAX_UNSIGNED;
				item->workFunction_ =
					command.sortMode_ == SORT_FRONTTOBACK ? SortBatchQueueFrontToBackWork : SortBatchQueueBackToFrontWork;
				item->start_ = command.batchQueue_;
				queue.AddWorkItem(item);
				
			}
#if false
			for (auto i = lightQueues_.Begin(); i != lightQueues_.End(); ++i)
			{
				SPtr<WorkItem> lightItem = queue->GetFreeItem();
				lightItem->priority_ = M_MAX_UNSIGNED;
				lightItem->workFunction_ = SortLightQueueWork;
				lightItem->start_ = &(*i);
				queue->AddWorkItem(lightItem);

				if (i->shadowSplits_.Size())
				{
					SPtr<WorkItem> shadowItem = queue->GetFreeItem();
					shadowItem->priority_ = M_MAX_UNSIGNED;
					shadowItem->workFunction_ = SortShadowQueueWork;
					shadowItem->start_ = &(*i);
					queue->AddWorkItem(shadowItem);
				}
			}
#endif
		}


		//renderPath_->Update(this);

		// Update geometries. Split into threaded and non-threaded updates.
		{
			if (threadedGeometries_.size())
			{
				// In special cases (context loss, multi-view) a drawable may theoretically first have reported a threaded update, but will actually
				// require a main thread update. Check these cases first and move as applicable. The threaded work routine will tolerate the null
				// pointer holes that we leave to the threaded update queue.
				for (auto i = threadedGeometries_.begin(); i != threadedGeometries_.end(); ++i)
				{
					if ((*i)->GetUpdateGeometryType() == UPDATE_MAIN_THREAD)
					{
						nonThreadedGeometries_.push_back(*i);
						*i = 0;
					}
				}

				int numWorkItems = (int)queue.GetNumThreads() + 1; // Worker threads + main thread
				int drawablesPerItem = (int)threadedGeometries_.size() / numWorkItems;

				auto start = &threadedGeometries_.front();
				for (int i = 0; i < numWorkItems; ++i)
				{
					auto end = &threadedGeometries_.back() + 1;
					if (i < numWorkItems - 1 && end - start > drawablesPerItem)
						end = start + drawablesPerItem;

					SPtr<WorkItem> item = queue.GetFreeItem();
					item->priority_ = M_MAX_UNSIGNED;
					item->workFunction_ = UpdateDrawableGeometriesWork;
					item->aux_ = const_cast<FrameInfo*>(&frame_);
					item->start_ = start;
					item->end_ = end;
					queue.AddWorkItem(item);

					start = end;
				}
			}

			// While the work queue is processed, update non-threaded geometries
			for (auto i = nonThreadedGeometries_.begin(); i != nonThreadedGeometries_.end(); ++i)
				(*i)->UpdateGeometry(frame_);
		}

		// Finally ensure all threaded work has completed
		queue.Complete(M_MAX_UNSIGNED);
		geometriesUpdated_ = true;
	}

	/// Query for lit geometries and shadow casters for a light.
	void View::ProcessLight(LightQueryResult& query, unsigned threadIndex)
	{

	}
	
	void View::FindZone(Drawable* drawable)
	{
		Vector3 center = drawable->GetWorldBoundingBox().Center();
		int bestPriority = M_MIN_INT;
		Zone* newZone = nullptr;

		// If bounding box center is in view, the zone assignment is conclusive also for next frames. Otherwise it is temporary
		// (possibly incorrect) and must be re-evaluated on the next frame
		bool temporary = !cullCamera_->GetFrustum().IsInside(center);

		// First check if the current zone remains a conclusive result
		Zone* lastZone = drawable->GetZone();

		if (lastZone && (lastZone->GetViewMask() & cullCamera_->GetViewMask()) && lastZone->GetPriority() >= highestZonePriority_ &&
			(drawable->GetZoneMask() & lastZone->GetZoneMask()) && lastZone->IsInside(center))
			newZone = lastZone;
		else
		{
			for (auto i = zones_.begin(); i != zones_.end(); ++i)
			{
				Zone* zone = *i;
				int priority = zone->GetPriority();
				if (priority > bestPriority && (drawable->GetZoneMask() & zone->GetZoneMask()) && zone->IsInside(center))
				{
					newZone = zone;
					bestPriority = priority;
				}
			}
		}

		drawable->SetZone(newZone, temporary);
	}

}

