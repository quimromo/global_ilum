
#include <optixu/optixu_math_namespace.h>
#include "pathtracer_types.h"
#include "types.h"


TSphereLight::TSphereLight() : 
	emission(),
	center(),
	radius(0.0f)
{
}

TSphereLight::TSphereLight(optix::float3 aemission, optix::float3 acenter, float aradius) :
	emission(aemission),
	center(acenter),
	radius(aradius)
{
}


TObjModel::TObjModel() :
	filename(NULL)
{

}

TObjModel::TObjModel(const char* afilename){
	filename = std::string(afilename);
}

void TObjModel::load(optix::GeometryGroup& group){
	
}



TCamera::TCamera() : 
	position(),
	target(),
	fov(0.0f),
	viewd(0.0f),
	front(),
	left(),
	up(),
	is_ok(false)
{

}

TCamera::TCamera(optix::float3 aposition, optix::float3 atarget, float afov)
{
	if(afov <= 0.0f || afov >= 179.0f) return;

	position = aposition;
	target = atarget;
	fov = afov;
	viewd = 0.0f;

	front = optix::normalize(target - position);
	left = optix::normalize(optix::cross(optix::make_float3(0.0f, 1.0f, 0.0f), front));
	up = optix::normalize(optix::cross(front, left));

	viewd = 1.0 / tanf( fov * 0.5f * DEG2RAD);

	is_ok = true;
		
}
