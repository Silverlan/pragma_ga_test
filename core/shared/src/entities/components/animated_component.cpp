/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_shared.h"
#include "pragma/entities/components/animated_component.hpp"
#include "pragma/entities/components/base_model_component.hpp"
#include "pragma/model/model.h"
#include "pragma/model/animation/animation_player.hpp"
#include "pragma/model/animation/animation.hpp"
#include "pragma/lua/l_entity_handles.hpp"

using namespace pragma;
#pragma optimize("",off)
ComponentEventId AnimatedComponent::EVENT_HANDLE_ANIMATION_EVENT = pragma::INVALID_COMPONENT_ID;
ComponentEventId AnimatedComponent::EVENT_ON_PLAY_ANIMATION = pragma::INVALID_COMPONENT_ID;
ComponentEventId AnimatedComponent::EVENT_ON_ANIMATION_COMPLETE = pragma::INVALID_COMPONENT_ID;
ComponentEventId AnimatedComponent::EVENT_ON_ANIMATION_START = pragma::INVALID_COMPONENT_ID;
ComponentEventId AnimatedComponent::EVENT_MAINTAIN_ANIMATIONS = pragma::INVALID_COMPONENT_ID;
ComponentEventId AnimatedComponent::EVENT_ON_ANIMATIONS_UPDATED = pragma::INVALID_COMPONENT_ID;
ComponentEventId AnimatedComponent::EVENT_PLAY_ANIMATION = pragma::INVALID_COMPONENT_ID;
ComponentEventId AnimatedComponent::EVENT_TRANSLATE_ANIMATION = pragma::INVALID_COMPONENT_ID;
void AnimatedComponent::RegisterEvents(pragma::EntityComponentManager &componentManager)
{
	EVENT_HANDLE_ANIMATION_EVENT = componentManager.RegisterEvent("HANDLE_ANIMATION_EVENT");
	EVENT_ON_PLAY_ANIMATION = componentManager.RegisterEvent("ON_PLAY_ANIMATION");
	EVENT_ON_ANIMATION_COMPLETE = componentManager.RegisterEvent("ON_ANIMATION_COMPLETE");
	EVENT_ON_ANIMATION_START = componentManager.RegisterEvent("ON_ANIMATION_START");
	EVENT_MAINTAIN_ANIMATIONS = componentManager.RegisterEvent("MAINTAIN_ANIMATIONS");
	EVENT_ON_ANIMATIONS_UPDATED = componentManager.RegisterEvent("ON_ANIMATIONS_UPDATED");
	EVENT_PLAY_ANIMATION = componentManager.RegisterEvent("PLAY_ANIMATION");
	EVENT_TRANSLATE_ANIMATION = componentManager.RegisterEvent("TRANSLATE_ANIMATION");
}
AnimatedComponent::AnimatedComponent(BaseEntity &ent)
	: BaseEntityComponent(ent),m_playbackRate(util::FloatProperty::Create(1.f))
{}
void AnimatedComponent::SetPlaybackRate(float rate) {*m_playbackRate = rate;}
float AnimatedComponent::GetPlaybackRate() const {return *m_playbackRate;}
const util::PFloatProperty &AnimatedComponent::GetPlaybackRateProperty() const {return m_playbackRate;}
animation::PAnimationPlayer AnimatedComponent::AddAnimationPlayer()
{
	auto mdl = GetEntity().GetModel();
	if(!mdl)
		return nullptr;
	auto player = animation::AnimationPlayer::Create(*mdl);

	animation::AnimationPlayerCallbackInterface callbackInteface {};
	callbackInteface.onPlayAnimation = [this](animation::AnimationId animId,FPlayAnim flags) -> bool {
		CEOnPlayAnimation evData{animId,flags};
		return InvokeEventCallbacks(EVENT_PLAY_ANIMATION,evData) != util::EventReply::Handled;
	};
	callbackInteface.onStopAnimation = []() {
	
	};
	callbackInteface.translateAnimation = [this](animation::AnimationId &animId,FPlayAnim &flags) {
		CETranslateAnimation evTranslateAnimData {animId,flags};
		InvokeEventCallbacks(EVENT_TRANSLATE_ANIMATION,evTranslateAnimData);
	};

	m_animationPlayers.push_back(player);
	return player;
}
void AnimatedComponent::RemoveAnimationPlayer(const animation::AnimationPlayer &player)
{
	auto it = std::find_if(m_animationPlayers.begin(),m_animationPlayers.end(),[&player](const animation::PAnimationPlayer &playerOther) {
		return playerOther.get() == &player;
	});
	if(it == m_animationPlayers.end())
		return;
	m_animationPlayers.erase(it);
}
void AnimatedComponent::ClearAnimationPlayers()
{
	m_animationPlayers.clear();
}
void AnimatedComponent::MaintainAnimations(double dt)
{
	dt *= GetPlaybackRate();
	for(auto &animPlayer : m_animationPlayers)
		animPlayer->Advance(dt);
}
luabind::object AnimatedComponent::InitializeLuaObject(lua_State *l) {return BaseEntityComponent::InitializeLuaObject<AnimatedComponentHandleWrapper>(l);}

void AnimatedComponent::Save(udm::LinkedPropertyWrapper &udm) {}
void AnimatedComponent::Load(udm::LinkedPropertyWrapper &udm,uint32_t version) {}
void AnimatedComponent::ResetAnimation(const std::shared_ptr<Model> &mdl) {}

/////////////////

CEMaintainAnimations::CEMaintainAnimations(double deltaTime)
	: deltaTime{deltaTime}
{}
void CEMaintainAnimations::PushArguments(lua_State *l)
{
	Lua::PushNumber(l,deltaTime);
}

/////////////////

CETranslateAnimation::CETranslateAnimation(animation::AnimationId &animation,pragma::FPlayAnim &flags)
	: animation(animation),flags(flags)
{}
void CETranslateAnimation::PushArguments(lua_State *l)
{
	Lua::PushInt(l,animation);
	Lua::PushInt(l,umath::to_integral(flags));
}
uint32_t CETranslateAnimation::GetReturnCount() {return 2;}
void CETranslateAnimation::HandleReturnValues(lua_State *l)
{
	if(Lua::IsSet(l,-2))
		animation = Lua::CheckInt(l,-2);
	if(Lua::IsSet(l,-1))
		flags = static_cast<pragma::FPlayAnim>(Lua::CheckInt(l,-1));
}

/////////////////

CEOnAnimationStart::CEOnAnimationStart(int32_t animation,Activity activity,pragma::FPlayAnim flags)
	: animation(animation),activity(activity),flags(flags)
{}
void CEOnAnimationStart::PushArguments(lua_State *l)
{
	Lua::PushInt(l,animation);
	Lua::PushInt(l,umath::to_integral(activity));
	Lua::PushInt(l,umath::to_integral(flags));
}

/////////////////

CEOnAnimationComplete::CEOnAnimationComplete(int32_t animation,Activity activity)
	: animation(animation),activity(activity)
{}
void CEOnAnimationComplete::PushArguments(lua_State *l)
{
	Lua::PushInt(l,animation);
	Lua::PushInt(l,umath::to_integral(activity));
}

/////////////////

CEHandleAnimationEvent::CEHandleAnimationEvent(const AnimationEvent &animationEvent)
	: animationEvent(animationEvent)
{}
void CEHandleAnimationEvent::PushArguments(lua_State *l)
{
	Lua::PushInt(l,static_cast<int32_t>(animationEvent.eventID));

	auto tArgs = Lua::CreateTable(l);
	auto &args = animationEvent.arguments;
	for(auto i=decltype(args.size()){0};i<args.size();++i)
	{
		Lua::PushInt(l,i +1);
		Lua::PushString(l,args.at(i));
		Lua::SetTableValue(l,tArgs);
	}
}
void CEHandleAnimationEvent::PushArgumentVariadic(lua_State *l)
{
	auto &args = animationEvent.arguments;
	for(auto &arg : args)
		Lua::PushString(l,arg);
}

/////////////////

CEOnPlayAnimation::CEOnPlayAnimation(animation::AnimationId animation,pragma::FPlayAnim flags)
	: animation(animation),flags(flags)
{}
void CEOnPlayAnimation::PushArguments(lua_State *l)
{
	Lua::PushInt(l,animation);
	Lua::PushInt(l,umath::to_integral(flags));
}
#pragma optimize("",on)
