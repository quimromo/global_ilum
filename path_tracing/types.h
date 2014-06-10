

#define PI		3.14159265f
#include <optix.h>
#include <optix_math.h>


struct PerRayData_radiance
{
  optix::float3 result;
  float  importance;
  int depth;
  unsigned int seed;
  unsigned int dist;
  bool is_light;
  optix::float3 contribution;
};

struct PerRayData_shadow
{
  optix::float3 contribution;
};

struct SphereLight{
	optix::float3 emission;
	optix::float3 center;
	float radius;
};



