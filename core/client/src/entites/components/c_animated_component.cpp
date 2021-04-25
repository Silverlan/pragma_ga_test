/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_client.h"
#include "pragma/entities/components/c_animated_component.hpp"
#include "pragma/entities/components/c_eye_component.hpp"
#include "pragma/lua/libraries/c_lua_vulkan.h"
#include "pragma/model/c_vertex_buffer_data.hpp"
#include <pragma/model/animation/animated_pose.hpp>
#include <pragma/model/model.h>
#include <prosper_util.hpp>
#include <prosper_command_buffer.hpp>
#include <buffers/prosper_uniform_resizable_buffer.hpp>
#include <buffers/prosper_swap_buffer.hpp>
#include <pragma/entities/entity_component_system_t.hpp>

extern DLLCLIENT CEngine *c_engine;

using namespace pragma;

extern DLLCLIENT CGame *c_game;

ComponentEventId CSkAnimatedComponent::EVENT_ON_SKELETON_UPDATED = INVALID_COMPONENT_ID;
ComponentEventId CSkAnimatedComponent::EVENT_ON_BONE_MATRICES_UPDATED = INVALID_COMPONENT_ID;
ComponentEventId CSkAnimatedComponent::EVENT_ON_BONE_BUFFER_INITIALIZED = INVALID_COMPONENT_ID;
void CSkAnimatedComponent::RegisterEvents(pragma::EntityComponentManager &componentManager)
{
	BaseSkAnimatedComponent::RegisterEvents(componentManager);
	EVENT_ON_SKELETON_UPDATED = componentManager.RegisterEvent("ON_SKELETON_UPDATED",std::type_index(typeid(CSkAnimatedComponent)));
	EVENT_ON_BONE_MATRICES_UPDATED = componentManager.RegisterEvent("ON_BONE_MATRICES_UPDATED",std::type_index(typeid(CSkAnimatedComponent)));
	EVENT_ON_BONE_BUFFER_INITIALIZED = componentManager.RegisterEvent("ON_BONE_BUFFER_INITIALIZED");
}
void CSkAnimatedComponent::GetBaseTypeIndex(std::type_index &outTypeIndex) const {outTypeIndex = std::type_index(typeid(BaseSkAnimatedComponent));}
luabind::object CSkAnimatedComponent::InitializeLuaObject(lua_State *l) {return BaseEntityComponent::InitializeLuaObject<CSkAnimatedComponentHandleWrapper>(l);}
static std::shared_ptr<prosper::IUniformResizableBuffer> s_instanceBoneBuffer = nullptr;
const std::shared_ptr<prosper::IUniformResizableBuffer> &pragma::get_instance_bone_buffer() {return s_instanceBoneBuffer;}
void pragma::initialize_articulated_buffers()
{
	auto instanceSize = umath::to_integral(GameLimits::MaxBones) *sizeof(Mat4);
	auto instanceCount = 512u;
	auto maxInstanceCount = instanceCount *4u;
	prosper::util::BufferCreateInfo createInfo {};

	if constexpr(CRenderComponent::USE_HOST_MEMORY_FOR_RENDER_DATA)
	{
		createInfo.memoryFeatures = prosper::MemoryFeatureFlags::HostAccessable | prosper::MemoryFeatureFlags::HostCoherent;
		createInfo.flags |= prosper::util::BufferCreateInfo::Flags::Persistent;
	}
	else
		createInfo.memoryFeatures = prosper::MemoryFeatureFlags::DeviceLocal;
	createInfo.size = instanceSize *maxInstanceCount;
	createInfo.usageFlags = prosper::BufferUsageFlags::UniformBufferBit | prosper::BufferUsageFlags::TransferSrcBit | prosper::BufferUsageFlags::TransferDstBit;
#ifdef ENABLE_VERTEX_BUFFER_AS_STORAGE_BUFFER
	createInfo.usageFlags |= prosper::BufferUsageFlags::StorageBufferBit;
#endif
	s_instanceBoneBuffer = c_engine->GetRenderContext().CreateUniformResizableBuffer(createInfo,instanceSize,instanceSize *maxInstanceCount,0.05f);
	s_instanceBoneBuffer->SetDebugName("entity_anim_bone_buf");

	if constexpr(CRenderComponent::USE_HOST_MEMORY_FOR_RENDER_DATA)
		s_instanceBoneBuffer->SetPermanentlyMapped(true,prosper::IBuffer::MapFlags::WriteBit | prosper::IBuffer::MapFlags::Unsynchronized);
}
void pragma::clear_articulated_buffers() {s_instanceBoneBuffer = nullptr;}

void CSkAnimatedComponent::SetBoneBufferDirty() {umath::set_flag(m_stateFlags,StateFlags::BoneBufferDirty);}
void CSkAnimatedComponent::SetSkeletonUpdateCallbacksEnabled(bool enabled) {umath::set_flag(m_stateFlags,StateFlags::EnableSkeletonUpdateCallbacks,enabled);}
bool CSkAnimatedComponent::AreSkeletonUpdateCallbacksEnabled() const {return umath::is_flag_set(m_stateFlags,StateFlags::EnableSkeletonUpdateCallbacks);}

void CSkAnimatedComponent::Initialize()
{
	BaseSkAnimatedComponent::Initialize();
	InitializeBoneBuffer();

	/*BindEventUnhandled(LogicComponent::EVENT_ON_TICK,[this](std::reference_wrapper<pragma::ComponentEvent> evData) {
		auto pRenderComponent = GetEntity().GetComponent<CRenderComponent>();
		if(pRenderComponent.valid())
			pRenderComponent->UpdateRenderBounds();
	});*/
	BindEventUnhandled(CRenderComponent::EVENT_ON_UPDATE_RENDER_DATA_MT,[this](std::reference_wrapper<pragma::ComponentEvent> evData) {
		if(AreSkeletonUpdateCallbacksEnabled())
			return; // Bone matrices will be updated from main thread
		UpdateBoneMatricesMT();
	});
	BindEventUnhandled(CRenderComponent::EVENT_ON_UPDATE_RENDER_BUFFERS,[this](std::reference_wrapper<pragma::ComponentEvent> evData) {
		auto isDirty = umath::is_flag_set(m_stateFlags,StateFlags::BoneBufferDirty);
		umath::set_flag(m_stateFlags,StateFlags::BoneBufferDirty,false);
		UpdateBoneBuffer(*static_cast<pragma::CEOnUpdateRenderData&>(evData.get()).commandBuffer,isDirty);
	});
	BindEvent(CRenderComponent::EVENT_UPDATE_INSTANTIABILITY,[this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		// TODO: Allow instantiability for animated entities
		static_cast<CEUpdateInstantiability&>(evData.get()).instantiable = false;
		return util::EventReply::Handled;
	});
	auto &ent = GetEntity();
	auto pRenderComponent = ent.GetComponent<CRenderComponent>();
	if(pRenderComponent.valid())
	{
		pRenderComponent->SetRenderBufferDirty();
		pRenderComponent->UpdateInstantiability();
	}
}

void CSkAnimatedComponent::OnRemove()
{
	BaseSkAnimatedComponent::OnRemove();
	auto pRenderComponent = GetEntity().GetComponent<CRenderComponent>();
	if(pRenderComponent.valid())
		pRenderComponent->SetRenderBufferDirty();
}

void CSkAnimatedComponent::ReceiveData(NetPacket &packet)
{
	int anim = packet->Read<int>();
	float cycle = packet->Read<float>();
	PlayAnimation(anim);
	SetCycle(cycle);
}

std::optional<Mat4> CSkAnimatedComponent::GetVertexTransformMatrix(const ModelSubMesh &subMesh,uint32_t vertexId) const
{
	return GetVertexTransformMatrix(subMesh,vertexId,nullptr,nullptr);
}
std::optional<Mat4> CSkAnimatedComponent::GetVertexTransformMatrix(const ModelSubMesh &subMesh,uint32_t vertexId,Vector3 *optOutNormalOffset,float *optOutDelta) const
{
	if(optOutNormalOffset)
		*optOutNormalOffset = {};
	if(optOutDelta)
		*optOutDelta = 0.f;
	auto t = BaseSkAnimatedComponent::GetVertexTransformMatrix(subMesh,vertexId);
	if(t.has_value() == false)
		return {};
	Vector3 vertexOffset;
	auto pVertexAnimatedComponent = GetEntity().GetComponent<pragma::CVertexAnimatedComponent>();
	if(pVertexAnimatedComponent.expired() || pVertexAnimatedComponent->GetLocalVertexPosition(subMesh,vertexId,vertexOffset,optOutNormalOffset,optOutDelta) == false)
		return t;
	return *t *glm::translate(umat::identity(),vertexOffset); // TODO: Confirm order!
}
#if ENABLE_LEGACY_ANIMATION_SYSTEM
void CSkAnimatedComponent::ResetAnimation(const std::shared_ptr<Model> &mdl)
{
	BaseSkAnimatedComponent::ResetAnimation(mdl);
	m_boneMatrices.clear();
	if(mdl == nullptr || GetBoneCount() == 0)
		return;
	m_boneMatrices.resize(mdl->GetBoneCount(),umat::identity());
	UpdateBoneMatricesMT();
	SetBoneBufferDirty();

	// Attach particles defined in the model
	auto &ent = GetEntity();
	auto pParentComponent = ent.GetComponent<CParentComponent>();
	for(auto &objAttachment : mdl->GetObjectAttachments())
	{
		switch(objAttachment.type)
		{
			case ObjectAttachment::Type::Model:
				Con::cwar<<"WARNING: Unsupported object attachment type '"<<umath::to_integral(objAttachment.type)<<"'!"<<Con::endl;
				break;
			case ObjectAttachment::Type::ParticleSystem:
				auto itParticleFile = objAttachment.keyValues.find("particle_file");
				if(itParticleFile != objAttachment.keyValues.end())
					CParticleSystemComponent::Precache(itParticleFile->second);
				auto itParticle = objAttachment.keyValues.find("particle");
				if(itParticle != objAttachment.keyValues.end())
				{
					Vector3 translation {};
					auto rotation = uquat::identity();

					auto itTranslation = objAttachment.keyValues.find("translation");
					if(itTranslation != objAttachment.keyValues.end())
						translation = uvec::create(itTranslation->second);

					auto itRotation = objAttachment.keyValues.find("rotation");
					if(itRotation != objAttachment.keyValues.end())
						rotation = uquat::create(itRotation->second);

					auto itAngles = objAttachment.keyValues.find("angles");
					if(itAngles != objAttachment.keyValues.end())
						rotation = uquat::create(EulerAngles(itAngles->second));

					auto *pt = CParticleSystemComponent::Create(itParticle->second);
					if(pt != nullptr)
					{
						auto &entPt = pt->GetEntity();
						auto pAttachableComponent = entPt.AddComponent<CAttachableComponent>();
						if(pAttachableComponent.valid())
						{
							auto pTrComponentPt = entPt.GetTransformComponent();
							if(pTrComponentPt)
							{
								pTrComponentPt->SetPosition(translation);
								pTrComponentPt->SetRotation(rotation);
							}
							pAttachableComponent->AttachToAttachment(&ent,objAttachment.attachment);
						}
						pt->Start();
					}
				}
				break;
		}
	}
}
#endif
prosper::SwapBuffer *CSkAnimatedComponent::GetSwapBoneBuffer() {return m_boneBuffer.get();}
const prosper::IBuffer *CSkAnimatedComponent::GetBoneBuffer() const {return m_boneBuffer ? &m_boneBuffer->GetBuffer() : nullptr;}
void CSkAnimatedComponent::InitializeBoneBuffer()
{
	if(m_boneBuffer != nullptr)
		return;
	m_boneBuffer = prosper::SwapBuffer::Create(*pragma::get_instance_bone_buffer());

	CEOnBoneBufferInitialized evData{m_boneBuffer};
	BroadcastEvent(EVENT_ON_BONE_BUFFER_INITIALIZED,evData);
}
void CSkAnimatedComponent::UpdateBoneBuffer(prosper::IPrimaryCommandBuffer &commandBuffer,bool flagAsDirty)
{
	auto numBones = GetBoneCount();
	if(m_boneBuffer && numBones > 0u && m_boneMatrices.empty() == false)
		m_boneBuffer->Update(0ull,GetBoneCount() *sizeof(Mat4),m_boneMatrices.data(),flagAsDirty);
}
const std::vector<Mat4> &CSkAnimatedComponent::GetBoneMatrices() const {return const_cast<CSkAnimatedComponent*>(this)->GetBoneMatrices();}
std::vector<Mat4> &CSkAnimatedComponent::GetBoneMatrices() {return m_boneMatrices;}

bool CSkAnimatedComponent::MaintainAnimations(double dt)
{
#pragma message("TODO: Undo this and do it properly!")
	//if(BaseEntity::MaintainAnimations() == false)
	//	return false;
	if(ShouldUpdateBones() == false)
		return false;
	BaseSkAnimatedComponent::MaintainAnimations(dt);
	SetBoneBufferDirty(); // TODO: Only if anything has actually changed
	return true;
}

void CSkAnimatedComponent::UpdateBoneMatricesMT()
{
	auto &mdl = GetEntity().GetModel();
	if(mdl == nullptr)
		return;
	auto *bindPose = GetBindPose();
	if(m_boneMatrices.empty() || bindPose == nullptr)
		return;
	UpdateSkeleton(); // Costly
	auto physRootBoneId = OnSkeletonUpdated();

	auto callbacksEnabled = AreSkeletonUpdateCallbacksEnabled();
	if(callbacksEnabled)
	{
		CEOnSkeletonUpdated evData{physRootBoneId};
		InvokeEventCallbacks(EVENT_ON_SKELETON_UPDATED,evData);
	}

	auto &refFrame = *bindPose;
	auto numBones = GetBoneCount();
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	if(numBones != m_processedBones.size())
	{
		Con::cwar<<"Bone count mismatch between processed bones and actual bones for entity ";
		GetEntity().print(Con::cout);
		Con::cwar<<Con::endl;
		return;
	}
	for(unsigned int i=0;i<GetBoneCount();i++)
	{
		if(i == m_rootPoseBoneId)
			continue;
		auto &t = m_processedBones.at(i);
		auto &pos = t.GetOrigin();
		auto &orientation = t.GetRotation();
		auto &scale = t.GetScale();

		auto *poseBind = refFrame.GetTransform(i);
		if(poseBind)
		{
			auto &mat = m_boneMatrices[i];
			if(i != physRootBoneId)
			{
				umath::Transform tBindPose {*poseBind};
				tBindPose = tBindPose.GetInverse();
				umath::ScaledTransform tBone {pos,orientation,scale};

				mat = tBone.ToMatrix() *tBindPose.ToMatrix();
				//mat = (tBone *tBindPose).ToMatrix();
			}
			else
				mat = umat::identity();
		}
		else
			Con::cwar<<"WARNING: Attempted to update bone "<<i<<" in "<<mdl->GetName()<<" which doesn't exist in the reference pose! Ignoring..."<<Con::endl;
	}
	if(callbacksEnabled)
		InvokeEventCallbacks(EVENT_ON_BONE_MATRICES_UPDATED);
#endif
}

uint32_t CSkAnimatedComponent::OnSkeletonUpdated() {return std::numeric_limits<uint32_t>::max();}

//////////////

CEOnSkeletonUpdated::CEOnSkeletonUpdated(uint32_t &physRootBoneId)
	: physRootBoneId{physRootBoneId}
{}
void CEOnSkeletonUpdated::PushArguments(lua_State *l)
{
	Lua::PushInt(l,physRootBoneId);
}
uint32_t CEOnSkeletonUpdated::GetReturnCount() {return 1u;}
void CEOnSkeletonUpdated::HandleReturnValues(lua_State *l)
{
	if(Lua::IsSet(l,-1))
		physRootBoneId = Lua::CheckInt(l,-1);
}

//////////////

CEOnBoneBufferInitialized::CEOnBoneBufferInitialized(const std::shared_ptr<prosper::SwapBuffer> &buffer)
	: buffer{buffer}
{}
void CEOnBoneBufferInitialized::PushArguments(lua_State *l)
{
	Lua::Push<std::shared_ptr<Lua::Vulkan::SwapBuffer>>(l,buffer);
}
