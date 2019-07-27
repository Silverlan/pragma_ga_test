#include "stdafx_cengine.h"
#include "pragma/rendering/c_sci_gpu_timer_manager.hpp"
#include "pragma/rendering/c_gpu_swapchain_timer.hpp"
#include <pragma/console/c_cvar.h>
#include <pragma/console/convars.h>
#include <pragma/console/c_cvar_global_functions.h>
#include <queries/prosper_query_pool.hpp>
#include <queries/prosper_timer_query.hpp>
#include <prosper_command_buffer.hpp>
#include <prosper_util.hpp>

using namespace pragma::debug;

extern DLLCENGINE CEngine *c_engine;

static CVar cvTimerQueries = GetClientConVar("cl_gpu_timer_queries_enabled");

std::shared_ptr<GPUProfilingStage> GPUProfilingStage::Create(Profiler &profiler,const std::string &name,Anvil::PipelineStageFlagBits stage,GPUProfilingStage *parent)
{
	auto result = ProfilingStage::Create<GPUProfilingStage>(profiler,name,parent);
	if(result == nullptr)
		return nullptr;
	result->m_stage = stage;
	return result;
}

GPUProfiler &GPUProfilingStage::GetProfiler() {return static_cast<GPUProfiler&>(ProfilingStage::GetProfiler());}
GPUProfilingStage *GPUProfilingStage::GetParent() {return static_cast<GPUProfilingStage*>(ProfilingStage::GetParent());}
const GPUSwapchainTimer &GPUProfilingStage::GetTimer() const {return const_cast<GPUProfilingStage*>(this)->GetTimer();}
GPUSwapchainTimer &GPUProfilingStage::GetTimer() {return static_cast<GPUSwapchainTimer&>(ProfilingStage::GetTimer());}
Anvil::PipelineStageFlagBits GPUProfilingStage::GetPipelineStage() const {return m_stage;}

void GPUProfilingStage::InitializeTimer()
{
	m_timer = GetProfiler().CreateTimer(m_stage);
}

GPUProfiler::GPUProfiler()
	: m_timerQueryPool{nullptr},m_statsQueryPool{nullptr}
{
	InitializeQueries();
}
void GPUProfiler::Initialize()
{
	m_rootStage = GPUProfilingStage::Create(*this,"root",Anvil::PipelineStageFlagBits::NONE);
}
std::shared_ptr<pragma::debug::Timer> GPUProfiler::CreateTimer(Anvil::PipelineStageFlagBits stage)
{
	return pragma::debug::GPUSwapchainTimer::Create(*m_timerQueryPool,*m_statsQueryPool,stage);
}
void GPUProfiler::Reset()
{
	for(auto &wpStage : GetStages())
	{
		if(wpStage.expired())
			continue;
		static_cast<GPUProfilingStage*>(wpStage.lock().get())->GetTimer().Reset();
	}
}
void GPUProfiler::InitializeQueries()
{
	if(m_timerQueryPool != nullptr || m_statsQueryPool != nullptr)
		return;
	auto swapchainImageCount = c_engine->GetSwapchainImageCount();
	const auto maxTimestampQueryCount = 200u; // Note: Every timer requires 2 timestamps
	const auto maxStatisticsQueryCount = 100u;
	m_timerQueryPool = prosper::util::create_query_pool(*c_engine,vk::QueryType::eTimestamp,maxTimestampQueryCount);
	m_statsQueryPool = prosper::util::create_query_pool(*c_engine,
		Anvil::QueryPipelineStatisticFlagBits::INPUT_ASSEMBLY_VERTICES_BIT |
		Anvil::QueryPipelineStatisticFlagBits::INPUT_ASSEMBLY_PRIMITIVES_BIT |
		Anvil::QueryPipelineStatisticFlagBits::VERTEX_SHADER_INVOCATIONS_BIT |
		Anvil::QueryPipelineStatisticFlagBits::GEOMETRY_SHADER_INVOCATIONS_BIT |
		Anvil::QueryPipelineStatisticFlagBits::GEOMETRY_SHADER_PRIMITIVES_BIT |
		Anvil::QueryPipelineStatisticFlagBits::CLIPPING_INVOCATIONS_BIT |
		Anvil::QueryPipelineStatisticFlagBits::CLIPPING_PRIMITIVES_BIT |
		Anvil::QueryPipelineStatisticFlagBits::FRAGMENT_SHADER_INVOCATIONS_BIT |
		Anvil::QueryPipelineStatisticFlagBits::TESSELLATION_CONTROL_SHADER_PATCHES_BIT |
		Anvil::QueryPipelineStatisticFlagBits::TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT |
		Anvil::QueryPipelineStatisticFlagBits::COMPUTE_SHADER_INVOCATIONS_BIT,
		maxStatisticsQueryCount
	);
}

/////////////////////////

void Console::commands::cl_gpu_timer_queries_dump(NetworkState *state,pragma::BasePlayerComponent *pl,std::vector<std::string> &argv)
{
	auto extended = false;
	if(argv.empty() == false)
		extended = util::to_boolean(argv.at(0));
	Con::cout<<"-------- GPU-Profiler Query Results --------"<<Con::endl;
	std::function<void(pragma::debug::ProfilingStage&,const std::string&,bool)> fPrintResults = nullptr;
	fPrintResults = [&fPrintResults,extended](pragma::debug::ProfilingStage &stage,const std::string &t,bool bRoot) {
		if(bRoot == false)
		{
			std::string sTime = "Pending";
			auto result = stage.GetResult();
			if(result && result->duration.has_value())
			{
				auto t = util::clock::to_milliseconds(*result->duration);
				sTime = util::round_string(t,2) +" ms";
				sTime += " (" +std::to_string(result->duration->count()) +" ns)";
			}
			Con::cout<<t<<stage.GetName()<<": "<<sTime;
			Con::cout<<" ("<<prosper::util::to_string(static_cast<pragma::debug::GPUProfilingStage&>(stage).GetPipelineStage())<<")"<<Con::endl;

			if(result && extended == true)
			{
				auto &statistics = static_cast<pragma::debug::GPUProfilerResult&>(*result).statistics;
				if(statistics.has_value())
				{
					auto &stats = *statistics;
					Con::cout<<t<<"{"<<Con::endl;
					Con::cout<<t<<"\tInput Assembly Vertices: "<<stats.inputAssemblyVertices<<Con::endl;
					Con::cout<<t<<"\tInput Assembly Primitives: "<<stats.inputAssemblyPrimitives<<Con::endl;
					Con::cout<<t<<"\tVertex Shader Invocations: "<<stats.vertexShaderInvocations<<Con::endl;
					Con::cout<<t<<"\tGeometry Shader Invocations: "<<stats.geometryShaderInvocations<<Con::endl;
					Con::cout<<t<<"\tGeometry Shader Primitives: "<<stats.geometryShaderPrimitives<<Con::endl;
					Con::cout<<t<<"\tClipping Invocations: "<<stats.clippingInvocations<<Con::endl;
					Con::cout<<t<<"\tClipping Primitives: "<<stats.clippingPrimitives<<Con::endl;
					Con::cout<<t<<"\tFragment Shader Invocations: "<<stats.fragmentShaderInvocations<<Con::endl;
					Con::cout<<t<<"\tTessellation Control Shader Patches: "<<stats.tessellationControlShaderPatches<<Con::endl;
					Con::cout<<t<<"\tTessellation Evaluation Shader Invocations: "<<stats.tessellationEvaluationShaderInvocations<<Con::endl;
					Con::cout<<t<<"\tCompute Shader Invocations: "<<stats.computeShaderInvocations<<Con::endl;
					Con::cout<<t<<"}"<<Con::endl;
					Con::cout<<Con::endl;
				}
			}
		}
		for(auto &wpChild : stage.GetChildren())
		{
			if(wpChild.expired())
				continue;
			fPrintResults(*wpChild.lock(),t +(bRoot ? "" : "\t"),false);
		}
	};
	auto &profiler = c_engine->GetGPUProfiler();
	fPrintResults(profiler.GetRootStage(),"",true);
	Con::cout<<"--------------------------------------------"<<Con::endl;
}
