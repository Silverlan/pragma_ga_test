#ifndef __S_ENV_LIGHT_SPOT_H__
#define __S_ENV_LIGHT_SPOT_H__
#include "pragma/serverdefinitions.h"
#include "pragma/entities/s_baseentity.h"
#include "pragma/entities/environment/lights/s_env_light.h"
#include <pragma/entities/environment/lights/env_light_spot.h>

namespace pragma
{
	class DLLSERVER SLightSpotComponent final
		: public BaseEnvLightSpotComponent,
		public SBaseNetComponent
	{
	public:
		SLightSpotComponent(BaseEntity &ent) : BaseEnvLightSpotComponent(ent) {}
		virtual void SendData(NetPacket &packet,nwm::RecipientFilter &rp) override;
		virtual void SetOuterCutoffAngle(float ang) override;
		virtual void SetInnerCutoffAngle(float ang) override;
		virtual bool ShouldTransmitNetData() const override {return true;}
		virtual luabind::object InitializeLuaObject(lua_State *l) override;
		virtual void SetConeStartOffset(float offset) override;
	};
};

class DLLSERVER EnvLightSpot
	: public SBaseEntity
{
public:
	virtual void Initialize() override;
};

#endif