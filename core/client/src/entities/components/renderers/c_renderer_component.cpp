/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_client.h"
#include "pragma/entities/components/renderers/c_renderer_component.hpp"
#include "pragma/rendering/scene/util_draw_scene_info.hpp"
#include "pragma/lua/c_lentity_handles.hpp"
#include "pragma/lua/libraries/c_lua_vulkan.h"
#include <pragma/lua/converters/game_type_converters_t.hpp>
#include <prosper_command_buffer.hpp>

extern DLLCLIENT CEngine *c_engine;

using namespace pragma;


ComponentEventId CRendererComponent::EVENT_RELOAD_RENDER_TARGET = INVALID_COMPONENT_ID;
ComponentEventId CRendererComponent::EVENT_RELOAD_BLOOM_RENDER_TARGET = INVALID_COMPONENT_ID;
ComponentEventId CRendererComponent::EVENT_BEGIN_RENDERING = INVALID_COMPONENT_ID;
ComponentEventId CRendererComponent::EVENT_END_RENDERING = INVALID_COMPONENT_ID;
ComponentEventId CRendererComponent::EVENT_UPDATE_CAMERA_DATA = INVALID_COMPONENT_ID;
ComponentEventId CRendererComponent::EVENT_UPDATE_RENDER_SETTINGS = INVALID_COMPONENT_ID;
ComponentEventId CRendererComponent::EVENT_UPDATE_RENDERER_BUFFER = INVALID_COMPONENT_ID;
ComponentEventId CRendererComponent::EVENT_GET_SCENE_TEXTURE = INVALID_COMPONENT_ID;
ComponentEventId CRendererComponent::EVENT_GET_PRESENTATION_TEXTURE = INVALID_COMPONENT_ID;
ComponentEventId CRendererComponent::EVENT_GET_HDR_PRESENTATION_TEXTURE = INVALID_COMPONENT_ID;
ComponentEventId CRendererComponent::EVENT_RECORD_COMMAND_BUFFERS = INVALID_COMPONENT_ID;
ComponentEventId CRendererComponent::EVENT_RENDER = INVALID_COMPONENT_ID;
void CRendererComponent::RegisterEvents(pragma::EntityComponentManager &componentManager,TRegisterComponentEvent registerEvent)
{
	EVENT_RELOAD_RENDER_TARGET = registerEvent("EVENT_RELOAD_RENDER_TARGET",EntityComponentManager::EventInfo::Type::Explicit);
	EVENT_RELOAD_BLOOM_RENDER_TARGET = registerEvent("EVENT_RELOAD_BLOOM_RENDER_TARGET",EntityComponentManager::EventInfo::Type::Explicit);
	EVENT_BEGIN_RENDERING = registerEvent("EVENT_BEGIN_RENDERING",EntityComponentManager::EventInfo::Type::Explicit);
	EVENT_END_RENDERING = registerEvent("EVENT_END_RENDERING",EntityComponentManager::EventInfo::Type::Explicit);
	EVENT_UPDATE_CAMERA_DATA = registerEvent("EVENT_UPDATE_CAMERA_DATA",EntityComponentManager::EventInfo::Type::Explicit);
	EVENT_UPDATE_RENDER_SETTINGS = registerEvent("EVENT_UPDATE_RENDER_SETTINGS",EntityComponentManager::EventInfo::Type::Explicit);
	EVENT_UPDATE_RENDERER_BUFFER = registerEvent("EVENT_UPDATE_RENDERER_BUFFER",EntityComponentManager::EventInfo::Type::Explicit);
	EVENT_GET_SCENE_TEXTURE = registerEvent("EVENT_GET_SCENE_TEXTURE",EntityComponentManager::EventInfo::Type::Explicit);
	EVENT_GET_PRESENTATION_TEXTURE = registerEvent("EVENT_GET_PRESENTATION_TEXTURE",EntityComponentManager::EventInfo::Type::Explicit);
	EVENT_GET_HDR_PRESENTATION_TEXTURE = registerEvent("EVENT_GET_HDR_PRESENTATION_TEXTURE",EntityComponentManager::EventInfo::Type::Explicit);
	EVENT_RECORD_COMMAND_BUFFERS = registerEvent("EVENT_RECORD_COMMAND_BUFFERS",EntityComponentManager::EventInfo::Type::Explicit);
	EVENT_RENDER = registerEvent("EVENT_RENDER",EntityComponentManager::EventInfo::Type::Explicit);
}

void CRendererComponent::InitializeLuaObject(lua_State *l) {return BaseEntityComponent::InitializeLuaObject<std::remove_reference_t<decltype(*this)>>(l);}

void CRendererComponent::UpdateRenderSettings() {InvokeEventCallbacks(EVENT_UPDATE_RENDER_SETTINGS);}

void CRendererComponent::UpdateRendererBuffer(std::shared_ptr<prosper::IPrimaryCommandBuffer> &drawCmd)
{
	CEUpdateRendererBuffer evData {drawCmd};
	InvokeEventCallbacks(EVENT_UPDATE_RENDERER_BUFFER,evData);
}

void CRendererComponent::UpdateCameraData(pragma::CSceneComponent &scene,pragma::CameraData &cameraData)
{
	pragma::CEUpdateCameraData evData {scene,cameraData};
	InvokeEventCallbacks(EVENT_UPDATE_CAMERA_DATA,evData);
}

void CRendererComponent::RecordCommandBuffers(const util::DrawSceneInfo &drawSceneInfo)
{
	pragma::CEDrawSceneInfo evData {drawSceneInfo};
	InvokeEventCallbacks(EVENT_RECORD_COMMAND_BUFFERS,evData);
}
void CRendererComponent::Render(const util::DrawSceneInfo &drawSceneInfo)
{
	BeginRendering(drawSceneInfo);
	pragma::CERender evData {drawSceneInfo};
	InvokeEventCallbacks(EVENT_RENDER,evData);
	EndRendering();
}

prosper::Texture *CRendererComponent::GetSceneTexture()
{
	pragma::CEGetSceneTexture evData {};
	InvokeEventCallbacks(EVENT_GET_SCENE_TEXTURE,evData);
	return evData.resultTexture;
}
prosper::Texture *CRendererComponent::GetPresentationTexture()
{
	pragma::CEGetPresentationTexture evData {};
	InvokeEventCallbacks(EVENT_GET_PRESENTATION_TEXTURE,evData);
	return evData.resultTexture;
}
prosper::Texture *CRendererComponent::GetHDRPresentationTexture()
{
	pragma::CEGetHdrPresentationTexture evData {};
	InvokeEventCallbacks(EVENT_GET_HDR_PRESENTATION_TEXTURE,evData);
	return evData.resultTexture;
}

bool CRendererComponent::ReloadRenderTarget(pragma::CSceneComponent &scene,uint32_t width,uint32_t height)
{
	pragma::CEReloadRenderTarget evData {scene,width,height};
	InvokeEventCallbacks(EVENT_RELOAD_RENDER_TARGET,evData);
	if(evData.resultSuccess)
	{
		m_width = width;
		m_height = height;
	}
	return evData.resultSuccess;
}

bool CRendererComponent::ReloadBloomRenderTarget(uint32_t width)
{
	pragma::CEReloadBloomRenderTarget evData {width};
	InvokeEventCallbacks(EVENT_RELOAD_BLOOM_RENDER_TARGET,evData);
	return evData.resultSuccess;
}

void CRendererComponent::EndRendering() {InvokeEventCallbacks(EVENT_END_RENDERING);}
void CRendererComponent::BeginRendering(const util::DrawSceneInfo &drawSceneInfo)
{
	const_cast<pragma::CSceneComponent*>(drawSceneInfo.scene.get())->UpdateBuffers(drawSceneInfo.commandBuffer);
	InvokeEventCallbacks(EVENT_BEGIN_RENDERING);
}

////////////

CEReloadRenderTarget::CEReloadRenderTarget(pragma::CSceneComponent &scene,uint32_t width,uint32_t height)
	: ComponentEvent{},scene{scene},width{width},height{height}
{}

void CEReloadRenderTarget::PushArguments(lua_State *l)
{
	scene.PushLuaObject(l);
	Lua::PushInt(l,width);
	Lua::PushInt(l,height);
}

void CEReloadRenderTarget::HandleReturnValues(lua_State *l)
{
	if(Lua::IsSet(l,-1))
		resultSuccess = Lua::CheckBool(l,-1);
}

////////////

CEBeginRendering::CEBeginRendering(const util::DrawSceneInfo &drawSceneInfo)
	: ComponentEvent{},drawSceneInfo{drawSceneInfo}
{}
void CEBeginRendering::PushArguments(lua_State *l)
{
	Lua::Push<const util::DrawSceneInfo*>(l,&drawSceneInfo);
}

////////////

CEReloadBloomRenderTarget::CEReloadBloomRenderTarget(uint32_t width)
	: ComponentEvent{},width{width}
{}
void CEReloadBloomRenderTarget::PushArguments(lua_State *l)
{
	Lua::PushInt(l,width);
}
void CEReloadBloomRenderTarget::HandleReturnValues(lua_State *l)
{
	if(Lua::IsSet(l,-1))
		resultSuccess = Lua::CheckBool(l,-1);
}

////////////

CEUpdateCameraData::CEUpdateCameraData(pragma::CSceneComponent &scene,pragma::CameraData &cameraData)
	: scene{scene},cameraData{cameraData}
{}

////////////

void CEGetSceneTexture::HandleReturnValues(lua_State *l)
{
	if(Lua::IsSet(l,-1))
		resultTexture = &Lua::Check<prosper::Texture>(l,-1);
}

////////////

CERender::CERender(const util::DrawSceneInfo &drawSceneInfo)
	: drawSceneInfo{drawSceneInfo}
{}
void CERender::PushArguments(lua_State *l)
{
	Lua::Push<const util::DrawSceneInfo*>(l,&drawSceneInfo);
}

////////////

CEUpdateRendererBuffer::CEUpdateRendererBuffer(const std::shared_ptr<prosper::IPrimaryCommandBuffer> &drawCommandBuffer)
	: drawCommandBuffer{drawCommandBuffer}
{}
void CEUpdateRendererBuffer::PushArguments(lua_State *l)
{
	Lua::Push<std::shared_ptr<Lua::Vulkan::CommandBuffer>>(l,std::static_pointer_cast<Lua::Vulkan::CommandBuffer>(drawCommandBuffer));
}
