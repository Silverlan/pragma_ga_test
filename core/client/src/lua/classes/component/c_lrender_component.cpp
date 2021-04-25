/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_client.h"
#include "pragma/lua/classes/components/c_lentity_components.hpp"
#include "pragma/model/c_modelmesh.h"
#include <prosper_command_buffer.hpp>
#include <buffers/prosper_swap_buffer.hpp>
#include <pragma/math/intersection.h>

void Lua::Render::register_class(lua_State *l,luabind::module_ &entsMod)
{
	auto defCRender = luabind::class_<CRenderHandle,BaseEntityComponentHandle>("RenderComponent");
	Lua::register_base_render_component_methods<luabind::class_<CRenderHandle,BaseEntityComponentHandle>,CRenderHandle>(l,defCRender);
	defCRender.def("GetTransformationMatrix",&Lua::Render::GetTransformationMatrix);
	defCRender.def("SetRenderMode",&Lua::Render::SetRenderMode);
	defCRender.def("GetRenderMode",&Lua::Render::GetRenderMode);
	defCRender.def("GetLocalRenderBounds",&Lua::Render::GetLocalRenderBounds);
	defCRender.def("GetLocalRenderSphereBounds",&Lua::Render::GetLocalRenderSphereBounds);
	defCRender.def("GetAbsoluteRenderBounds",&Lua::Render::GetAbsoluteRenderBounds);
	defCRender.def("GetAbsoluteRenderSphereBounds",&Lua::Render::GetAbsoluteRenderSphereBounds);
	defCRender.def("SetLocalRenderBounds",&Lua::Render::SetLocalRenderBounds);
	// defCRender.def("UpdateRenderBuffers",static_cast<void(*)(lua_State*,CRenderHandle&,std::shared_ptr<prosper::ICommandBuffer>&,CSceneHandle&,CCameraHandle&,bool)>(&Lua::Render::UpdateRenderBuffers));
	// defCRender.def("UpdateRenderBuffers",static_cast<void(*)(lua_State*,CRenderHandle&,std::shared_ptr<prosper::ICommandBuffer>&,CSceneHandle&,CCameraHandle&)>(&Lua::Render::UpdateRenderBuffers));
	defCRender.def("GetRenderBuffer",&Lua::Render::GetRenderBuffer);
	defCRender.def("GetBoneBuffer",&Lua::Render::GetBoneBuffer);
	defCRender.def("GetLODMeshes",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		auto &lodMeshes = hComponent->GetLODMeshes();
		auto t = Lua::CreateTable(l);
		int32_t idx = 1;
		for(auto &lodMesh : lodMeshes)
		{
			Lua::PushInt(l,idx++);
			Lua::Push(l,lodMesh);
			Lua::SetTableValue(l,t);
		}
		}));
	defCRender.def("GetRenderMeshes",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		auto &renderMeshes = hComponent->GetRenderMeshes();
		auto t = Lua::CreateTable(l);
		int32_t idx = 1;
		for(auto &mesh : renderMeshes)
		{
			Lua::PushInt(l,idx++);
			Lua::Push(l,mesh);
			Lua::SetTableValue(l,t);
		}
		}));
	defCRender.def("GetLodRenderMeshes",static_cast<void(*)(lua_State*,CRenderHandle&,uint32_t)>([](lua_State *l,CRenderHandle &hComponent,uint32_t lod) {
		pragma::Lua::check_component(l,hComponent);
		auto &renderMeshes = hComponent->GetRenderMeshes();
		auto &renderMeshGroup = hComponent->GetLodRenderMeshGroup(lod);
		auto t = Lua::CreateTable(l);
		int32_t idx = 1;
		for(auto i=renderMeshGroup.first;i<renderMeshGroup.second;++i)
		{
			Lua::PushInt(l,idx++);
			Lua::Push(l,renderMeshes[i]);
			Lua::SetTableValue(l,t);
		}
		}));
	defCRender.def("SetExemptFromOcclusionCulling",static_cast<void(*)(lua_State*,CRenderHandle&,bool)>([](lua_State *l,CRenderHandle &hComponent,bool exempt) {
		pragma::Lua::check_component(l,hComponent);
		hComponent->SetExemptFromOcclusionCulling(exempt);
		}));
	defCRender.def("IsExemptFromOcclusionCulling",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		Lua::PushBool(l,hComponent->IsExemptFromOcclusionCulling());
		}));
	defCRender.def("SetReceiveShadows",static_cast<void(*)(lua_State*,CRenderHandle&,bool)>([](lua_State *l,CRenderHandle &hComponent,bool enabled) {
		pragma::Lua::check_component(l,hComponent);
		hComponent->SetReceiveShadows(enabled);
	}));
	defCRender.def("IsReceivingShadows",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		Lua::PushBool(l,hComponent->IsReceivingShadows());
	}));
	defCRender.def("SetRenderBufferDirty",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		hComponent->SetRenderBufferDirty();
		}));
	/*defCRender.def("GetDepthBias",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		float constantFactor,biasClamp,slopeFactor;
		hComponent->GetDepthBias(constantFactor,biasClamp,slopeFactor);
		Lua::PushNumber(l,constantFactor);
		Lua::PushNumber(l,biasClamp);
		Lua::PushNumber(l,slopeFactor);
	}));
	defCRender.def("SetDepthBias",static_cast<void(*)(lua_State*,CRenderHandle&,float,float,float)>([](lua_State *l,CRenderHandle &hComponent,float constantFactor,float biasClamp,float slopeFactor) {
		pragma::Lua::check_component(l,hComponent);
		hComponent->SetDepthBias(constantFactor,biasClamp,slopeFactor);
	}));*/
	defCRender.def("CalcRayIntersection",static_cast<void(*)(lua_State*,CRenderHandle&,const Vector3&,const Vector3&,bool)>(&Lua::Render::CalcRayIntersection));
	defCRender.def("CalcRayIntersection",static_cast<void(*)(lua_State*,CRenderHandle&,const Vector3&,const Vector3&)>(&Lua::Render::CalcRayIntersection));
	defCRender.def("SetDepthPassEnabled",static_cast<void(*)(lua_State*,CRenderHandle&,bool)>([](lua_State *l,CRenderHandle &hComponent,bool depthPassEnabled) {
		pragma::Lua::check_component(l,hComponent);
		hComponent->SetDepthPassEnabled(depthPassEnabled);
	}));
	defCRender.def("IsDepthPassEnabled",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		Lua::PushBool(l,hComponent->IsDepthPassEnabled());
	}));
	defCRender.def("GetRenderClipPlane",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		auto *clipPlane = hComponent->GetRenderClipPlane();
		if(clipPlane == nullptr)
			return;
		Lua::Push(l,*clipPlane);
	}));
	defCRender.def("SetRenderClipPlane",static_cast<void(*)(lua_State*,CRenderHandle&,const Vector4&)>([](lua_State *l,CRenderHandle &hComponent,const Vector4 &clipPlane) {
		pragma::Lua::check_component(l,hComponent);
		hComponent->SetRenderClipPlane(clipPlane);
	}));
	defCRender.def("ClearRenderClipPlane",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		hComponent->ClearRenderClipPlane();
	}));
	defCRender.def("GetDepthBias",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		auto *depthBias = hComponent->GetDepthBias();
		if(depthBias == nullptr)
			return;
		Lua::PushNumber(l,depthBias->x);
		Lua::PushNumber(l,depthBias->y);
	}));
	defCRender.def("SetDepthBias",static_cast<void(*)(lua_State*,CRenderHandle&,float,float)>([](lua_State *l,CRenderHandle &hComponent,float d,float delta) {
		pragma::Lua::check_component(l,hComponent);
		hComponent->SetDepthBias(d,delta);
	}));
	defCRender.def("ClearDepthBias",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		hComponent->ClearDepthBias();
	}));
	defCRender.def("GetRenderPose",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		Lua::Push(l,hComponent->GetRenderPose());
	}));
	defCRender.def("SetRenderOffsetTransform",static_cast<void(*)(lua_State*,CRenderHandle&,const umath::ScaledTransform&)>([](lua_State *l,CRenderHandle &hComponent,const umath::ScaledTransform &pose) {
		pragma::Lua::check_component(l,hComponent);
		hComponent->SetRenderOffsetTransform(pose);
	}));
	defCRender.def("ClearRenderOffsetTransform",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		hComponent->ClearRenderOffsetTransform();
	}));
	defCRender.def("GetRenderOffsetTransform",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		auto *t = hComponent->GetRenderOffsetTransform();
		if(t == nullptr)
			return;
		Lua::Push(l,*t);
	}));
	defCRender.def("ShouldCastShadows",static_cast<bool(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) -> bool {
		pragma::Lua::check_component(l,hComponent);
		return hComponent->GetCastShadows();
	}));
	defCRender.def("ShouldDraw",static_cast<bool(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) -> bool {
		pragma::Lua::check_component(l,hComponent);
		return hComponent->ShouldDraw();
	}));
	defCRender.def("ShouldDrawShadow",static_cast<bool(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) -> bool {
		pragma::Lua::check_component(l,hComponent);
		return hComponent->ShouldDrawShadow();
	}));
	defCRender.def("ClearBuffers",static_cast<void(*)(lua_State*,CRenderHandle&)>([](lua_State *l,CRenderHandle &hComponent) {
		pragma::Lua::check_component(l,hComponent);
		hComponent->ClearRenderBuffers();
	}));
	// defCRender.add_static_constant("EVENT_ON_UPDATE_RENDER_DATA",pragma::CRenderComponent::EVENT_ON_UPDATE_RENDER_DATA);
	defCRender.add_static_constant("EVENT_ON_RENDER_BOUNDS_CHANGED",pragma::CRenderComponent::EVENT_ON_RENDER_BOUNDS_CHANGED);
	defCRender.add_static_constant("EVENT_ON_RENDER_MODE_CHANGED",pragma::CRenderComponent::EVENT_ON_RENDER_MODE_CHANGED);
	defCRender.add_static_constant("EVENT_ON_RENDER_BUFFERS_INITIALIZED",pragma::CRenderComponent::EVENT_ON_RENDER_BUFFERS_INITIALIZED);
	defCRender.add_static_constant("EVENT_SHOULD_DRAW",pragma::CRenderComponent::EVENT_SHOULD_DRAW);
	defCRender.add_static_constant("EVENT_SHOULD_DRAW_SHADOW",pragma::CRenderComponent::EVENT_SHOULD_DRAW_SHADOW);
	defCRender.add_static_constant("EVENT_ON_UPDATE_RENDER_MATRICES",pragma::CRenderComponent::EVENT_ON_UPDATE_RENDER_MATRICES);
	defCRender.add_static_constant("EVENT_UPDATE_INSTANTIABILITY",pragma::CRenderComponent::EVENT_UPDATE_INSTANTIABILITY);
	defCRender.add_static_constant("EVENT_ON_CLIP_PLANE_CHANGED",pragma::CRenderComponent::EVENT_ON_CLIP_PLANE_CHANGED);
	defCRender.add_static_constant("EVENT_ON_DEPTH_BIAS_CHANGED",pragma::CRenderComponent::EVENT_ON_DEPTH_BIAS_CHANGED);

	// Enums
	defCRender.add_static_constant("RENDERMODE_NONE",umath::to_integral(RenderMode::None));
	defCRender.add_static_constant("RENDERMODE_WORLD",umath::to_integral(RenderMode::World));
	defCRender.add_static_constant("RENDERMODE_VIEW",umath::to_integral(RenderMode::View));
	defCRender.add_static_constant("RENDERMODE_SKYBOX",umath::to_integral(RenderMode::Skybox));
	defCRender.add_static_constant("RENDERMODE_WATER",umath::to_integral(RenderMode::Water));
	entsMod[defCRender];
}
void Lua::Render::CalcRayIntersection(lua_State *l,CRenderHandle &hComponent,const Vector3 &start,const Vector3 &dir,bool precise)
{
	pragma::Lua::check_component(l,hComponent);
	auto result = hComponent->CalcRayIntersection(start,dir,precise);
	if(result.has_value() == false)
	{
		Lua::PushInt(l,umath::to_integral(umath::intersection::Result::NoIntersection));
		return;
	}
	Lua::Push(l,umath::to_integral(result->result));

	auto t = Lua::CreateTable(l);

	Lua::PushString(l,"position"); /* 1 */
	Lua::Push<Vector3>(l,result->hitPos); /* 2 */
	Lua::SetTableValue(l,t); /* 0 */

	Lua::PushString(l,"distance"); /* 1 */
	Lua::PushNumber(l,result->hitValue); /* 2 */
	Lua::SetTableValue(l,t); /* 0 */

	if(precise && result->precise)
	{
		Lua::PushString(l,"uv"); /* 1 */
		Lua::Push<Vector2>(l,Vector2{result->precise->u,result->precise->v}); /* 2 */
		Lua::SetTableValue(l,t); /* 0 */
		return;
	}
	
	Lua::PushString(l,"boneId"); /* 1 */
	Lua::PushInt(l,result->boneId); /* 2 */
	Lua::SetTableValue(l,t); /* 0 */
}
void Lua::Render::CalcRayIntersection(lua_State *l,CRenderHandle &hComponent,const Vector3 &start,const Vector3 &dir) {CalcRayIntersection(l,hComponent,start,dir,false);}
void Lua::Render::GetTransformationMatrix(lua_State *l,CRenderHandle &hEnt)
{
	pragma::Lua::check_component(l,hEnt);
	Mat4 mat = hEnt->GetTransformationMatrix();
	luabind::object(l,mat).push(l);
}

void Lua::Render::SetRenderMode(lua_State *l,CRenderHandle &hEnt,unsigned int mode)
{
	pragma::Lua::check_component(l,hEnt);
	hEnt->SetRenderMode(RenderMode(mode));
}

void Lua::Render::GetRenderMode(lua_State *l,CRenderHandle &hEnt)
{
	pragma::Lua::check_component(l,hEnt);
	RenderMode mode = hEnt->GetRenderMode();
	Lua::PushInt(l,static_cast<int>(mode));
}
void Lua::Render::GetLocalRenderBounds(lua_State *l,CRenderHandle &hEnt)
{
	pragma::Lua::check_component(l,hEnt);
	auto &aabb = hEnt->GetLocalRenderBounds();
	Lua::Push<Vector3>(l,aabb.min);
	Lua::Push<Vector3>(l,aabb.max);
}
void Lua::Render::GetLocalRenderSphereBounds(lua_State *l,CRenderHandle &hEnt)
{
	pragma::Lua::check_component(l,hEnt);
	auto &sphere = hEnt->GetLocalRenderSphere();
	Lua::Push<Vector3>(l,sphere.pos);
	Lua::PushNumber(l,sphere.radius);
}
void Lua::Render::GetAbsoluteRenderBounds(lua_State *l,CRenderHandle &hEnt)
{
	pragma::Lua::check_component(l,hEnt);
	auto &aabb = hEnt->GetUpdatedAbsoluteRenderBounds();
	Lua::Push<Vector3>(l,aabb.min);
	Lua::Push<Vector3>(l,aabb.max);
}
void Lua::Render::GetAbsoluteRenderSphereBounds(lua_State *l,CRenderHandle &hEnt)
{
	pragma::Lua::check_component(l,hEnt);
	auto &sphere = hEnt->GetUpdatedAbsoluteRenderSphere();
	Lua::Push<Vector3>(l,sphere.pos);
	Lua::PushNumber(l,sphere.radius);
}

void Lua::Render::SetLocalRenderBounds(lua_State *l,CRenderHandle &hEnt,Vector3 &min,Vector3 &max)
{
	pragma::Lua::check_component(l,hEnt);
	hEnt->SetLocalRenderBounds(min,max);
}

/*void Lua::Render::UpdateRenderBuffers(lua_State *l,CRenderHandle &hEnt,std::shared_ptr<prosper::ICommandBuffer> &drawCmd,CSceneHandle &hScene,CCameraHandle &hCam,bool bForceBufferUpdate)
{
	pragma::Lua::check_component(l,hEnt);
	pragma::Lua::check_component(l,hScene);
	pragma::Lua::check_component(l,hCam);
	if(drawCmd->IsPrimary() == false)
		return;
	auto vp = hCam->GetProjectionMatrix() *hCam->GetViewMatrix();
	hEnt->UpdateRenderData(std::dynamic_pointer_cast<prosper::IPrimaryCommandBuffer>(drawCmd),*hScene,*hCam,vp,bForceBufferUpdate);
}
void Lua::Render::UpdateRenderBuffers(lua_State *l,CRenderHandle &hEnt,std::shared_ptr<prosper::ICommandBuffer> &drawCmd,CSceneHandle &hScene,CCameraHandle &hCam) {UpdateRenderBuffers(l,hEnt,drawCmd,hScene,hCam,false);}*/
void Lua::Render::GetRenderBuffer(lua_State *l,CRenderHandle &hEnt)
{
	pragma::Lua::check_component(l,hEnt);
	if(hEnt->GetSwapRenderBuffer() == nullptr)
		return;
	auto &buf = hEnt->GetRenderBuffer();
	//if(buf == nullptr)
	//	return;
	Lua::Push(l,buf.shared_from_this());
}
void Lua::Render::GetBoneBuffer(lua_State *l,CRenderHandle &hEnt)
{
	pragma::Lua::check_component(l,hEnt);
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto *pAnimComponent = static_cast<pragma::CAnimatedComponent*>(hEnt->GetEntity().GetAnimatedComponent().get());
	if(pAnimComponent == nullptr)
		return;
	auto buf = pAnimComponent->GetSwapBoneBuffer();
	if(!buf)
		return;
	Lua::Push(l,buf->shared_from_this());
#endif
}
