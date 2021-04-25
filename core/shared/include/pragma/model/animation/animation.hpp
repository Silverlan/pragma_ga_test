/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#ifndef __ANIMATION_H__
#define __ANIMATION_H__

#include "pragma/networkdefinitions.h"
#include "pragma/model/animation/frame.h"
#include "pragma/model/animation/fanim.h"
#include "pragma/model/animation/activities.h"
#include "pragma/model/animation/animation_event.h"
#include <sharedutils/util_enum_register.hpp>
#include <optional>
#include <vector>
#include <udm.hpp>

#define PRAGMA_ANIMATION_VERSION 2

struct DLLNETWORK AnimationBlendControllerTransition
{
	uint32_t animation = std::numeric_limits<uint32_t>::max();
	float transition = 0.f;

	bool operator==(const AnimationBlendControllerTransition &other) const
	{
		return animation == other.animation && umath::abs(transition -other.transition) < 0.001f;
	}
	bool operator!=(const AnimationBlendControllerTransition &other) const {return !operator==(other);}
};

struct DLLNETWORK AnimationBlendController
{
	uint32_t controller;
	std::vector<AnimationBlendControllerTransition> transitions;

	// An optional post blend target, which will be blended towards depending on the specified controller.
	// Primary used for directional movement animations with several cardinal animations and one center animation.
	uint32_t animationPostBlendTarget = std::numeric_limits<uint32_t>::max();
	uint32_t animationPostBlendController = std::numeric_limits<uint32_t>::max();

	bool operator==(const AnimationBlendController &other) const
	{
		return controller == other.controller && transitions == other.transitions && animationPostBlendTarget == other.animationPostBlendTarget &&
			animationPostBlendController == other.animationPostBlendController;
	}
	bool operator!=(const AnimationBlendController &other) const {return !operator==(other);}
};

class VFilePtrInternalReal;
namespace udm {struct AssetData;};
namespace pragma::animation
{
	class AnimatedPose;
	struct AnimationChannel;
	enum class AnimationChannelNodeType : uint8_t;
	class DLLNETWORK Animation
		: public std::enable_shared_from_this<Animation>
	{
	public:
		static constexpr uint32_t PANIM_VERSION = 1;
		static constexpr auto PANIM_IDENTIFIER = "PANI";
		static util::EnumRegister &GetEventEnumRegister();
		enum class DLLNETWORK ShareMode : uint32_t
		{
			None = 0,
			Frames = 1,
			Events = 2,
		};
		static std::shared_ptr<Animation> Create();
		static std::shared_ptr<Animation> Create(const Animation &other,ShareMode share=ShareMode::None);
		static std::shared_ptr<Animation> Load(const udm::AssetData &data,std::string &outErr,const Skeleton *optSkeleton=nullptr,const AnimatedPose *optReference=nullptr);
		float GetAnimationSpeedFactor() const {return m_speedFactor;}
		void SetAnimationSpeedFactor(float f) {m_speedFactor = f;}
		void SetFlags(FAnim flags);
		FAnim GetFlags() const;
		bool HasFlag(FAnim flag) const;
		void AddFlags(FAnim flags);
		void RemoveFlags(FAnim flags);
		void AddChannel(const std::shared_ptr<AnimationChannel> &channel);
		const std::vector<std::shared_ptr<AnimationChannel>> &GetChannels() const {return const_cast<Animation*>(this)->GetChannels();}
		std::vector<std::shared_ptr<AnimationChannel>> &GetChannels() {return m_channels;}
		uint32_t GetChannelCount() const {return m_channels.size();}
		float GetFadeInTime();
		float GetFadeOutTime();
		bool HasFadeInTime();
		bool HasFadeOutTime();
		void SetFadeInTime(float t);
		void SetFadeOutTime(float t);
		AnimationChannel *FindChannel(const std::string &node,const std::string &path);
		const AnimationChannel *FindChannel(const std::string &node,const std::string &path) const {return const_cast<Animation*>(this)->FindChannel(node,path);}
		AnimationChannel *AddChannel(AnimationChannelNodeType type,const std::string &node,const std::string &path,udm::Type valueType);

		void AddEvent(const AnimationEvent &ev);
		std::vector<AnimationEvent> &GetEvents() {return m_events;}
		const std::vector<AnimationEvent> &GetEvents() const {return const_cast<Animation*>(this)->GetEvents();}

		float GetDuration() const {return m_duration;}
		void SetDuration(float duration) {m_duration = duration;}

		udm::Property &GetProperties() {return *m_properties.get();}
		const udm::Property &GetProperties() const {return const_cast<Animation*>(this)->GetProperties();}

		// If reference frame is specified, it will be used to optimize frame data and reduce the file size
		bool Save(udm::AssetData &outData,std::string &outErr,const Frame *optReference=nullptr);
		bool SaveLegacy(std::shared_ptr<VFilePtrInternalReal> &f);

		bool operator==(const Animation &other) const;
		bool operator!=(const Animation &other) const {return !operator==(other);}
	private:
		static util::EnumRegister s_eventEnumRegister;
		bool LoadFromAssetData(const udm::AssetData &data,std::string &outErr,const Skeleton *optSkeleton=nullptr,const AnimatedPose *optReference=nullptr);
		Animation();
		Animation(const Animation &other,ShareMode share=ShareMode::None);
		
		std::vector<AnimationEvent> m_events;
		std::vector<std::shared_ptr<AnimationChannel>> m_channels;
		udm::PProperty m_properties = nullptr;
		float m_speedFactor = 1.f;
		float m_duration = 0.f;
	};
};
REGISTER_BASIC_ARITHMETIC_OPERATORS(pragma::animation::Animation::ShareMode)

#endif
