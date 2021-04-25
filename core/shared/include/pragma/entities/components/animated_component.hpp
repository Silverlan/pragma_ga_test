/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan */

#ifndef __ANIMATED_COMPONENT_HPP__
#define __ANIMATED_COMPONENT_HPP__

#include "pragma/entities/components/base_entity_component.hpp"
#include "pragma/types.hpp"
#include <sharedutils/property/util_property.hpp>

struct AnimationEvent;
namespace pragma
{
	class DLLNETWORK AnimatedComponent final
		: public BaseEntityComponent
	{
	public:
		static ComponentEventId EVENT_HANDLE_ANIMATION_EVENT;
		static ComponentEventId EVENT_ON_PLAY_ANIMATION;
		static ComponentEventId EVENT_ON_ANIMATION_COMPLETE;
		static ComponentEventId EVENT_ON_ANIMATION_START;
		static ComponentEventId EVENT_MAINTAIN_ANIMATIONS;
		static ComponentEventId EVENT_ON_ANIMATIONS_UPDATED;
		static ComponentEventId EVENT_PLAY_ANIMATION;
		static ComponentEventId EVENT_TRANSLATE_ANIMATION;
		static void RegisterEvents(pragma::EntityComponentManager &componentManager);
		
		AnimatedComponent(BaseEntity &ent);
		void SetPlaybackRate(float rate);
		float GetPlaybackRate() const;
		const util::PFloatProperty &GetPlaybackRateProperty() const;

		animation::PAnimationPlayer AddAnimationPlayer();
		void RemoveAnimationPlayer(const animation::AnimationPlayer &player);
		void ClearAnimationPlayers();
		
		virtual luabind::object InitializeLuaObject(lua_State *l) override;
		virtual void Save(udm::LinkedPropertyWrapper &udm) override;
		using BaseEntityComponent::Load;
	protected:
		virtual void Load(udm::LinkedPropertyWrapper &udm,uint32_t version) override;
		void ResetAnimation(const std::shared_ptr<Model> &mdl);
		void MaintainAnimations(double dt);
		util::PFloatProperty m_playbackRate = nullptr;
		std::vector<animation::PAnimationPlayer> m_animationPlayers;
	};

	struct DLLNETWORK CEOnAnimationComplete
		: public ComponentEvent
	{
		CEOnAnimationComplete(int32_t animation,Activity activity);
		virtual void PushArguments(lua_State *l) override;
		int32_t animation;
		Activity activity;
	};
	struct DLLNETWORK CEHandleAnimationEvent
		: public ComponentEvent
	{
		CEHandleAnimationEvent(const AnimationEvent &animationEvent);
		virtual void PushArguments(lua_State *l) override;
		void PushArgumentVariadic(lua_State *l);
		const AnimationEvent &animationEvent;
	};
	struct DLLNETWORK CEOnPlayAnimation
		: public ComponentEvent
	{
		CEOnPlayAnimation(animation::AnimationId animation,pragma::FPlayAnim flags);
		virtual void PushArguments(lua_State *l) override;
		animation::AnimationId animation;
		pragma::FPlayAnim flags;
	};
	struct DLLNETWORK CEOnAnimationStart
		: public ComponentEvent
	{
		CEOnAnimationStart(int32_t animation,Activity activity,pragma::FPlayAnim flags);
		virtual void PushArguments(lua_State *l) override;
		int32_t animation;
		Activity activity;
		pragma::FPlayAnim flags;
	};
	struct DLLNETWORK CETranslateAnimation
		: public ComponentEvent
	{
		CETranslateAnimation(animation::AnimationId &animation,pragma::FPlayAnim &flags);
		virtual void PushArguments(lua_State *l) override;
		virtual uint32_t GetReturnCount() override;
		virtual void HandleReturnValues(lua_State *l) override;
		animation::AnimationId &animation;
		pragma::FPlayAnim &flags;
	};
	struct DLLNETWORK CEMaintainAnimations
		: public ComponentEvent
	{
		CEMaintainAnimations(double deltaTime);
		virtual void PushArguments(lua_State *l) override;
		double deltaTime;
	};
};

#endif
