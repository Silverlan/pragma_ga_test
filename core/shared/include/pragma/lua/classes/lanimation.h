/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan */

#ifndef __LANIMATION_H__
#define __LANIMATION_H__

#include "pragma/networkdefinitions.h"
#include <pragma/lua/luaapi.h>

class MeshVertexAnimation;
class MeshVertexFrame;
class VertexAnimation;
class LFile;
class Frame;
class Skeleton;
struct AnimationEvent;
namespace pragma::animation {class Animation;};
namespace Lua
{
	namespace Animation
	{
		DLLNETWORK void PushAnimationEvent(lua_State *l,const AnimationEvent &ev);
		DLLNETWORK void GetAnimationEventArguments(lua_State *l,int32_t tArgs,std::vector<std::string> &args);

		DLLNETWORK void Create(lua_State *l);
		DLLNETWORK void Load(lua_State *l,LFile &f);
		DLLNETWORK void RegisterActivityEnum(lua_State *l,const std::string &name);
		DLLNETWORK void RegisterEventEnum(lua_State *l,const std::string &name);
		DLLNETWORK void GetActivityEnums(lua_State *l);
		DLLNETWORK void GetEventEnums(lua_State *l);
		DLLNETWORK void GetActivityEnumName(lua_State *l,uint32_t id);
		DLLNETWORK void GetEventEnumName(lua_State *l,uint32_t id);
		DLLNETWORK void FindActivityId(lua_State *l,const std::string &name);
		DLLNETWORK void FindEventId(lua_State *l,const std::string &name);

		DLLNETWORK void GetFrame(lua_State *l,pragma::animation::Animation &anim,unsigned int ID);
		DLLNETWORK void GetActivity(lua_State *l,pragma::animation::Animation &anim);
		DLLNETWORK void SetActivity(lua_State *l,pragma::animation::Animation &anim,uint16_t act);
		DLLNETWORK void GetActivityWeight(lua_State *l,pragma::animation::Animation &anim);
		DLLNETWORK void SetActivityWeight(lua_State *l,pragma::animation::Animation &anim,uint8_t weight);
		DLLNETWORK void GetSpeedFactor(lua_State *l,pragma::animation::Animation &anim);
		DLLNETWORK void SetSpeedFactor(lua_State *l,pragma::animation::Animation &anim,float factor);
		DLLNETWORK void GetFlags(lua_State *l,pragma::animation::Animation &anim);
		DLLNETWORK void SetFlags(lua_State *l,pragma::animation::Animation &anim,uint32_t flags);
		DLLNETWORK void AddFlags(lua_State *l,pragma::animation::Animation &anim,uint32_t flags);
		DLLNETWORK void RemoveFlags(lua_State *l,pragma::animation::Animation &anim,uint32_t flags);
		DLLNETWORK void AddFrame(lua_State *l,pragma::animation::Animation &anim,::Frame &frame);
		DLLNETWORK void GetFrames(lua_State *l,pragma::animation::Animation &anim);
		DLLNETWORK void GetDuration(lua_State *l,pragma::animation::Animation &anim);
		DLLNETWORK void AddEvent(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId,uint32_t eventId,luabind::object tArgs);
		DLLNETWORK void GetEvents(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId);
		DLLNETWORK void GetEvents(lua_State *l,pragma::animation::Animation &anim);
		DLLNETWORK void GetEventCount(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId);
		DLLNETWORK void GetEventCount(lua_State *l,pragma::animation::Animation &anim);
		DLLNETWORK void GetFadeInTime(lua_State *l,pragma::animation::Animation &anim);
		DLLNETWORK void GetFadeOutTime(lua_State *l,pragma::animation::Animation &anim);
		DLLNETWORK void GetBlendController(lua_State *l,pragma::animation::Animation &anim);
		DLLNETWORK void CalcRenderBounds(lua_State *l,pragma::animation::Animation &anim,const std::shared_ptr<::Model> &mdl);
		DLLNETWORK void GetRenderBounds(lua_State *l,pragma::animation::Animation &anim,const std::shared_ptr<::Model> &mdl);
		DLLNETWORK void Rotate(lua_State *l,pragma::animation::Animation &anim,::Skeleton *skeleton,const Quat &rot);
		DLLNETWORK void Translate(lua_State *l,pragma::animation::Animation &anim,::Skeleton *skeleton,const Vector3 &t);
		DLLNETWORK void Scale(lua_State *l,pragma::animation::Animation &anim,const Vector3 &scale);
		DLLNETWORK void RemoveEvent(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId,uint32_t idx);
		DLLNETWORK void SetEventData(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId,uint32_t idx,uint32_t type,luabind::object tArgs);
		DLLNETWORK void SetEventType(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId,uint32_t idx,uint32_t type);
		DLLNETWORK void SetEventArgs(lua_State *l,pragma::animation::Animation &anim,uint32_t frameId,uint32_t idx,luabind::object tArgs);
		DLLNETWORK void LookupBone(lua_State *l,pragma::animation::Animation &anim,uint32_t boneId);
		DLLNETWORK void SetBoneList(lua_State *l,pragma::animation::Animation &anim,luabind::object);
		DLLNETWORK void SetFadeInTime(lua_State *l,pragma::animation::Animation &anim,float t);
		DLLNETWORK void SetFadeOutTime(lua_State *l,pragma::animation::Animation &anim,float t);

		DLLNETWORK void SetBoneWeight(lua_State *l,pragma::animation::Animation &anim,uint32_t boneId,float t);
		DLLNETWORK void GetBoneWeight(lua_State *l,pragma::animation::Animation &anim,uint32_t boneId);
		DLLNETWORK void GetBoneWeights(lua_State *l,pragma::animation::Animation &anim);
	};
	namespace Frame
	{
		DLLNETWORK void Create(lua_State *l,uint32_t numBones);
		DLLNETWORK void GetBoneMatrix(lua_State *l,::Frame &frame,unsigned int boneID);
		DLLNETWORK void GetBonePosition(lua_State *l,::Frame &frame,unsigned int boneID);
		DLLNETWORK void GetBoneOrientation(lua_State *l,::Frame &frame,unsigned int boneID);
		DLLNETWORK void SetBonePosition(lua_State *l,::Frame &frame,unsigned int boneID,const Vector3 &pos);
		DLLNETWORK void SetBoneOrientation(lua_State *l,::Frame &frame,unsigned int boneID,const Quat &rot);
		DLLNETWORK void SetBoneTransform(lua_State *l,::Frame &frame,unsigned int boneID,const Vector3 &pos,const Quat &rot);
		DLLNETWORK void SetBoneTransform(lua_State *l,::Frame &frame,unsigned int boneID,const Vector3 &pos,const Quat &rot,const Vector3 &scale);
		DLLNETWORK void Localize(lua_State *l,::Frame &frame,pragma::animation::Animation &anim,::Skeleton *skeleton);
		DLLNETWORK void Globalize(lua_State *l,::Frame &frame,pragma::animation::Animation &anim,::Skeleton *skeleton);
		DLLNETWORK void Localize(lua_State *l,::Frame &frame,::Skeleton *skeleton);
		DLLNETWORK void Globalize(lua_State *l,::Frame &frame,::Skeleton *skeleton);
		DLLNETWORK void CalcRenderBounds(lua_State *l,::Frame &frame,pragma::animation::Animation &anim,const std::shared_ptr<::Model> &mdl);
		DLLNETWORK void Rotate(lua_State *l,::Frame &frame,pragma::animation::Animation &anim,::Skeleton *skeleton,const Quat &rot);
		DLLNETWORK void Translate(lua_State *l,::Frame &frame,pragma::animation::Animation &anim,::Skeleton *skeleton,const Vector3 &t);
		DLLNETWORK void Scale(lua_State *l,::Frame &frame,const Vector3 &scale);
		DLLNETWORK void GetMoveTranslation(lua_State *l,::Frame &frame);
		DLLNETWORK void GetMoveTranslationX(lua_State *l,::Frame &frame);
		DLLNETWORK void GetMoveTranslationZ(lua_State *l,::Frame &frame);
		DLLNETWORK void SetMoveTranslation(lua_State *l,::Frame &frame,float x,float z);
		DLLNETWORK void SetMoveTranslationX(lua_State *l,::Frame &frame,float x);
		DLLNETWORK void SetMoveTranslationZ(lua_State *l,::Frame &frame,float z);
		DLLNETWORK void SetBoneScale(lua_State *l,::Frame &frame,uint32_t boneId,const Vector3 &scale);
		DLLNETWORK void GetBoneScale(lua_State *l,::Frame &frame,uint32_t boneId);
		DLLNETWORK void GetLocalBoneTransform(lua_State*,::Frame &frame,::Skeleton &skeleton,uint32_t boneId);
		DLLNETWORK void GetBoneCount(lua_State*,::Frame &frame);
		DLLNETWORK void SetBoneCount(lua_State*,::Frame &frame,uint32_t boneCount);
		DLLNETWORK void SetBonePose(lua_State*,::Frame &frame,uint32_t boneId,const umath::ScaledTransform &pose);
		DLLNETWORK void SetBonePose(lua_State*,::Frame &frame,uint32_t boneId,const umath::Transform &pose);
		DLLNETWORK void GetBonePose(lua_State*,::Frame &frame,uint32_t boneId);
	};
	namespace VertexAnimation
	{
		DLLNETWORK void GetMeshAnimations(lua_State *l,::VertexAnimation &anim);
		DLLNETWORK void GetName(lua_State *l,::VertexAnimation &anim);
	};
	namespace MeshVertexAnimation
	{
		DLLNETWORK void GetFrames(lua_State *l,::MeshVertexAnimation &anim);
		DLLNETWORK void GetMesh(lua_State *l,::MeshVertexAnimation &anim);
	};
	namespace MeshVertexFrame
	{
		DLLNETWORK void GetVertices(lua_State *l,::MeshVertexFrame &frame);
		DLLNETWORK void SetVertexCount(lua_State *l,::MeshVertexFrame &frame,uint32_t numVerts);
		DLLNETWORK void SetVertexPosition(lua_State *l,::MeshVertexFrame &frame,uint32_t vertIdx,const Vector3 &pos);
		DLLNETWORK void GetVertexPosition(lua_State *l,::MeshVertexFrame &frame,uint32_t vertIdx);
		DLLNETWORK void SetVertexNormal(lua_State *l,::MeshVertexFrame &frame,uint32_t vertIdx,const Vector3 &n);
		DLLNETWORK void GetVertexNormal(lua_State *l,::MeshVertexFrame &frame,uint32_t vertIdx);
	};
};

#endif
