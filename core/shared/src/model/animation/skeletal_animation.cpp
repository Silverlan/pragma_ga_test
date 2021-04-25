/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#include "stdafx_shared.h"
#include "pragma/model/animation/skeletal_animation.hpp"
#include "pragma/model/animation/animation.hpp"
#include "pragma/model/animation/animation_channel.hpp"
#include "pragma/model/animation/animation_player.hpp"
#include "pragma/model/animation/animated_pose.hpp"
#include <udm.hpp>
#pragma optimize("",off)
Activity pragma::animation::skeletal::get_activity(const Animation &anim)
{
	auto act = Activity::Invalid;
	const_cast<Animation&>(anim).GetProperties()["activity"](act);
	return act;
}
void pragma::animation::skeletal::set_activity(Animation &anim,Activity act)
{
	anim.GetProperties()["activity"] = act;
}
uint8_t pragma::animation::skeletal::get_activity_weight(const Animation &anim)
{
	uint8_t weight = 1;
	const_cast<Animation&>(anim).GetProperties()["activityWeight"](weight);
	return weight;
}
void pragma::animation::skeletal::set_activity_weight(Animation &anim,uint8_t weight)
{
	anim.GetProperties()["activityWeight"] = weight;
}
std::pair<Vector3,Vector3> pragma::animation::skeletal::get_render_bounds(const Animation &anim)
{
	auto udmRenderBounds = const_cast<Animation&>(anim).GetProperties()["renderBounds"];
	std::pair<Vector3,Vector3> bounds {};
	udmRenderBounds["min"](bounds.first);
	udmRenderBounds["max"](bounds.second);
	return bounds;
}
void pragma::animation::skeletal::set_render_bounds(Animation &anim,const Vector3 &min,const Vector3 &max)
{
	auto udmRenderBounds = anim.GetProperties()["renderBounds"];
	udmRenderBounds["min"] = min;
	udmRenderBounds["max"] = max;
}
pragma::animation::skeletal::BoneChannelMap pragma::animation::skeletal::get_bone_channel_map(const Animation &animation,const Skeleton &skeleton)
{
	BoneChannelMap boneChannelMap;
	auto &channels = animation.GetChannels();
	for(auto i=decltype(channels.size()){0u};i<channels.size();++i)
	{
		auto &channel = channels[i];
		auto it = channel->targetPath.begin();
		auto end = channel->targetPath.end();
		if(it == end || *it != SK_ANIMATED_COMPONENT_NAME)
			continue;
		++it;
		auto boneId = (it != end) ? skeleton.LookupBone(*it) : -1;
		if(boneId == -1)
			continue;
		++it;
		if(it == end)
			continue;
		auto &attr = *it;
		auto &channelDesc = boneChannelMap[boneId];
		if(attr == ANIMATION_CHANNEL_PATH_POSITION)
		{
			channelDesc.positionChannel = i;
			continue;
		}
		if(attr == ANIMATION_CHANNEL_PATH_ROTATION)
		{
			channelDesc.rotationChannel = i;
			continue;
		}
		if(attr == ANIMATION_CHANNEL_PATH_SCALE)
		{
			channelDesc.scaleChannel = i;
			continue;
		}
	}
	return boneChannelMap;
}
void pragma::animation::skeletal::animation_slice_to_animated_pose(const BoneChannelMap &boneChannelMap,const AnimationSlice &slice,AnimatedPose &pose)
{
	auto &transforms = pose.GetTransforms();
	pose.SetTransformCount(boneChannelMap.size());
	uint32_t idx = 0;
	for(auto &pair : boneChannelMap)
	{
		auto boneId = pair.first;
		pose.SetBoneIndex(idx,boneId);

		auto &pose = transforms[idx];
		auto &channelDesc = pair.second;
		if(channelDesc.positionChannel != AnimBoneChannelDesc::INVALID_CHANNEL)
			pose.SetOrigin(slice.channelValues[channelDesc.positionChannel].GetValue<Vector3>());
		if(channelDesc.rotationChannel != AnimBoneChannelDesc::INVALID_CHANNEL)
			pose.SetRotation(slice.channelValues[channelDesc.rotationChannel].GetValue<Quat>());
		if(channelDesc.scaleChannel != AnimBoneChannelDesc::INVALID_CHANNEL)
			pose.SetScale(slice.channelValues[channelDesc.scaleChannel].GetValue<Vector3>());
		++idx;
	}
}
util::EnumRegister &pragma::animation::skeletal::get_activity_enum_register()
{
	static util::EnumRegister g_reg {};
	return g_reg;
}
bool pragma::animation::skeletal::is_bone_position_channel(const AnimationChannel &channel)
{
	auto it = channel.targetPath.begin();
	auto end = channel.targetPath.end();
	if(it == end)
		return false;
	--end;
	return *it == SK_ANIMATED_COMPONENT_NAME && *end == ANIMATION_CHANNEL_PATH_POSITION;
}
bool pragma::animation::skeletal::is_bone_rotation_channel(const AnimationChannel &channel)
{
	auto it = channel.targetPath.begin();
	auto end = channel.targetPath.end();
	if(it == end)
		return false;
	--end;
	return *it == SK_ANIMATED_COMPONENT_NAME && *end == ANIMATION_CHANNEL_PATH_ROTATION;
}
bool pragma::animation::skeletal::is_bone_scale_channel(const AnimationChannel &channel)
{
	auto it = channel.targetPath.begin();
	auto end = channel.targetPath.end();
	if(it == end)
		return false;
	--end;
	return *it == SK_ANIMATED_COMPONENT_NAME && *end == ANIMATION_CHANNEL_PATH_SCALE;
}
void pragma::animation::skeletal::translate(Animation &anim,const Vector3 &translation)
{
	auto renderBounds = get_render_bounds(anim);
	renderBounds.first += translation;
	renderBounds.second += translation;
	set_render_bounds(anim,renderBounds.first,renderBounds.second);
	for(auto &channel : anim.GetChannels())
	{
		if(is_bone_position_channel(*channel) == false)
			continue;
		for(auto &v : channel->It<Vector3>())
			v += translation;
	}
}
void pragma::animation::skeletal::rotate(Animation &anim,const Quat &rotation)
{
	auto renderBounds = get_render_bounds(anim);
	uvec::rotate(&renderBounds.first,rotation);
	uvec::rotate(&renderBounds.second ,rotation);
	set_render_bounds(anim,renderBounds.first,renderBounds.second);
	for(auto &channel : anim.GetChannels())
	{
		if(is_bone_position_channel(*channel))
		{
			for(auto &v : channel->It<Vector3>())
				uvec::rotate(&v,rotation);
			continue;
		}
		if(is_bone_rotation_channel(*channel))
		{
			for(auto &v : channel->It<Quat>())
				v = rotation *v;
		}
	}
}
void pragma::animation::skeletal::scale(Animation &anim,const Vector3 &scale)
{
	auto renderBounds = get_render_bounds(anim);
	renderBounds.first *= scale;
	renderBounds.second *= scale;
	set_render_bounds(anim,renderBounds.first,renderBounds.second);
	for(auto &channel : anim.GetChannels())
	{
		if(!is_bone_scale_channel(*channel))
			continue;
		for(auto &v : channel->It<Vector3>())
			v *= scale;
	}
}
#pragma optimize("",on)
