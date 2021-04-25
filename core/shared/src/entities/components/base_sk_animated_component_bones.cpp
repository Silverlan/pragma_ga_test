/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_shared.h"
#include "pragma/entities/components/base_sk_animated_component.hpp"
#include "pragma/entities/components/base_model_component.hpp"
#include "pragma/entities/components/base_transform_component.hpp"
#include "pragma/model/animation/animation.hpp"
#include "pragma/model/model.h"

using namespace pragma;
#pragma optimize("",off)
static void get_local_bone_position(std::vector<umath::ScaledTransform> &transforms,std::shared_ptr<Bone> &bone,const Vector3 &fscale={1.f,1.f,1.f},Vector3 *pos=nullptr,Quat *rot=nullptr,Vector3 *scale=nullptr)
{
	std::function<void(std::shared_ptr<Bone>&,Vector3*,Quat*,Vector3*)> apply;
	apply = [&transforms,&apply,fscale](std::shared_ptr<Bone> &bone,Vector3 *pos,Quat *rot,Vector3 *scale) {
		auto parent = bone->parent.lock();
		if(parent != nullptr)
			apply(parent,pos,rot,scale);
		auto &tParent = transforms[bone->ID];
		auto &posParent = tParent.GetOrigin();
		auto &rotParent = tParent.GetRotation();
		auto inv = uquat::get_inverse(rotParent);
		if(pos != nullptr)
		{
			*pos -= posParent *fscale;
			uvec::rotate(pos,inv);
		}
		if(rot != nullptr)
			*rot = inv *(*rot);
	};
	auto parent = bone->parent.lock();
	if(parent != nullptr)
		apply(parent,pos,rot,scale);
}
static void get_local_bone_position(const std::shared_ptr<Model> &mdl,std::vector<umath::ScaledTransform> &transforms,std::shared_ptr<Bone> &bone,const Vector3 &fscale={1.f,1.f,1.f},Vector3 *pos=nullptr,Quat *rot=nullptr,Vector3 *scale=nullptr)
{
	get_local_bone_position(transforms,bone,fscale,pos,rot,scale);

	// Obsolete? Not sure what this was for
	/*if(rot == nullptr)
		return;
	auto anim = mdl->GetAnimation(0);
	if(anim != nullptr)
	{
		auto frame = anim->GetFrame(0); // Reference pose
		if(frame != nullptr)
		{
			auto *frameRot = frame->GetBoneOrientation(0); // Rotation of root bone
			if(frameRot != nullptr)
				*rot *= *frameRot;
		}
	}*/
}
UInt32 BaseSkAnimatedComponent::GetBoneCount() const {return CInt32(m_currentPose.GetTransforms().size());}
const pragma::animation::AnimatedPose &BaseSkAnimatedComponent::GetBoneTransforms() const {return const_cast<BaseSkAnimatedComponent&>(*this).GetBoneTransforms();}
pragma::animation::AnimatedPose &BaseSkAnimatedComponent::GetBoneTransforms() {return m_currentPose;}
const pragma::animation::AnimatedPose &BaseSkAnimatedComponent::GetProcessedBoneTransforms() const {return const_cast<BaseSkAnimatedComponent&>(*this).GetProcessedBoneTransforms();}
pragma::animation::AnimatedPose &BaseSkAnimatedComponent::GetProcessedBoneTransforms() {return m_currentPoseEntitySpace;}

Bool BaseSkAnimatedComponent::GetBonePosition(UInt32 boneId,Vector3 &pos,Quat &rot,Vector3 &scale) const
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return false;
	pos = transform[boneId].GetOrigin();
	rot = transform[boneId].GetRotation();
	scale = transform[boneId].GetScale();
	return true;
}
Bool BaseSkAnimatedComponent::GetBonePosition(UInt32 boneId,Vector3 &pos,Quat &rot) const
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return false;
	pos = transform[boneId].GetOrigin();
	rot = transform[boneId].GetRotation();
	return true;
}
Bool BaseSkAnimatedComponent::GetBonePosition(UInt32 boneId,Vector3 &pos) const
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return false;
	pos = transform[boneId].GetOrigin();
	return true;
}
Bool BaseSkAnimatedComponent::GetBoneRotation(UInt32 boneId,Quat &rot) const
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return false;
	rot = transform[boneId].GetRotation();
	return true;
}
Bool BaseSkAnimatedComponent::GetBonePosition(UInt32 boneId,Vector3 &pos,EulerAngles &ang) const
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return false;
	pos = transform[boneId].GetOrigin();
	ang = EulerAngles(transform[boneId].GetRotation());
	return true;
}
Bool BaseSkAnimatedComponent::GetBoneAngles(UInt32 boneId,EulerAngles &ang) const
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return false;
	ang = EulerAngles(transform[boneId].GetRotation());
	return true;
}
const Vector3 *BaseSkAnimatedComponent::GetBonePosition(UInt32 boneId) const
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return nullptr;
	return &transform[boneId].GetOrigin();
}
const Quat *BaseSkAnimatedComponent::GetBoneRotation(UInt32 boneId) const
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return nullptr;
	return &transform[boneId].GetRotation();
}
// See also lanimation.cpp
Bool BaseSkAnimatedComponent::GetLocalBonePosition(UInt32 boneId,Vector3 &pos,Quat &rot,Vector3 *scale) const
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return false;
	auto &hModel = GetEntity().GetModel();
	if(hModel == nullptr)
		return false;
	auto &skeleton = hModel->GetSkeleton();
	auto bone = skeleton.GetBone(boneId).lock();
	if(bone == nullptr)
		return false;
	umath::ScaledTransform t {};
	while(bone)
	{
		auto &boneTransform = transform.at(bone->ID);
		t = boneTransform *t;
		bone = bone->parent.lock();
	}
	pos = t.GetOrigin();
	rot = t.GetRotation();
	if(scale)
		*scale = t.GetScale();
	return true;
}
Bool BaseSkAnimatedComponent::GetLocalBonePosition(UInt32 boneId,Vector3 &pos) const
{
	Quat rot;
	if(GetLocalBonePosition(boneId,pos,rot) == false)
		return false;
	return true;
}
Bool BaseSkAnimatedComponent::GetLocalBoneRotation(UInt32 boneId,Quat &rot) const
{
	Vector3 pos;
	if(GetLocalBonePosition(boneId,pos,rot) == false)
		return false;
	return true;
}
Bool BaseSkAnimatedComponent::GetGlobalBonePosition(UInt32 boneId,Vector3 &pos,Quat &rot,Vector3 *scale) const
{
	if(GetLocalBonePosition(boneId,pos,rot,scale) == false)
		return false;
	auto pTrComponent = GetEntity().GetTransformComponent();
	if(!pTrComponent)
		return true;
	uvec::local_to_world(pTrComponent->GetOrigin(),pTrComponent->GetRotation(),pos,rot);//uvec::local_to_world(GetOrigin(),GetOrientation(),pos,rot);
	return true;
}
Bool BaseSkAnimatedComponent::GetGlobalBonePosition(UInt32 boneId,Vector3 &pos) const
{
	if(GetLocalBonePosition(boneId,pos) == false)
		return false;
	auto pTrComponent = GetEntity().GetTransformComponent();
	if(!pTrComponent)
		return true;
	uvec::local_to_world(pTrComponent->GetOrigin(),pTrComponent->GetRotation(),pos);//uvec::local_to_world(GetOrigin(),GetOrientation(),pos);
	return true;
}
Bool BaseSkAnimatedComponent::GetGlobalBoneRotation(UInt32 boneId,Quat &rot) const
{
	if(GetLocalBoneRotation(boneId,rot) == false)
		return false;
	auto pTrComponent = GetEntity().GetTransformComponent();
	if(!pTrComponent)
		return true;
	uvec::local_to_world(pTrComponent->GetRotation(),rot);
	return true;
}
void BaseSkAnimatedComponent::SetBoneScale(uint32_t boneId,const Vector3 &scale)
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return;
	transform.at(boneId).SetScale(scale);
	umath::set_flag(m_stateFlags,StateFlags::AbsolutePosesDirty);
}
const Vector3 *BaseSkAnimatedComponent::GetBoneScale(uint32_t boneId) const
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return nullptr;
	return &transform.at(boneId).GetScale();
}
void BaseSkAnimatedComponent::SetBonePosition(UInt32 boneId,const Vector3 &pos,const Quat &rot,const Vector3 *scale,Bool updatePhysics)
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return;
	transform[boneId].SetOrigin(pos);
	transform[boneId].SetRotation(rot);
	if(scale != nullptr)
		transform[boneId].SetScale(*scale);
	umath::set_flag(m_stateFlags,StateFlags::AbsolutePosesDirty);
	//if(updatePhysics == false)
	//	return;
	CEOnBoneTransformChanged evData {boneId,&pos,&rot,scale};
	InvokeEventCallbacks(EVENT_ON_BONE_TRANSFORM_CHANGED,evData);
}
void BaseSkAnimatedComponent::SetBonePosition(UInt32 boneId,const Vector3 &pos,const Quat &rot,const Vector3 &scale) {SetBonePosition(boneId,pos,rot,&scale,true);}
void BaseSkAnimatedComponent::SetBonePosition(UInt32 boneId,const Vector3 &pos,const Quat &rot) {SetBonePosition(boneId,pos,rot,nullptr,true);}
void BaseSkAnimatedComponent::SetBonePosition(UInt32 boneId,const Vector3 &pos,const EulerAngles &ang) {SetBonePosition(boneId,pos,uquat::create(ang));}
void BaseSkAnimatedComponent::SetBonePosition(UInt32 boneId,const Vector3 &pos)
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return;
	transform[boneId].SetOrigin(pos);
	umath::set_flag(m_stateFlags,StateFlags::AbsolutePosesDirty);

	CEOnBoneTransformChanged evData {boneId,&pos,nullptr,nullptr};
	InvokeEventCallbacks(EVENT_ON_BONE_TRANSFORM_CHANGED,evData);
}
void BaseSkAnimatedComponent::SetBoneRotation(UInt32 boneId,const Quat &rot)
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return;
	transform[boneId].SetRotation(rot);
	umath::set_flag(m_stateFlags,StateFlags::AbsolutePosesDirty);

	CEOnBoneTransformChanged evData {boneId,nullptr,&rot,nullptr};
	InvokeEventCallbacks(EVENT_ON_BONE_TRANSFORM_CHANGED,evData);
}

void BaseSkAnimatedComponent::SetLocalBonePosition(UInt32 boneId,const Vector3 &pos,const Quat &rot,const Vector3 &scale)
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return;
	auto &hModel = GetEntity().GetModel();
	if(hModel == nullptr)
		return;
	auto &skeleton = hModel->GetSkeleton();
	auto bone = skeleton.GetBone(boneId).lock();
	if(bone == nullptr)
		return;
	auto npos = pos;
	auto nrot = rot;
	auto nscale = scale;
	auto pTrComponent = GetEntity().GetTransformComponent();
	get_local_bone_position(hModel,transform,bone,pTrComponent ? pTrComponent->GetScale() : Vector3{1.f,1.f,1.f},&npos,&nrot,&nscale);
	SetBonePosition(boneId,npos,nrot,nscale);
}
void BaseSkAnimatedComponent::SetLocalBonePosition(UInt32 boneId,const Vector3 &pos,const Quat &rot)
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return;
	auto &hModel = GetEntity().GetModel();
	if(hModel == nullptr)
		return;
	auto &skeleton = hModel->GetSkeleton();
	auto bone = skeleton.GetBone(boneId).lock();
	if(bone == nullptr)
		return;
	auto npos = pos;
	auto nrot = rot;
	auto pTrComponent = GetEntity().GetTransformComponent();
	get_local_bone_position(hModel,transform,bone,pTrComponent ? pTrComponent->GetScale() : Vector3{1.f,1.f,1.f},&npos,&nrot);
	SetBonePosition(boneId,npos,nrot);
}
void BaseSkAnimatedComponent::SetLocalBonePosition(UInt32 boneId,const Vector3 &pos)
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return;
	auto &hModel = GetEntity().GetModel();
	if(hModel == nullptr)
		return;
	auto &skeleton = hModel->GetSkeleton();
	auto bone = skeleton.GetBone(boneId).lock();
	if(bone == nullptr)
		return;
	auto npos = pos;
	auto pTrComponent = GetEntity().GetTransformComponent();
	get_local_bone_position(hModel,transform,bone,pTrComponent ? pTrComponent->GetScale() : Vector3{1.f,1.f,1.f},&npos);
	SetBonePosition(boneId,npos);
}
void BaseSkAnimatedComponent::SetLocalBoneRotation(UInt32 boneId,const Quat &rot)
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneId >= transform.size())
		return;
	auto &hModel = GetEntity().GetModel();
	if(hModel == nullptr)
		return;
	auto &skeleton = hModel->GetSkeleton();
	auto bone = skeleton.GetBone(boneId).lock();
	if(bone == nullptr)
		return;
	auto nrot = rot;
	auto pTrComponent = GetEntity().GetTransformComponent();
	get_local_bone_position(hModel,transform,bone,pTrComponent ? pTrComponent->GetScale() : Vector3{1.f,1.f,1.f},nullptr,&nrot);
	SetBoneRotation(boneId,nrot);
}

std::optional<Mat4> BaseSkAnimatedComponent::GetBoneMatrix(unsigned int boneID) const
{
	auto &transform = m_currentPose.GetTransforms();
	if(boneID >= transform.size())
		return {};
	return transform[boneID].ToMatrix();
}
bool BaseSkAnimatedComponent::ShouldUpdateBones() const
{
	if(IsPlayingAnimation())
		return true;
	CEShouldUpdateBones evData {};
	return InvokeEventCallbacks(EVENT_SHOULD_UPDATE_BONES,evData) == util::EventReply::Handled && evData.shouldUpdate;
}

#if ENABLE_LEGACY_ANIMATION_SYSTEM
FPlayAnim BaseSkAnimatedComponent::GetBaseAnimationFlags() const {return m_baseAnim.flags;}
void BaseSkAnimatedComponent::SetBaseAnimationFlags(FPlayAnim flags) {m_baseAnim.flags = flags;}

std::optional<FPlayAnim> BaseSkAnimatedComponent::GetLayeredAnimationFlags(uint32_t layerIdx) const
{
	auto it = m_animSlots.find(layerIdx);
	if(it == m_animSlots.end())
		return {};
	return it->second.flags;
}
void BaseSkAnimatedComponent::SetLayeredAnimationFlags(uint32_t layerIdx,FPlayAnim flags)
{
	auto it = m_animSlots.find(layerIdx);
	if(it == m_animSlots.end())
		return;
	it->second.flags = flags;
}

void BaseSkAnimatedComponent::TransformBoneFrames(std::vector<umath::Transform> &bonePoses,std::vector<Vector3> *boneScales,Animation &anim,Frame *frameBlend,bool bAdd)
{
	for(unsigned int i=0;i<bonePoses.size();i++)
	{
		auto &pose = bonePoses[i];
		auto *poseFrame = frameBlend->GetBoneTransform(i);
		auto weight = anim.GetBoneWeight(i);
		if(poseFrame)
		{
			if(bAdd == true)
				pose *= *poseFrame *weight;
			else
				pose.Interpolate(*poseFrame,weight);
		}
		if(boneScales != nullptr)
		{
			auto *scale = frameBlend->GetBoneScale(i);
			if(scale != nullptr)
			{
				if(bAdd == true)
				{
					auto boneScale = *scale;
					for(uint8_t i=0;i<3;++i)
						boneScale[i] = umath::lerp(1.0,boneScale[i],weight);
					boneScales->at(i) *= boneScale;
				}
				else
					boneScales->at(i) = uvec::lerp(boneScales->at(i),*scale,weight);
			}
		}
	}
}
void BaseSkAnimatedComponent::TransformBoneFrames(std::vector<umath::Transform> &tgt,std::vector<Vector3> *boneScales,const std::shared_ptr<Animation> &anim,std::vector<umath::Transform> &add,std::vector<Vector3> *addScales,bool bAdd)
{
	for(auto i=decltype(tgt.size()){0};i<tgt.size();++i)
	{
		auto animBoneIdx = anim->LookupBone(i);
		if(animBoneIdx == -1 || animBoneIdx >= add.size())
			continue;
		auto &pose = tgt.at(i);
		auto weight = anim->GetBoneWeight(i);
		if(bAdd == true)
			pose *= add.at(animBoneIdx) *weight;
		else
			pose.Interpolate(add.at(animBoneIdx),weight);
		if(boneScales != nullptr && addScales != nullptr)
		{
			if(bAdd == true)
			{
				auto boneScale = addScales->at(animBoneIdx);
				for(uint8_t i=0;i<3;++i)
					boneScale[i] = umath::lerp(1.0,boneScale[i],weight);
				boneScales->at(i) *= boneScale;
			}
			else
				boneScales->at(i) = uvec::lerp(boneScales->at(i),addScales->at(animBoneIdx),weight);
		}
	}
}
void BaseSkAnimatedComponent::BlendBonePoses(
	const std::vector<umath::Transform> &srcBonePoses,const std::vector<Vector3> *optSrcBoneScales,
	const std::vector<umath::Transform> &dstBonePoses,const std::vector<Vector3> *optDstBoneScales,
	std::vector<umath::Transform> &outBonePoses,std::vector<Vector3> *optOutBoneScales,
	Animation &anim,float interpFactor
) const
{
	auto numBones = umath::min(srcBonePoses.size(),dstBonePoses.size(),outBonePoses.size());
	auto numScales = (optSrcBoneScales && optDstBoneScales && optOutBoneScales) ? umath::min(optSrcBoneScales->size(),optDstBoneScales->size(),optOutBoneScales->size(),numBones) : 0;
	for(auto boneId=decltype(numBones){0u};boneId<numBones;++boneId)
	{
		auto &srcPose = srcBonePoses.at(boneId);
		auto dstPose = dstBonePoses.at(boneId);
		auto boneWeight = anim.GetBoneWeight(boneId);
		auto boneInterpFactor = boneWeight *interpFactor;
		auto &outPose = (outBonePoses.at(boneId) = srcPose);
		outPose.Interpolate(dstPose,boneInterpFactor);

		// Scaling
		if(boneId >= numScales)
			continue;
		optOutBoneScales->at(boneId) = uvec::lerp(optSrcBoneScales->at(boneId),optDstBoneScales->at(boneId) *boneWeight,interpFactor);
	}
}
#endif
void BaseSkAnimatedComponent::BlendBoneFrames(std::vector<umath::Transform> &tgt,std::vector<Vector3> *tgtScales,std::vector<umath::Transform> &add,std::vector<Vector3> *addScales,float blendScale) const
{
	if(blendScale == 0.f)
		return;
	for(unsigned int i=0;i<umath::min(tgt.size(),add.size());i++)
	{
		auto &pose = tgt.at(i);
		pose.Interpolate(add.at(i),blendScale);
		if(tgtScales != nullptr && addScales != nullptr)
			tgtScales->at(i) = uvec::lerp(tgtScales->at(i),addScales->at(i),blendScale);
	}
}

static void get_global_bone_transforms(std::vector<umath::ScaledTransform> &transforms,std::unordered_map<uint32_t,std::shared_ptr<Bone>> &childBones,const umath::ScaledTransform &tParent={})
{
	for(auto &pair : childBones)
	{
		auto boneId = pair.first;
		auto &bone = pair.second;
		if(boneId >= transforms.size())
			continue;
		auto &t = transforms.at(boneId);
		t.SetOrigin(t.GetOrigin() *tParent.GetScale());
		t = tParent *t;
		get_global_bone_transforms(transforms,bone->children,t);
	}
}
#if ENABLE_LEGACY_ANIMATION_SYSTEM
void BaseSkAnimatedComponent::UpdateSkeleton()
{
	if(umath::is_flag_set(m_stateFlags,StateFlags::AbsolutePosesDirty) == false)
		return;
	auto &hModel = GetEntity().GetModel();
	if(hModel == nullptr)
		return;
	umath::set_flag(m_stateFlags,StateFlags::AbsolutePosesDirty,false);
	auto &skeleton = hModel->GetSkeleton();
	m_currentPoseEntitySpace = m_currentPose;
	get_global_bone_transforms(m_currentPoseEntitySpace,skeleton.GetRootBones());
}
#endif
#pragma optimize("",on)
