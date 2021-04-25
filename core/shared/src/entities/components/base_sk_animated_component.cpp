/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_shared.h"
#include "pragma/entities/components/animated_component.hpp"
#include "pragma/entities/components/base_sk_animated_component.hpp"
#include "pragma/entities/components/base_model_component.hpp"
#include "pragma/entities/components/base_time_scale_component.hpp"
#include "pragma/entities/components/base_physics_component.hpp"
#include "pragma/entities/components/base_sound_emitter_component.hpp"
#include "pragma/entities/entity_component_system_t.hpp"
#include "pragma/model/model.h"
#include "pragma/model/animation/animation_player.hpp"
#include "pragma/model/animation/skeletal_animation.hpp"
#include "pragma/model/animation/animated_pose.hpp"
#include "pragma/model/animation/animation.hpp"
#include "pragma/model/animation/skeleton.h"
#include "pragma/audio/alsound_type.h"
#include "pragma/lua/luafunction_call.h"
#include <sharedutils/datastream.h>
#include <udm.hpp>

#define DEBUG_VERBOSE_ANIMATION 0

using namespace pragma;
#pragma optimize("",off)
ComponentEventId BaseSkAnimatedComponent::EVENT_ON_PLAY_LAYERED_ANIMATION = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_ON_PLAY_LAYERED_ACTIVITY = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_ON_LAYERED_ANIMATION_START = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_ON_LAYERED_ANIMATION_COMPLETE = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_TRANSLATE_LAYERED_ANIMATION = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_TRANSLATE_ACTIVITY = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_MAINTAIN_ANIMATION_MOVEMENT = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_SHOULD_UPDATE_BONES = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_ON_ANIMATION_RESET = pragma::INVALID_COMPONENT_ID;

ComponentEventId BaseSkAnimatedComponent::EVENT_ON_PLAY_ACTIVITY = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_ON_STOP_LAYERED_ANIMATION = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_ON_BONE_TRANSFORM_CHANGED = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_ON_BLEND_ANIMATION = pragma::INVALID_COMPONENT_ID;

ComponentEventId BaseSkAnimatedComponent::EVENT_ON_PLAY_ANIMATION = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_ON_ANIMATION_COMPLETE = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_ON_ANIMATION_START = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_MAINTAIN_ANIMATIONS = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_ON_ANIMATIONS_UPDATED = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_PLAY_ANIMATION = pragma::INVALID_COMPONENT_ID;
ComponentEventId BaseSkAnimatedComponent::EVENT_TRANSLATE_ANIMATION = pragma::INVALID_COMPONENT_ID;
void BaseSkAnimatedComponent::RegisterEvents(pragma::EntityComponentManager &componentManager)
{
	auto componentType = std::type_index(typeid(BaseSkAnimatedComponent));
	EVENT_ON_PLAY_LAYERED_ANIMATION = componentManager.RegisterEvent("ON_PLAY_LAYERED_ANIMATION",componentType);
	EVENT_ON_PLAY_LAYERED_ACTIVITY = componentManager.RegisterEvent("ON_PLAY_LAYERED_ACTIVITY",componentType);
	EVENT_ON_LAYERED_ANIMATION_START = componentManager.RegisterEvent("ON_LAYERED_ANIMATION_START",componentType);
	EVENT_ON_LAYERED_ANIMATION_COMPLETE = componentManager.RegisterEvent("ON_LAYERED_ANIMATION_COMPLETE",componentType);
	EVENT_TRANSLATE_LAYERED_ANIMATION = componentManager.RegisterEvent("TRANSLATE_LAYERED_ANIMATION",componentType);
	EVENT_TRANSLATE_ACTIVITY = componentManager.RegisterEvent("TRANSLATE_ACTIVITY",componentType);
	EVENT_MAINTAIN_ANIMATION_MOVEMENT = componentManager.RegisterEvent("MAINTAIN_ANIMATION_MOVEMENT",componentType);
	EVENT_SHOULD_UPDATE_BONES = componentManager.RegisterEvent("SHOULD_UPDATE_BONES",componentType);

	EVENT_ON_PLAY_ACTIVITY = componentManager.RegisterEvent("ON_PLAY_ACTIVITY",componentType);
	EVENT_ON_STOP_LAYERED_ANIMATION = componentManager.RegisterEvent("ON_STOP_LAYERED_ANIMATION",componentType);
	EVENT_ON_BONE_TRANSFORM_CHANGED = componentManager.RegisterEvent("ON_BONE_TRANSFORM_CHANGED");
	EVENT_ON_BLEND_ANIMATION = componentManager.RegisterEvent("ON_BLEND_ANIMATION",componentType);
	EVENT_ON_ANIMATION_RESET = componentManager.RegisterEvent("ON_ANIMATION_RESET");
	
	EVENT_ON_PLAY_ANIMATION = componentManager.RegisterEvent("ON_PLAY_ANIMATION",componentType);
	EVENT_ON_ANIMATION_COMPLETE = componentManager.RegisterEvent("ON_ANIMATION_COMPLETE",componentType);
	EVENT_ON_ANIMATION_START = componentManager.RegisterEvent("ON_ANIMATION_START",componentType);
	EVENT_MAINTAIN_ANIMATIONS = componentManager.RegisterEvent("MAINTAIN_ANIMATIONS",componentType);
	EVENT_ON_ANIMATIONS_UPDATED = componentManager.RegisterEvent("ON_ANIMATIONS_UPDATED",componentType);
	EVENT_PLAY_ANIMATION = componentManager.RegisterEvent("PLAY_ANIMATION",componentType);
	EVENT_TRANSLATE_ANIMATION = componentManager.RegisterEvent("TRANSLATE_ANIMATION",componentType);
}

BaseSkAnimatedComponent::BaseSkAnimatedComponent(BaseEntity &ent)
	: BaseEntityComponent(ent)
{}

void BaseSkAnimatedComponent::Initialize()
{
	BaseEntityComponent::Initialize();

	BindEventUnhandled(BaseModelComponent::EVENT_ON_MODEL_CHANGED,[this](std::reference_wrapper<pragma::ComponentEvent> evData) {
		OnModelChanged(static_cast<pragma::CEOnModelChanged&>(evData.get()).model);
	});

	BindEventUnhandled(BasePhysicsComponent::EVENT_ON_PRE_PHYSICS_SIMULATE,[this](std::reference_wrapper<pragma::ComponentEvent> evData) {
		if(IsPlayingAnimation() == false)
			return;
		auto pPhysComponent = GetEntity().GetPhysicsComponent();
		if(!pPhysComponent)
			return;
		auto *phys = pPhysComponent->GetPhysicsObject();
		if(phys != nullptr && phys->IsController() == true)
			MaintainAnimationMovement(m_animDisplacement);
	});

	auto &ent = GetEntity();
	ent.AddComponent<AnimatedComponent>();
	auto *mdlComponent = static_cast<pragma::BaseModelComponent*>(ent.AddComponent("model").get());
	if(mdlComponent != nullptr)
	{
		auto &mdl = mdlComponent->GetModel();
		OnModelChanged(mdl);
	}

	SetTickPolicy(TickPolicy::WhenVisible);
}

void BaseSkAnimatedComponent::OnTick(double dt)
{
	if(ShouldUpdateBones() == false)
		return;
	auto &ent = GetEntity();
	auto pTimeScaleComponent = ent.GetTimeScaleComponent();
	MaintainAnimations(dt *(pTimeScaleComponent.valid() ? pTimeScaleComponent->GetEffectiveTimeScale() : 1.f));
}

void BaseSkAnimatedComponent::OnModelChanged(const std::shared_ptr<Model> &mdl)
{
	ResetAnimation(mdl);
	BroadcastEvent(EVENT_ON_ANIMATION_RESET);
}

void BaseSkAnimatedComponent::ResetAnimation(const std::shared_ptr<Model> &mdl)
{
	m_baseAnimationPlayer = nullptr;
	m_layeredAnimationPlayers.clear();
	m_blendControllers.clear();
	m_currentPose.Clear();
	m_currentPoseEntitySpace.Clear();
	m_bindPose = nullptr;
	m_rootPoseBoneId = std::numeric_limits<decltype(m_rootPoseBoneId)>::max();
	umath::set_flag(m_stateFlags,StateFlags::AbsolutePosesDirty);
	ApplyAnimationEventTemplates();
	if(mdl == nullptr || mdl->HasVertexWeights() == false)
		return;
	m_bindPose = mdl->GetReferencePtr();
	std::vector<BlendController> &blendControllers = mdl->GetBlendControllers();
	for(unsigned int i=0;i<blendControllers.size();i++)
	{
		BlendController &blend = blendControllers[i];
		int val;
		if(blend.max < 0)
			val = blend.max;
		else
			val = 0;
		m_blendControllers.insert(std::unordered_map<unsigned int,int>::value_type(i,val));
	}
	auto rootPoseBoneId = mdl->LookupBone(ROOT_POSE_BONE_NAME);
	if(rootPoseBoneId != -1)
		m_rootPoseBoneId = rootPoseBoneId;
	Skeleton &skeleton = mdl->GetSkeleton();
	std::unordered_map<std::string,unsigned int> *animations;
	mdl->GetAnimations(&animations);
	std::unordered_map<std::string,unsigned int>::iterator it;
	int autoplaySlot = 1'200; // Arbitrary start slot number for autoplay layered animations
	for(it=animations->begin();it!=animations->end();it++)
	{
		unsigned int animID = it->second;
		auto anim = mdl->GetAnimation(animID);
		if(anim->HasFlag(FAnim::Autoplay))
		{
			PlayLayeredAnimation(autoplaySlot,animID);
			autoplaySlot++;
		}
	}

	m_currentPose = mdl->GetReference();
	m_currentPose.Localize(skeleton);
}

CallbackHandle BaseSkAnimatedComponent::BindAnimationEvent(AnimationEvent::Type eventId,const std::function<void(std::reference_wrapper<const AnimationEvent>)> &fCallback)
{
	auto it = m_boundAnimEvents.find(eventId);
	if(it != m_boundAnimEvents.end())
	{
		if(it->second.IsValid())
			it->second.Remove();
		m_boundAnimEvents.erase(it);
	}
	auto hCb = FunctionCallback<void,std::reference_wrapper<const AnimationEvent>>::Create(fCallback);
	m_boundAnimEvents.insert(std::make_pair(eventId,hCb));
	return hCb;
}

animation::AnimationPlayer *BaseSkAnimatedComponent::GetBaseAnimationPlayer() {return m_baseAnimationPlayer.get();}
animation::AnimationPlayer *BaseSkAnimatedComponent::GetLayeredAnimationPlayer(animation::LayeredAnimationSlot slot)
{
	auto it = m_layeredAnimationPlayers.find(slot);
	return (it != m_layeredAnimationPlayers.end()) ? it->second.get() : nullptr;
}

void BaseSkAnimatedComponent::OnEntityComponentAdded(BaseEntityComponent &component)
{
	BaseEntityComponent::OnEntityComponentAdded(component);
	if(typeid(component) == typeid(AnimatedComponent))
		m_baseAnimationPlayer = static_cast<AnimatedComponent&>(component).AddAnimationPlayer();
}
void BaseSkAnimatedComponent::OnEntityComponentRemoved(BaseEntityComponent &component)
{
	BaseEntityComponent::OnEntityComponentRemoved(component);
	if(typeid(component) == typeid(AnimatedComponent))
	{
		m_baseAnimationPlayer = nullptr;
		m_layeredAnimationPlayers.clear();
	}
}

bool BaseSkAnimatedComponent::IsPlayingAnimation() const {return (GetAnimation() >= 0) ? true : false;}

float BaseSkAnimatedComponent::GetRemainingAnimationDuration() const
{
	auto *animPlayer = GetBaseAnimationPlayer();
	return animPlayer ? animPlayer->GetRemainingAnimationDuration() : 0.f;
}

int BaseSkAnimatedComponent::SelectWeightedAnimation(Activity activity,animation::AnimationId animAvoid) const
{
	auto &hModel = GetEntity().GetModel();
	if(hModel == nullptr)
		return -1;
	return hModel->SelectWeightedAnimation(activity,animAvoid);
}

void BaseSkAnimatedComponent::SetBlendController(unsigned int controller,float val)
{
	auto &hModel = GetEntity().GetModel();
	if(hModel == nullptr)
		return;
	auto it = m_blendControllers.find(controller);
	if(it == m_blendControllers.end())
		return;
	BlendController *blend = hModel->GetBlendController(it->first);
	if(blend == NULL)
		return;
	//if(it->second != val)
	//	std::cout<<"Changed from "<<it->second<<" to "<<val<<std::endl;
	auto min = static_cast<float>(blend->min);
	auto max = static_cast<float>(blend->max);
	if(val > max)
	{
		if(blend->loop == true)
			val = min +(val -max);
		else
			val = max;
	}
	else if(val < min)
	{
		if(blend->loop == true)
			val = max +val;
		else
			val = min;
	}
	it->second = val;
}
void BaseSkAnimatedComponent::SetBlendController(const std::string &controller,float val)
{
	auto mdlComponent = GetEntity().GetModelComponent();
	if(!mdlComponent)
		return;
	int id = mdlComponent->LookupBlendController(controller);
	if(id == -1)
		return;
	SetBlendController(id,val);
}
const std::unordered_map<unsigned int,float> &BaseSkAnimatedComponent::GetBlendControllers() const {return m_blendControllers;}
float BaseSkAnimatedComponent::GetBlendController(const std::string &controller) const
{
	auto mdlComponent = GetEntity().GetModelComponent();
	if(!mdlComponent)
		return 0.f;
	int id = mdlComponent->LookupBlendController(controller);
	if(id == -1)
		return 0.f;
	return GetBlendController(id);
}
float BaseSkAnimatedComponent::GetBlendController(unsigned int controller) const
{
	auto it = m_blendControllers.find(controller);
	if(it == m_blendControllers.end())
		return 0;
	return it->second;
}
#if ENABLE_LEGACY_ANIMATION_SYSTEM
bool BaseSkAnimatedComponent::MaintainAnimation(AnimationSlotInfo &animInfo,double dt,int32_t layeredSlot)
{
	auto &hModel = GetEntity().GetModel();
	if(hModel == nullptr)
		return false;
	CEMaintainAnimation evData{animInfo,dt};
	if(InvokeEventCallbacks(EVENT_MAINTAIN_ANIMATION,evData) == util::EventReply::Handled)
		return false;
	if(animInfo.animation == -1)
		return false;
	auto animId = animInfo.animation;
	auto anim = hModel->GetAnimation(animId);
	if(anim == nullptr)
		return false;
	auto act = anim->GetActivity();
	auto animSpeed = GetPlaybackRate() *anim->GetAnimationSpeedFactor();

	auto &cycle = animInfo.cycle;
	auto cycleLast = cycle;
	auto cycleNew = cycle +static_cast<float>(dt) *animSpeed;
	if(layeredSlot == -1)
	{
		if(umath::abs(cycleNew -cycleLast) < 0.001f && umath::is_flag_set(m_stateFlags,StateFlags::BaseAnimationDirty) == false)
			return false;
		umath::set_flag(m_stateFlags,StateFlags::BaseAnimationDirty,false);
	}
	auto bLoop = anim->HasFlag(FAnim::Loop) || umath::is_flag_set(animInfo.flags,FPlayAnim::Loop);
	auto bComplete = (cycleNew >= 1.f) ? true : false;
	if(bComplete == true)
	{
		cycle = 1.f;
		if(&animInfo == &m_baseAnim) // Only if this is the main animation
		{
			CEOnAnimationComplete evData{animId,act};
			InvokeEventCallbacks(EVENT_ON_ANIMATION_COMPLETE,evData);
		}
		else
		{
			CELayeredAnimationInfo evData{layeredSlot,animId,act};
			InvokeEventCallbacks(EVENT_ON_LAYERED_ANIMATION_COMPLETE,evData);
		}
		if(cycleLast > 0.f) // If current cycle is 0 but we're also complete, that means the animation was started and finished within a single frame. Calling the block below may result in endless recursion, so we need to make sure the animation stays for this frame.
		{
			if(cycle != 1.f || animId != animInfo.animation)
			{
				SetBaseAnimationDirty();
				return MaintainAnimation(animInfo,dt);
			}
			if(bLoop == true)
			{
				cycleNew -= floor(cycleNew);
				if(anim->HasFlag(FAnim::NoRepeat))
				{
					animId = SelectWeightedAnimation(act,animId);
					cycle = cycleNew;
					SetBaseAnimationDirty();
					return MaintainAnimation(animInfo,dt);
				}
			}
			else
				cycleNew = 1.f;
		}
		else
			cycleNew = 1.f;
		cycle = cycleNew;
	}
	else
		cycle = cycleNew;

#if DEBUG_VERBOSE_ANIMATION == 1
	if(&animInfo == &m_baseAnim)
	{
		Con::cout<<
			GetEntity().GetClass()<<" is playing base animation '"<<hModel->GetAnimationName(animId)<<"'"
			<<": Cycle "<<cycle<<" => "<<cycleNew
			<<"; Looping: "<<(bLoop ? "true" : "false")
			<<"; Frame Count: "<<numFrames
			<<"; Speed: "<<animSpeed<<Con::endl;
	}
#endif

	// TODO: All of this is very inefficient and involves a lot of unnecessary buffers. FIXME

	// Initialize buffer for blended/interpolated animation data
	auto &animBoneList = anim->GetBoneList();
	auto numBones = animBoneList.size();
	std::vector<umath::Transform> bonePoses {};
	std::vector<Vector3> boneScales {};

	// Blend between the last frame and the current frame of this animation.
	Frame *srcFrame,*dstFrame;
	float interpFactor;
	if(GetBlendFramesFromCycle(*anim,cycle,&srcFrame,&dstFrame,interpFactor) == false)
		return false; // This shouldn't happen unless the animation has no frames

	if(dstFrame)
	{
		bonePoses.resize(numBones);
		boneScales.resize(numBones,Vector3{1.f,1.f,1.f});
		BlendBonePoses(
			srcFrame->GetBoneTransforms(),&srcFrame->GetBoneScales(),
			dstFrame->GetBoneTransforms(),&dstFrame->GetBoneScales(),
			bonePoses,&boneScales,
			*anim,interpFactor
		);
	}
	else
	{
		// Destination frame can be nullptr if no interpolation is required.
		bonePoses = srcFrame->GetBoneTransforms();
		boneScales = srcFrame->GetBoneScales();
	}
	//

	// Blend between previous animation and this animation
	float interpFactorLastAnim;
	auto *lastPlayedFrameOfPreviousAnim = GetPreviousAnimationBlendFrame(animInfo,dt,interpFactorLastAnim);
	if(lastPlayedFrameOfPreviousAnim)
	{
		auto lastAnim = hModel->GetAnimation(animInfo.lastAnim.animation);
		if(lastAnim)
		{
			BlendBonePoses(
				lastPlayedFrameOfPreviousAnim->GetBoneTransforms(),&lastPlayedFrameOfPreviousAnim->GetBoneScales(),
				bonePoses,&boneScales,
				bonePoses,&boneScales,
				*lastAnim,interpFactorLastAnim
			);
		}
	}
	//

	// Blend Controllers
	auto *animBcData = anim->GetBlendController();
	if(animBcData)
	{
		auto *bc = hModel->GetBlendController(animBcData->controller);
		if(animBcData->transitions.empty() == false && bc != nullptr)
		{
			auto bcValue = GetBlendController(animBcData->controller);
			auto *trSrc = &animBcData->transitions.front();
			auto *trDst = &animBcData->transitions.back();
			for(auto &tr : animBcData->transitions)
			{
				if(tr.transition <= bcValue && tr.transition > trSrc->transition)
					trSrc = &tr;
				if(tr.transition >= bcValue && tr.transition < trDst->transition)
					trDst = &tr;
			}
			auto offset = (trDst->transition -trSrc->transition);
			auto interpFactor = 0.f;
			if(offset > 0.f)
				interpFactor = (bcValue -trSrc->transition) /offset;

			auto blendAnimSrc = hModel->GetAnimation(trSrc->animation);
			auto blendAnimDst = hModel->GetAnimation(trDst->animation);
			if(blendAnimSrc != nullptr && blendAnimDst != nullptr)
			{
				// Note: A blend controller blends between two different animations. That means that for each animation
				// we have to interpolate the animation's frame, and then interpolate (i.e. blend) the resulting bone poses
				// of both animations.

				// Interpolated poses of source animation
				Frame *srcFrame,*dstFrame;
				float animInterpFactor;
				std::vector<umath::Transform> ppBonePosesSrc {};
				std::vector<Vector3> ppBoneScalesSrc {};
				if(GetBlendFramesFromCycle(*blendAnimSrc,cycle,&srcFrame,&dstFrame,animInterpFactor))
				{
					if(dstFrame)
					{
						ppBonePosesSrc.resize(numBones);
						ppBoneScalesSrc.resize(numBones,Vector3{1.f,1.f,1.f});
						BlendBonePoses(
							srcFrame->GetBoneTransforms(),&srcFrame->GetBoneScales(),
							dstFrame->GetBoneTransforms(),&dstFrame->GetBoneScales(),
							ppBonePosesSrc,&ppBoneScalesSrc,
							*blendAnimSrc,animInterpFactor
						);
					}
					else
					{
						ppBonePosesSrc = srcFrame->GetBoneTransforms();
						ppBoneScalesSrc = srcFrame->GetBoneScales();
					}
				}

				// Interpolated poses of destination animation
				std::vector<umath::Transform> ppBonePosesDst {};
				std::vector<Vector3> ppBoneScalesDst {};
				if(GetBlendFramesFromCycle(*blendAnimDst,cycle,&srcFrame,&dstFrame,animInterpFactor))
				{
					if(dstFrame)
					{
						ppBonePosesDst.resize(numBones);
						ppBoneScalesDst.resize(numBones,Vector3{1.f,1.f,1.f});
						BlendBonePoses(
							srcFrame->GetBoneTransforms(),&srcFrame->GetBoneScales(),
							dstFrame->GetBoneTransforms(),&dstFrame->GetBoneScales(),
							ppBonePosesDst,&ppBoneScalesDst,
							*blendAnimSrc,animInterpFactor
						);
					}
					else
					{
						ppBonePosesDst = srcFrame->GetBoneTransforms();
						ppBoneScalesDst = srcFrame->GetBoneScales();
					}
				}

				// Interpolate between the two frames
				BlendBonePoses(
					ppBonePosesSrc,&ppBoneScalesSrc,
					ppBonePosesDst,&ppBoneScalesDst,
					bonePoses,&boneScales,
					*blendAnimSrc,interpFactor
				);

				if(animBcData->animationPostBlendController != std::numeric_limits<uint32_t>::max() && animBcData->animationPostBlendTarget != std::numeric_limits<uint32_t>::max())
				{
					auto blendAnimPost = hModel->GetAnimation(animBcData->animationPostBlendTarget);
					if(blendAnimPost && GetBlendFramesFromCycle(*blendAnimPost,cycle,&srcFrame,&dstFrame,animInterpFactor))
					{
						if(dstFrame)
						{
							ppBonePosesSrc.resize(numBones);
							ppBoneScalesSrc.resize(numBones,Vector3{1.f,1.f,1.f});
							BlendBonePoses(
								srcFrame->GetBoneTransforms(),&srcFrame->GetBoneScales(),
								dstFrame->GetBoneTransforms(),&dstFrame->GetBoneScales(),
								ppBonePosesSrc,&ppBoneScalesSrc,
								*blendAnimPost,animInterpFactor
							);
						}
						else
						{
							ppBonePosesSrc = srcFrame->GetBoneTransforms();
							ppBoneScalesSrc = srcFrame->GetBoneScales();
						}

						// Interpolate between the two frames
						auto bcValuePostBlend = GetBlendController(animBcData->animationPostBlendController);
						auto interpFactor = 1.f -bcValuePostBlend;
						BlendBonePoses(
							ppBonePosesSrc,&ppBoneScalesSrc,
							bonePoses,&boneScales,
							bonePoses,&boneScales,
							*blendAnimPost,interpFactor
						);
					}
				}
			}
		}
	}
	//

	animInfo.bonePoses = std::move(bonePoses);
	animInfo.boneScales = std::move(boneScales);

	CEOnBlendAnimation evDataBlend{animInfo,act,animInfo.bonePoses,(animInfo.boneScales.empty() == false) ? &animInfo.boneScales : nullptr};
	InvokeEventCallbacks(EVENT_ON_BLEND_ANIMATION,evDataBlend);

	// Animation events
	auto frameLast = (cycleLast != 0.f) ? static_cast<int32_t>((numFrames -1) *cycleLast) : -1;
	auto frameCycle = (numFrames -1) *cycle;
	auto frameID = umath::floor(frameCycle);

	if(frameID < frameLast)
		frameID = numFrames;

	m_animEventQueue.push({});
	auto &eventItem = m_animEventQueue.back();
	eventItem.animId = animId;
	eventItem.animation = anim;
	eventItem.frameId = frameID;
	eventItem.lastFrame = frameLast;
	return true;
}
#endif
void BaseSkAnimatedComponent::SetBindPose(const std::shared_ptr<pragma::animation::AnimatedPose> &bindPose) {m_bindPose = bindPose;}
const pragma::animation::AnimatedPose *BaseSkAnimatedComponent::GetBindPose() const {return m_bindPose.get();}

void BaseSkAnimatedComponent::SetAnimatedRootPoseTransformEnabled(bool enabled) {umath::set_flag(m_stateFlags,StateFlags::RootPoseTransformEnabled,enabled);}
bool BaseSkAnimatedComponent::IsAnimatedRootPoseTransformEnabled() const {return umath::is_flag_set(m_stateFlags,StateFlags::RootPoseTransformEnabled);}
#if ENABLE_LEGACY_ANIMATION_SYSTEM
BoneId BaseSkAnimatedComponent::AddRootPoseBone()
{
	auto &ent = GetEntity();
	auto &mdl = ent.GetModel();
	if(mdl == nullptr)
		return std::numeric_limits<BoneId>::max();
	auto &skeleton = mdl->GetSkeleton();
	auto boneId = skeleton.LookupBone(ROOT_POSE_BONE_NAME);
	if(boneId == -1)
	{
		auto *bone = new Bone {};
		bone->name = ROOT_POSE_BONE_NAME;
		boneId = skeleton.AddBone(bone);
		skeleton.GetRootBones()[boneId] = bone->shared_from_this();
	}
	if(boneId >= m_bones.size())
	{
		m_bones.resize(boneId +1,umath::ScaledTransform{});
		m_processedBones.resize(boneId +1,umath::ScaledTransform{});
	}
	SetRootPoseBoneId(boneId);
	return boneId;
}
#endif
void BaseSkAnimatedComponent::SetRootPoseBoneId(BoneId boneId) {m_rootPoseBoneId = boneId;}

bool BaseSkAnimatedComponent::MaintainAnimations(double dt)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto &hModel = GetEntity().GetModel();
	if(hModel == nullptr)
		return false;
	CEMaintainAnimations evData{dt};
	if(InvokeEventCallbacks(EVENT_MAINTAIN_ANIMATIONS,evData) == util::EventReply::Handled)
		return true;

	auto r = MaintainAnimation(m_baseAnim,dt);
	if(r == true)
	{
		auto &animInfo = m_baseAnim;
		auto anim = hModel->GetAnimation(animInfo.animation);
		auto &bones = anim->GetBoneList();
		auto &bonePoses = animInfo.bonePoses;
		auto &boneScales = animInfo.boneScales;

		// Update gestures
		for(auto it=m_animSlots.begin();it!=m_animSlots.end();)
		{
			auto &animInfo = it->second;
			if(MaintainAnimation(animInfo,dt,it->first) == true)
			{
				auto anim = hModel->GetAnimation(animInfo.animation);
				TransformBoneFrames(bonePoses,!boneScales.empty() ? &boneScales : nullptr,anim,animInfo.bonePoses,!animInfo.boneScales.empty() ? &animInfo.boneScales : nullptr,anim->HasFlag(FAnim::Gesture));
				if(animInfo.cycle >= 1.f)
				{
					if(anim->HasFlag(FAnim::Loop) == false)
					{
						it = m_animSlots.erase(it); // No need to keep the gesture information around anymore
						continue;
					}
				}
			}
			++it;
		}

		// Apply animation to skeleton
		for(auto i=decltype(bonePoses.size()){0};i<bonePoses.size();++i)
		{
			auto boneId = bones[i];
			auto &orientation = bonePoses.at(i);
			SetBonePosition(boneId,orientation.GetOrigin(),orientation.GetRotation(),nullptr,false);
			if(boneScales.empty() == false)
				SetBoneScale(boneId,boneScales.at(i));
		}
	}

	if(IsAnimatedRootPoseTransformEnabled() && m_rootPoseBoneId != std::numeric_limits<decltype(m_rootPoseBoneId)>::max() && m_rootPoseBoneId < m_bones.size())
	{
		auto &pose = m_bones[m_rootPoseBoneId];
		auto &ent = GetEntity();
		umath::Transform poseWithoutScale {pose};
		ent.SetPose(poseWithoutScale);
	}

	InvokeEventCallbacks(EVENT_ON_ANIMATIONS_UPDATED);

	// It's now safe to execute animation events
	const auto fHandleAnimationEvents = [this](uint32_t animId,const std::shared_ptr<animation::Animation> &anim,int32_t frameId) {
		auto *events = anim->GetEvents(frameId);
		if(events)
		{
			for(auto &ev : *events)
				HandleAnimationEvent(*ev);
		}
		auto *customEvents = GetAnimationEvents(animId,frameId);
		if(customEvents != nullptr)
		{
			for(auto &ev : *customEvents)
			{
				if(ev.callback.first == true) // Is it a callback event?
				{
					if(ev.callback.second.IsValid())
					{
						auto *f = ev.callback.second.get();
						if(typeid(*f) == typeid(LuaCallback))
						{
							auto *lf = static_cast<LuaCallback*>(f);
							lf->Call<void>();
						}
						else
							(*f)();
					}
				}
				else
					HandleAnimationEvent(ev);
			}
		}
	};
	while(!m_animEventQueue.empty())
	{
		auto &eventItem = m_animEventQueue.front();

		for(auto i=eventItem.lastFrame +1;i<=eventItem.frameId;++i)
			fHandleAnimationEvents(eventItem.animId,eventItem.animation,i);

		if(static_cast<int32_t>(eventItem.frameId) < eventItem.lastFrame)
		{
			for(auto i=decltype(eventItem.frameId){0};i<=eventItem.frameId;++i)
				fHandleAnimationEvents(eventItem.animId,eventItem.animation,i);
		}

		m_animEventQueue.pop();
	}
	return r;
#endif
}
Activity BaseSkAnimatedComponent::TranslateActivity(Activity act)
{
	CETranslateActivity evTranslateActivityData {act};
	InvokeEventCallbacks(EVENT_TRANSLATE_ACTIVITY,evTranslateActivityData);
	return act;
}

float BaseSkAnimatedComponent::GetCycle() const
{
	if(!m_baseAnimationPlayer)
		return 0.f;
	return m_baseAnimationPlayer->GetCurrentTime();
}
void BaseSkAnimatedComponent::SetCycle(float cycle)
{
	if(cycle == GetCycle())
		return;
	if(!m_baseAnimationPlayer)
		return;
	m_baseAnimationPlayer->SetCurrentTime(cycle);
	SetBaseAnimationDirty();
}

animation::AnimationId BaseSkAnimatedComponent::GetAnimation() const
{
	return m_baseAnimationPlayer ? m_baseAnimationPlayer->GetCurrentAnimationId() : animation::INVALID_ANIMATION;
}
pragma::animation::Animation *BaseSkAnimatedComponent::GetAnimationObject() const
{
	auto animId = GetAnimation();
	if(animId == -1)
		return nullptr;
	auto &hModel = GetEntity().GetModel();
	if(hModel == nullptr)
		return nullptr;
	auto anim = hModel->GetAnimation(animId);
	if(anim == nullptr)
		return nullptr;
	return anim.get();
}
animation::AnimationId BaseSkAnimatedComponent::GetLayeredAnimation(uint32_t slot) const
{
	auto it = m_layeredAnimationPlayers.find(slot);
	if(it == m_layeredAnimationPlayers.end())
		return animation::INVALID_ANIMATION;
	return it->second->GetCurrentAnimationId();
}
Activity BaseSkAnimatedComponent::GetLayeredActivity(uint32_t slot) const
{
	auto it = m_layeredAnimationPlayers.find(slot);
	if(it == m_layeredAnimationPlayers.end())
		return Activity::Invalid;
	auto &hModel = GetEntity().GetModel();
	auto anim = hModel ? hModel->GetAnimation(it->second->GetCurrentAnimationId()) : nullptr;
	return anim ? animation::skeletal::get_activity(*anim) : Activity::Invalid;
}

void BaseSkAnimatedComponent::PlayAnimation(animation::AnimationId animation,FPlayAnim flags)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto bSkipAnim = false;
	CEOnPlayAnimation evData{m_baseAnim.animation,animation,flags};
	if(InvokeEventCallbacks(EVENT_PLAY_ANIMATION,evData) == util::EventReply::Handled)
		return;
	if(m_baseAnim.animation == animation && (flags &FPlayAnim::Reset) == FPlayAnim::None)
	{
		auto &hModel = GetEntity().GetModel();
		if(hModel != nullptr)
		{
			auto anim = hModel->GetAnimation(animation);
			if(anim != NULL && anim->HasFlag(FAnim::Loop))
				return;
		}
	}
	if(animation < -1)
		animation = -1;

	CETranslateAnimation evTranslateAnimData {animation,flags};
	InvokeEventCallbacks(EVENT_TRANSLATE_ANIMATION,evTranslateAnimData);

	if(animation == m_baseAnim.animation && m_baseAnim.cycle == 0.f && m_baseAnim.flags == flags)
		return; // No change
	auto &lastAnim = m_baseAnim.lastAnim;
	if(m_baseAnim.animation != -1 && m_baseAnim.animation != animation && m_baseAnim.cycle > 0.f)
	{
		lastAnim.animation = m_baseAnim.animation;
		lastAnim.cycle = m_baseAnim.cycle;
		lastAnim.flags = m_baseAnim.flags;
		lastAnim.blendTimeScale = {0.f,0.f};
		lastAnim.blendScale = 1.f;

		// Update animation fade time
		auto &hModel = GetEntity().GetModel();
		if(hModel != nullptr)
		{
			auto anim = hModel->GetAnimation(animation);
			auto animLast = hModel->GetAnimation(m_baseAnim.animation);
			if(anim != nullptr && animLast != nullptr)
			{
				auto bAnimFadeIn = anim->HasFadeInTime();
				auto bAnimLastFadeOut = animLast->HasFadeOutTime();
				auto animFadeIn = anim->GetFadeInTime();
				auto animFadeOut = anim->GetFadeOutTime();
				UNUSED(animFadeOut);
				auto animLastFadeOut = animLast->GetFadeOutTime();
				const auto defaultFadeOutTime = 0.2f;
				if(bAnimFadeIn == true)
				{
					if(bAnimLastFadeOut == true)
						lastAnim.blendTimeScale.first = (animFadeIn > animLastFadeOut) ? animFadeIn : animLastFadeOut;
					else
						lastAnim.blendTimeScale.first = animFadeIn;
				}
				else if(bAnimLastFadeOut == true)
					lastAnim.blendTimeScale.first = animLastFadeOut;
				else
					lastAnim.blendTimeScale.first = defaultFadeOutTime;
				lastAnim.blendTimeScale.second = lastAnim.blendTimeScale.first;
			}
		}
		//
	}
	else
		lastAnim.animation = -1;
	m_baseAnim.animation = animation;
	m_baseAnim.cycle = 0;
	m_baseAnim.flags = flags;
	m_baseAnim.activity = Activity::Invalid;
	SetBaseAnimationDirty();
	auto &hModel = GetEntity().GetModel();
	if(hModel != nullptr)
	{
		auto anim = hModel->GetAnimation(animation);
		if(anim != nullptr)
		{
			m_baseAnim.activity = pragma::animation::skeletal::get_activity(*anim);

			// We'll set all bones that are unused by the animation to
			// their respective reference pose
			auto &ref = hModel->GetReference();
			auto &boneMap = anim->GetBoneMap();
			auto &skeleton = hModel->GetSkeleton();
			auto numBones = skeleton.GetBoneCount();
			for(auto i=decltype(numBones){0u};i<numBones;++i)
			{
				auto bone = skeleton.GetBone(i);
				auto it = boneMap.find(i);
				if(it != boneMap.end() || bone.expired())
					continue;
				auto parent = bone.lock()->parent;
				umath::ScaledTransform poseParent {};
				if(!parent.expired())
				{
					ref.GetBonePose(parent.lock()->ID,poseParent);
					poseParent = poseParent.GetInverse();
				}

				umath::ScaledTransform pose {};
				auto *pos = ref.GetBonePosition(i);
				if(pos)
					pose.SetOrigin(*pos);
				auto *rot = ref.GetBoneOrientation(i);
				if(rot)
					pose.SetRotation(*rot);
				auto *scale = ref.GetBoneScale(i);
				if(scale)
					pose.SetScale(*scale);

				pose = poseParent *pose;
				if(pos)
					SetBonePosition(i,pose.GetOrigin());
				if(rot)
					SetBoneRotation(i,pose.GetRotation());
				if(scale)
					SetBoneScale(i,pose.GetScale());
			}
		}
	}

	CEOnAnimationStart evAnimStartData {m_baseAnim.animation,m_baseAnim.activity,m_baseAnim.flags};
	InvokeEventCallbacks(EVENT_ON_ANIMATION_START,evAnimStartData);
#endif
}
void BaseSkAnimatedComponent::SetBaseAnimationDirty() {umath::set_flag(m_stateFlags,StateFlags::BaseAnimationDirty,true);}
#if ENABLE_LEGACY_ANIMATION_SYSTEM
pragma::animation::AnimationId BaseSkAnimatedComponent::SelectTranslatedAnimation(Activity &inOutActivity) const
{
	inOutActivity = const_cast<BaseSkAnimatedComponent*>(this)->TranslateActivity(inOutActivity);
	return SelectWeightedAnimation(inOutActivity,m_baseAnim.animation);
}
#endif
bool BaseSkAnimatedComponent::PlayActivity(Activity activity,FPlayAnim flags)
{
	if(GetActivity() == activity && (flags &FPlayAnim::Reset) == FPlayAnim::None)
		return true;
	auto seq = SelectTranslatedAnimation(activity);

	CEOnPlayActivity evDataActivity {activity,flags};
	InvokeEventCallbacks(EVENT_ON_PLAY_ACTIVITY,evDataActivity);

	PlayAnimation(seq,flags);
	//m_baseAnim.activity = activity;
	return (seq == -1) ? false : true;
}

Activity BaseSkAnimatedComponent::GetActivity() const
{
	if(!m_baseAnimationPlayer)
		return Activity::Invalid;
	auto &hModel = GetEntity().GetModel();
	auto anim = hModel ? hModel->GetAnimation(m_baseAnimationPlayer->GetCurrentAnimationId()) : nullptr;
	return anim ? animation::skeletal::get_activity(*anim) : Activity::Invalid;
}

#if ENABLE_LEGACY_ANIMATION_SYSTEM
void BaseSkAnimatedComponent::HandleAnimationEvent(const AnimationEvent &ev)
{
	auto bHandled = false;
	CEHandleAnimationEvent evData{ev};
	if(InvokeEventCallbacks(EVENT_HANDLE_ANIMATION_EVENT,evData) == util::EventReply::Handled)
		return;
	auto it = m_boundAnimEvents.find(ev.eventID);
	if(it != m_boundAnimEvents.end())
	{
		it->second.Call<void,std::reference_wrapper<const AnimationEvent>>(ev);
		return;
	}
	switch(ev.eventID)
	{
		case AnimationEvent::Type::EmitSound:
		{
			if(ev.arguments.size() > 0)
			{
				auto pSoundEmitterComponent = static_cast<pragma::BaseSoundEmitterComponent*>(GetEntity().FindComponent("sound_emitter").get());
				if(pSoundEmitterComponent != nullptr)
					pSoundEmitterComponent->EmitSharedSound(ev.arguments.front(),ALSoundType::Generic);
			}
			break;
		}
		default:
			;//Con::cout<<"WARNING: Unhandled animation event "<<ev->eventID<<Con::endl;
	}
}
#endif
bool BaseSkAnimatedComponent::PlayAnimation(const std::string &name,FPlayAnim flags)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto mdlComponent = GetEntity().GetModelComponent();
	if(!mdlComponent)
		return false;
	auto prevAnim = GetAnimation();
	int anim = mdlComponent->LookupAnimation(name);

	CEOnPlayAnimation evData {prevAnim,anim,flags};
	if(InvokeEventCallbacks(EVENT_ON_PLAY_ANIMATION,evData) == util::EventReply::Handled)
		return false;

	PlayAnimation(anim,flags);
	return true;
#endif
}
void BaseSkAnimatedComponent::PlayLayeredAnimation(animation::LayeredAnimationSlot slot,animation::AnimationId animation,FPlayAnim flags)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto prevAnim = GetLayeredAnimation(slot);

	CETranslateLayeredAnimation evData {slot,animation,flags};
	InvokeEventCallbacks(EVENT_TRANSLATE_LAYERED_ANIMATION,evData);

	CEOnPlayLayeredAnimation evDataPlay {slot,prevAnim,animation,flags};
	InvokeEventCallbacks(EVENT_ON_PLAY_LAYERED_ANIMATION,evDataPlay);

	auto &slotInfo = m_animSlots[slot] = {animation};
	slotInfo.flags = flags;
	if(animInfo != nullptr)
		*animInfo = &slotInfo;

	CELayeredAnimationInfo evDataStart {slot,slotInfo.animation,slotInfo.activity};
	InvokeEventCallbacks(EVENT_ON_LAYERED_ANIMATION_START,evDataStart);
#endif
}
void BaseSkAnimatedComponent::PlayLayeredAnimation(animation::LayeredAnimationSlot slot,animation::AnimationId animation,FPlayAnim flags)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	PlayLayeredAnimation(slot,animation,flags,nullptr);
#endif
}
bool BaseSkAnimatedComponent::PlayLayeredAnimation(animation::LayeredAnimationSlot slot,std::string animation,FPlayAnim flags)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto mdlComponent = GetEntity().GetModelComponent();
	if(!mdlComponent)
		return false;
	auto anim = mdlComponent->LookupAnimation(animation);
	if(anim == -1)
		return false;
	PlayLayeredAnimation(slot,anim,flags);
	return true;
#endif
}
bool BaseSkAnimatedComponent::PlayLayeredActivity(animation::LayeredAnimationSlot slot,Activity activity,FPlayAnim flags)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	CEOnPlayLayeredActivity evData{slot,activity,flags};
	InvokeEventCallbacks(EVENT_ON_PLAY_LAYERED_ACTIVITY,evData);

	int32_t animAvoid = -1;
	auto it = m_animSlots.find(slot);
	if(it != m_animSlots.end())
		animAvoid = it->second.animation;
	auto seq = SelectWeightedAnimation(activity,animAvoid);
	AnimationSlotInfo *animInfo = nullptr;
	PlayLayeredAnimation(slot,seq,flags,&animInfo);
	if(animInfo != nullptr)
		animInfo->activity = activity;
	return (seq == -1) ? false : true;
#endif
}
void BaseSkAnimatedComponent::StopLayeredAnimation(animation::LayeredAnimationSlot slot)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto it = m_animSlots.find(slot);
	if(it == m_animSlots.end())
		return;
	CEOnStopLayeredAnimation evData{slot,it->second};
	InvokeEventCallbacks(EVENT_ON_STOP_LAYERED_ANIMATION,evData);
	m_animSlots.erase(it);
#endif
}
const pragma::animation::AnimatedPose &BaseSkAnimatedComponent::GetProcessedBones() const {return const_cast<BaseSkAnimatedComponent*>(this)->GetProcessedBones();}
pragma::animation::AnimatedPose &BaseSkAnimatedComponent::GetProcessedBones()
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	return m_processedBones;
#endif
}

bool BaseSkAnimatedComponent::CalcAnimationMovementSpeed(float *x,float *z,int32_t frameOffset) const
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto &ent = GetEntity();
	auto &hMdl = ent.GetModel();
	auto animId = GetAnimation();
	if(hMdl == nullptr || animId == -1)
		return false;
	auto anim = hMdl->GetAnimation(animId);
	if(anim == nullptr || (((x != nullptr && anim->HasFlag(FAnim::MoveX) == false) || x == nullptr) && ((z != nullptr && anim->HasFlag(FAnim::MoveZ) == false) || z == nullptr)))
		return false;

	std::array<Frame*,2> frames = {nullptr,nullptr};
	auto blendScale = 0.f;
	if(GetBlendFramesFromCycle(*anim,GetCycle(),&frames[0],&frames[1],blendScale,frameOffset) == false)
		return false; // Animation doesn't have any frames?
	auto animSpeed = GetPlaybackRate();
	std::array<float,2> blendScales = {1.f -blendScale,blendScale};
	Vector2 mvOffset {0.f,0.f};
	for(auto i=decltype(frames.size()){0};i<frames.size();++i)
	{
		auto *frame = frames[i];
		if(frame == nullptr)
			continue;
		auto *moveOffset = frame->GetMoveOffset();
		if(moveOffset == nullptr)
			continue;
		mvOffset += *moveOffset *blendScales[i] *animSpeed;
	}
	if(x != nullptr)
		*x = mvOffset.x;
	if(z != nullptr)
		*z = mvOffset.y;
	return true;
#endif
}
static void write_anim_flags(udm::LinkedPropertyWrapper &udm,FPlayAnim flags)
{
	udm::write_flag(udm["flags"],flags,FPlayAnim::Reset,"reset");
	udm::write_flag(udm["flags"],flags,FPlayAnim::Transmit,"transmit");
	udm::write_flag(udm["flags"],flags,FPlayAnim::SnapTo,"snapTo");
	udm::write_flag(udm["flags"],flags,FPlayAnim::Loop,"loop");
	static_assert(magic_enum::flags::enum_count<FPlayAnim>() == 4);
}

static FPlayAnim read_anim_flags(udm::LinkedPropertyWrapper &udm)
{
	auto flags = FPlayAnim::None;
	udm::read_flag(udm["flags"],flags,FPlayAnim::Reset,"reset");
	udm::read_flag(udm["flags"],flags,FPlayAnim::Transmit,"transmit");
	udm::read_flag(udm["flags"],flags,FPlayAnim::SnapTo,"snapTo");
	udm::read_flag(udm["flags"],flags,FPlayAnim::Loop,"loop");
	static_assert(magic_enum::flags::enum_count<FPlayAnim>() == 4);
	return flags;
}

#if ENABLE_LEGACY_ANIMATION_SYSTEM
static void write_animation_slot_info(udm::LinkedPropertyWrapper &udm,const BaseSkAnimatedComponent::AnimationSlotInfo &slotInfo)
{
	udm["activity"] = slotInfo.activity;
	udm["animation"] = slotInfo.animation;
	udm["cycle"] = slotInfo.cycle;
	write_anim_flags(udm["flags"],slotInfo.flags);

	udm["bonePoses"] = udm::compress_lz4_blob(slotInfo.bonePoses);
	udm["boneScales"] = udm::compress_lz4_blob(slotInfo.boneScales);
	udm["bonePosesBc"] = udm::compress_lz4_blob(slotInfo.bonePosesBc);
	udm["boneScalesBc"] = udm::compress_lz4_blob(slotInfo.boneScalesBc);
		
	udm["lastAnimation"]["animation"] = slotInfo.lastAnim.animation;
	udm["lastAnimation"]["cycle"] = slotInfo.lastAnim.cycle;
	write_anim_flags(udm["lastAnimation"]["flags"],slotInfo.lastAnim.flags);
	udm["lastAnimation"]["blendFadeIn"] = slotInfo.lastAnim.blendTimeScale.first;
	udm["lastAnimation"]["blendFadeOut"] = slotInfo.lastAnim.blendTimeScale.second;
	udm["lastAnimation"]["blendScale"] = slotInfo.lastAnim.blendScale;
}
#endif
void BaseSkAnimatedComponent::Save(udm::LinkedPropertyWrapper &udm)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	BaseEntityComponent::Save(udm);
	udm["playbackRate"] = GetPlaybackRate();

	// Write blend controllers
	auto &blendControllers = GetBlendControllers();
	auto udmBlendControllers = udm.AddArray("blendControllers",blendControllers.size());
	uint32_t idx = 0;
	for(auto &pair : blendControllers)
	{
		auto udmBlendController = udmBlendControllers[idx++];
		udmBlendController["slot"] = pair.first;
		udmBlendController["value"] = pair.second;
	}

	// Write animations
	write_animation_slot_info(udm,GetBaseAnimationInfo());
	auto &animSlotInfos = GetAnimationSlotInfos();
	auto udmAnimations = udm.AddArray("animations",animSlotInfos.size());
	idx = 0;
	for(auto &pair : animSlotInfos)
	{
		auto udmAnimation = udmAnimations[idx++];
		udmAnimation["slot"] = pair.first;
		write_animation_slot_info(udmAnimation,pair.second);
	}

	udm["animDisplacement"] = m_animDisplacement;
#endif
}

#if ENABLE_LEGACY_ANIMATION_SYSTEM
static void read_animation_slot_info(udm::LinkedPropertyWrapper &udm,BaseSkAnimatedComponent::AnimationSlotInfo &slotInfo)
{
	udm["activity"](slotInfo.activity);
	udm["animation"](slotInfo.animation);
	udm["cycle"](slotInfo.cycle);
	slotInfo.flags = read_anim_flags(udm["flags"]);

	udm["bonePoses"].GetBlobData(slotInfo.bonePoses);
	udm["boneScales"].GetBlobData(slotInfo.boneScales);
	udm["bonePosesBc"].GetBlobData(slotInfo.bonePosesBc);
	udm["boneScalesBc"].GetBlobData(slotInfo.boneScalesBc);

	udm["lastAnimation"]["animation"](slotInfo.lastAnim.animation);
	udm["lastAnimation"]["cycle"](slotInfo.lastAnim.cycle);
	udm["lastAnimation"]["blendFadeIn"](slotInfo.lastAnim.blendTimeScale.first);
	udm["lastAnimation"]["blendFadeOut"](slotInfo.lastAnim.blendTimeScale.second);
	slotInfo.lastAnim.flags = read_anim_flags(udm["lastAnimation"]["flags"]);
	udm["lastAnimation"]["blendScale"](slotInfo.lastAnim.blendScale);
}
#endif

void BaseSkAnimatedComponent::Load(udm::LinkedPropertyWrapper &udm,uint32_t version)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	BaseEntityComponent::Load(udm,version);
	auto playbackRate = GetPlaybackRate();;
	udm["playbackRate"](playbackRate);
	SetPlaybackRate(playbackRate);

	// Read blend controllers
	auto udmBlendControllers = udm["blendControllers"];
	auto numBlendControllers = udmBlendControllers.GetSize();
	m_blendControllers.reserve(numBlendControllers);
	for(auto i=decltype(numBlendControllers){0u};i<numBlendControllers;++i)
	{
		auto udmBlendController = udmBlendControllers[i];
		uint32_t slot = 0;
		udmBlendController["slot"](slot);
		auto value = 0.f;
		udmBlendController["value"](value);
		m_blendControllers[slot] = value;
	}

	// Read animations
	read_animation_slot_info(udm,GetBaseAnimationInfo());
	auto &animSlots = GetAnimationSlotInfos();
	auto udmAnimations = udm["animations"];
	auto numAnims = udmAnimations.GetSize();
	animSlots.reserve(numAnims);
	for(auto i=decltype(numAnims){0u};i<numAnims;++i)
	{
		auto udmAnimation = udmAnimations[i];
		uint32_t slot = 0;
		udmAnimation["slot"](slot);
		auto it = animSlots.insert(std::make_pair(slot,AnimationSlotInfo{})).first;
		read_animation_slot_info(udmAnimation,it->second);
	}

	udm["animDisplacement"](m_animDisplacement);
#endif
}
/////////////////

CESkelOnPlayAnimation::CESkelOnPlayAnimation(animation::AnimationId prevAnim,animation::AnimationId animation,pragma::FPlayAnim flags)
	: previousAnimation{prevAnim},animation(animation),flags(flags)
{}
void CESkelOnPlayAnimation::PushArguments(lua_State *l)
{
	Lua::PushInt(l,previousAnimation);
	Lua::PushInt(l,animation);
	Lua::PushInt(l,umath::to_integral(flags));
}

/////////////////

CEOnPlayLayeredAnimation::CEOnPlayLayeredAnimation(animation::LayeredAnimationSlot slot,animation::AnimationId previousAnimation,animation::AnimationId animation,pragma::FPlayAnim flags)
	: CESkelOnPlayAnimation(previousAnimation,animation,flags),slot(slot)
{}
void CEOnPlayLayeredAnimation::PushArguments(lua_State *l)
{
	CESkelOnPlayAnimation::PushArguments(l);
	Lua::PushInt(l,slot);
}

/////////////////

CETranslateLayeredActivity::CETranslateLayeredActivity(animation::LayeredAnimationSlot &slot,Activity &activity,pragma::FPlayAnim &flags)
	: slot(slot),activity(activity),flags(flags)
{}
void CETranslateLayeredActivity::PushArguments(lua_State *l)
{
	Lua::PushInt(l,slot);
	Lua::PushInt(l,umath::to_integral(activity));
	Lua::PushInt(l,umath::to_integral(flags));
}
uint32_t CETranslateLayeredActivity::GetReturnCount() {return 3;}
void CETranslateLayeredActivity::HandleReturnValues(lua_State *l)
{
	if(Lua::IsSet(l,-3))
		slot = Lua::CheckInt(l,-3);
	if(Lua::IsSet(l,-2))
		activity = static_cast<Activity>(Lua::CheckInt(l,-2));
	if(Lua::IsSet(l,-1))
		flags = static_cast<pragma::FPlayAnim>(Lua::CheckInt(l,-1));
}

/////////////////

CELayeredAnimationInfo::CELayeredAnimationInfo(animation::LayeredAnimationSlot slot,animation::AnimationId animation,Activity activity)
	: slot(slot),animation(animation),activity(activity)
{}
void CELayeredAnimationInfo::PushArguments(lua_State *l)
{
	Lua::PushInt(l,slot);
	Lua::PushInt(l,animation);
	Lua::PushInt(l,umath::to_integral(activity));
}

/////////////////

CETranslateLayeredAnimation::CETranslateLayeredAnimation(animation::LayeredAnimationSlot &slot,animation::AnimationId &animation,pragma::FPlayAnim &flags)
	: slot(slot),animation(animation),flags(flags)
{}
void CETranslateLayeredAnimation::PushArguments(lua_State *l)
{
	Lua::PushInt(l,slot);
	Lua::PushInt(l,animation);
	Lua::PushInt(l,umath::to_integral(flags));
}
uint32_t CETranslateLayeredAnimation::GetReturnCount() {return 3;}
void CETranslateLayeredAnimation::HandleReturnValues(lua_State *l)
{
	if(Lua::IsSet(l,-3))
		slot = Lua::CheckInt(l,-3);
	if(Lua::IsSet(l,-2))
		animation = Lua::CheckInt(l,-2);
	if(Lua::IsSet(l,-1))
		flags = static_cast<pragma::FPlayAnim>(Lua::CheckInt(l,-1));
}

/////////////////

CETranslateActivity::CETranslateActivity(Activity &activity)
	: activity(activity)
{}
void CETranslateActivity::PushArguments(lua_State *l)
{
	Lua::PushInt(l,umath::to_integral(activity));
}
uint32_t CETranslateActivity::GetReturnCount() {return 1;}
void CETranslateActivity::HandleReturnValues(lua_State *l)
{
	if(Lua::IsSet(l,-1))
		activity = static_cast<Activity>(Lua::CheckInt(l,-1));
}

/////////////////

CEOnBoneTransformChanged::CEOnBoneTransformChanged(UInt32 boneId,const Vector3 *pos,const Quat *rot,const Vector3 *scale)
	: boneId{boneId},pos{pos},rot{rot},scale{scale}
{}
void CEOnBoneTransformChanged::CEOnBoneTransformChanged::PushArguments(lua_State *l)
{
	Lua::PushInt(l,boneId);
	if(pos != nullptr)
		Lua::Push<Vector3>(l,*pos);
	else
		Lua::PushNil(l);

	if(rot != nullptr)
		Lua::Push<Quat>(l,*rot);
	else
		Lua::PushNil(l);

	if(scale != nullptr)
		Lua::Push<Vector3>(l,*scale);
	else
		Lua::PushNil(l);
}

/////////////////

CEOnPlayActivity::CEOnPlayActivity(Activity activity,FPlayAnim flags)
	: activity{activity},flags{flags}
{}
void CEOnPlayActivity::PushArguments(lua_State *l)
{
	Lua::PushInt(l,umath::to_integral(activity));
	Lua::PushInt(l,umath::to_integral(flags));
}

/////////////////

CEOnPlayLayeredActivity::CEOnPlayLayeredActivity(animation::LayeredAnimationSlot slot,Activity activity,FPlayAnim flags)
	: slot{slot},activity{activity},flags{flags}
{}
void CEOnPlayLayeredActivity::PushArguments(lua_State *l)
{
	Lua::PushInt(l,slot);
	Lua::PushInt(l,umath::to_integral(activity));
	Lua::PushInt(l,umath::to_integral(flags));
}

/////////////////

CEOnStopLayeredAnimation::CEOnStopLayeredAnimation(animation::LayeredAnimationSlot slot)
	: slot{slot}
{}
void CEOnStopLayeredAnimation::PushArguments(lua_State *l)
{
	Lua::PushInt(l,slot);
}

/////////////////

CEMaintainAnimationMovement::CEMaintainAnimationMovement(const Vector3 &displacement)
	: displacement{displacement}
{}
void CEMaintainAnimationMovement::PushArguments(lua_State *l)
{
	Lua::Push<Vector3>(l,displacement);
}

/////////////////

CEShouldUpdateBones::CEShouldUpdateBones()
{}
void CEShouldUpdateBones::PushArguments(lua_State *l) {}
#pragma optimize("",on)
