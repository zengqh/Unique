//
// Copyright (c) 2008-2017 the Unique project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "Precompiled.h"
#include "staticmodel.h"
#include "Core/Context.h"
#include "Core/Profiler.h"
//#include "../Graphics/Batch.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Model.h"
#include "../Scene/Node.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/Material.h"
//#include "../Graphics/OcclusionBuffer.h"
#include "../Graphics/OctreeQuery.h"
#include "../Graphics/VertexBuffer.h"
#include "IO/FileSystem.h"
#include "IO/Log.h"
#include "Resource/ResourceCache.h"
#include "Resource/ResourceEvents.h"

#include "DebugNew.h"

namespace Unique
{

extern const char* GEOMETRY_CATEGORY;

uObject(StaticModel)
{
	uFactory("Object")
		uMixedAccessor("Model", GetModelAttr, SetModelAttr)
		uAccessor("Material", GetMaterialsAttr, SetMaterialsAttr)
		uAttribute("Is Occluder", occluder_)
		uAccessor("Can Be Occluded", IsOccludee, SetOccludee)
		uAttribute("Cast Shadows", castShadows_)
		uAccessor("Draw Distance", GetDrawDistance, SetDrawDistance)
		uAccessor("Shadow Distance", GetShadowDistance, SetShadowDistance)
		uAccessor("LOD Bias", GetLodBias, SetLodBias)
		uAttribute("Occlusion LOD Level", occlusionLodLevel_)
}

StaticModel::StaticModel() :
    Drawable(DRAWABLE_GEOMETRY),
    occlusionLodLevel_(M_MAX_UNSIGNED),
    materialsAttr_(Material::GetTypeStatic())
{
}

StaticModel::~StaticModel()
{
}

void StaticModel::ProcessRayQuery(const RayOctreeQuery& query, PODVector<RayQueryResult>& results)
{/*
    RayQueryLevel level = query.level_;

    switch (level)
    {
    case RAY_AABB:
        Drawable::ProcessRayQuery(query, results);
        break;

    case RAY_OBB:
    case RAY_TRIANGLE:
    case RAY_TRIANGLE_UV:
        Matrix3x4 inverse(node_->GetWorldTransform().Inverse());
        Ray localRay = query.ray_.Transformed(inverse);
        float distance = localRay.HitDistance(boundingBox_);
        Vector3 normal = -query.ray_.direction_;
        Vector2 geometryUV;
        unsigned hitBatch = M_MAX_UNSIGNED;

        if (level >= RAY_TRIANGLE && distance < query.maxDistance_)
        {
            distance = M_INFINITY;

            for (unsigned i = 0; i < batches_.size(); ++i)
            {
                Geometry* geometry = batches_[i].geometry_;
                if (geometry)
                {
                    Vector3 geometryNormal;
                    float geometryDistance = level == RAY_TRIANGLE ? geometry->GetHitDistance(localRay, &geometryNormal) :
                        geometry->GetHitDistance(localRay, &geometryNormal, &geometryUV);
                    if (geometryDistance < query.maxDistance_ && geometryDistance < distance)
                    {
                        distance = geometryDistance;
                        normal = (node_->GetWorldTransform() * geometryNormal).Normalized();
                        hitBatch = i;
                    }
                }
            }
        }

        if (distance < query.maxDistance_)
        {
            RayQueryResult result;
            result.position_ = query.ray_.origin_ + distance * query.ray_.direction_;
            result.normal_ = normal;
            result.textureUV_ = geometryUV;
            result.distance_ = distance;
            result.drawable_ = this;
            result.node_ = node_;
            result.subObject_ = hitBatch;
            results.push_back(result);
        }
        break;
    }*/
}

void StaticModel::UpdateBatches(const FrameInfo& frame)
{
    const BoundingBox& worldBoundingBox = GetWorldBoundingBox();
    distance_ = frame.camera_->GetDistance(worldBoundingBox.Center());

    if (batches_.size() == 1)
        batches_[0].distance_ = distance_;
    else
    {
        const Matrix3x4& worldTransform = node_->GetWorldTransform();
        for (unsigned i = 0; i < batches_.size(); ++i)
            batches_[i].distance_ = frame.camera_->GetDistance(worldTransform * geometryData_[i].center_);
    }

    float scale = worldBoundingBox.Size().DotProduct(DOT_SCALE);
    float newLodDistance = frame.camera_->GetLodDistance(distance_, scale, lodBias_);

    if (newLodDistance != lodDistance_)
    {
        lodDistance_ = newLodDistance;
        CalculateLodLevels();
    }

}

Geometry* StaticModel::GetLodGeometry(unsigned batchIndex, unsigned level)
{
    if (batchIndex >= geometries_.size())
        return 0;

    // If level is out of range, use visible geometry
    if (level < geometries_[batchIndex].size())
        return geometries_[batchIndex][level];
    else
        return batches_[batchIndex].geometry_;
}

unsigned StaticModel::GetNumOccluderTriangles()
{
    unsigned triangles = 0;
#if false
    for (unsigned i = 0; i < batches_.size(); ++i)
    {
        Geometry* geometry = GetLodGeometry(i, occlusionLodLevel_);
        if (!geometry)
            continue;

        // Check that the material is suitable for occlusion (default material always is)
        Material* mat = batches_[i].material_;
        if (mat && !mat->GetOcclusion())
            continue;

        triangles += geometry->GetIndexCount() / 3;
    }
#endif
    return triangles;
}

bool StaticModel::DrawOcclusion(OcclusionBuffer* buffer)
{
#if false
    for (unsigned i = 0; i < batches_.size(); ++i)
    {
        Geometry* geometry = GetLodGeometry(i, occlusionLodLevel_);
        if (!geometry)
            continue;

        // Check that the material is suitable for occlusion (default material always is) and set culling mode
        Material* material = batches_[i].material_;
        if (material)
        {
            if (!material->GetOcclusion())
                continue;
            buffer->SetCullMode(material->GetCullMode());
        }
        else
            buffer->SetCullMode(CULL_CCW);

        const unsigned char* vertexData;
        unsigned vertexSize;
        const unsigned char* indexData;
        unsigned indexSize;
        const PODVector<VertexElement>* elements;

        geometry->GetRawData(vertexData, vertexSize, indexData, indexSize, elements);
        // Check for valid geometry data
        if (!vertexData || !indexData || !elements || VertexBuffer::GetElementOffset(*elements, TYPE_VECTOR3, SEM_POSITION) != 0)
            continue;

        unsigned indexStart = geometry->GetIndexStart();
        unsigned indexCount = geometry->GetIndexCount();

        // Draw and check for running out of triangles
        if (!buffer->AddTriangles(node_->GetWorldTransform(), vertexData, vertexSize, indexData, indexSize, indexStart, indexCount))
            return false;
    }
#endif
    return true;
}

void StaticModel::SetModel(Model* model)
{
    if (model == model_)
        return;

    if (!node_)
    {
        UNIQUE_LOGERROR("Can not set model while model component is not attached to a scene node");
        return;
    }

    // Unsubscribe from the reload event of previous model (if any), then subscribe to the new
    if (model_)
        Unsubscribe<ReloadFinished>(model_);

    model_ = model;

    if (model)
    {
        Subscribe(model, &StaticModel::HandleModelReloadFinished);

        // Copy the subgeometry & LOD level structure
        SetNumGeometries(model->GetNumGeometries());
        const Vector<Vector<SPtr<Geometry> > >& geometries = model->GetGeometries();
        const PODVector<Vector3>& geometryCenters = model->GetGeometryCenters();
        const Matrix3x4* worldTransform = node_ ? &node_->GetWorldTransform() : (const Matrix3x4*)0;
        for (unsigned i = 0; i < geometries.size(); ++i)
        {
            batches_[i].worldTransform_ = worldTransform;
            geometries_[i] = geometries[i];
            geometryData_[i].center_ = geometryCenters[i];
        }

        SetBoundingBox(model->GetBoundingBox());
        ResetLodLevels();
    }
    else
    {
        SetNumGeometries(0);
        SetBoundingBox(BoundingBox());
    }

    
}

void StaticModel::SetMaterial(Material* material)
{
    for (unsigned i = 0; i < batches_.size(); ++i)
        batches_[i].material_ = material;   
}

bool StaticModel::SetMaterial(unsigned index, Material* material)
{
    if (index >= batches_.size())
    {
        UNIQUE_LOGERROR("Material index out of bounds");
        return false;
    }

    batches_[index].material_ = material;
    
    return true;
}

void StaticModel::SetOcclusionLodLevel(unsigned level)
{
    occlusionLodLevel_ = level;
    
}

void StaticModel::ApplyMaterialList(const String& fileName)
{
    String useFileName = fileName;
    if (useFileName.Trimmed().Empty() && model_)
        useFileName = ReplaceExtension(model_->GetName(), ".txt");

    ResourceCache& cache = GetSubsystem<ResourceCache>();
    SPtr<File> file = cache.GetFile(useFileName, false);
    if (!file)
        return;

    unsigned index = 0;
    while (!file->IsEof() && index < batches_.size())
    {
        Material* material = cache.GetResource<Material>(file->ReadLine());
        if (material)
            SetMaterial(index, material);

        ++index;
    }
}

Material* StaticModel::GetMaterial(unsigned index) const
{
    return index < batches_.size() ? batches_[index].material_ : (Material*)0;
}

bool StaticModel::IsInside(const Vector3& point) const
{
    if (!node_)
        return false;

    Vector3 localPosition = node_->GetWorldTransform().Inverse() * point;
    return IsInsideLocal(localPosition);
}

bool StaticModel::IsInsideLocal(const Vector3& point) const
{
    // Early-out if point is not inside bounding box
    if (boundingBox_.IsInside(point) == OUTSIDE)
        return false;

	return true;
	/*
    Ray localRay(point, Vector3(1.0f, -1.0f, 1.0f));

    for (unsigned i = 0; i < batches_.size(); ++i)
    {
        Geometry* geometry = batches_[i].geometry_;
        if (geometry)
        {
            if (geometry->IsInside(localRay))
                return true;
        }
    }

    return false;*/
}

void StaticModel::SetBoundingBox(const BoundingBox& box)
{
    boundingBox_ = box;
    OnMarkedDirty(node_);
}

void StaticModel::SetNumGeometries(unsigned num)
{
    batches_.resize(num);
    geometries_.resize(num);
    geometryData_.resize(num);
    ResetLodLevels();
}

void StaticModel::SetModelAttr(const ResourceRef& value)
{
    ResourceCache& cache = GetSubsystem<ResourceCache>();
    SetModel(cache.GetResource<Model>(value.name_));
}

void StaticModel::SetMaterialsAttr(const ResourceRefList& value)
{
    ResourceCache& cache = GetSubsystem<ResourceCache>();
    for (unsigned i = 0; i < value.names_.size(); ++i)
        SetMaterial(i, cache.GetResource<Material>(value.names_[i]));
}

ResourceRef StaticModel::GetModelAttr() const
{
    return GetResourceRef(model_, Model::GetTypeStatic());
}

const ResourceRefList& StaticModel::GetMaterialsAttr() const
{
    materialsAttr_.names_.resize(batches_.size());
    for (unsigned i = 0; i < batches_.size(); ++i)
        materialsAttr_.names_[i] = GetResourceName(GetMaterial(i));

    return materialsAttr_;
}

void StaticModel::OnWorldBoundingBoxUpdate()
{
    worldBoundingBox_ = boundingBox_.Transformed(node_->GetWorldTransform());
}

void StaticModel::ResetLodLevels()
{
    // Ensure that each subgeometry has at least one LOD level, and reset the current LOD level
    for (unsigned i = 0; i < batches_.size(); ++i)
    {
        if (!geometries_[i].size())
            geometries_[i].resize(1);
        batches_[i].geometry_ = geometries_[i][0];
        geometryData_[i].lodLevel_ = 0;
    }

    // Find out the real LOD levels on next geometry update
    lodDistance_ = M_INFINITY;
}

void StaticModel::CalculateLodLevels()
{
    for (unsigned i = 0; i < batches_.size(); ++i)
    {
        const Vector<SPtr<Geometry> >& batchGeometries = geometries_[i];
        // If only one LOD geometry, no reason to go through the LOD calculation
        if (batchGeometries.size() <= 1)
            continue;

        unsigned j;

        for (j = 1; j < batchGeometries.size(); ++j)
        {
            if (batchGeometries[j] && lodDistance_ <= batchGeometries[j]->GetLodDistance())
                break;
        }

        unsigned newLodLevel = j - 1;
        if (geometryData_[i].lodLevel_ != newLodLevel)
        {
            geometryData_[i].lodLevel_ = newLodLevel;
            batches_[i].geometry_ = batchGeometries[newLodLevel];
        }
    }
}

void StaticModel::HandleModelReloadFinished(const ReloadFinished& eventData)
{
    Model* currentModel = model_;
    model_.Reset(); // Set null to allow to be re-set
    SetModel(currentModel);
}

}