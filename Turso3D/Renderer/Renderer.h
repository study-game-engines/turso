// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Graphics/GraphicsDefs.h"
#include "../Math/Color.h"
#include "../Math/Frustum.h"
#include "../Object/AutoPtr.h"
#include "../Resource/Image.h"
#include "../Thread/WorkQueue.h"
#include "Batch.h"

#include <atomic>

class Camera;
class FrameBuffer;
class GeometryDrawable;
class Graphics;
class LightDrawable;
class LightEnvironment;
class Material;
class OccluderDrawable;
class OcclusionBuffer;
class Octant;
class Octree;
class RenderBuffer;
class Scene;
class ShaderProgram;
class Texture;
class UniformBuffer;
class VertexBuffer;
struct CollectOctantsTask;
struct CollectBatchesTask;
struct CollectShadowBatchesTask;
struct CollectShadowCastersTask;
struct CullLightsTask;
struct ShadowView;
struct ThreadOctantResult;

static const size_t NUM_CLUSTER_X = 16;
static const size_t NUM_CLUSTER_Y = 8;
static const size_t NUM_CLUSTER_Z = 8;
static const size_t MAX_LIGHTS = 255;
static const size_t MAX_LIGHTS_CLUSTER = 16;
static const size_t NUM_OCTANT_TASKS = 9;

// Texture units with built-in meanings.
static const size_t TU_DIRLIGHTSHADOW = 8;
static const size_t TU_SHADOWATLAS = 9;
static const size_t TU_FACESELECTION1 = 10;
static const size_t TU_FACESELECTION2 = 11;
static const size_t TU_LIGHTCLUSTERDATA = 12;

/// Per-thread results for octant collection.
struct ThreadOctantResult
{
    /// Clear for the next frame.
    void Clear();

    /// Drawable accumulator. When full, queue the next batch collection task.
    size_t drawableAcc;
    /// Starting octant index for current task.
    size_t taskOctantIdx;
    /// Batch collection task index.
    size_t batchTaskIdx;
    /// Intermediate octant list.
    std::vector<std::pair<Octant*, unsigned char> > octants;
    /// Intermediate light drawable list.
    std::vector<LightDrawable*> lights;
    /// Tasks for main view batches collection, queued by the octant collection task when it finishes.
    std::vector<AutoPtr<CollectBatchesTask> > collectBatchesTasks;
};

/// Per-thread results for batch collection.
struct ThreadBatchResult
{
    /// Clear for the next frame.
    void Clear();

    /// Minimum geometry Z value.
    float minZ;
    /// Maximum geometry Z value.
    float maxZ;
    /// Combined bounding box of the visible geometries.
    BoundingBox geometryBounds;
    /// Initial opaque batches.
    std::vector<Batch> opaqueBatches;
    /// Initial alpha batches.
    std::vector<Batch> alphaBatches;
};

/// Shadow map data structure. May be shared by several lights.
struct ShadowMap
{
    /// Default-construct.
    ShadowMap();
    /// Destruct.
    ~ShadowMap();

    /// Clear for the next frame.
    void Clear();

    /// Next free batch queue.
    size_t freeQueueIdx;
    /// Next free shadowcaster list index.
    size_t freeCasterListIdx;
    /// Rectangle allocator.
    AreaAllocator allocator;
    /// Shadow map texture.
    SharedPtr<Texture> texture;
    /// Shadow map framebuffer.
    SharedPtr<FrameBuffer> fbo;
    /// Shadow views that use this shadow map.
    std::vector<ShadowView*> shadowViews;
    /// Shadow batch queues used by the shadow views.
    std::vector<BatchQueue> shadowBatches;
    /// Intermediate shadowcaster lists for processing.
    std::vector<std::vector<Drawable*> > shadowCasters;
    /// Instancing transforms for shadowcasters.
    std::vector<Matrix3x4> instanceTransforms;
};

/// Per-view uniform buffer data.
struct PerViewUniforms
{
    /// Current camera's view matrix
    Matrix3x4 viewMatrix;
    /// Current camera's projection matrix.
    Matrix4 projectionMatrix;
    /// Current camera's combined view and projection matrix.
    Matrix4 viewProjMatrix;
    /// Current camera's depth parameters.
    Vector4 depthParameters;
    /// Current camera's world position.
    Vector4 cameraPosition;
    /// Current scene's ambient color.
    Vector4 ambientColor;
    /// Current scene's fog color.
    Vector4 fogColor;
    /// Current scene's fog start and end parameters.
    Vector4 fogParameters;
    /// Data for the directional light.
    Vector4 dirLightData[12];
};

/// Per-light data for cluster light shader.
struct LightData
{
    /// %Light position.
    Vector4 position;
    /// %Light direction.
    Vector4 direction;
    /// %Light attenuation parameters.
    Vector4 attenuation;
    /// %Light color.
    Color color;
    /// Shadow parameters.
    Vector4 shadowParameters;
    /// Shadow matrix. For point lights, contains extra parameters.
    Matrix4 shadowMatrix;
};

/// High-level rendering subsystem. Performs rendering of 3D scenes.
class Renderer : public Object
{
    OBJECT(Renderer);

public:
    /// Construct. Register subsystem and objects. Graphics and WorkQueue subsystems must have been initialized.
    Renderer();
    /// Destruct.
    ~Renderer();

    /// Set size and format of shadow maps. First map is used for a directional light, the second as an atlas for others.
    void SetupShadowMaps(int dirLightSize, int lightAtlasSize, ImageFormat format);
    /// Set global depth bias multipiers for shadow maps.
    void SetShadowDepthBiasMul(float depthBiasMul, float slopeScaleBiasMul);
    /// Prepare view for rendering. This will utilize worker threads.
    void PrepareView(Scene* scene, Camera* camera, bool drawShadows, bool useOcclusion);
    /// Render shadowmaps before rendering the view. Last shadow framebuffer will be left bound.
    void RenderShadowMaps();
    /// Clear with fog color (optional), then render opaque objects into the currently set framebuffer and viewport.
    void RenderOpaque(bool clear = true);
    /// Render transparent objects into the currently set framebuffer and viewport.
    void RenderAlpha();
    /// Add debug geometry from the objects in frustum into DebugRenderer. Note: does not automatically render, to allow more geometry to be added elsewhere.
    void RenderDebug();

    /// Return a shadow map texture by index for debugging.
    Texture* ShadowMapTexture(size_t index) const;
    /// Return the occlusion buffer for debugging.
    OcclusionBuffer* GetOcclusionBuffer() const;

private:
    /// Submit occluders to the software occlusion buffer and begin rasterizing them.
    void SubmitOccluders();
    /// Collect octants and lights from the octree recursively. Queue batch collection tasks while ongoing.
    void CollectOctantsAndLights(Octant* octant, ThreadOctantResult& result, unsigned char planeMask = 0x3f);
    /// Allocate shadow map for a light. Return true on success.
    bool AllocateShadowMap(LightDrawable* light);
    /// Sort main opaque and alpha batch queues.
    void SortMainBatches();
    /// Sort all batch queues of a shadowmap.
    void SortShadowBatches(ShadowMap& shadowMap);
    /// Upload instance transforms before rendering.
    void UpdateInstanceTransforms(const std::vector<Matrix3x4>& transforms);
    /// Render a batch queue.
    void RenderBatches(Camera* camera, const BatchQueue& queue);
    /// Define face selection texture for point light shadows.
    void DefineFaceSelectionTextures();
    /// Setup light cluster frustums and bounding boxes if necessary.
    void DefineClusterFrustums();
    /// Work function to collect octants.
    void CollectOctantsWork(Task* task, unsigned threadIndex);
    /// Process lights collected by octant tasks, and queue shadowcaster query tasks for them as necessary.
    void ProcessLightsWork(Task* task, unsigned threadIndex);
    /// Work function to collect main view batches from geometries.
    void CollectBatchesWork(Task* task, unsigned threadIndex);
    /// Work function to collect shadowcasters per shadowcasting light.
    void CollectShadowCastersWork(Task* task, unsigned threadIndex);
    /// Work function to queue shadowcaster batch collection tasks. Requires batch collection and shadowcaster query tasks to be complete.
    void ProcessShadowCastersWork(Task* task, unsigned threadIndex);
    /// Work function to collect shadowcaster batches per shadow view.
    void CollectShadowBatchesWork(Task* task, unsigned threadIndex);
    /// Work function to cull lights against a Z-slice of the frustum grid.
    void CullLightsToFrustumWork(Task* task, unsigned threadIndex);

    /// Current scene.
    Scene* scene;
    /// Current scene octree.
    Octree* octree;
    /// Current scene light environment.
    LightEnvironment* lightEnvironment;
    /// Current camera.
    Camera* camera;
    /// Camera frustum.
    Frustum frustum;
    /// Cached graphics subsystem.
    Graphics* graphics;
    /// Cached work queue subsystem.
    WorkQueue* workQueue;
    /// Camera view mask.
    unsigned viewMask;
    /// Framenumber.
    unsigned short frameNumber;
    /// Shadow use flag.
    bool drawShadows;
    /// Shadow maps globally dirty flag. All cached shadow content should be reset.
    bool shadowMapsDirty;
    /// Cluster frustums dirty flag.
    bool clusterFrustumsDirty;
    /// Instancing supported flag.
    bool hasInstancing;
    /// Root-level octants, used as a starting point for octant and batch collection. The root octant is included if it also contains drawables.
    std::vector<Octant*> rootLevelOctants;
    /// Counter for batch collection tasks remaining. When zero, main batch sorting can begin while other tasks go on.
    std::atomic<int> numPendingBatchTasks;
    /// Counters for shadow views remaining per shadowmap. When zero, the shadow batches can be sorted.
    std::atomic<int> numPendingShadowViews[2];
    /// Per-octree branch octant collection results.
    ThreadOctantResult octantResults[NUM_OCTANT_TASKS];
    /// Per-worker thread batch collection results.
    std::vector<ThreadBatchResult> batchResults;
    /// Minimum Z value for all geometries in frustum.
    float minZ;
    /// Maximum Z value for all geometries in frustum.
    float maxZ;
    /// Combined bounding box of the visible geometries.
    BoundingBox geometryBounds;
    /// Brightest directional light in frustum.
    LightDrawable* dirLight;
    /// Accepted point and spot lights in frustum.
    std::vector<LightDrawable*> lights;
    /// Found occluders.
    std::vector<OccluderDrawable*> occluders;
    /// Shadow maps.
    std::vector<ShadowMap> shadowMaps;
    /// Opaque batches.
    BatchQueue opaqueBatches;
    /// Transparent batches.
    BatchQueue alphaBatches;
    /// Instance transforms for opaque and alpha batches.
    std::vector<Matrix3x4> instanceTransforms;
    /// Last camera used for rendering.
    Camera* lastCamera;
    /// Last material pass used for rendering.
    Pass* lastPass;
    /// Last material used for rendering.
    Material* lastMaterial;
    /// Last material uniforms assignment number.
    unsigned lastPerMaterialUniforms;
    /// Constant depth bias multiplier.
    float depthBiasMul;
    /// Slope-scaled depth bias multiplier.
    float slopeScaleBiasMul;
    /// Occlusion buffer for occlusion tests.
    AutoPtr<OcclusionBuffer> occlusionBuffer;
    /// Tasks for octant collection.
    AutoPtr<CollectOctantsTask> collectOctantsTasks[NUM_OCTANT_TASKS];
    /// %Task for light processing.
    AutoPtr<Task> processLightsTask;
    /// Tasks for shadow light processing.
    std::vector<AutoPtr<CollectShadowCastersTask> > collectShadowCastersTasks;
    /// %Task for queuing shadow views for further processing.
    AutoPtr<Task> processShadowCastersTask;
    /// Tasks for shadow batch processing.
    std::vector<AutoPtr<CollectShadowBatchesTask> > collectShadowBatchesTasks;
    /// Tasks for light grid culling.
    AutoPtr<CullLightsTask> cullLightsTasks[NUM_CLUSTER_Z];
    /// Face selection UV indirection texture 1.
    AutoPtr<Texture> faceSelectionTexture1;
    /// Face selection UV indirection texture 2.
    AutoPtr<Texture> faceSelectionTexture2;
    /// Cluster lookup 3D texture.
    AutoPtr<Texture> clusterTexture;
    /// Per-view uniform buffer.
    AutoPtr<UniformBuffer> perViewDataBuffer;
    /// Light data uniform buffer.
    AutoPtr<UniformBuffer> lightDataBuffer;
    /// Instancing vertex buffer.
    AutoPtr<VertexBuffer> instanceVertexBuffer;
    /// Cached static object shadow buffer. Note: only needed for the light atlas, not the directional light shadowmap.
    AutoPtr<RenderBuffer> staticObjectShadowBuffer;
    /// Cached static object shadow framebuffer.
    AutoPtr<FrameBuffer> staticObjectShadowFbo;
    /// Vertex elements for the instancing buffer.
    std::vector<VertexElement> instanceVertexElements;
    /// Last projection matrix used to initialize cluster frustums.
    Matrix4 lastClusterFrustumProj;
    /// Amount of lights per cluster.
    unsigned char numClusterLights[NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    /// Cluster frustums for lights.
    AutoArrayPtr<Frustum> clusterFrustums;
    /// Cluster bounding boxes.
    AutoArrayPtr<BoundingBox> clusterBoundingBoxes;
    /// Cluster data CPU copy.
    AutoArrayPtr<unsigned char> clusterData;
    /// Light constantbuffer data CPU copy.
    AutoArrayPtr<LightData> lightData;
    /// Per-view uniform data CPU copy.
    PerViewUniforms perViewData;
};

/// Register Renderer related object factories and attributes.
void RegisterRendererLibrary();
