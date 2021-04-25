/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan */

#ifndef __S_ANIMATED_COMPONENT_HPP__
#define __S_ANIMATED_COMPONENT_HPP__

#include "pragma/serverdefinitions.h"
#include "pragma/entities/components/s_entity_component.hpp"
#include <pragma/entities/components/base_sk_animated_component.hpp>

namespace pragma
{
	class DLLSERVER SSkAnimatedComponent final
		: public BaseSkAnimatedComponent,
		public SBaseNetComponent
	{
	public:
		static void RegisterEvents(pragma::EntityComponentManager &componentManager);

		SSkAnimatedComponent(BaseEntity &ent) : BaseSkAnimatedComponent(ent) {}
		virtual void Initialize() override;
		virtual void SendData(NetPacket &packet,networking::ClientRecipientFilter &rp) override;
		virtual bool ShouldTransmitNetData() const override {return true;}
		virtual luabind::object InitializeLuaObject(lua_State *l) override;

		using BaseSkAnimatedComponent::PlayAnimation;
		using BaseSkAnimatedComponent::StopLayeredAnimation;
		using BaseSkAnimatedComponent::PlayLayeredAnimation;
		virtual void PlayAnimation(animation::AnimationId animation,FPlayAnim flags=FPlayAnim::Default) override;
		virtual void StopLayeredAnimation(animation::LayeredAnimationSlot slot) override;
		virtual void PlayLayeredAnimation(animation::LayeredAnimationSlot slot,animation::AnimationId animation,FPlayAnim flags=FPlayAnim::Default) override;
	protected:
		virtual void GetBaseTypeIndex(std::type_index &outTypeIndex) const override;
	};
};

#endif
