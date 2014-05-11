
#include <optix.h>
#include <optixu/optixu_math_namespace.h>
#include <optixu/optixu_matrix_namespace.h>
#include <optixu/optixu_aabb_namespace.h>

using namespace optix;

rtDeclareVariable(float3, center, , );
rtDeclareVariable(float, radius, , );
rtDeclareVariable(optix::Ray, ray, rtCurrentRay, );
rtDeclareVariable(float3, texcoord, attribute texcoord, ); 
rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, ); 
rtDeclareVariable(float3, shading_normal, attribute shading_normal, ); 

RT_PROGRAM void sphere_isect(int)
{
	float3 ro = ray.origin - center;
	float3 rd = ray.direction;

	float a = dot(rd, rd);
	float b = 2*dot(rd, ro);
	float c = dot(ro, ro) - radius*radius;

	float discr = b*b - 4*a*c;
	if(discr < 0){
		return;
		// si no hay raiz real no hay interseccion
	} 
	
	float t0 = (-b - sqrtf(discr))/(2*a);
	if(rtPotentialIntersection(t0)){
		shading_normal = geometric_normal = (ro + rd*t0)/radius;
		if(rtReportIntersection(0))
			return;
	}

	float t1 = (-b + sqrtf(discr))/(2*a);
	if(rtPotentialIntersection(t1)){
		shading_normal = geometric_normal = (ro + rd*t1)/radius;
		rtReportIntersection(0);
	}
}

RT_PROGRAM void sphere_bounds (int, float result[6])
{
  optix::Aabb* aabb = (optix::Aabb*)result;
  float3 radVec = make_float3(radius);
  float3 boxmin = center - radVec;
  float3 boxmax = center + radVec;
  aabb->set(boxmin, boxmax);
}

