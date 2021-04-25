/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_shared.h"
#include "pragma/model/animation/animation.hpp"
#include "pragma/model/animation/animated_pose.hpp"
#include "pragma/model/animation/animation_channel.hpp"
#include "pragma/model/animation/skeletal_animation.hpp"
#include "pragma/model/animation/activities.h"
#include "pragma/model/model.h"
#include <udm.hpp>
#include <mathutil/umath.h>
#pragma optimize("",off)
decltype(pragma::animation::Animation::s_eventEnumRegister) pragma::animation::Animation::s_eventEnumRegister;
util::EnumRegister &pragma::animation::Animation::GetEventEnumRegister() {return s_eventEnumRegister;}

std::shared_ptr<pragma::animation::Animation> pragma::animation::Animation::Load(const udm::AssetData &data,std::string &outErr,const Skeleton *optSkeleton,const AnimatedPose *optReference)
{
	auto anim = Animation::Create();
	if(anim->LoadFromAssetData(data,outErr,optSkeleton,optReference) == false)
		return nullptr;
	return anim;
}

template<class T0,class T1>
	static void copy_safe(const T0 &src,T1 &dst,uint32_t srcStartIndex,uint32_t dstStartIndex,uint32_t count)
{
	auto *memPtr0 = reinterpret_cast<const uint8_t*>(src.data());
	auto *memPtr1 = reinterpret_cast<uint8_t*>(dst.data());
	memPtr0 += srcStartIndex *sizeof(T0::value_type);
	memPtr1 += dstStartIndex *sizeof(T1::value_type);

	auto *memPtr0End = memPtr0 +sizeof(T0::value_type) *src.size();
	auto *memPtr1End = memPtr1 +sizeof(T1::value_type) *dst.size();

	if(memPtr0 > memPtr0End)
		throw std::runtime_error{"Memory out of bounds!"};

	if(memPtr1 > memPtr1End)
		throw std::runtime_error{"Memory out of bounds!"};

	auto *memPtr0Write = memPtr0 +count *sizeof(T0::value_type);
	auto *memPtr1WriteEnd = memPtr1 +count *sizeof(T0::value_type);

	if(memPtr0Write > memPtr0End)
		throw std::runtime_error{"Memory out of bounds!"};

	if(memPtr1WriteEnd > memPtr1End)
		throw std::runtime_error{"Memory out of bounds!"};

	memcpy(memPtr1,memPtr0,count *sizeof(T0::value_type));
}

template<typename T>
	static void apply_channel_animation_values(
		udm::LinkedPropertyWrapper &udmProp,float fps,const std::vector<float> &times,const std::vector<std::shared_ptr<Frame>> &frames,
		const std::function<void(Frame&,const T&)> &applyValue
)
{
	if(fps == 0.f)
		return;
	std::vector<T> values;
	udmProp(values);
	auto stepTime = 1.f /fps;
	for(auto i=decltype(times.size()){0u};i<times.size();++i)
	{
		auto t = times[i];
		auto frameEnd = umath::round(t *fps);
		auto frameStart = frameEnd;
		if(i > 0)
		{
			auto tPrev = times[i -1];
			auto dt = t -tPrev;
			auto nFrames = umath::round(dt /stepTime);
			assert(nFrames > 0);
			if(nFrames > 0)
				frameStart = frameEnd -(nFrames -1);
		}
		else if(i == 0 && times.size() == 1)
			frameEnd = frames.size() -1;
		for(auto frameIdx=frameStart;frameIdx<=frameEnd;++frameIdx)
		{
			auto &frame = frames[frameIdx];
			applyValue(*frame,values[i]);
		}
	}
}

bool pragma::animation::Animation::LoadFromAssetData(const udm::AssetData &data,std::string &outErr,const Skeleton *optSkeleton,const AnimatedPose *optReference)
{
	if(data.GetAssetType() != PANIM_IDENTIFIER)
	{
		outErr = "Incorrect format!";
		return false;
	}

	auto udm = *data;
	auto version = data.GetAssetVersion();
	if(version < 1)
	{
		outErr = "Invalid version!";
		return false;
	}
	// if(version > PANIM_VERSION)
	// 	return false;
	auto udmProperties = udm["properties"];
	if(udmProperties)
		m_properties = udmProperties.ClaimOwnership();
	udm["duration"](m_duration);

#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto fadeInTime = udm["fadeInTime"];
	if(fadeInTime)
		m_fadeIn = std::make_unique<float>(fadeInTime(0.f));

	auto fadeOutTime = udm["fadeOutTime"];
	if(fadeOutTime)
		m_fadeOut = std::make_unique<float>(fadeOutTime(0.f));

	auto udmBlendController = udm["blendController"];
	if(udmBlendController)
	{
		m_blendController = AnimationBlendController{};
		udmBlendController["controller"](m_blendController->controller);
		
		auto udmTransitions = udmBlendController["transitions"];
		m_blendController->transitions.resize(udmTransitions.GetSize());
		uint32_t idxTransition = 0;
		for(auto &udmTransition : udmTransitions)
		{
			udmTransition["animation"](m_blendController->transitions[idxTransition].animation);
			udmTransition["transition"](m_blendController->transitions[idxTransition].transition);
			++idxTransition;
		}
		udmBlendController["animationPostBlendController"](m_blendController->animationPostBlendController);
		udmBlendController["animationPostBlendTarget"](m_blendController->animationPostBlendTarget);
	}

	m_flags = FAnim::None;
	auto udmFlags = udm["flags"];
	if(udmFlags)
	{
		auto readFlag = [this,&udmFlags](FAnim flag,const std::string &name) {
			auto udmFlag = udmFlags[name];
			if(udmFlag && udmFlag(false))
				m_flags |= flag;
		};
		readFlag(FAnim::Loop,"loop");
		readFlag(FAnim::NoRepeat,"noRepeat");
		readFlag(FAnim::Autoplay,"autoplay");
		readFlag(FAnim::Gesture,"gesture");
		readFlag(FAnim::NoMoveBlend,"noMoveBlend");
		static_assert(umath::to_integral(FAnim::Count) == 7,"Update this list when new flags have been added!");
	}

	std::vector<BoneId> nodeToLocalBoneId;
	if(udm["bones"])
	{
		// Backwards compatibility
		udm["bones"](m_boneIds);
		auto numBones = m_boneIds.size();
		m_boneIdMap.reserve(numBones);
		for(auto i=decltype(numBones){0u};i<numBones;++i)
			m_boneIdMap[m_boneIds[i]] = i;
	
		udm["boneWeights"](m_boneWeights);
	}
	else
	{
		auto udmNodes = udm["nodes"];
		m_boneIds.reserve(udmNodes.GetSize());
		uint32_t nodeIdx = 0;
		for(auto udmNode : udmNodes)
		{
			std::string type;
			udmNode["type"](type);
			if(type == "bone")
			{
				if(udmNode["bone"])
				{
					uint32_t boneIdx = 0;
					udmNode["bone"](boneIdx);
					nodeToLocalBoneId[nodeIdx] = m_boneIds.size();
					m_boneIds.push_back(boneIdx);

					if(udmNode["weight"])
					{
						auto weight = 0.f;
						udmNode["weight"](weight);
						m_boneWeights.push_back(weight);
					}
				}
				else
				{
					auto offset = m_boneIds.size();
					auto udmSet = udmNode["set"];
					if(m_boneIds.empty())
						udmSet(m_boneIds);
					else
					{
						auto n = udmSet.GetSize();
						m_boneIds.resize(offset +n);
						udmSet.GetBlobData(m_boneIds.data() +offset,n *sizeof(BoneId),udm::Type::UInt16);
					}
					nodeToLocalBoneId.resize(m_boneIds.size(),std::numeric_limits<BoneId>::max());
					for(auto i=offset;i<m_boneIds.size();++i)
						nodeToLocalBoneId[nodeIdx +(i -offset)] = i;

					auto udmWeights = udmNode["weights"];
					if(udmWeights)
					{
						if(m_boneWeights.empty())
							udmSet(m_boneWeights);
						else
						{
							auto n = udmSet.GetSize();
							auto offset = m_boneWeights.size();
							m_boneWeights.resize(offset +n);
							udmSet.GetBlobData(m_boneWeights.data() +offset,n *sizeof(float),udm::Type::Float);
						}
					}
				}
			}
			auto udmSet = udmNode["set"];
			nodeIdx += udmSet ? udmSet.GetSize() : 1;
		}

		auto numBones = m_boneIds.size();
		m_boneIdMap.reserve(numBones);
		for(auto i=decltype(numBones){0u};i<numBones;++i)
			m_boneIdMap[m_boneIds[i]] = i;
	}

	auto udmFrameData = udm["frameTransforms"];
	if(udmFrameData)
	{
		// Backwards compatibility
		auto numBones = m_boneIds.size();
		std::vector<umath::Transform> transforms;
		udmFrameData(transforms);
		if(!transforms.empty())
		{
			auto numFrames = transforms.size() /m_boneIds.size();
			m_frames.resize(numFrames);
			uint32_t offset = 0;
			for(auto &frame : m_frames)
			{
				frame = Frame::Create(numBones);
				auto &frameTransforms = frame->GetBoneTransforms();
				copy_safe(transforms,frameTransforms,offset,0,frameTransforms.size());
				offset += frameTransforms.size();
			}
		}

		auto udmFrameScales = udm["frameScales"];
		if(udmFrameScales)
		{
			std::vector<Vector3> scales;
			udmFrameScales(scales);
			if(!scales.empty())
			{
				uint32_t offset = 0;
				for(auto &frame : m_frames)
				{
					auto &frameScales = frame->GetBoneScales();
					frameScales.resize(numBones);
					copy_safe(scales,frameScales,offset,0,frameScales.size());
					offset += frameScales.size();
				}
			}
		}

		auto udmFrameMoveTranslations = udm["frameMoveTranslations"];
		if(udmFrameMoveTranslations)
		{
			std::vector<Vector2> moveTranslations;
			udmFrameMoveTranslations(moveTranslations);
			if(!moveTranslations.empty())
			{
				for(auto i=decltype(m_frames.size()){0u};i<m_frames.size();++i)
				{
					auto &frame = m_frames[i];
					frame->SetMoveOffset(moveTranslations[i]);
				}
			}
		}
	}
	else
	{
		auto numFrames = umath::round(duration *m_fps);
		m_frames.resize(numFrames);
		auto isGesture = umath::is_flag_set(m_flags,FAnim::Gesture);
		for(auto &frame : m_frames)
		{
			frame = Frame::Create(m_boneIds.size());
			if(isGesture || !optReference || !optSkeleton)
				continue;
			auto &refBones = optSkeleton->GetBones();
			for(auto boneIdx=decltype(refBones.size()){0u};boneIdx<refBones.size();++boneIdx)
			{
				auto it = m_boneIdMap.find(boneIdx);
				if(it == m_boneIdMap.end())
					continue;
				auto localBoneId = it->second;
				auto *pos = optReference->GetBonePosition(boneIdx);
				if(pos)
					frame->SetBonePosition(localBoneId,*pos);

				auto *rot = optReference->GetBoneOrientation(boneIdx);
				if(rot)
					frame->SetBoneOrientation(localBoneId,*rot);

				auto *scale = optReference->GetBoneScale(boneIdx);
				if(scale)
					frame->SetBoneScale(localBoneId,*scale);
			}
		}
		auto udmChannels = udm["channels"];
		for(auto udmChannel : udmChannels)
		{
			uint16_t nodeId = 0;
			udmChannel["node"](nodeId);
			auto localBoneId = nodeToLocalBoneId[nodeId];

			std::vector<float> times;
			udmChannel["times"](times);

			std::string property;
			udmChannel["property"](property);

			auto udmValues = udmChannel["values"];
			if(property == "position")
			{
				apply_channel_animation_values<Vector3>(
					udmValues,m_fps,times,m_frames,
					[localBoneId](Frame &frame,const Vector3 &val) {frame.SetBonePosition(localBoneId,val);
				});
			}
			else if(property == "rotation")
			{
				apply_channel_animation_values<Quat>(
					udmValues,m_fps,times,m_frames,
					[localBoneId](Frame &frame,const Quat &val) {frame.SetBoneOrientation(localBoneId,val);
				});
			}
			else if(property == "scale")
			{
				apply_channel_animation_values<Vector3>(
					udmValues,m_fps,times,m_frames,
					[localBoneId](Frame &frame,const Vector3 &val) {frame.SetBoneScale(localBoneId,val);
				});
			}
			else if(property == "move")
			{
				// TODO
				std::vector<Vector2> moveOffsets;
				udmValues(moveOffsets);
				for(auto i=decltype(times.size()){0u};i<times.size();++i)
				{
					auto t = times[i];
					auto frameIdx = umath::round(t *m_fps);
					auto &frame = m_frames[frameIdx];
					frame->SetMoveOffset(moveOffsets[i]);
				}
			}
		}
	}

	auto udmEvents = udm["events"];
	if(udmEvents)
	{
		for(auto &udmEvent : udmEvents)
		{
			auto frameIndex = udmEvent["frame"].ToValue<uint32_t>();
			if(frameIndex.has_value() == false)
				continue;
			auto it = m_events.find(*frameIndex);
			if(it == m_events.end())
				it = m_events.insert(std::make_pair(*frameIndex,std::vector<std::shared_ptr<AnimationEvent>>{})).first;

			auto &frameEvents = it->second;
			if(frameEvents.size() == frameEvents.capacity())
				frameEvents.reserve(frameEvents.size() *1.1);

			auto name = udmEvent["name"](std::string{});
			if(name.empty())
				continue;
			auto id = Animation::GetEventEnumRegister().RegisterEnum(name);
			if(id == util::EnumRegister::InvalidEnum)
				continue;
			auto ev = std::make_shared<AnimationEvent>();
			ev->eventID = static_cast<AnimationEvent::Type>(id);
			udmEvent["args"](ev->arguments);
			frameEvents.push_back(ev);
		}
	}
#endif
	return true;
}

struct Channel
{
	std::vector<float> times;
	virtual bool CompareValues(const void *v0,const void *v1) const=0;
	virtual bool CompareWithDefault(const void *v) const=0;
	virtual const void *GetValue(size_t idx) const=0;
	virtual size_t GetValueCount() const=0;
	virtual void AddValue(const void *v)=0;
	virtual void PopBack() {times.pop_back();}
	virtual const void *GetReferenceValue(const Frame &reference,uint32_t boneId) const=0;
};

struct PositionChannel : public Channel
{
	std::vector<Vector3> values;
	static const Vector3 &Cast(const void *v) {return *static_cast<const Vector3*>(v);}
	virtual bool CompareValues(const void *v0,const void *v1) const override
	{
		return uvec::distance_sqr(Cast(v0),Cast(v1)) < 0.001f;
	}
	virtual bool CompareWithDefault(const void *v) const override
	{
		return uvec::distance_sqr(Cast(v),uvec::ORIGIN) < 0.001f;
	}
	virtual const void *GetValue(size_t idx) const override {return &values[idx];}
	virtual size_t GetValueCount() const override {return values.size();}
	virtual void AddValue(const void *v) override {values.push_back(Cast(v));}
	virtual void PopBack() override {values.pop_back(); Channel::PopBack();}
	virtual const void *GetReferenceValue(const Frame &reference,uint32_t boneId) const override {return reference.GetBonePosition(boneId);}
};

struct RotationChannel : public Channel
{
	std::vector<Quat> values;
	static const Quat &Cast(const void *v) {return *static_cast<const Quat*>(v);}
	virtual bool CompareValues(const void *v0,const void *v1) const override
	{
		return uquat::distance(Cast(v0),Cast(v1)) < 0.001f;
	}
	virtual bool CompareWithDefault(const void *v) const override
	{
		constexpr Quat identity {1.f,0.f,0.f,0.f};
		return uquat::distance(Cast(v),identity) < 0.001f;
	}
	virtual const void *GetValue(size_t idx) const override {return &values[idx];}
	virtual size_t GetValueCount() const override {return values.size();}
	virtual void AddValue(const void *v) override {values.push_back(Cast(v));}
	virtual void PopBack() override {values.pop_back(); Channel::PopBack();}
	virtual const void *GetReferenceValue(const Frame &reference,uint32_t boneId) const override {return reference.GetBoneOrientation(boneId);}
};

struct ScaleChannel : public Channel
{
	std::vector<Vector3> values;
	static const Vector3 &Cast(const void *v) {return *static_cast<const Vector3*>(v);}
	virtual bool CompareValues(const void *v0,const void *v1) const override
	{
		return uvec::distance_sqr(Cast(v0),Cast(v1)) < 0.001f;
	}
	virtual bool CompareWithDefault(const void *v) const override
	{
		constexpr Vector3 identity {1.f,1.f,1.f};
		return uvec::distance_sqr(Cast(v),identity) < 0.001f;
	}
	virtual const void *GetValue(size_t idx) const override {return &values[idx];}
	virtual size_t GetValueCount() const override {return values.size();}
	virtual void AddValue(const void *v) override {values.push_back(Cast(v));}
	virtual void PopBack() override {values.pop_back(); Channel::PopBack();}
	virtual const void *GetReferenceValue(const Frame &reference,uint32_t boneId) const override {return reference.GetBoneScale(boneId);}
};

struct MoveChannel : public Channel
{
	std::vector<Vector2> values;
	static const Vector2 &Cast(const void *v) {return *static_cast<const Vector2*>(v);}
	virtual bool CompareValues(const void *v0,const void *v1) const override
	{
		return glm::length2(Cast(v0) -Cast(v1)) < 0.001f;
	}
	virtual bool CompareWithDefault(const void *v) const override
	{
		return glm::length2(Cast(v)) < 0.001f;
	}
	virtual const void *GetValue(size_t idx) const override {return &values[idx];}
	virtual size_t GetValueCount() const override {return values.size();}
	virtual void AddValue(const void *v) override {values.push_back(Cast(v));}
	virtual void PopBack() override {values.pop_back(); Channel::PopBack();}
	virtual const void *GetReferenceValue(const Frame &reference,uint32_t boneId) const override {return nullptr;}
};

static constexpr auto ENABLE_ANIMATION_SAVE_OPTIMIZATION = true;
template<class TChannel>
	static void write_channel_value(
	std::shared_ptr<Channel> &channel,uint32_t numFrames,uint32_t frameIdx,float t,
	const std::function<const void*()> &fGetValue
)
{
	if(!channel)
	{
		channel = std::make_shared<TChannel>();
		channel->times.reserve(numFrames);
		static_cast<TChannel*>(channel.get())->values.reserve(numFrames);
	}
	auto *curVal = fGetValue();
	if constexpr(ENABLE_ANIMATION_SAVE_OPTIMIZATION)
	{
		auto &values = static_cast<TChannel*>(channel.get())->values;
		if(values.size() > 1)
		{
			auto &prevPrevVal = values[values.size() -2];
			auto &prevVal = values.back();
			if(channel->CompareValues(&prevVal,&prevPrevVal) && channel->CompareValues(curVal,&prevVal))
			{
				// We got a pair of three matching values, so we
				// can just change the time-value for the last value to
				// our new time
				channel->times.back() = t;
				return;
			}
		}
	}
	channel->times.push_back(t);
	channel->AddValue(curVal);
};

bool pragma::animation::Animation::Save(udm::AssetData &outData,std::string &outErr,const Frame *optReference)
{
	outData.SetAssetType(PANIM_IDENTIFIER);
	outData.SetAssetVersion(PANIM_VERSION);
	auto udm = *outData;

#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto act = GetActivity();
	auto *activityName = Animation::GetActivityEnumRegister().GetEnumName(umath::to_integral(act));
	if(activityName)
		udm["activity"] = *activityName;
	else
		udm.Add("activity",udm::Type::Nil);
	udm["activityWeight"] = static_cast<uint8_t>(GetActivityWeight());

	auto &renderBounds = GetRenderBounds();
	udm["renderBounds"]["min"] = renderBounds.first;
	udm["renderBounds"]["max"] = renderBounds.second;

	udm["properties"] = m_properties;

	udm["duration"] = GetDuration();

	if(HasFadeInTime())
		udm["fadeInTime"] = GetFadeInTime();
	else
		udm.Add("fadeInTime",udm::Type::Nil);
	
	if(HasFadeOutTime())
		udm["fadeOutTime"] = GetFadeOutTime();
	else
		udm.Add("fadeOutTime",udm::Type::Nil);

	auto bones = GetBoneList();
	auto numBones = bones.size();
	auto hasMove = false;
	for(auto &frame : m_frames)
	{
		if(frame->GetMoveOffset())
		{
			hasMove = true;
			break;
		}
	}
	auto numNodes = numBones +(hasMove ? 1u : 0u);

	auto *blendController = GetBlendController();
	if(blendController)
	{
		udm["blendController"]["controller"] = blendController->controller;
		auto &transitions = blendController->transitions;
		auto udmTransitions = udm["blendController"].AddArray("transitions",transitions.size());
		for(auto i=decltype(transitions.size()){0u};i<transitions.size();++i)
		{
			auto &transition = transitions[i];
			auto udmTransition = udmTransitions[i];
			udmTransition["animation"] = transition.animation;
			udmTransition["transition"] = transition.transition;
		}
		udm["blendController"]["animationPostBlendController"] = blendController->animationPostBlendController;
		udm["blendController"]["animationPostBlendTarget"] = blendController->animationPostBlendTarget;
	}

	auto animFlags = GetFlags();
	auto writeFlag = [&udm,animFlags](FAnim flag,const std::string &name) {
		if(umath::is_flag_set(animFlags,flag) == false)
			return;
		udm["flags"][name] = true;
	};
	writeFlag(FAnim::Loop,"loop");
	writeFlag(FAnim::NoRepeat,"noRepeat");
	writeFlag(FAnim::Autoplay,"autoplay");
	writeFlag(FAnim::Gesture,"gesture");
	writeFlag(FAnim::NoMoveBlend,"noMoveBlend");
	static_assert(umath::to_integral(FAnim::Count) == 7,"Update this list when new flags have been added!");

	std::vector<std::unordered_map<std::string,std::shared_ptr<Channel>>> nodeChannels {};
	nodeChannels.resize(numNodes);

	auto numFrames = m_frames.size();
	const auto defaultRotation = uquat::identity();
	const Vector3 defaultScale {1.f,1.f,1.f};
	// Note: Pragma currently uses a frame-based animation system, but the UDM format is
	// channel-based. Pragma will be transitioned to a channel-based system in the future, but
	// until then we'll have to convert it when saving or loading an animation.
	// The saving process also automatically does several optimizations, such as erasing redundant
	// positions/rotations/scales from the animation data.
	for(auto frameIdx=decltype(numFrames){0u};frameIdx<numFrames;++frameIdx)
	{
		auto &frame = m_frames[frameIdx];
		auto &frameTransforms = frame->GetBoneTransforms();
		auto &scales = frame->GetBoneScales();
		if(frameTransforms.size() != numBones || (!scales.empty() && scales.size() != numBones))
		{
			outErr = "Number of transforms (" +std::to_string(frameTransforms.size()) +" in frame does not match number of bones (" +std::to_string(numBones) +")!";
			return false;
		}
		auto t = frameIdx /static_cast<float>(m_fps);
		auto hasScales = frame->HasScaleTransforms();
		for(auto i=decltype(numBones){0u};i<numBones;++i)
		{
			auto &channels = nodeChannels[i];

			write_channel_value<PositionChannel>(channels["position"],numFrames,frameIdx,t,[&frameTransforms,i]() -> const void* {
				return &frameTransforms[i].GetOrigin();
			});
			write_channel_value<RotationChannel>(channels["rotation"],numFrames,frameIdx,t,[&frameTransforms,i]() -> const void* {
				return &frameTransforms[i].GetRotation();
			});
			if(hasScales)
			{
				write_channel_value<ScaleChannel>(channels["scale"],numFrames,frameIdx,t,[&scales,i]() -> const void* {
					return &scales[i];
				});
			}
		}
	}
	
	auto weights = GetBoneWeights();
	if constexpr(ENABLE_ANIMATION_SAVE_OPTIMIZATION)
	{
		// We may be able to remove some channels altogether if they're empty,
		// or are equivalent to the reference pose (or identity value if the animation
		// is a gesture)
		auto isGesture = HasFlag(FAnim::Gesture);
		for(auto it=nodeChannels.begin();it!=nodeChannels.begin() +numBones;)
		{
			auto boneId = bones[it -nodeChannels.begin()];
			auto &channels = *it;
			for(auto &pair : channels)
			{
				// If channel only has two values and they're both the same, we can get rid of the second one
				auto &channel = pair.second;
				auto n = channel->GetValueCount();
				if(n == 2)
				{
					auto *v0 = channel->GetValue(0);
					auto *v1 = channel->GetValue(1);
					if(channel->CompareValues(v0,v1) == false)
						continue;
					channel->PopBack();
				}

				n = channel->GetValueCount();
				if(n != 1)
					continue;
				if(isGesture)
				{
					if(channel->CompareWithDefault(channel->GetValue(0)))
						channel->PopBack();
					continue;
				}
				if(!optReference)
					continue;
				auto *ref = channel->GetReferenceValue(*optReference,boneId);
				if(!ref || !channel->CompareValues(channel->GetValue(0),ref))
					continue;
				channel->PopBack();
			}
			for(auto itChannel=channels.begin();itChannel!=channels.end();)
			{
				auto &channel = itChannel->second;
				if(channel->times.empty())
					itChannel = channels.erase(itChannel);
				else
					++itChannel;
			}
			if(channels.empty())
			{
				auto i = (it -nodeChannels.begin());
				bones.erase(bones.begin() +i);
				if(!weights.empty())
					weights.erase(weights.begin() +i);
				--numBones;
				it = nodeChannels.erase(it);
			}
			else
				++it;
		}
	}

	auto it = std::find_if(weights.begin(),weights.end(),[](const float weight){
		return (weight != 1.f) ? true : false;
	});
	auto hasWeights = (it != weights.end());

	auto udmNodes = udm.AddArray("nodes",1u +(hasMove ? 1u : 0u));
	auto udmNodeBones = udmNodes[0];
	udmNodeBones["type"] = "bone";
	udmNodeBones["set"] = bones;
	if(hasWeights)
		udmNodeBones["weights"] = weights;
	std::optional<uint32_t> nodeMove {};
	if(hasMove)
	{
		auto udmNodeMove = udmNodes[1];
		udmNodeMove["type"] = "move";
		nodeMove = numBones;
	}

	if(nodeMove.has_value())
	{
		for(auto frameIdx=decltype(numFrames){0u};frameIdx<numFrames;++frameIdx)
		{
			auto &frame = m_frames[frameIdx];
			if(!frame->GetMoveOffset())
				continue;
			auto &moveOffset = *frame->GetMoveOffset();
			auto &channels = nodeChannels[*nodeMove];
			auto &moveChannel = channels["offset"];
			if(!moveChannel)
			{
				moveChannel = std::make_shared<MoveChannel>();
				moveChannel->times.reserve(numFrames);
				static_cast<MoveChannel*>(moveChannel.get())->values.reserve(numFrames);
			}
			auto t = frameIdx /static_cast<float>(m_fps);
			moveChannel->times.push_back(t);
			static_cast<MoveChannel*>(moveChannel.get())->values.push_back(*frame->GetMoveOffset());
		}
	}

	uint32_t numChannels = 0;
	for(auto &nodeChannel : nodeChannels)
		numChannels += nodeChannel.size();
	auto udmChannels = udm.AddArray("channels",numChannels,udm::Type::Element,udm::ArrayType::Compressed);
	uint32_t channelIdx = 0;
	for(auto nodeIdx=decltype(nodeChannels.size()){0u};nodeIdx<nodeChannels.size();++nodeIdx)
	{
		for(auto &pair : nodeChannels[nodeIdx])
		{
			auto &channel = pair.second;
			auto udmChannel = udmChannels[channelIdx++];
			udmChannel["node"] = static_cast<uint16_t>(nodeIdx);
			udmChannel["property"] = pair.first;
			udmChannel.AddArray("times",channel->times);
			if(pair.first == "offset")
				udmChannel.AddArray("values",static_cast<MoveChannel&>(*channel).values);
			else if(pair.first == "position")
				udmChannel.AddArray("values",static_cast<PositionChannel&>(*channel).values);
			else if(pair.first == "rotation")
				udmChannel.AddArray("values",static_cast<RotationChannel&>(*channel).values);
			else if(pair.first == "scale")
				udmChannel.AddArray("values",static_cast<ScaleChannel&>(*channel).values);
		}
	}

	uint32_t numEvents = 0;
	for(auto &pair : m_events)
		numEvents += pair.second.size();
	auto udmEvents = udm.AddArray("events",numEvents);
	uint32_t evIdx = 0;
	for(auto &pair : m_events)
	{
		for(auto &ev : pair.second)
		{
			auto udmEvent = udmEvents[evIdx++];
			udmEvent["time"] = pair.first /static_cast<float>(m_fps);

			auto *eventName = Animation::GetEventEnumRegister().GetEnumName(umath::to_integral(ev->eventID));
			udmEvent["name"] = (eventName != nullptr) ? *eventName : "";
			udmEvent["args"] = ev->arguments;
		}
	}
#endif
	return true;
}

bool pragma::animation::Animation::SaveLegacy(VFilePtrReal &f)
{
	f->Write<uint32_t>(PRAGMA_ANIMATION_VERSION);
	auto offsetToLen = f->Tell();
	f->Write<uint64_t>(0);
	auto animFlags = GetFlags();
	auto bMoveX = ((animFlags &FAnim::MoveX) == FAnim::MoveX) ? true : false;
	auto bMoveZ = ((animFlags &FAnim::MoveZ) == FAnim::MoveZ) ? true : false;
	auto bHasMovement = (bMoveX || bMoveZ) ? true : false;

#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto act = GetActivity();
	auto *activityName = Animation::GetActivityEnumRegister().GetEnumName(umath::to_integral(act));
	f->WriteString((activityName != nullptr) ? *activityName : "");

	f->Write<uint8_t>(GetActivityWeight());
	f->Write<uint32_t>(umath::to_integral(animFlags));
	f->Write<uint32_t>(GetFPS());

	// Version 0x0007
	auto &renderBounds = GetRenderBounds();
	f->Write<Vector3>(renderBounds.first);
	f->Write<Vector3>(renderBounds.second);

	auto bFadeIn = HasFadeInTime();
	f->Write<bool>(bFadeIn);
	if(bFadeIn == true)
		f->Write<float>(GetFadeInTime());

	auto bFadeOut = HasFadeOutTime();
	f->Write<bool>(bFadeOut);
	if(bFadeOut == true)
		f->Write<float>(GetFadeOutTime());

	auto &bones = GetBoneList();
	auto numBones = bones.size();
	f->Write<uint32_t>(static_cast<uint32_t>(numBones));
	for(auto &boneId : bones)
		f->Write<uint32_t>(boneId);

	// Version 0x0012
	auto &weights = GetBoneWeights();
	auto it = std::find_if(weights.begin(),weights.end(),[](const float weight){
		return (weight != 1.f) ? true : false;
		});
	if(it == weights.end())
		f->Write<bool>(false);
	else
	{
		f->Write<bool>(true);
		for(auto i=decltype(numBones){0};i<numBones;++i)
			f->Write<float>((i < weights.size()) ? weights.at(i) : 1.f);
	}

	auto *blendController = GetBlendController();
	f->Write<bool>(blendController);
	if(blendController)
	{
		f->Write<int32_t>(blendController->controller);
		auto &transitions = blendController->transitions;
		f->Write<int8_t>(static_cast<int8_t>(transitions.size()));
		for(auto &t : transitions)
		{
			f->Write<uint32_t>(t.animation -1); // Account for reference pose
			f->Write<float>(t.transition);
		}

		f->Write<int32_t>(blendController->animationPostBlendController);
		f->Write<int32_t>(blendController->animationPostBlendTarget);
	}

	auto numFrames = GetFrameCount();
	f->Write<uint32_t>(numFrames);
	for(auto i=decltype(numFrames){0};i<numFrames;++i)
	{
		auto &frame = *GetFrame(i);
		for(auto j=decltype(numBones){0};j<numBones;++j)
		{
			auto &pos = *frame.GetBonePosition(static_cast<uint32_t>(j));
			auto &rot = *frame.GetBoneOrientation(static_cast<uint32_t>(j));
			f->Write<Quat>(rot);
			f->Write<Vector3>(pos);
		}

		if(frame.HasScaleTransforms())
		{
			auto &scales = frame.GetBoneScales();
			f->Write<uint32_t>(scales.size());
			f->Write(scales.data(),scales.size() *sizeof(scales.front()));
		}
		else
			f->Write<uint32_t>(static_cast<uint32_t>(0u));

		auto *animEvents = GetEvents(i);
		auto numEvents = (animEvents != nullptr) ? animEvents->size() : 0;
		f->Write<uint16_t>(static_cast<uint16_t>(numEvents));
		if(animEvents != nullptr)
		{
			for(auto &ev : *animEvents)
			{
				auto *eventName = Animation::GetEventEnumRegister().GetEnumName(umath::to_integral(ev->eventID));
				f->WriteString((eventName != nullptr) ? *eventName : "");
				f->Write<uint8_t>(static_cast<uint8_t>(ev->arguments.size()));
				for(auto &arg : ev->arguments)
					f->WriteString(arg);
			}
		}

		if(bHasMovement == true)
		{
			auto &moveOffset = *frame.GetMoveOffset();
			if(bMoveX == true)
				f->Write<float>(moveOffset.x);
			if(bMoveZ == true)
				f->Write<float>(moveOffset.y);
		}
	}

	auto curOffset = f->Tell();
	auto len = curOffset -offsetToLen;
	f->Seek(offsetToLen);
	f->Write<uint64_t>(len);
	f->Seek(curOffset);
#endif
	return true;
}

std::shared_ptr<pragma::animation::Animation> pragma::animation::Animation::Create()
{
	return std::shared_ptr<Animation>(new Animation{});
}
std::shared_ptr<pragma::animation::Animation> pragma::animation::Animation::Create(const Animation &other,ShareMode share)
{
	return std::shared_ptr<Animation>(new Animation{other,share});
}

pragma::animation::Animation::Animation()
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	: m_flags(FAnim::None),m_activity(Activity::Invalid),m_activityWeight(1),
	m_fadeIn(nullptr),m_fadeOut(nullptr)
#endif
{}

pragma::animation::Animation::Animation(const Animation &other,ShareMode share)
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	: m_flags(other.m_flags),m_activity(other.m_activity),
	m_activityWeight(other.m_activityWeight),m_duration(other.m_duration),m_boneWeights(other.m_boneWeights),
	m_renderBounds(other.m_renderBounds),m_blendController{other.m_blendController}
#endif
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	m_fadeIn = (other.m_fadeIn != nullptr) ? std::make_unique<float>(*other.m_fadeIn) : nullptr;
	m_fadeOut = (other.m_fadeOut != nullptr) ? std::make_unique<float>(*other.m_fadeOut) : nullptr;
#endif
	if((share &ShareMode::Frames) != ShareMode::None)
		m_channels = other.m_channels;
	else
	{
		m_channels.reserve(other.m_channels.size());
		for(auto &channel : other.m_channels)
			m_channels.push_back(std::make_shared<AnimationChannel>(*channel));
	}

#if ENABLE_LEGACY_ANIMATION_SYSTEM
	if((share &ShareMode::Events) != ShareMode::None)
		m_events = other.m_events;
	else
	{
		for(auto &pair : other.m_events)
		{
			m_events[pair.first] = std::vector<std::shared_ptr<AnimationEvent>>{};
			auto &events = m_events[pair.first];
			events.reserve(pair.second.size());
			for(auto &ev : pair.second)
				events.push_back(std::make_unique<AnimationEvent>(*ev));
		}
	}
	static_assert(sizeof(Animation) == 240,"Update this function when making changes to this class!");
#endif
}

#if ENABLE_LEGACY_ANIMATION_SYSTEM
void pragma::animation::Animation::Rotate(const Skeleton &skeleton,const Quat &rot)
{
	uvec::rotate(&m_renderBounds.first,rot);
	uvec::rotate(&m_renderBounds.second,rot);
	for(auto &channel : m_channels)
	{
		if(channel->IsPositionChannel())
		{
			for(auto &v : channel->It<Vector3>())
				uvec::rotate(&v,rot);
			continue;
		}
		if(channel->IsRotationChannel())
		{
			for(auto &v : channel->It<Quat>())
				v = rot *v;
			continue;
		}
	}
}
void pragma::animation::Animation::Translate(const Skeleton &skeleton,const Vector3 &t)
{
	m_renderBounds.first += t;
	m_renderBounds.second += t;
	for(auto &channel : m_channels)
	{
		if(!channel->IsPositionChannel())
			continue;
		for(auto &v : channel->It<Vector3>())
			v += t;
	}
}

void pragma::animation::Animation::Scale(const Vector3 &scale)
{
	m_renderBounds.first *= scale;
	m_renderBounds.second *= scale;
	for(auto &channel : m_channels)
	{
		if(!channel->IsScaleChannel())
			continue;
		for(auto &v : channel->It<Vector3>())
			v *= scale;
	}
}
void pragma::animation::Animation::ToPose(float t,AnimatedPose &outPose,const Skeleton &skeleton,const AnimatedPose *optRefPose)
{
	if(optRefPose)
		outPose = *optRefPose;
	outPose.SetTransformCount(skeleton.GetBoneCount());
	auto &poses = outPose.GetTransforms();
	for(auto &channel : m_channels)
	{
		if(channel->target.nodeType != AnimationChannelNodeType::Bone)
			continue;
		auto boneId = skeleton.LookupBone(channel->target.node);
		if(boneId == -1)
			continue;
		if(channel->IsPositionChannel())
		{
			auto pos = channel->GetInterpolatedValue<Vector3>(t);
			if(boneId < poses.size())
				poses[boneId].SetOrigin(pos);
			continue;
		}
		if(channel->IsRotationChannel())
		{
			auto rot = channel->GetInterpolatedValue<Quat>(t);
			if(boneId < poses.size())
				poses[boneId].SetRotation(rot);
			continue;
		}
		if(channel->IsScaleChannel())
		{
			auto scale = channel->GetInterpolatedValue<Vector3>(t);
			if(boneId < poses.size())
				poses[boneId].SetScale(scale);
			continue;
		}
	}
}

void pragma::animation::Animation::PopulateFromFrames(const Model &mdl,const std::vector<Frame*> &frames,const std::vector<BoneId> &boneIds)
{
	auto &skeleton = mdl.GetSkeleton();
	std::vector<std::array<AnimationChannel*,3>> channels;
	channels.resize(boneIds.size(),std::array<AnimationChannel*,3>{nullptr,nullptr,nullptr});
	m_channels.reserve(m_channels.size() +boneIds.size());
	for(auto i=decltype(boneIds.size()){0u};i<boneIds.size();++i)
	{
		auto boneId = boneIds[i];
		auto bone = skeleton.GetBone(boneId);
		if(bone.expired())
			continue;
		channels[i][0] = AddChannel(AnimationChannelNodeType::Bone,bone.lock()->name,ANIMATION_CHANNEL_PATH_POSITION,ANIMATION_CHANNEL_TYPE_POSITION);
		channels[i][1] = AddChannel(AnimationChannelNodeType::Bone,bone.lock()->name,ANIMATION_CHANNEL_PATH_ROTATION,ANIMATION_CHANNEL_TYPE_ROTATION);
		channels[i][2] = AddChannel(AnimationChannelNodeType::Bone,bone.lock()->name,ANIMATION_CHANNEL_PATH_SCALE,ANIMATION_CHANNEL_TYPE_SCALE);

		for(uint8_t j=0;j<3;++j)
		{
			channels[i][j]->times.resize(frames.size());
			channels[i][j]->values.resize(frames.size());
		}
		
		auto &posChannel = channels[i][0];
		auto &rotChannel = channels[i][1];
		auto &scaleChannel = channels[i][2];
		for(auto iframe=decltype(frames.size()){0u};iframe<frames.size();++iframe)
		{
			auto &frame = *frames[iframe];
			umath::ScaledTransform pose;
			if(frame.GetBonePose(i,pose) == false)
				continue;
			/ TODO: Optimize
		}
	}
}
#endif
pragma::animation::AnimationChannel *pragma::animation::Animation::AddChannel(AnimationChannelNodeType type,const std::string &node,const std::string &path,udm::Type valueType)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto *channel = FindChannel(node,path);
	if(channel)
		return (channel->target.nodeType == type && channel->valueType == valueType) ? channel : nullptr;
	m_channels.push_back(std::make_shared<AnimationChannel>());
	channel = m_channels.back().get();
	channel->valueType = valueType;
	channel->target.nodeType = type;
	channel->target.node = node;
	channel->target.path = path;
	return channel;
#else
	return nullptr;
#endif
}

pragma::animation::AnimationChannel *pragma::animation::Animation::FindChannel(const std::string &node,const std::string &path)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto it = std::find_if(m_channels.begin(),m_channels.end(),[&node,&path](const std::shared_ptr<AnimationChannel> &channel) {
		return channel->target.node == node && channel->target.path == path;
	});
	if(it == m_channels.end())
		return nullptr;
	return it->get();
#else
	return nullptr;
#endif
}
#if ENABLE_LEGACY_ANIMATION_SYSTEM
void pragma::animation::Animation::FindBoneChannels(const std::string &boneName,AnimationChannel **outPos,AnimationChannel **outRot,AnimationChannel **outScale)
{
	for(auto &channel : m_channels)
	{
		if(channel->target.nodeType != pragma::animation::AnimationChannelNodeType::Bone || ustring::compare(boneName,channel->target.node,false) == false)
			continue;
		if(channel->IsPositionChannel())
		{
			if(outPos)
				*outPos = channel.get();
			continue;
		}
		if(channel->IsRotationChannel())
		{
			if(outRot)
				*outRot = channel.get();
			continue;
		}
		if(channel->IsScaleChannel())
		{
			if(outScale)
				*outScale = channel.get();
			continue;
		}
	}
}
void pragma::animation::Animation::FindBoneChannels(const std::string &boneName,const AnimationChannel **outPos,const AnimationChannel **outRot,const AnimationChannel **outScale) const
{
	return const_cast<Animation*>(this)->FindBoneChannels(boneName,const_cast<AnimationChannel**>(outPos),const_cast<AnimationChannel**>(outRot),const_cast<AnimationChannel**>(outScale));
}

void pragma::animation::Animation::CalcRenderBounds(Model &mdl)
{
	// TODO: Base animation / gesture
	m_renderBounds = {
		{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()},
		{std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest()}
	};
	auto duration = GetDuration();
	constexpr uint32_t SAMPLE_RATE = 24; // Samples per second
	auto dt = 1.f /static_cast<float>(SAMPLE_RATE);
	auto &hitboxes = mdl.GetHitboxes();
	auto &skeleton = mdl.GetSkeleton();
	auto &refPose = mdl.GetReference();
	pragma::AnimatedPose pose;
	for(auto &channel : m_channels)
	{
		if(channel->target.nodeType != AnimationChannelNodeType::Bone || !channel->IsPositionChannel())
			continue;
		auto boneId = skeleton.LookupBone(channel->target.node);
		if(boneId == -1)
			continue;
		auto it = hitboxes.find(boneId);
		if(it == hitboxes.end())
			continue;
		auto &hb = *it;

	}


	/*std::unordered_map<BoneId,
	for(auto t=0.f;t<=duration;t=umath::min<float>(t +dt,duration))
	{
		ToPose(t,pose,skeleton,&refPose);
		pose.Globalize(skeleton);

	}*/

	for(auto &frame : m_frames)
	{
		auto frameBounds = frame->CalcRenderBounds(*this,mdl);
		for(uint8_t j=0;j<3;++j)
		{
			if(frameBounds.first[j] < m_renderBounds.first[j])
				m_renderBounds.first[j] = frameBounds.first[j];
			if(frameBounds.first[j] < m_renderBounds.second[j])
				m_renderBounds.second[j] = frameBounds.first[j];

			if(frameBounds.second[j] < m_renderBounds.first[j])
				m_renderBounds.first[j] = frameBounds.second[j];
			if(frameBounds.second[j] < m_renderBounds.second[j])
				m_renderBounds.second[j] = frameBounds.second[j];
		}
	}
}
void SkeletalAnimation::InitializeBoneChannels(const Skeleton &skeleton)
{
	auto numBones = skeleton.GetBoneCount();
	m_boneChannels.resize(numBones);
	for(auto i=decltype(numBones){0u};i<numBones;++i)
	{
		auto wpBone = skeleton.GetBone(i);
		if(wpBone.expired())
			continue;
		auto bone = wpBone.lock();
		pragma::animation::AnimationChannel *posChannel = nullptr;
		pragma::animation::AnimationChannel *rotChannel = nullptr;
		pragma::animation::AnimationChannel *scaleChannel = nullptr;
		Animation::FindBoneChannels(bone->name,&posChannel,&rotChannel,&scaleChannel);

		auto &boneChannels = m_boneChannels[i];
		if(posChannel)
			boneChannels.positionChannel = posChannel->shared_from_this();
		if(rotChannel)
			boneChannels.rotationChannel = rotChannel->shared_from_this();
		if(scaleChannel)
			boneChannels.scaleChannel = scaleChannel->shared_from_this();
	}
}

AnimationBlendController &pragma::animation::Animation::SetBlendController(uint32_t controller)
{
	m_blendController = AnimationBlendController{};
	m_blendController->controller = controller;
	return *m_blendController;
}
AnimationBlendController *pragma::animation::Animation::GetBlendController() {return m_blendController.has_value() ? &*m_blendController : nullptr;}
const AnimationBlendController *pragma::animation::Animation::GetBlendController() const {return const_cast<Animation*>(this)->GetBlendController();}
void pragma::animation::Animation::ClearBlendController() {m_blendController = {};}
#endif
float pragma::animation::Animation::GetFadeInTime()
{
	if(!m_properties)
		return 0.f;
	float fadeIn = 0.f;
	(*m_properties)["fadeIn"](fadeIn);
	return fadeIn;
}
float pragma::animation::Animation::GetFadeOutTime()
{
	if(!m_properties)
		return 0.f;
	float fadeIn = 0.f;
	(*m_properties)["fadeOut"](fadeIn);
	return fadeIn;
}
void pragma::animation::Animation::SetFadeInTime(float t)
{
	if(!m_properties)
		return;
	(*m_properties)["fadeIn"] = t;
}
void pragma::animation::Animation::SetFadeOutTime(float t)
{
	if(!m_properties)
		return;
	(*m_properties)["fadeOut"] = t;
}
bool pragma::animation::Animation::HasFadeInTime() {return m_properties && (*m_properties)["fadeIn"];}
bool pragma::animation::Animation::HasFadeOutTime() {return m_properties && (*m_properties)["fadeOut"];}

#if ENABLE_LEGACY_ANIMATION_SYSTEM
FAnim pragma::animation::Animation::GetFlags() const
{
	auto flags = FAnim::None;
	if(m_properties)
		(*m_properties)["flags"](flags);
	return m_flags;
}
void pragma::animation::Animation::SetFlags(FAnim flags) {m_flags = flags;}
bool pragma::animation::Animation::HasFlag(FAnim flag) const {return ((m_flags &flag) == flag) ? true : false;}
void pragma::animation::Animation::AddFlags(FAnim flags) {m_flags |= flags;}
void pragma::animation::Animation::RemoveFlags(FAnim flags) {m_flags &= ~flags;}
void pragma::animation::Animation::AddEvent(unsigned int frame,AnimationEvent *ev)
{
	auto it = m_events.find(frame);
	if(it == m_events.end())
		m_events[frame] = std::vector<std::shared_ptr<AnimationEvent>>{};
	m_events[frame].push_back(std::shared_ptr<AnimationEvent>(ev));
}

std::vector<std::shared_ptr<AnimationEvent>> *pragma::animation::Animation::GetEvents(unsigned int frame)
{
	auto it = m_events.find(frame);
	if(it == m_events.end())
		return nullptr;
	return &it->second;
}

float pragma::animation::Animation::GetBoneWeight(uint32_t boneId) const
{
	auto weight = 1.f;
	GetBoneWeight(boneId,weight);
	return weight;
}
bool pragma::animation::Animation::GetBoneWeight(uint32_t boneId,float &weight) const
{
	if(boneId >= m_boneWeights.size())
		return false;
	weight = m_boneWeights.at(boneId);
	return true;
}
const std::vector<float> &pragma::animation::Animation::GetBoneWeights() const {return const_cast<Animation*>(this)->GetBoneWeights();}
std::vector<float> &pragma::animation::Animation::GetBoneWeights() {return m_boneWeights;}
void pragma::animation::Animation::SetBoneWeight(uint32_t boneId,float weight)
{
	if(boneId >= m_boneIds.size())
		return;
	if(m_boneIds.size() > m_boneWeights.size())
		m_boneWeights.resize(m_boneIds.size(),1.f);
	m_boneWeights.at(boneId) = weight;
}
#endif

bool pragma::animation::Animation::operator==(const Animation &other) const
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	if(m_frames.size() != other.m_frames.size() || m_boneWeights.size() != other.m_boneWeights.size() || static_cast<bool>(m_fadeIn) != static_cast<bool>(other.m_fadeIn) || static_cast<bool>(m_fadeOut) != static_cast<bool>(other.m_fadeOut) || m_events.size() != other.m_events.size())
		return false;
	if(m_fadeIn && umath::abs(*m_fadeIn -*other.m_fadeIn) > 0.001f)
		return false;
	if(m_fadeOut && umath::abs(*m_fadeOut -*other.m_fadeOut) > 0.001f)
		return false;
	for(auto i=decltype(m_frames.size()){0u};i<m_frames.size();++i)
	{
		if(*m_frames[i] != *other.m_frames[i])
			return false;
	}
	for(auto &pair : m_events)
	{
		if(other.m_events.find(pair.first) == other.m_events.end())
			return false;
	}
	for(auto i=decltype(m_boneWeights.size()){0u};i<m_boneWeights.size();++i)
	{
		if(umath::abs(m_boneWeights[i] -other.m_boneWeights[i]) > 0.001f)
			return false;
	}
	static_assert(sizeof(Animation) == 312,"Update this function when making changes to this class!");
	return m_boneIds == other.m_boneIds && m_boneIdMap == other.m_boneIdMap && m_flags == other.m_flags && m_activity == other.m_activity &&
		m_activityWeight == other.m_activityWeight && uvec::cmp(m_renderBounds.first,other.m_renderBounds.first) && uvec::cmp(m_renderBounds.second,other.m_renderBounds.second) && m_blendController == other.m_blendController;
#endif
	return false;
}
#pragma optimize("",on)
