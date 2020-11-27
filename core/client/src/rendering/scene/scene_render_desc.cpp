/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2020 Florian Weischer
 */

#include "stdafx_client.h"
#include "pragma/entities/components/c_scene_component.hpp"
#include "pragma/entities/entity_instance_index_buffer.hpp"
#include "pragma/rendering/occlusion_culling/occlusion_culling_handler_brute_force.hpp"
#include "pragma/rendering/occlusion_culling/occlusion_culling_handler_bsp.hpp"
#include "pragma/rendering/occlusion_culling/occlusion_culling_handler_chc.hpp"
#include "pragma/rendering/occlusion_culling/occlusion_culling_handler_inert.hpp"
#include "pragma/rendering/occlusion_culling/occlusion_culling_handler_octtree.hpp"
#include "pragma/rendering/occlusion_culling/c_occlusion_octree_impl.hpp"
#include "pragma/rendering/renderers/base_renderer.hpp"
#include "pragma/rendering/renderers/rasterization_renderer.hpp"
#include "pragma/rendering/renderers/rasterization/culled_mesh_data.hpp"
#include "pragma/rendering/shaders/world/c_shader_textured.hpp"
#include "pragma/rendering/render_queue.hpp"
#include "pragma/model/c_model.h"
#include "pragma/model/c_modelmesh.h"
#include "pragma/console/c_cvar.h"
#include <sharedutils/util_shaderinfo.hpp>
#include <sharedutils/alpha_mode.hpp>
#include <sharedutils/util_hash.hpp>
#include <pragma/entities/entity_iterator.hpp>

extern DLLCLIENT ClientState *client;
extern DLLCLIENT CGame *c_game;
#pragma optimize("",off)
SceneRenderDesc::SceneRenderDesc(pragma::CSceneComponent &scene)
	: m_scene{scene}
{
	for(auto &renderQueue : m_renderQueues)
		renderQueue = pragma::rendering::RenderQueue::Create();
	ReloadOcclusionCullingHandler();
}
SceneRenderDesc::~SceneRenderDesc()
{
	m_occlusionCullingHandler = nullptr;
}

const std::vector<pragma::OcclusionMeshInfo> &SceneRenderDesc::GetCulledMeshes() const {return m_renderMeshCollectionHandler.GetOcclusionFilteredMeshes();}
std::vector<pragma::OcclusionMeshInfo> &SceneRenderDesc::GetCulledMeshes() {return m_renderMeshCollectionHandler.GetOcclusionFilteredMeshes();}
const std::vector<pragma::CParticleSystemComponent*> &SceneRenderDesc::GetCulledParticles() const {return m_renderMeshCollectionHandler.GetOcclusionFilteredParticleSystems();}
std::vector<pragma::CParticleSystemComponent*> &SceneRenderDesc::GetCulledParticles() {return m_renderMeshCollectionHandler.GetOcclusionFilteredParticleSystems();}

const pragma::OcclusionCullingHandler &SceneRenderDesc::GetOcclusionCullingHandler() const {return const_cast<SceneRenderDesc*>(this)->GetOcclusionCullingHandler();}
pragma::OcclusionCullingHandler &SceneRenderDesc::GetOcclusionCullingHandler() {return *m_occlusionCullingHandler;}
void SceneRenderDesc::SetOcclusionCullingHandler(const std::shared_ptr<pragma::OcclusionCullingHandler> &handler) {m_occlusionCullingHandler = handler;}
void SceneRenderDesc::SetOcclusionCullingMethod(OcclusionCullingMethod method)
{
	switch(method)
	{
	case OcclusionCullingMethod::BruteForce: /* Brute-force */
		m_occlusionCullingHandler = std::make_shared<pragma::OcclusionCullingHandlerBruteForce>();
		break;
	case OcclusionCullingMethod::CHCPP: /* CHC++ */
		m_occlusionCullingHandler = std::make_shared<pragma::OcclusionCullingHandlerCHC>();
		break;
	case OcclusionCullingMethod::BSP: /* BSP */
	{
		auto *world = c_game->GetWorld();
		if(world)
		{
			auto &entWorld = world->GetEntity();
			auto pWorldComponent = entWorld.GetComponent<pragma::CWorldComponent>();
			auto bspTree = pWorldComponent.valid() ? pWorldComponent->GetBSPTree() : nullptr;
			if(bspTree != nullptr && bspTree->GetNodes().size() > 1u)
			{
				m_occlusionCullingHandler = std::make_shared<pragma::OcclusionCullingHandlerBSP>(bspTree);
				break;
			}
		}
	}
	case OcclusionCullingMethod::Octree: /* Octtree */
		m_occlusionCullingHandler = std::make_shared<pragma::OcclusionCullingHandlerOctTree>();
		break;
	case OcclusionCullingMethod::Inert: /* Off */
	default:
		m_occlusionCullingHandler = std::make_shared<pragma::OcclusionCullingHandlerInert>();
		break;
	}
	m_occlusionCullingHandler->Initialize();
}
void SceneRenderDesc::ReloadOcclusionCullingHandler()
{
	auto occlusionCullingMode = static_cast<OcclusionCullingMethod>(c_game->GetConVarInt("cl_render_occlusion_culling"));
	SetOcclusionCullingMethod(occlusionCullingMode);
}

static auto cvDrawGlow = GetClientConVar("render_draw_glow");
static auto cvDrawTranslucent = GetClientConVar("render_draw_translucent");
static auto cvDrawSky = GetClientConVar("render_draw_sky");
static auto cvDrawWater = GetClientConVar("render_draw_water");
static auto cvDrawView = GetClientConVar("render_draw_view");
pragma::rendering::CulledMeshData *SceneRenderDesc::GetRenderInfo(RenderMode renderMode) const
{
	auto &renderMeshData = m_renderMeshCollectionHandler.GetRenderMeshData();
	auto it = renderMeshData.find(renderMode);
	if(it == renderMeshData.end())
		return nullptr;
	return it->second.get();
}

pragma::rendering::RenderMeshCollectionHandler &SceneRenderDesc::GetRenderMeshCollectionHandler() {return m_renderMeshCollectionHandler;}
const pragma::rendering::RenderMeshCollectionHandler &SceneRenderDesc::GetRenderMeshCollectionHandler() const {return const_cast<SceneRenderDesc*>(this)->GetRenderMeshCollectionHandler();}

static FRender render_mode_to_render_flag(RenderMode renderMode)
{
	switch(renderMode)
	{
	case RenderMode::World:
		return FRender::World;
	case RenderMode::View:
		return FRender::View;
	case RenderMode::Skybox:
		return FRender::Skybox;
	case RenderMode::Water:
		return FRender::Water;
	}
	return FRender::None;
}

SceneRenderDesc::RenderQueueId SceneRenderDesc::GetRenderQueueId(RenderMode renderMode,bool translucent) const
{
	switch(renderMode)
	{
	case RenderMode::Skybox:
		return !translucent ? RenderQueueId::Skybox : RenderQueueId::SkyboxTranslucent;
	case RenderMode::View:
		return !translucent ? RenderQueueId::View : RenderQueueId::ViewTranslucent;
	case RenderMode::Water:
		return !translucent ? RenderQueueId::Water : RenderQueueId::Invalid;
	case RenderMode::World:
		return !translucent ? RenderQueueId::World : RenderQueueId::WorldTranslucent;
	}
	static_assert(umath::to_integral(RenderQueueId::Count) == 7u);
	return RenderQueueId::Invalid;
}
pragma::rendering::RenderQueue *SceneRenderDesc::GetRenderQueue(RenderMode renderMode,bool translucent)
{
	auto renderQueueId = GetRenderQueueId(renderMode,translucent);
	return (renderQueueId != RenderQueueId::Invalid) ? m_renderQueues.at(umath::to_integral(renderQueueId)).get() : nullptr;
}
const pragma::rendering::RenderQueue *SceneRenderDesc::GetRenderQueue(RenderMode renderMode,bool translucent) const
{
	return const_cast<SceneRenderDesc*>(this)->GetRenderQueue(renderMode,translucent);
}
const std::vector<std::shared_ptr<const pragma::rendering::RenderQueue>> &SceneRenderDesc::GetWorldRenderQueues() const {return m_worldRenderQueues;}
void SceneRenderDesc::AddRenderMeshesToRenderQueue(
	const util::DrawSceneInfo &drawSceneInfo,pragma::CRenderComponent &renderC,
	const std::function<pragma::rendering::RenderQueue*(RenderMode,bool)> &getRenderQueue,
	const pragma::CSceneComponent &scene,const pragma::CCameraComponent &cam,const Mat4 &vp,const std::function<bool(const Vector3&,const Vector3&)> &fShouldCull,
	int32_t lodBias
)
{
	auto &mdlC = renderC.GetModelComponent();
	auto lod = umath::max(static_cast<int32_t>(mdlC->GetLOD()) +lodBias,0);
	auto &renderMeshes = renderC.GetRenderMeshes();
	auto &lodGroup = renderC.GetLodRenderMeshGroup(lod);
	auto renderMode = renderC.GetRenderMode();
	auto first = false;
	for(auto meshIdx=lodGroup.first;meshIdx<lodGroup.first +lodGroup.second;++meshIdx)
	{
		if(fShouldCull && ShouldCull(renderC,meshIdx,fShouldCull))
			continue;
		auto &renderMesh = renderMeshes[meshIdx];
		auto *mat = mdlC->GetRenderMaterial(renderMesh->GetSkinTextureIndex());
		auto *shader = mat ? dynamic_cast<pragma::ShaderTextured3DBase*>(mat->GetPrimaryShader().get()) : nullptr;
		if(shader == nullptr)
			continue;
		auto nonOpaque = mat->GetAlphaMode() != AlphaMode::Opaque;
		if(nonOpaque && umath::is_flag_set(drawSceneInfo.renderFlags,FRender::Translucent) == false)
			continue;
		auto *renderQueue = getRenderQueue(renderMode,nonOpaque);
		if(renderQueue == nullptr)
			continue;
		if(first == false)
		{
			first = true;
			renderC.UpdateRenderDataMT(drawSceneInfo.commandBuffer,scene,cam,vp);
		}
		renderQueue->Add(static_cast<CBaseEntity&>(renderC.GetEntity()),meshIdx,*mat,*shader,nonOpaque ? &cam : nullptr);
	}
}
void SceneRenderDesc::AddRenderMeshesToRenderQueue(
	const util::DrawSceneInfo &drawSceneInfo,pragma::CRenderComponent &renderC,const pragma::CSceneComponent &scene,const pragma::CCameraComponent &cam,const Mat4 &vp,
	const std::function<bool(const Vector3&,const Vector3&)> &fShouldCull
)
{
	AddRenderMeshesToRenderQueue(drawSceneInfo,renderC,[this](RenderMode renderMode,bool translucent) {return GetRenderQueue(renderMode,translucent);},scene,cam,vp,fShouldCull);
}

bool SceneRenderDesc::ShouldCull(CBaseEntity &ent,const std::function<bool(const Vector3&,const Vector3&)> &fShouldCull)
{
	auto *renderC = ent.GetRenderComponent();
	return !renderC || ShouldCull(*renderC,fShouldCull);
}
bool SceneRenderDesc::ShouldCull(pragma::CRenderComponent &renderC,const std::function<bool(const Vector3&,const Vector3&)> &fShouldCull)
{
	Vector3 min,max;
	renderC.GetRenderBounds(&min,&max);
	auto &pos = renderC.GetEntity().GetPosition();
	min += pos;
	max += pos;
	return fShouldCull(min,max);
}
bool SceneRenderDesc::ShouldCull(pragma::CRenderComponent &renderC,pragma::RenderMeshIndex meshIdx,const std::function<bool(const Vector3&,const Vector3&)> &fShouldCull)
{
	auto &renderMeshes = renderC.GetRenderMeshes();
	if(meshIdx >= renderMeshes.size())
		return false;
	auto &renderMesh = renderMeshes[meshIdx];
	Vector3 min,max;
	renderMesh->GetBounds(min,max);
	auto &pos = renderC.GetEntity().GetPosition();
	min += pos;
	max += pos;
	return fShouldCull(min,max);
}
bool SceneRenderDesc::ShouldCull(const Vector3 &min,const Vector3 &max,const std::vector<Plane> &frustumPlanes)
{
	return Intersection::AABBInPlaneMesh(min,max,frustumPlanes) == Intersection::Intersect::Outside;
}

void SceneRenderDesc::CollectRenderMeshesFromOctree(
	const util::DrawSceneInfo &drawSceneInfo,const OcclusionOctree<CBaseEntity*> &tree,const pragma::CSceneComponent &scene,const pragma::CCameraComponent &cam,const Mat4 &vp,FRender renderFlags,
	const std::function<pragma::rendering::RenderQueue*(RenderMode,bool)> &getRenderQueue,
	const std::function<bool(const Vector3&,const Vector3&)> &fShouldCull,const std::vector<util::BSPTree::Node*> *bspLeafNodes,
	int32_t lodBias
)
{
	std::function<void(const OcclusionOctree<CBaseEntity*>::Node &node)> iterateTree = nullptr;
	iterateTree = [&iterateTree,&scene,&cam,renderFlags,fShouldCull,&drawSceneInfo,&getRenderQueue,&vp,bspLeafNodes,lodBias](const OcclusionOctree<CBaseEntity*>::Node &node) {
		auto &nodeBounds = node.GetWorldBounds();
		if(fShouldCull && fShouldCull(nodeBounds.first,nodeBounds.second))
			return;
		if(bspLeafNodes)
		{
			auto hasIntersection = false;
			for(auto *node : *bspLeafNodes)
			{
				if(Intersection::AABBAABB(nodeBounds.first,nodeBounds.second,node->minVisible,node->maxVisible) == Intersection::Intersect::Outside)
					continue;
				hasIntersection = true;
				break;
			}
			if(hasIntersection == false)
				return;
		}
		auto &objs = node.GetObjects();
		for(auto *ent : objs)
		{
			assert(ent);
			if(ent == nullptr)
			{
				// This should NEVER occur, but seems to anyway in some rare cases
				Con::cerr<<"ERROR: NULL Entity in dynamic scene occlusion octree! Ignoring..."<<Con::endl;
				continue;
			}
			if(ent->IsWorld())
				continue; // World entities are handled separately
			auto *renderC = static_cast<CBaseEntity*>(ent)->GetRenderComponent();
			if(!renderC || renderC->IsExemptFromOcclusionCulling() || ShouldConsiderEntity(*static_cast<CBaseEntity*>(ent),scene,cam.GetEntity().GetPosition(),renderFlags) == false)
				continue;
			if(fShouldCull && ShouldCull(*renderC,fShouldCull))
				continue;
			AddRenderMeshesToRenderQueue(drawSceneInfo,*renderC,getRenderQueue,scene,cam,vp,fShouldCull,lodBias);
		}
		auto *children = node.GetChildren();
		if(children == nullptr)
			return;
		for(auto &c : *children)
			iterateTree(static_cast<OcclusionOctree<CBaseEntity*>::Node&>(*c));
	};
	iterateTree(tree.GetRootNode());
}
void SceneRenderDesc::CollectRenderMeshesFromOctree(
	const util::DrawSceneInfo &drawSceneInfo,const OcclusionOctree<CBaseEntity*> &tree,const pragma::CSceneComponent &scene,const pragma::CCameraComponent &cam,const Mat4 &vp,FRender renderFlags,
	const std::vector<Plane> &frustumPlanes,const std::vector<util::BSPTree::Node*> *bspLeafNodes
)
{
	CollectRenderMeshesFromOctree(drawSceneInfo,tree,scene,cam,vp,renderFlags,[this](RenderMode renderMode,bool translucent) {return GetRenderQueue(renderMode,translucent);},
	[&frustumPlanes](const Vector3 &min,const Vector3 &max) -> bool {
		return Intersection::AABBInPlaneMesh(min,max,frustumPlanes) == Intersection::Intersect::Outside;
	},bspLeafNodes);
}

bool SceneRenderDesc::ShouldConsiderEntity(CBaseEntity &ent,const pragma::CSceneComponent &scene,const Vector3 &camOrigin,FRender renderFlags)
{
	if(ent.IsInScene(scene) == false || !ent.GetRenderComponent())
		return false;
	auto *renderC = ent.GetRenderComponent();
	auto renderMode = renderC->GetRenderMode();
	return umath::is_flag_set(renderFlags,render_mode_to_render_flag(renderMode)) && ent.GetModel() != nullptr && renderC->ShouldDraw(camOrigin);
}

struct DebugFreezeCamData
{
	Vector3 pos;
	std::vector<Plane> frustumPlanes;
};
static std::optional<DebugFreezeCamData> g_debugFreezeCamData = {};
static void cmd_debug_occlusion_culling_freeze_camera(NetworkState*,ConVar*,bool,bool val)
{
	g_debugFreezeCamData = {};
	if(val == false)
		return;
	auto *scene = c_game->GetRenderScene();
	if(scene == nullptr)
		return;
	auto &cam = scene->GetActiveCamera();
	if(cam.expired())
		return;
	g_debugFreezeCamData = DebugFreezeCamData{};
	g_debugFreezeCamData->pos = cam->GetEntity().GetPosition();
	g_debugFreezeCamData->frustumPlanes = cam->GetFrustumPlanes();
}
REGISTER_CONVAR_CALLBACK_CL(debug_occlusion_culling_freeze_camera,cmd_debug_occlusion_culling_freeze_camera);

bool SceneRenderDesc::IsWorldMeshVisible(uint32_t worldRenderQueueIndex,pragma::RenderMeshIndex meshIdx) const
{
	if(worldRenderQueueIndex >= m_worldMeshVisibility.size())
		return false;
	auto &worldMeshVisibility = m_worldMeshVisibility.at(worldRenderQueueIndex);
	return (meshIdx < worldMeshVisibility.size()) ? worldMeshVisibility.at(meshIdx) : false;
}

void SceneRenderDesc::WaitForWorldRenderQueues() const {while(m_worldRenderQueuesReady == false);}

static auto cvInstancingThreshold = GetClientConVar("render_instancing_threshold");
static auto cvInstancingEnabled = GetClientConVar("render_instancing_enabled");
class RenderMeshInstancer
{
public:
	RenderMeshInstancer(pragma::rendering::RenderQueue &renderQueue);
	void Process();
private:
	util::Hash CalcNextEntityHash(uint32_t &outNumMeshes,EntityIndex &entIndex);
	void ProcessInstantiableList(uint32_t endIndex,uint32_t numMeshes,util::Hash hash);

	pragma::rendering::RenderQueue &m_renderQueue;
	uint32_t m_curIndex = 0;
	uint32_t m_instanceThreshold = 2;
	std::vector<EntityIndex> m_instantiableEntityList;
};

RenderMeshInstancer::RenderMeshInstancer(pragma::rendering::RenderQueue &renderQueue)
	: m_renderQueue{renderQueue},m_instanceThreshold{static_cast<uint32_t>(umath::max(cvInstancingThreshold->GetInt(),2))}
{}

void RenderMeshInstancer::Process()
{
	uint32_t prevNumMeshes = 0;
	EntityIndex entIndex;
	auto prevHash = CalcNextEntityHash(prevNumMeshes,entIndex);
	m_instantiableEntityList.push_back(entIndex);
	uint32_t numMeshes = 0;
	auto &sortedItemIndices = m_renderQueue.sortedItemIndices;
	while(m_curIndex < sortedItemIndices.size())
	{
		auto hash = CalcNextEntityHash(numMeshes,entIndex);
		if(hash != prevHash) // New entity is different; no instantiation possible
		{
			// Process the instantiation list for everything before the current entity
			ProcessInstantiableList(m_curIndex -1,prevNumMeshes,prevHash);
			m_instantiableEntityList.push_back(entIndex);
			prevHash = hash;
			prevNumMeshes = numMeshes;
			continue;
		}
		m_instantiableEntityList.push_back(entIndex);
	}
	ProcessInstantiableList(m_curIndex,prevNumMeshes,prevHash);
}

void RenderMeshInstancer::ProcessInstantiableList(uint32_t endIndex,uint32_t numMeshes,util::Hash hash)
{
	auto numInstantiableEntities = m_instantiableEntityList.size();
	if(numInstantiableEntities < m_instanceThreshold)
	{
		m_instantiableEntityList.clear();
		return;
	}

	std::vector<pragma::RenderBufferIndex> renderBufferIndices {};
	renderBufferIndices.reserve(numInstantiableEntities);
	for(auto entIdx : m_instantiableEntityList)
	{
		auto renderBufferIndex = static_cast<CBaseEntity*>(c_game->GetEntityByLocalIndex(entIdx))->GetRenderComponent()->GetRenderBufferIndex();
		renderBufferIndices.push_back(*renderBufferIndex);
	}
		
	auto &instanceIndexBuffer = pragma::CSceneComponent::GetEntityInstanceIndexBuffer();
	auto instanceBuf = instanceIndexBuffer->AddInstanceList(m_renderQueue,std::move(renderBufferIndices),util::hash_combine<uint64_t>(hash,numInstantiableEntities));

	m_renderQueue.instanceSets.push_back({});

	auto setIdx = m_renderQueue.instanceSets.size() -1;
	auto startIndex = endIndex -(numInstantiableEntities *numMeshes);

	auto &instanceSet = m_renderQueue.instanceSets.back();
	instanceSet.instanceCount = numInstantiableEntities;
	instanceSet.instanceBuffer = instanceBuf;
	instanceSet.meshCount = numMeshes;
	instanceSet.startSkipIndex = startIndex;

	for(auto i=startIndex;i<(startIndex +numMeshes);++i)
	{
		auto &item = m_renderQueue.queue[m_renderQueue.sortedItemIndices[i].first];
		item.instanceSetIndex = setIdx;
	}
	// TODO: Instanced items are skipped anyway, so technically we don't need this second loop
	for(auto i=(startIndex +numMeshes);i<endIndex;++i)
	{
		auto &item = m_renderQueue.queue[m_renderQueue.sortedItemIndices[i].first];
		item.instanceSetIndex = pragma::rendering::RenderQueueItem::INSTANCED;
	}

	m_instantiableEntityList.clear();
}

util::Hash RenderMeshInstancer::CalcNextEntityHash(uint32_t &outNumMeshes,EntityIndex &entIndex)
{
	auto &sortedItemIndices = m_renderQueue.sortedItemIndices;
	if(m_curIndex >= sortedItemIndices.size())
		return 0;
	util::Hash hash = 0;
	auto entity = m_renderQueue.queue[sortedItemIndices[m_curIndex].first].entity;
	entIndex = entity;
	outNumMeshes = 0;
	while(m_curIndex < sortedItemIndices.size())
	{
		auto &sortKey = sortedItemIndices[m_curIndex];
		auto &item = m_renderQueue.queue[sortKey.first];
		if(item.entity != entity)
			break;
		++outNumMeshes;
		++m_curIndex;
		hash = util::hash_combine<uint64_t>(hash,*reinterpret_cast<uint64_t*>(&sortKey.second));
	}
	return hash;
}








void SceneRenderDesc::BuildRenderQueueInstanceLists(pragma::rendering::RenderQueue &renderQueue)
{
	renderQueue.instanceSets.clear();
	if(cvInstancingEnabled->GetBool() == false)
		return;
	RenderMeshInstancer instancer {renderQueue};
	instancer.Process();

#if 0
	util::Hash prevEntityHash = 0;
	util::Hash curEntityHash = 0;
	uint32_t numHashMatches = 0;
	auto instanceThreshold = umath::max(cvInstancingThreshold->GetInt(),2);
	std::vector<EntityIndex> instantiableEntityList;
	auto fUpdateEntityInstanceLists = [&instantiableEntityList,&curEntityHash,&prevEntityHash,instanceThreshold](
		pragma::rendering::RenderQueue &renderQueue,EntityIndex entIdx,uint32_t numMeshes,pragma::rendering::RenderQueueItemSortPair *sortItem
	) {
		// New entity
		auto *ent = static_cast<CBaseEntity*>(c_game->GetEntityByLocalIndex(entIdx));
		auto *renderC = ent ? ent ->GetRenderComponent() : nullptr;
		if(instantiableEntityList.size() > 1 && curEntityHash != prevEntityHash)
		{
			// Last entity has a different hash than the one before; We'll close the instantiation list and open a new one
			auto numInstantiable = instantiableEntityList.size() -1;
			if(numInstantiable < instanceThreshold)
				instantiableEntityList.erase(instantiableEntityList.begin(),instantiableEntityList.begin() +(numInstantiable -1));
			else
			{
				auto newItem = instantiableEntityList.back(); // NOT part of our instance list!
				instantiableEntityList.erase(instantiableEntityList.end() -1);

				// [0,#instantiableEntityList) can be instanced
				std::vector<pragma::RenderBufferIndex> renderBufferIndices {};
				renderBufferIndices.reserve(instantiableEntityList.size());
				for(auto &entIdx : instantiableEntityList)
				{
					auto renderBufferIndex = static_cast<CBaseEntity*>(c_game->GetEntityByLocalIndex(entIdx))->GetRenderComponent()->GetRenderBufferIndex();
					renderBufferIndices.push_back(*renderBufferIndex);
				}
				auto &instanceIndexBuffer = pragma::CSceneComponent::GetEntityInstanceIndexBuffer();
				auto numInstances = renderBufferIndices.size();
				auto instanceBuf = instanceIndexBuffer->AddInstanceList(renderQueue,std::move(renderBufferIndices),util::hash_combine<uint64_t>(prevEntityHash,numInstances));
				// We'll iterate backwards through the sorted render queue and mark the
				// items as instanced.
				for(auto i=decltype(numInstances){0u};i<(numInstances -1);++i)
				{
					// The renderer will know to skip all items mark as 'INSTANCED'
					// (Marking the first mesh per entity is enough)
					sortItem -= numMeshes;
					renderQueue.queue[sortItem->first].instanceSetIndex = pragma::rendering::RenderQueueItem::INSTANCED;
				}
				
				auto startSkipIndex = (sortItem -renderQueue.sortedItemIndices.data()) /sizeof(decltype(renderQueue.sortedItemIndices.front()));
				sortItem -= numMeshes;
				renderQueue.instanceSets.push_back({});
				renderQueue.queue[sortItem->first].instanceSetIndex = renderQueue.instanceSets.size() -1;
				auto &instanceSet = renderQueue.instanceSets.back();
				instanceSet.instanceCount = numInstances;
				instanceSet.instanceBuffer = instanceBuf;
				instanceSet.meshCount = numMeshes;
				instanceSet.startSkipIndex = startSkipIndex;

				instantiableEntityList = {};
				instantiableEntityList.push_back(newItem); // Restore the last item
			}
		}
		
		prevEntityHash = curEntityHash;
		curEntityHash = 0;
		
		if(renderC && sortItem->second.instantiable)
			instantiableEntityList.push_back(entIdx);
	};
	auto &sortedItemIndices = renderQueue.sortedItemIndices;
	auto fCalcNextEntityHash = [&sortedItemIndices,&renderQueue](uint32_t &inOutStartIndex,uint32_t &outNumMeshes) -> util::Hash {
		if(inOutStartIndex >= sortedItemIndices.size())
			return 0;
		util::Hash hash = 0;
		auto entity = renderQueue.queue[sortedItemIndices[inOutStartIndex].first].entity;
		uint32_t numMeshes = 0;
		while(inOutStartIndex < sortedItemIndices.size())
		{
			auto &sortKey = sortedItemIndices[inOutStartIndex];
			auto &item = renderQueue.queue[sortKey.first];
			if(item.entity != entity)
				break;
			++numMeshes;
			hash = util::hash_combine<uint64_t>(hash,*reinterpret_cast<uint64_t*>(&sortKey.second));
		}
		return hash;
	};

	auto fProcessInstantiable = [&renderQueue,instanceThreshold](uint32_t index,uint32_t numInstantiableEntities,uint32_t numMeshes) {
		if(numInstantiableEntities < instanceThreshold)
			return;
		auto startIndex = index -(numInstantiableEntities *numMeshes);

		std::vector<pragma::RenderBufferIndex> renderBufferIndices {};
		renderBufferIndices.reserve(instantiableEntityList.size());
		for(auto &entIdx : instantiableEntityList)
		{
			auto renderBufferIndex = static_cast<CBaseEntity*>(c_game->GetEntityByLocalIndex(entIdx))->GetRenderComponent()->GetRenderBufferIndex();
			renderBufferIndices.push_back(*renderBufferIndex);
		}
		
		auto &instanceIndexBuffer = pragma::CSceneComponent::GetEntityInstanceIndexBuffer();
		auto instanceBuf = instanceIndexBuffer->AddInstanceList(renderQueue,std::move(renderBufferIndices),util::hash_combine<uint64_t>(prevEntityHash,numInstantiableEntities));

		auto &instanceSet = renderQueue.instanceSets.back();
		instanceSet.instanceCount = numInstantiableEntities;
		instanceSet.instanceBuffer = instanceBuf;
		instanceSet.meshCount = numMeshes;
		instanceSet.startSkipIndex = startSkipIndex;
	};

	uint32_t prevNumMeshes = 0;
	uint32_t startIdx = 0;
	auto prevHash = fCalcNextEntityHash(startIdx,prevNumMeshes);
	uint32_t numInstantiableEntities = 1;
	uint32_t numMeshes = 0;
	while(startIdx < sortedItemIndices.size())
	{
		auto hash = fCalcNextEntityHash(startIdx,numMeshes);
		if(hash != prevHash) // New entity is different; no instantiation possible
		{
			// Process the instantiation list for everything before the current entity
			fProcessInstantiable(startIdx -numMeshes,numInstantiableEntities,prevNumMeshes);
			numInstantiableEntities = 1;
			continue;
		}
		++numInstantiableEntities;
		// Add to instance list
		// TODO
	}
	fProcessInstantiable(startIdx -numMeshes,numInstantiableEntities,prevNumMeshes);


	if(sortedItemIndices.empty() == false)
	{
		uint32_t curEntityMeshes = 0;
		auto prevEntityIndex = renderQueue.queue[sortedItemIndices.front().first].entity;
		for(auto i=decltype(sortedItemIndices.size()){0u};i<sortedItemIndices.size();++i)
		{
			auto &sortKey = sortedItemIndices.at(i);
			auto &item = renderQueue.queue[sortKey.first];
			if(item.entity != prevEntityIndex)
			{
				instantiableEntityList.push_back(entIdx);
				fUpdateEntityInstanceLists(renderQueue,item.entity,curEntityMeshes,&sortKey);

				prevEntityIndex = item.entity;
				curEntityMeshes = 0;
			}
			++curEntityMeshes;

			static_assert(sizeof(decltype(sortKey.second)) == sizeof(uint64_t));
			curEntityHash = util::hash_combine<uint64_t>(curEntityHash,*reinterpret_cast<uint64_t*>(&sortKey.second));
		}
		curEntityHash = 0;
		instantiableEntityList.push_back(std::numeric_limits<EntityIndex>::max());
		fUpdateEntityInstanceLists(renderQueue,instantiableEntityList.back(),curEntityMeshes,(&sortedItemIndices.back()) +1);
	}
#endif
}

static auto cvDrawWorld = GetClientConVar("render_draw_world");
void SceneRenderDesc::BuildRenderQueues(const util::DrawSceneInfo &drawSceneInfo)
{
	auto &hCam = m_scene.GetActiveCamera();
	if(hCam.expired())
		return;

	auto &cam = *hCam;
	// c_game->StartProfilingStage(CGame::CPUProfilingPhase::BuildRenderQueue);
	auto &posCam = g_debugFreezeCamData.has_value() ? g_debugFreezeCamData->pos : cam.GetEntity().GetPosition();

	for(auto &renderQueue : m_renderQueues)
	{
		renderQueue->Clear();
		renderQueue->Lock();
	}
	m_worldRenderQueues.clear();

	m_worldRenderQueuesReady = false;
	c_game->GetRenderQueueBuilder().Append([this,&cam,posCam,&drawSceneInfo]() {
		pragma::CSceneComponent::GetEntityInstanceIndexBuffer()->UpdateAndClearUnusedBuffers();

		auto &frustumPlanes = g_debugFreezeCamData.has_value() ? g_debugFreezeCamData->frustumPlanes : cam.GetFrustumPlanes();
		auto fShouldCull = [&frustumPlanes](const Vector3 &min,const Vector3 &max) -> bool {return SceneRenderDesc::ShouldCull(min,max,frustumPlanes);};
		auto vp = cam.GetProjectionMatrix() *cam.GetViewMatrix();

		std::vector<util::BSPTree::Node*> bspLeafNodes;
		// Note: World geometry is handled differently than other entities. World entities have their
		// own pre-built render queues, which we only have to iterate for maximum efficiency. Whether or not a world mesh is culled from the
		// camera frustum is stored in 'm_worldMeshVisibility', which is simply a boolean array so we don't have to copy any
		// data between render queues. (The data in 'm_worldMeshVisibility' is only valid for this render pass.)
		// Translucent world meshes still need to be sorted with other entity meshes, so they are just copied over to the
		// main render queue.
		EntityIterator entItWorld {*c_game};
		entItWorld.AttachFilter<TEntityIteratorFilterComponent<pragma::CWorldComponent>>();
		bspLeafNodes.reserve(entItWorld.GetCount());
		m_worldMeshVisibility.reserve(entItWorld.GetCount());
		for(auto *entWorld : entItWorld)
		{
			if(ShouldConsiderEntity(*static_cast<CBaseEntity*>(entWorld),m_scene,posCam,drawSceneInfo.renderFlags) == false)
				continue;
			auto worldC = entWorld->GetComponent<pragma::CWorldComponent>();
			auto &bspTree = worldC->GetBSPTree();
			auto *node = bspTree ? bspTree->FindLeafNode(posCam) : nullptr;
			if(node == nullptr)
				continue;
			bspLeafNodes.push_back(node);

			if(umath::is_flag_set(drawSceneInfo.renderFlags,FRender::Static) == false)
				continue;

			auto *renderC = static_cast<CBaseEntity&>(worldC->GetEntity()).GetRenderComponent();
			renderC->UpdateRenderDataMT(drawSceneInfo.commandBuffer,m_scene,cam,vp);
			auto *renderQueue = worldC->GetClusterRenderQueue(node->cluster);
			if(renderQueue)
			{
				auto idx = m_worldRenderQueues.size();
				if(idx >= m_worldMeshVisibility.size())
					m_worldMeshVisibility.push_back({});
				auto &worldMeshVisibility = m_worldMeshVisibility.at(idx);

				m_worldRenderQueues.push_back(renderQueue->shared_from_this());
				auto &pos = entWorld->GetPosition();
				auto &renderMeshes = renderC->GetRenderMeshes();
				worldMeshVisibility.resize(renderMeshes.size());
				for(auto i=decltype(renderQueue->queue.size()){0u};i<renderQueue->queue.size();++i)
				{
					auto &item = renderQueue->queue.at(i);
					if(item.mesh >= renderMeshes.size())
						continue;
					worldMeshVisibility.at(item.mesh) = !ShouldCull(*renderC,item.mesh,fShouldCull);
				}
			}

			if(umath::is_flag_set(drawSceneInfo.renderFlags,FRender::Translucent))
			{
				// Translucent meshes will have to be sorted dynamically with all other non-world translucent objects,
				// so we'll copy the information to the dynamic queue
				auto *renderQueueTranslucentSrc = worldC->GetClusterRenderQueue(node->cluster,true /* translucent */);
				auto *renderQueueTranslucentDst = renderQueueTranslucentSrc ? GetRenderQueue(static_cast<CBaseEntity*>(entWorld)->GetRenderComponent()->GetRenderMode(),true) : nullptr;
				if(renderQueueTranslucentDst == nullptr || renderQueueTranslucentSrc->queue.empty())
					continue;
				renderQueueTranslucentDst->queue.reserve(renderQueueTranslucentDst->queue.size() +renderQueueTranslucentSrc->queue.size());
				renderQueueTranslucentDst->sortedItemIndices.reserve(renderQueueTranslucentDst->queue.size());
				auto &pose = entWorld->GetPose();
				for(auto i=decltype(renderQueueTranslucentSrc->queue.size()){0u};i<renderQueueTranslucentSrc->queue.size();++i)
				{
					auto &item = renderQueueTranslucentSrc->queue.at(i);
					if(ShouldCull(*renderC,item.mesh,fShouldCull))
						continue;
					renderQueueTranslucentDst->queue.push_back(item);
					renderQueueTranslucentDst->sortedItemIndices.push_back(renderQueueTranslucentSrc->sortedItemIndices.at(i));
					renderQueueTranslucentDst->sortedItemIndices.back().first = renderQueueTranslucentDst->queue.size() -1;

					auto &renderMeshes = renderC->GetRenderMeshes();
					if(item.mesh >= renderMeshes.size())
						continue;
					auto &pos = pose *renderMeshes[item.mesh]->GetCenter();
					renderQueueTranslucentDst->queue.back().sortingKey.SetDistance(pos,cam);
				}
			}
		}
		m_worldMeshVisibility.resize(m_worldRenderQueues.size());
		m_worldRenderQueuesReady = true;

		if(umath::is_flag_set(drawSceneInfo.renderFlags,FRender::Dynamic))
		{
			// Some entities are exempt from occlusion culling altogether, we'll handle them here
			for(auto *pRenderComponent : pragma::CRenderComponent::GetEntitiesExemptFromOcclusionCulling())
			{
				if(ShouldConsiderEntity(static_cast<CBaseEntity&>(pRenderComponent->GetEntity()),m_scene,posCam,drawSceneInfo.renderFlags) == false)
					continue;
				AddRenderMeshesToRenderQueue(drawSceneInfo,*pRenderComponent,m_scene,cam,vp,nullptr);
			}

			// Now we just need the remaining entities, for which we'll use the scene octree
			auto *culler = m_scene.FindOcclusionCuller();
			if(culler)
			{
				auto &dynOctree = culler->GetOcclusionOctree();
				CollectRenderMeshesFromOctree(drawSceneInfo,dynOctree,m_scene,cam,vp,drawSceneInfo.renderFlags,frustumPlanes,&bspLeafNodes);
			}
		}

		// All render queues (aside from world render queues) need to be sorted
		for(auto &renderQueue : m_renderQueues)
		{
			renderQueue->Sort();
			BuildRenderQueueInstanceLists(*renderQueue);
			renderQueue->Unlock();
		}
		// c_game->StopProfilingStage(CGame::CPUProfilingPhase::BuildRenderQueue);
	});
}
#pragma optimize("",on)
