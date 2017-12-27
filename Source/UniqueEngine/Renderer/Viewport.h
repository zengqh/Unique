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

#pragma once

#include "Core/Object.h"
#include "Container/Ptr.h"
#include "Math/Ray.h"
#include "Math/Rect.h"
#include "Math/Vector2.h"

namespace Unique
{

class Camera;
class RenderPath;
class Scene;
class View;

/// %Viewport definition either for a render surface or the backbuffer.
class UNIQUE_API Viewport : public Object
{
    uRTTI(Viewport, Object)

public:
    /// Construct with defaults.
    Viewport();
    /// Construct with a full rectangle.
    Viewport(Scene* scene, Camera* camera, RenderPath* renderPath = 0);
    /// Construct with a specified rectangle.
    Viewport(Scene* scene, Camera* camera, const IntRect& rect, RenderPath* renderPath = 0);
    /// Destruct.
    ~Viewport();

    /// Set scene.
    void SetScene(Scene* scene);
    /// Set viewport camera.
    void SetCamera(Camera* camera);
    /// Set view rectangle. A zero rectangle (0 0 0 0) means to use the rendertarget's full dimensions.
    void SetRect(const IntRect& rect);
    /// Set rendering path.
    void SetRenderPath(RenderPath* path);
    /// Set whether to render debug geometry. Default true.
    void SetDrawDebug(bool enable);
    /// Set separate camera to use for culling. Sharing a culling camera between several viewports allows to prepare the view only once, saving in CPU use. The culling camera's frustum should cover all the viewport cameras' frusta or else objects may be missing from the rendered view.
    void SetCullCamera(Camera* camera);

    /// Return scene.
    Scene* GetScene() const;
    /// Return viewport camera.
    Camera* GetCamera() const;
    /// Return the internal rendering structure. May be null if the viewport has not been rendered yet.
    View* GetView() const;

    /// Return view rectangle. A zero rectangle (0 0 0 0) means to use the rendertarget's full dimensions. In this case you could fetch the actual view rectangle from View object, though it will be valid only after the first frame.
    const IntRect& GetRect() const { return rect_; }

    /// Return rendering path.
    RenderPath* GetRenderPath() const;

    /// Return whether to draw debug geometry.
    bool GetDrawDebug() const { return drawDebug_; }

    /// Return the culling camera. If null, the viewport camera will be used for culling (normal case.)
    Camera* GetCullCamera() const;

    /// Return ray corresponding to normalized screen coordinates.
    Ray GetScreenRay(int x, int y) const;
    /// Convert a world space point to normalized screen coordinates.
    IntVector2 WorldToScreenPoint(const Vector3& worldPos) const;
    /// Convert screen coordinates and depth to a world space point.
    Vector3 ScreenToWorldPoint(int x, int y, float depth) const;

    /// Allocate the view structure. Called by Renderer.
    void AllocateView();

private:
    /// Scene pointer.
    WPtr<Scene> scene_;
    /// Camera pointer.
    WPtr<Camera> camera_;
    /// Culling camera pointer.
    WPtr<Camera> cullCamera_;
    /// Viewport rectangle.
    IntRect rect_;
    /// Rendering path.
	SPtr<RenderPath> renderPath_;
    /// Internal rendering structure.
    SPtr<View> view_;
    /// Debug draw flag.
    bool drawDebug_;
};

}