/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan */

#include "stdafx_server.h"
#include "pragma/entities/components/s_animated_component.hpp"
#include "pragma/lua/s_lentity_handles.hpp"
#include <pragma/networking/enums.hpp>
#include <pragma/networking/nwm_util.h>
#include <pragma/networking/enums.hpp>

using namespace pragma;

extern DLLSERVER ServerState *server;

void SSkAnimatedComponent::Initialize()
{
	BaseSkAnimatedComponent::Initialize();
}
luabind::object SSkAnimatedComponent::InitializeLuaObject(lua_State *l) {return BaseEntityComponent::InitializeLuaObject<SSkAnimatedComponentHandleWrapper>(l);}
void SSkAnimatedComponent::RegisterEvents(pragma::EntityComponentManager &componentManager)
{
	BaseSkAnimatedComponent::RegisterEvents(componentManager);
}
void SSkAnimatedComponent::GetBaseTypeIndex(std::type_index &outTypeIndex) const {outTypeIndex = std::type_index(typeid(BaseSkAnimatedComponent));}
void SSkAnimatedComponent::SendData(NetPacket &packet,networking::ClientRecipientFilter &rp)
{
	packet->Write<int>(GetAnimation());
	packet->Write<float>(GetCycle());
}

void SSkAnimatedComponent::PlayAnimation(animation::AnimationId animation,FPlayAnim flags)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	BaseSkAnimatedComponent::PlayAnimation(animation,flags);

	auto &ent = static_cast<SBaseEntity&>(GetEntity());
	if(ent.IsShared() == false)
		return;
	if((flags &pragma::FPlayAnim::Transmit) != pragma::FPlayAnim::None)
	{
		NetPacket p;
		nwm::write_entity(p,&ent);
		p->Write<int>(GetBaseAnimationInfo().animation);
		server->SendPacket("ent_anim_play",p,pragma::networking::Protocol::FastUnreliable);
	}
#endif
}
void SSkAnimatedComponent::StopLayeredAnimation(animation::LayeredAnimationSlot slot)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto it = m_animSlots.find(slot);
	if(it == m_animSlots.end())
		return;
	BaseSkAnimatedComponent::StopLayeredAnimation(slot);
	auto &ent = static_cast<SBaseEntity&>(GetEntity());
	if(ent.IsShared() == false)
		return;
	auto &animInfo = it->second;
	if((animInfo.flags &pragma::FPlayAnim::Transmit) != pragma::FPlayAnim::None)
	{
		NetPacket p;
		nwm::write_entity(p,&ent);
		p->Write<int>(slot);
		server->SendPacket("ent_anim_gesture_stop",p,pragma::networking::Protocol::SlowReliable);
	}
#endif
}
void SSkAnimatedComponent::PlayLayeredAnimation(animation::LayeredAnimationSlot slot,animation::AnimationId animation,FPlayAnim flags)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	BaseSkAnimatedComponent::PlayLayeredAnimation(slot,animation,flags);
	auto &ent = static_cast<SBaseEntity&>(GetEntity());
	if(ent.IsShared() == false)
		return;
	if((flags &pragma::FPlayAnim::Transmit) != pragma::FPlayAnim::None)
	{
		auto it = m_animSlots.find(slot);
		if(it == m_animSlots.end())
			return;
		auto &animInfo = it->second;
		NetPacket p;
		nwm::write_entity(p,&ent);
		p->Write<int>(slot);
		p->Write<int>(animInfo.animation);
		server->SendPacket("ent_anim_gesture_play",p,pragma::networking::Protocol::SlowReliable);
	}
#endif
}
