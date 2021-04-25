/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2021 Silverlan
 */

#ifndef __ANIMATION_CHANNEL_HPP__
#define __ANIMATION_CHANNEL_HPP__

#include "pragma/networkdefinitions.h"
#include <sharedutils/util_path.hpp>
#include <udm.hpp>

namespace pragma::animation
{
	constexpr std::string_view ANIMATION_CHANNEL_PATH_POSITION = "position";
	constexpr std::string_view ANIMATION_CHANNEL_PATH_ROTATION = "rotation";
	constexpr std::string_view ANIMATION_CHANNEL_PATH_SCALE = "scale";

	constexpr auto ANIMATION_CHANNEL_TYPE_POSITION = udm::Type::Vector3;
	constexpr auto ANIMATION_CHANNEL_TYPE_ROTATION = udm::Type::Quaternion;
	constexpr auto ANIMATION_CHANNEL_TYPE_SCALE = udm::Type::Vector3;
	enum class AnimationChannelInterpolation : uint8_t
	{
		Linear = 0,
		Step,
		CubicSpline
	};
	struct DLLNETWORK AnimationChannel
		: public std::enable_shared_from_this<AnimationChannel>
	{
		template<typename T>
			class Iterator
		{
		public:
			using iterator_category = std::forward_iterator_tag;
			using value_type = T&;
			using difference_type = std::ptrdiff_t;
			using pointer = T*;
			using reference = T&;
	
			Iterator(std::vector<uint8_t> &values,bool end);
			Iterator &operator++();
			Iterator operator++(int);
			reference operator*();
			pointer operator->();
			bool operator==(const Iterator<T> &other) const;
			bool operator!=(const Iterator<T> &other) const;
		private:
			std::vector<uint8_t>::iterator m_it;
		};

		template<typename T>
			struct IteratorWrapper
		{
			IteratorWrapper(std::vector<uint8_t> &values);
			IteratorWrapper()=default;
			Iterator<T> begin();
			Iterator<T> end();
		private:
			std::vector<uint8_t> &m_values;
		};

		AnimationChannel()=default;
		AnimationChannel(const AnimationChannel &other)=default;
		AnimationChannel(AnimationChannel &&other)=default;
		AnimationChannel &operator=(const AnimationChannel&)=default;
		AnimationChannel &operator=(AnimationChannel&&)=default;
		udm::Type valueType = udm::Type::Float;
		std::vector<float> times;
		std::vector<uint8_t> values;
		AnimationChannelInterpolation interpolation = AnimationChannelInterpolation::Linear;
		util::Path targetPath;

		bool Save(udm::LinkedPropertyWrapper &prop) const;
		bool Load(udm::LinkedPropertyWrapper &prop);
		
		std::pair<uint32_t,uint32_t> FindInterpolationIndices(float t,float &outInterpFactor,uint32_t pivotIndex) const;
		std::pair<uint32_t,uint32_t> FindInterpolationIndices(float t,float &outInterpFactor) const;
		template<typename T>
			bool IsValueType() const {return udm::type_to_enum<T>() == valueType;}
		template<typename T>
			IteratorWrapper<T> It();
		template<typename T>
			IteratorWrapper<T> It() const {return const_cast<AnimationChannel*>(this)->It();}
		template<typename T>
			T &GetValue(uint32_t idx);
		template<typename T>
			const T &GetValue(uint32_t idx) const {return const_cast<AnimationChannel*>(this)->GetValue<T>(idx);}
		template<typename T>
			auto GetInterpolationFunction() const;
		template<typename T>
			T GetInterpolatedValue(float t,uint32_t &inOutPivotTimeIndex) const;
		template<typename T>
			T GetInterpolatedValue(float t) const;
	private:
		std::pair<uint32_t,uint32_t> FindInterpolationIndices(float t,float &outInterpFactor,uint32_t pivotIndex,uint32_t recursionDepth) const;
	};
};

/////////////////////

template<typename T>
	pragma::animation::AnimationChannel::Iterator<T>::Iterator(std::vector<uint8_t> &values,bool end)
		: m_it{end ? values.end() : values.begin()}
{}
template<typename T>
	pragma::animation::AnimationChannel::Iterator<T> &pragma::animation::AnimationChannel::Iterator<T>::operator++()
{
	m_it += udm::size_of_base_type(udm::type_to_enum<T>());
	return *this;
}
template<typename T>
	pragma::animation::AnimationChannel::Iterator<T> pragma::animation::AnimationChannel::Iterator<T>::operator++(int)
{
	auto it = *this;
	it.operator++();
	return it;
}
template<typename T>
	typename pragma::animation::AnimationChannel::Iterator<T>::reference pragma::animation::AnimationChannel::Iterator<T>::operator*()
{
	return *operator->();
}
template<typename T>
	typename pragma::animation::AnimationChannel::Iterator<T>::pointer pragma::animation::AnimationChannel::Iterator<T>::operator->()
{
	return reinterpret_cast<T*>(&*m_it);
}
template<typename T>
	bool pragma::animation::AnimationChannel::Iterator<T>::operator==(const Iterator<T> &other) const {return m_it == other.m_it;}
template<typename T>
	bool pragma::animation::AnimationChannel::Iterator<T>::operator!=(const Iterator<T> &other) const {return !operator==(other);}

/////////////////////

template<typename T>
	pragma::animation::AnimationChannel::IteratorWrapper<T>::IteratorWrapper(std::vector<uint8_t> &values)
		: m_values{values}
{}
template<typename T>
	pragma::animation::AnimationChannel::Iterator<T> begin()
{return Iterator<T>{m_values,false};}
template<typename T>
	pragma::animation::AnimationChannel::Iterator<T> end()
{return Iterator<T>{m_values,true};}

template<typename T>
	pragma::animation::AnimationChannel::IteratorWrapper<T> pragma::animation::AnimationChannel::It()
{
	static std::vector<uint8_t> empty;
	return IsValueType<T>() ? IteratorWrapper<T>{values} : IteratorWrapper<T>{empty};
}

/////////////////////

template<typename T>
	T &pragma::animation::AnimationChannel::GetValue(uint32_t idx) {return *(reinterpret_cast<T*>(values.data()) +idx);}

template<typename T>
	auto pragma::animation::AnimationChannel::GetInterpolationFunction() const
{
	constexpr auto type = udm::type_to_enum<T>();
	if constexpr(std::is_same_v<T,Vector3>)
		return &uvec::lerp;
	else if constexpr(std::is_same_v<T,Quat>)
		return &uquat::lerp; // TODO: Maybe use slerp? Test performance!
	else
		return [](const T &v0,const T &v1,float f) -> T {return (v0 +f *(v1 -v0));};
}

template<typename T>
	T pragma::animation::AnimationChannel::GetInterpolatedValue(float t,uint32_t &inOutPivotTimeIndex) const
{
	if(udm::type_to_enum<T>() != valueType || times.empty())
		return {};
	float factor;
	auto indices = FindInterpolationIndices(t,factor,inOutPivotTimeIndex);
	inOutPivotTimeIndex = indices.first;
	return GetInterpolationFunction<T>()(GetValue<T>(indices.first),GetValue<T>(indices.second),factor);
}

template<typename T>
	T pragma::animation::AnimationChannel::GetInterpolatedValue(float t) const
{
	if(udm::type_to_enum<T>() != valueType || times.empty())
		return {};
	float factor;
	auto indices = FindInterpolationIndices(t,factor);
	return GetInterpolationFunction<T>()(GetValue<T>(indices.first),GetValue<T>(indices.second),factor);
}

#endif
