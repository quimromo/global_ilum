
#include <optix.h>
#include <optixu/optixu_math_namespace.h>
#include <optixu/optixu_matrix_namespace.h>
#include <optixu/optixu_aabb_namespace.h>

using namespace optix;

rtDeclareVariable(float3, v0, , );
rtDeclareVariable(float3, v1, , );
rtDeclareVariable(float3, v2, , );
rtDeclareVariable(optix::Ray, ray, rtCurrentRay, );
//rtDeclareVariable(float3, texcoord, attribute texcoord, ); 
rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, ); 
rtDeclareVariable(float3, shading_normal, attribute shading_normal, );

RT_PROGRAM void triangle_isect(int)
{
	float3 D = ray.direction;
	float3 O = ray.origin;

	float3 e1 = v1 - v0;
	float3 e2 = v2 - v0;
	float3 T = O - v0;

	float3 P = cross(D, e2);
	float3 Q = cross(T, e1);

	float3 tuv = (1.0/dot(P, e1)) * make_float3(dot(Q, e2), dot(P, T), dot(Q, D));
	
	float t = tuv.x;
	float u = tuv.y;
	float v = tuv.z;

	if(t > 0.0001 && u >= 0.0 && v >= 0.0 && u + v <= 1.0){
		if(rtPotentialIntersection(t)){
			float3 norm = normalize(cross(e1, e2));
			if(dot(norm, -D) < 0.0){
				norm = -norm;
			}
		
			shading_normal = geometric_normal = norm;
			rtReportIntersection(0);
			return;
		}
	}
}

RT_PROGRAM void triangle_bounds (int, float result[6])
{
  optix::Aabb* aabb = (optix::Aabb*)result;
  float3 boxmin;
  float3 boxmax;

  boxmin.x = min(min(v0.x, v1.x), v2.x);
  boxmin.y = min(min(v0.y, v1.y), v2.y);
  boxmin.z = min(min(v0.z, v1.z), v2.z);
  
  boxmax.x = max(max(v0.x, v1.x), v2.x);
  boxmax.y = max(max(v0.y, v1.y), v2.y);
  boxmax.z = max(max(v0.z, v1.z), v2.z);
  
  aabb->set(boxmin, boxmax);
}