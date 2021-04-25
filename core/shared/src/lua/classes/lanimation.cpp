/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_shared.h"
#include "pragma/lua/classes/lanimation.h"
#include "pragma/lua/libraries/lfile.h"
#include <pragma/math/angle/wvquaternion.h>
#include "luasystem.h"
#include "pragma/model/animation/vertex_animation.hpp"
#include "pragma/model/animation/animation.hpp"
#include "pragma/model/animation/skeletal_animation.hpp"
#include "pragma/model/modelmesh.h"
#include "pragma/file_formats/wad.h"
#include "pragma/util/util_game.hpp"
#include <udm.hpp>

void Lua::Animation::Create(lua_State *l)
{
	auto anim = pragma::animation::Animation::Create();
	Lua::Push<std::shared_ptr<pragma::animation::Animation>>(l,anim);
}
void Lua::Animation::Load(lua_State *l,LFile &f)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto fptr = f.GetHandle();
	auto offset = fptr->Tell();
	auto len = strlen(udm::HEADER_IDENTIFIER);
	auto isUdmFormat = true;
	for(auto i=decltype(len){0u};i<len;++i)
	{
		if(fptr->ReadChar() != udm::HEADER_IDENTIFIER[i])
		{
			isUdmFormat = false;
			break;
		}
	}
	fptr->Seek(offset);
	if(isUdmFormat)
	{
		auto udmData = util::load_udm_asset(fptr);
		if(udmData == nullptr)
			return;
		std::string err;
		auto anim = pragma::animation::Animation::Load(udmData->GetAssetData(),err);
		if(anim == nullptr)
			return;
		Lua::Push(l,anim);
		return;
	}

	FWAD wad;
	auto anim = std::shared_ptr<pragma::animation::Animation>(wad.ReadData(33,f.GetHandle())); // Animation version has been introduced at model version 33, after which the model version is no longer taken into account, so we'll always treat it as model version 33 here
	if(anim == nullptr)
		return;
	Lua::Push<std::shared_ptr<pragma::animation::Animation>>(l,anim);
#endif
}
void Lua::Animation::RegisterActivityEnum(lua_State *l,const std::string &name)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto &reg = pragma::animation::Animation::GetActivityEnumRegister();
	auto id = reg.RegisterEnum(name);
	Lua::PushInt(l,id);
#endif
}
void Lua::Animation::RegisterEventEnum(lua_State *l,const std::string &name)
{
	auto &reg = pragma::animation::Animation::GetEventEnumRegister();
	auto id = reg.RegisterEnum(name);
	Lua::PushInt(l,id);
}
void Lua::Animation::GetActivityEnums(lua_State *l)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto &reg = pragma::animation::Animation::GetActivityEnumRegister();
	auto t = Lua::CreateTable(l);
	uint32_t idx = 1;
	for(auto &name : reg.GetEnums())
	{
		Lua::PushInt(l,idx++);
		Lua::PushString(l,name);
		Lua::SetTableValue(l,t);
	}
#endif
}
void Lua::Animation::GetEventEnums(lua_State *l)
{
	auto &reg = pragma::animation::Animation::GetEventEnumRegister();
	auto t = Lua::CreateTable(l);
	uint32_t idx = 1;
	for(auto &name : reg.GetEnums())
	{
		Lua::PushInt(l,idx++);
		Lua::PushString(l,name);
		Lua::SetTableValue(l,t);
	}
}
void Lua::Animation::GetActivityEnumName(lua_State *l,uint32_t id)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto &reg = pragma::animation::Animation::GetActivityEnumRegister();
	auto *name = reg.GetEnumName(id);
	if(name == nullptr)
		return;
	Lua::PushString(l,*name);
#endif
}
void Lua::Animation::GetEventEnumName(lua_State *l,uint32_t id)
{
	auto &reg = pragma::animation::Animation::GetEventEnumRegister();
	auto *name = reg.GetEnumName(id);
	if(name == nullptr)
		return;
	Lua::PushString(l,*name);
}
void Lua::Animation::FindActivityId(lua_State *l,const std::string &name)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto &reg = pragma::animation::Animation::GetActivityEnumRegister();
	uint32_t id;
	if(reg.GetEnumValue(name,id) == false)
		return;
	Lua::PushInt(l,id);
#endif
}
void Lua::Animation::FindEventId(lua_State *l,const std::string &name)
{
	auto &reg = pragma::animation::Animation::GetEventEnumRegister();
	uint32_t id;
	if(reg.GetEnumValue(name,id) == false)
		return;
	Lua::PushInt(l,id);
}
void Lua::Animation::GetActivity(lua_State *l,pragma::animation::Animation &anim) {Lua::PushInt(l,pragma::animation::skeletal::get_activity(anim));}
void Lua::Animation::SetActivity(lua_State*,pragma::animation::Animation &anim,uint16_t act) {pragma::animation::skeletal::set_activity(anim,static_cast<Activity>(act));}
void Lua::Animation::GetActivityWeight(lua_State *l,pragma::animation::Animation &anim) {Lua::PushInt(l,pragma::animation::skeletal::get_activity_weight(anim));}
void Lua::Animation::SetActivityWeight(lua_State*,pragma::animation::Animation &anim,uint8_t weight) {pragma::animation::skeletal::set_activity_weight(anim,weight);}
void Lua::Animation::GetSpeedFactor(lua_State *l,pragma::animation::Animation &anim) {Lua::PushInt(l,anim.GetAnimationSpeedFactor());}
void Lua::Animation::SetSpeedFactor(lua_State*,pragma::animation::Animation &anim,float factor) {anim.SetAnimationSpeedFactor(factor);}
void Lua::Animation::GetFlags(lua_State *l,pragma::animation::Animation &anim) {Lua::PushInt(l,umath::to_integral(anim.GetFlags()));}
void Lua::Animation::SetFlags(lua_State*,pragma::animation::Animation &anim,uint32_t flags) {anim.SetFlags(static_cast<FAnim>(flags));}
void Lua::Animation::AddFlags(lua_State*,pragma::animation::Animation &anim,uint32_t flags) {anim.AddFlags(static_cast<FAnim>(flags));}
void Lua::Animation::RemoveFlags(lua_State*,pragma::animation::Animation &anim,uint32_t flags) {anim.RemoveFlags(static_cast<FAnim>(flags));}
void Lua::Animation::GetFrame(lua_State *l,pragma::animation::Animation &anim,unsigned int ID)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto frame = anim.GetFrame(ID);
	if(frame == nullptr)
		return;
	luabind::object(l,frame).push(l);
#endif
}
void Lua::Animation::AddFrame(lua_State*,pragma::animation::Animation &anim,::Frame &frame)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	anim.AddFrame(frame.shared_from_this());
#endif
}
void Lua::Animation::GetFrames(lua_State *l,pragma::animation::Animation &anim)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto numFrames = anim.GetFrameCount();
	auto t = Lua::CreateTable(l);
	for(auto i=decltype(numFrames){0};i<numFrames;++i)
	{
		auto frame = anim.GetFrame(i);
		Lua::PushInt(l,i +1);
		Lua::Push<std::shared_ptr<::Frame>>(l,frame);
		Lua::SetTableValue(l,t);
	}
#endif
}
void Lua::Animation::GetDuration(lua_State *l,pragma::animation::Animation &anim) {Lua::PushNumber(l,anim.GetDuration());}

void Lua::Animation::GetAnimationEventArguments(lua_State *l,int32_t tArgs,std::vector<std::string> &args)
{
	auto numArgs = Lua::GetObjectLength(l,tArgs);
	for(auto i=decltype(numArgs){0};i<numArgs;++i)
	{
		Lua::PushInt(l,i +1); /* 1 */
		Lua::GetTableValue(l,tArgs);

		auto *arg = Lua::CheckString(l,-1);
		args.push_back(arg);

		Lua::Pop(l,1);
	}
}
void Lua::Animation::AddEvent(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId,uint32_t eventId,luabind::object args)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	int32_t tArgs = 4;
	Lua::CheckTable(l,tArgs);
	auto ev = std::make_unique<AnimationEvent>();
	ev->eventID = static_cast<AnimationEvent::Type>(eventId);

	GetAnimationEventArguments(l,tArgs,ev->arguments);

	auto *pEv = ev.get();
	ev.release();
	anim.AddEvent(frameId,pEv);
#endif
}
void Lua::Animation::PushAnimationEvent(lua_State *l,const AnimationEvent &ev)
{
	auto tEvent = Lua::CreateTable(l);

	Lua::PushString(l,"type");
	Lua::PushInt(l,ev.eventID);
	Lua::SetTableValue(l,tEvent);

	Lua::PushString(l,"arguments");
	auto tArgs = Lua::CreateTable(l);
	auto &args = ev.arguments;
	for(auto j=decltype(args.size()){0};j<args.size();++j)
	{
		Lua::PushInt(l,j +1);
		Lua::PushString(l,args[j]);
		Lua::SetTableValue(l,tArgs);
	}
	Lua::SetTableValue(l,tEvent);
}
void Lua::Animation::GetEvents(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto *events = anim.GetEvents(frameId);

	auto t = Lua::CreateTable(l);
	if(events == nullptr)
		return;
	for(auto i=decltype(events->size()){0};i<events->size();++i)
	{
		Lua::PushInt(l,i +1);
		PushAnimationEvent(l,*(*events)[i]);

		Lua::SetTableValue(l,t);
	}
#endif
}
void Lua::Animation::GetEvents(lua_State *l,pragma::animation::Animation &anim)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto numFrames = anim.GetFrameCount();
	auto t = Lua::CreateTable(l);
	for(auto i=decltype(numFrames){0};i<numFrames;++i)
	{
		auto *events = anim.GetEvents(i);
		if(events == nullptr)
			continue;
		Lua::PushInt(l,i);
		auto tEvent = Lua::CreateTable(l);
		for(auto j=decltype(events->size()){0};j<events->size();++j)
		{
			auto &ev = (*events)[j];
			Lua::PushInt(l,j +1);
			PushAnimationEvent(l,*ev);
			Lua::SetTableValue(l,tEvent);
		}
		Lua::SetTableValue(l,t);
	}
#endif
}
void Lua::Animation::GetEventCount(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto *events = anim.GetEvents(frameId);
	Lua::PushInt(l,(events != nullptr) ? events->size() : 0);
#endif
}
void Lua::Animation::GetEventCount(lua_State *l,pragma::animation::Animation &anim)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto numFrames = anim.GetFrameCount();
	uint32_t numEvents = 0;
	for(auto i=decltype(numFrames){0};i<numFrames;++i)
	{
		auto *events = anim.GetEvents(i);
		if(events == nullptr)
			continue;
		numEvents += static_cast<uint32_t>(events->size());
	}
	Lua::PushInt(l,numEvents);
#endif
}
void Lua::Animation::GetFadeInTime(lua_State *l,pragma::animation::Animation &anim) {Lua::PushNumber(l,anim.GetFadeInTime());}
void Lua::Animation::GetFadeOutTime(lua_State *l,pragma::animation::Animation &anim) {Lua::PushNumber(l,anim.GetFadeOutTime());}
static void push_blend_controller(lua_State *l,AnimationBlendController &bc)
{
	auto t = Lua::CreateTable(l); /* 1 */

	Lua::PushString(l,"controller"); /* 2 */
	Lua::PushInt(l,bc.controller); /* 3 */
	Lua::SetTableValue(l,t); /* 1 */

	Lua::PushString(l,"transitions"); /* 2 */
	auto tTransitions = Lua::CreateTable(l); /* 3 */
	auto &transitions = bc.transitions;
	for(auto i=decltype(transitions.size()){0};i<transitions.size();++i)
	{
		Lua::PushInt(l,i +1); /* 4 */

		auto tTransition = Lua::CreateTable(l); /* 5 */
		auto &t = transitions[i];

		Lua::PushString(l,"animation"); /* 6 */
		Lua::PushInt(l,t.animation); /* 7 */
		Lua::SetTableValue(l,tTransition); /* 5 */

		Lua::PushString(l,"transition"); /* 6 */
		Lua::PushInt(l,t.transition); /* 7 */
		Lua::SetTableValue(l,tTransition); /* 5 */

		Lua::SetTableValue(l,tTransitions); /* 3 */
	}

	Lua::PushString(l,"animationPostBlendController"); /* 2 */
	Lua::PushInt(l,bc.animationPostBlendController); /* 3 */
	Lua::SetTableValue(l,t); /* 1 */

	Lua::PushString(l,"animationPostBlendTarget"); /* 2 */
	Lua::PushInt(l,bc.animationPostBlendTarget); /* 3 */
	Lua::SetTableValue(l,t); /* 1 */

	Lua::SetTableValue(l,t); /* 1 */
}
void Lua::Animation::GetBlendController(lua_State *l,pragma::animation::Animation &anim)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto *bc = anim.GetBlendController();
	if(bc == nullptr)
		return;
	push_blend_controller(l,*bc);
#endif
}
void Lua::Animation::CalcRenderBounds(lua_State *l,pragma::animation::Animation &anim,const std::shared_ptr<::Model> &mdl)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	anim.CalcRenderBounds(*mdl);
	GetRenderBounds(l,anim,mdl);
#endif
}
void Lua::Animation::GetRenderBounds(lua_State *l,pragma::animation::Animation &anim,const std::shared_ptr<::Model>&)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto &bounds = anim.GetRenderBounds();
	Lua::Push<Vector3>(l,bounds.first);
	Lua::Push<Vector3>(l,bounds.second);
#endif
}
void Lua::Animation::Rotate(lua_State*,pragma::animation::Animation &anim,::Skeleton *skeleton,const Quat &rot)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	anim.Rotate(*skeleton,rot);
#endif
}
void Lua::Animation::Translate(lua_State*,pragma::animation::Animation &anim,::Skeleton *skeleton,const Vector3 &t)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	anim.Translate(*skeleton,t);
#endif
}
void Lua::Animation::Scale(lua_State *l,pragma::animation::Animation &anim,const Vector3 &scale)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	anim.Scale(scale);
#endif
}
void Lua::Animation::RemoveEvent(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId,uint32_t idx)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto *events = anim.GetEvents(frameId);
	if(events == nullptr || idx >= events->size())
		return;
	events->erase(events->begin() +idx);
#endif
}
void Lua::Animation::SetEventData(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId,uint32_t idx,uint32_t type,luabind::object oArgs)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto *events = anim.GetEvents(frameId);
	if(events == nullptr || idx >= events->size())
		return;
	auto &ev = events->at(idx);

	int32_t tArgs = 5;
	Lua::CheckTable(l,tArgs);

	ev->eventID = static_cast<AnimationEvent::Type>(type);
	ev->arguments.clear();
	GetAnimationEventArguments(l,tArgs,ev->arguments);
#endif
}
void Lua::Animation::SetEventType(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId,uint32_t idx,uint32_t type)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto *events = anim.GetEvents(frameId);
	if(events == nullptr || idx >= events->size())
		return;
	auto &ev = events->at(idx);

	ev->eventID = static_cast<AnimationEvent::Type>(type);
#endif
}
void Lua::Animation::SetEventArgs(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId,uint32_t idx,luabind::object oArgs)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto *events = anim.GetEvents(frameId);
	if(events == nullptr || idx >= events->size())
		return;
	auto &ev = events->at(idx);

	int32_t tArgs = 4;
	Lua::CheckTable(l,tArgs);

	ev->arguments.clear();
	GetAnimationEventArguments(l,tArgs,ev->arguments);
#endif
}
void Lua::Animation::LookupBone(lua_State *l,pragma::animation::Animation &anim,uint32_t boneId)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto &list = anim.GetBoneList();
	auto it = std::find(list.begin(),list.end(),boneId);
	if(it == list.end())
		return;
	Lua::PushInt(l,it -list.begin());
#endif
}
void Lua::Animation::SetBoneList(lua_State *l,pragma::animation::Animation &anim,luabind::object o)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	int32_t t = 2;
	auto numBones = Lua::GetObjectLength(l,t);
	std::vector<uint16_t> list;
	list.reserve(numBones);
	for(auto i=decltype(numBones){0};i<numBones;++i)
	{
		Lua::PushInt(l,i +1);
		Lua::GetTableValue(l,t);
		list.push_back(Lua::CheckInt(l,-1));

		Lua::Pop(l,1);
	}
	anim.SetBoneList(list);
#endif
}
void Lua::Animation::SetFadeInTime(lua_State *l,pragma::animation::Animation &anim,float t) {anim.SetFadeInTime(t);}
void Lua::Animation::SetFadeOutTime(lua_State *l,pragma::animation::Animation &anim,float t) {anim.SetFadeOutTime(t);}

void Lua::Animation::SetBoneWeight(lua_State *l,pragma::animation::Animation &anim,uint32_t boneId,float t)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	anim.SetBoneWeight(boneId,t);
#endif
}
void Lua::Animation::GetBoneWeight(lua_State *l,pragma::animation::Animation &anim,uint32_t boneId)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto weight = anim.GetBoneWeight(boneId);
	Lua::PushNumber(l,weight);
#endif
}
void Lua::Animation::GetBoneWeights(lua_State *l,pragma::animation::Animation &anim)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto &weights = anim.GetBoneWeights();
	auto t = Lua::CreateTable(l);
	for(auto i=decltype(weights.size()){0};i<weights.size();++i)
	{
		Lua::PushInt(l,i +1);
		Lua::PushNumber(l,weights.at(i));
		Lua::SetTableValue(l,t);
	}
#endif
}

///////////////////////////////////////

void Lua::Frame::Create(lua_State *l,uint32_t numBones) {Lua::Push<std::shared_ptr<::Frame>>(l,::Frame::Create(numBones));}
void Lua::Frame::GetBoneMatrix(lua_State *l,::Frame &frame,unsigned int boneID)
{
	auto mat = umat::identity();
	if(frame.GetBoneMatrix(boneID,&mat) == false)
		return;
	luabind::object(l,mat).push(l);
}

void Lua::Frame::GetBonePosition(lua_State *l,::Frame &frame,unsigned int boneID)
{
	Vector3 *pos = frame.GetBonePosition(boneID);
	if(pos == NULL)
		return;
	auto *rot = frame.GetBoneOrientation(boneID);
	auto *scale = frame.GetBoneScale(boneID);
	luabind::object(l,*pos).push(l);
	luabind::object(l,*rot).push(l);
	Lua::Push<Vector3>(l,(scale != nullptr) ? *scale : Vector3{1.f,1.f,1.f});
}

void Lua::Frame::GetBoneOrientation(lua_State *l,::Frame &frame,unsigned int boneID)
{
	auto *rot = frame.GetBoneOrientation(boneID);
	if(rot == nullptr)
		return;
	luabind::object(l,*rot).push(l);
}
void Lua::Frame::SetBonePosition(lua_State*,::Frame &frame,unsigned int boneID,const Vector3 &pos)
{
	frame.SetBonePosition(boneID,pos);
}
void Lua::Frame::SetBoneOrientation(lua_State*,::Frame &frame,unsigned int boneID,const Quat &rot)
{
	frame.SetBoneOrientation(boneID,rot);
}
void Lua::Frame::SetBoneTransform(lua_State *l,::Frame &frame,unsigned int boneID,const Vector3 &pos,const Quat &rot)
{
	frame.SetBonePosition(boneID,pos);
	frame.SetBoneOrientation(boneID,rot);
}
void Lua::Frame::SetBoneTransform(lua_State *l,::Frame &frame,unsigned int boneID,const Vector3 &pos,const Quat &rot,const Vector3 &scale)
{
	SetBoneTransform(l,frame,boneID,pos,rot);
	frame.SetBoneScale(boneID,scale);
}
void Lua::Frame::Localize(lua_State*,::Frame &frame,pragma::animation::Animation &anim,::Skeleton *skeleton)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	frame.Localize(anim,*skeleton);
#endif
}
void Lua::Frame::Globalize(lua_State*,::Frame &frame,pragma::animation::Animation &anim,::Skeleton *skeleton)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	frame.Globalize(anim,*skeleton);
#endif
}
void Lua::Frame::Localize(lua_State*,::Frame &frame,::Skeleton *skeleton)
{
	frame.Localize(*skeleton);
}
void Lua::Frame::Globalize(lua_State*,::Frame &frame,::Skeleton *skeleton)
{
	frame.Globalize(*skeleton);
}
void Lua::Frame::CalcRenderBounds(lua_State *l,::Frame &frame,pragma::animation::Animation &anim,const std::shared_ptr<::Model> &mdl)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	auto renderBounds = frame.CalcRenderBounds(anim,*mdl);
	Lua::Push<Vector3>(l,renderBounds.first);
	Lua::Push<Vector3>(l,renderBounds.second);
#endif
}
void Lua::Frame::Rotate(lua_State*,::Frame &frame,pragma::animation::Animation &anim,::Skeleton *skeleton,const Quat &rot)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	frame.Rotate(anim,*skeleton,rot);
#endif
}
void Lua::Frame::Translate(lua_State*,::Frame &frame,pragma::animation::Animation &anim,::Skeleton *skeleton,const Vector3 &t)
{
#if ENABLE_LEGACY_ANIMATION_SYSTEM
	frame.Translate(anim,*skeleton,t);
#endif
}
void Lua::Frame::Scale(lua_State *l,::Frame &frame,const Vector3 &scale)
{
	frame.Scale(scale);
}
void Lua::Frame::GetMoveTranslation(lua_State *l,::Frame &frame)
{
	Vector2 translation;
	frame.GetMoveOffset(&translation.x,&translation.y);
	Lua::PushNumber(l,translation.x);
	Lua::PushNumber(l,translation.y);
}
void Lua::Frame::GetMoveTranslationX(lua_State *l,::Frame &frame)
{
	Vector2 translation;
	frame.GetMoveOffset(&translation.x,&translation.y);
	Lua::PushNumber(l,translation.x);
}
void Lua::Frame::GetMoveTranslationZ(lua_State *l,::Frame &frame)
{
	Vector2 translation;
	frame.GetMoveOffset(&translation.x,&translation.y);
	Lua::PushNumber(l,translation.y);
}
void Lua::Frame::SetMoveTranslation(lua_State *l,::Frame &frame,float x,float z)
{
	frame.SetMoveOffset(x,z);
}
void Lua::Frame::SetMoveTranslationX(lua_State *l,::Frame &frame,float x)
{
	Vector2 translation;
	frame.GetMoveOffset(&translation.x,&translation.y);
	frame.SetMoveOffset(x,translation.y);
}
void Lua::Frame::SetMoveTranslationZ(lua_State *l,::Frame &frame,float z)
{
	Vector2 translation;
	frame.GetMoveOffset(&translation.x,&translation.y);
	frame.SetMoveOffset(translation.x,z);
}
void Lua::Frame::SetBoneScale(lua_State *l,::Frame &frame,uint32_t boneId,const Vector3 &scale)
{
	frame.SetBoneScale(boneId,scale);
}
void Lua::Frame::GetBoneScale(lua_State *l,::Frame &frame,uint32_t boneId)
{
	auto *scale = frame.GetBoneScale(boneId);
	if(scale == nullptr)
		return;
	Lua::Push<Vector3>(l,*scale);
}
void Lua::Frame::GetLocalBoneTransform(lua_State *l,::Frame &frame,::Skeleton &skeleton,uint32_t boneId)
{
	auto wpBone = skeleton.GetBone(boneId);
	if(wpBone.expired())
		return;
	auto bone = wpBone.lock();
	Vector3 pos {};
	auto rot = uquat::identity();

	auto fGetTransform = [&frame](uint32_t boneId,Vector3 **pos,Quat **rot,Vector3 **scale) {
		*pos = frame.GetBonePosition(boneId);
		*rot = frame.GetBoneOrientation(boneId);
		*scale = frame.GetBoneScale(boneId);
	};

	// See also baseentity_bones.cpp
	std::function<void(const std::shared_ptr<Bone>&,Vector3&,Quat&,Vector3*)> fIterateHierarchy = nullptr;
	fIterateHierarchy = [fGetTransform,&fIterateHierarchy](const std::shared_ptr<Bone> &bone,Vector3 &pos,Quat &rot,Vector3 *scale) {
		Vector3 *tpos = nullptr;
		Quat *trot = nullptr;
		Vector3 *tscale = nullptr;
		fGetTransform(bone->ID,&tpos,&trot,&tscale);
		assert(tpos != nullptr && trot != nullptr);
		if(tpos == nullptr || trot == nullptr)
		{
			pos = {};
			rot = {};
			if(scale != nullptr)
				*scale = Vector3{1.f,1.f,1.f};
			return;
		}
		if(bone->parent.expired() == true)
		{
			pos = *tpos;
			rot = *trot;
			if(scale != nullptr)
				*scale = (tscale != nullptr) ? *tscale : Vector3{1.f,1.f,1.f};
			return;
		}
		fIterateHierarchy(bone->parent.lock(),pos,rot,scale);
		auto bonePos = *tpos;
		auto &boneRot = *trot;
		uvec::rotate(&bonePos,rot);
		if(scale != nullptr)
			bonePos *= *scale;
		pos += bonePos;

		if(scale != nullptr)
			*scale *= (tscale != nullptr) ? *tscale : Vector3{1.f,1.f,1.f};

		rot = rot *boneRot;
	};
	Vector3 nscale(1.f,1.f,1.f);
	fIterateHierarchy(bone,pos,rot,&nscale);

	Lua::Push<Vector3>(l,pos);
	Lua::Push<Quat>(l,rot);
	Lua::Push<Vector3>(l,nscale);
}
void Lua::Frame::GetBoneCount(lua_State *l,::Frame &frame) {Lua::PushInt(l,frame.GetBoneCount());}
void Lua::Frame::SetBoneCount(lua_State *l,::Frame &frame,uint32_t boneCount) {frame.SetBoneCount(boneCount);}
void Lua::Frame::SetBonePose(lua_State*,::Frame &frame,uint32_t boneId,const umath::ScaledTransform &pose) {frame.SetBonePose(boneId,pose);}
void Lua::Frame::SetBonePose(lua_State*,::Frame &frame,uint32_t boneId,const umath::Transform &pose) {frame.SetBonePose(boneId,pose);}
void Lua::Frame::GetBonePose(lua_State *l,::Frame &frame,uint32_t boneId)
{
	umath::ScaledTransform pose;
	if(frame.GetBonePose(boneId,pose) == false)
		return;
	Lua::Push(l,pose);
}
///////////////////////////////////////
void Lua::VertexAnimation::GetMeshAnimations(lua_State *l,::VertexAnimation &anim)
{
	auto &meshAnims = anim.GetMeshAnimations();
	auto t = Lua::CreateTable(l);
	auto meshAnimIdx = 1u;
	for(auto meshAnim : meshAnims)
	{
		Lua::PushInt(l,meshAnimIdx++);
		Lua::Push<std::shared_ptr<::MeshVertexAnimation>>(l,meshAnim);
		Lua::SetTableValue(l,t);
	}
}
void Lua::VertexAnimation::GetName(lua_State *l,::VertexAnimation &anim) {Lua::PushString(l,anim.GetName());}
///////////////////////////////////////
void Lua::MeshVertexAnimation::GetFrames(lua_State *l,::MeshVertexAnimation &anim)
{
	auto &frames = anim.GetFrames();
	auto t = Lua::CreateTable(l);
	auto frameIdx = 1u;
	for(auto &frame : frames)
	{
		Lua::PushInt(l,frameIdx++);
		Lua::Push<std::shared_ptr<::MeshVertexFrame>>(l,frame);
		Lua::SetTableValue(l,t);
	}
}
void Lua::MeshVertexAnimation::GetMesh(lua_State *l,::MeshVertexAnimation &anim)
{
	auto *mesh = anim.GetMesh();
	auto *subMesh = anim.GetSubMesh();
	if(mesh == nullptr || subMesh == nullptr)
		return;
	Lua::Push<std::shared_ptr<ModelMesh>>(l,mesh->shared_from_this());
	Lua::Push<std::shared_ptr<ModelSubMesh>>(l,subMesh->shared_from_this());
}
///////////////////////////////////////
void Lua::MeshVertexFrame::GetVertices(lua_State *l,::MeshVertexFrame &frame)
{
	auto &verts = frame.GetVertices();
	auto t = Lua::CreateTable(l);
	auto vertIdx = 1u;
	for(auto &v : verts)
	{
		Lua::PushInt(l,vertIdx++);
		Lua::Push<Vector3>(l,{umath::float16_to_float32(v.at(0)),umath::float16_to_float32(v.at(1)),umath::float16_to_float32(v.at(2))});
		Lua::SetTableValue(l,t);
	}
}
void Lua::MeshVertexFrame::SetVertexCount(lua_State *l,::MeshVertexFrame &frame,uint32_t numVerts) {frame.SetVertexCount(numVerts);}
void Lua::MeshVertexFrame::SetVertexPosition(lua_State *l,::MeshVertexFrame &frame,uint32_t vertIdx,const Vector3 &pos) {frame.SetVertexPosition(vertIdx,pos);}
void Lua::MeshVertexFrame::GetVertexPosition(lua_State *l,::MeshVertexFrame &frame,uint32_t vertIdx)
{
	Vector3 pos {};
	if(frame.GetVertexPosition(vertIdx,pos) == false)
		return;
	Lua::Push<Vector3>(l,pos);
}
void Lua::MeshVertexFrame::SetVertexNormal(lua_State *l,::MeshVertexFrame &frame,uint32_t vertIdx,const Vector3 &n) {frame.SetVertexNormal(vertIdx,n);}
void Lua::MeshVertexFrame::GetVertexNormal(lua_State *l,::MeshVertexFrame &frame,uint32_t vertIdx)
{
	Vector3 n {};
	if(frame.GetVertexPosition(vertIdx,n) == false)
		return;
	Lua::Push<Vector3>(l,n);
}
