/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_client.h"
#include "pragma/model/model.h"
#include "pragma/lua/classes/components/c_lentity_components.hpp"
#include "pragma/model/c_modelmesh.h"
#include <buffers/prosper_swap_buffer.hpp>
#include <prosper_command_buffer.hpp>

void Lua::Animated::register_class(lua_State *l,luabind::module_ &entsMod)
{
	auto defCAnimated = luabind::class_<CSkAnimatedHandle,BaseEntityComponentHandle>("SkAnimatedComponent");
	Lua::register_base_animated_component_methods<luabind::class_<CSkAnimatedHandle,BaseEntityComponentHandle>,CSkAnimatedHandle>(l,defCAnimated);
	defCAnimated.def("GetBoneBuffer",static_cast<void(*)(lua_State*,CSkAnimatedHandle&)>([](lua_State *l,CSkAnimatedHandle &hAnim) {
		pragma::Lua::check_component(l,hAnim);
		auto *pAnimComponent = hAnim.get();
		if(pAnimComponent == nullptr)
			return;
		auto buf = pAnimComponent->GetSwapBoneBuffer();
		if(!buf)
			return;
		Lua::Push(l,buf->shared_from_this());
		}));
	defCAnimated.def("GetBoneRenderMatrices",static_cast<void(*)(lua_State*,CSkAnimatedHandle&)>([](lua_State *l,CSkAnimatedHandle &hAnim) {
		pragma::Lua::check_component(l,hAnim);
		auto *pAnimComponent = hAnim.get();
		if(pAnimComponent == nullptr)
			return;
		auto &mats = pAnimComponent->GetBoneMatrices();
		auto t = Lua::CreateTable(l);
		auto idx = 1u;
		for(auto &m : mats)
		{
			Lua::PushInt(l,idx++);
			Lua::Push<Mat4>(l,m);
			Lua::SetTableValue(l,t);
		}
		}));
	defCAnimated.def("GetBoneRenderMatrix",static_cast<void(*)(lua_State*,CSkAnimatedHandle&,uint32_t)>([](lua_State *l,CSkAnimatedHandle &hAnim,uint32_t boneIndex) {
		pragma::Lua::check_component(l,hAnim);
		auto *pAnimComponent = hAnim.get();
		if(pAnimComponent == nullptr)
			return;
		auto &mats = pAnimComponent->GetBoneMatrices();
		if(boneIndex >= mats.size())
			return;
		auto &m = mats.at(boneIndex);
		Lua::Push<Mat4>(l,m);
		}));
	defCAnimated.def("SetBoneRenderMatrix",static_cast<void(*)(lua_State*,CSkAnimatedHandle&,uint32_t,const Mat4&)>([](lua_State *l,CSkAnimatedHandle &hAnim,uint32_t boneIndex,const Mat4 &m) {
		pragma::Lua::check_component(l,hAnim);
		auto *pAnimComponent = hAnim.get();
		if(pAnimComponent == nullptr)
			return;
		auto &mats = pAnimComponent->GetBoneMatrices();
		if(boneIndex >= mats.size())
			return;
		mats.at(boneIndex) = m;
	}));
	defCAnimated.def("GetLocalVertexPosition",static_cast<void(*)(lua_State*,CSkAnimatedHandle&,std::shared_ptr<::ModelSubMesh>&,uint32_t)>([](lua_State *l,CSkAnimatedHandle &hAnim,std::shared_ptr<::ModelSubMesh> &subMesh,uint32_t vertexId) {
		pragma::Lua::check_component(l,hAnim);
		Vector3 pos,n;
		auto b = hAnim->GetLocalVertexPosition(static_cast<CModelSubMesh&>(*subMesh),vertexId,pos,n);
		if(b == false)
			return;
		Lua::Push<Vector3>(l,pos);
		}));
	defCAnimated.def("AreSkeletonUpdateCallbacksEnabled",static_cast<void(*)(lua_State*,CSkAnimatedHandle&)>([](lua_State *l,CSkAnimatedHandle &hAnim) {
		pragma::Lua::check_component(l,hAnim);
		Lua::PushBool(l,hAnim->AreSkeletonUpdateCallbacksEnabled());
	}));
	defCAnimated.def("SetSkeletonUpdateCallbacksEnabled",static_cast<void(*)(lua_State*,CSkAnimatedHandle&,bool)>([](lua_State *l,CSkAnimatedHandle &hAnim,bool enabled) {
		pragma::Lua::check_component(l,hAnim);
		hAnim->SetSkeletonUpdateCallbacksEnabled(enabled);
	}));
	defCAnimated.add_static_constant("EVENT_ON_SKELETON_UPDATED",pragma::CSkAnimatedComponent::EVENT_ON_SKELETON_UPDATED);
	defCAnimated.add_static_constant("EVENT_ON_BONE_MATRICES_UPDATED",pragma::CSkAnimatedComponent::EVENT_ON_BONE_MATRICES_UPDATED);
	defCAnimated.add_static_constant("EVENT_ON_BONE_BUFFER_INITIALIZED",pragma::CSkAnimatedComponent::EVENT_ON_BONE_BUFFER_INITIALIZED);
	entsMod[defCAnimated];
}
