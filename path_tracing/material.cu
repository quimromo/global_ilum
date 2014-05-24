
#include "types.h"
#include "commonStructs.h"
#include "random.h"
#include <optixu/optixu_math_namespace.h>

rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, ); 
rtDeclareVariable(float3, shading_normal,   attribute shading_normal, ); 

rtDeclareVariable(PerRayData_radiance, prd_radiance, rtPayload, );
rtDeclareVariable(PerRayData_shadow,   prd_shadow,   rtPayload, );

rtDeclareVariable(optix::Ray, ray,          rtCurrentRay, );
rtDeclareVariable(float,      t_hit,        rtIntersectionDistance, );

rtDeclareVariable(unsigned int, radiance_ray_type, , );
rtDeclareVariable(unsigned int, shadow_ray_type , , );
rtDeclareVariable(float,        scene_epsilon, , );
rtDeclareVariable(rtObject,     top_object, , );

rtDeclareVariable(float3,   Ka, , );
rtDeclareVariable(float3,   Ks, , );
rtDeclareVariable(float,    phong_exp, , );
rtDeclareVariable(float3,   Kd, , );
rtDeclareVariable(float3,   ambient_light_color, , );
rtBuffer<BasicLight>        lights;
rtDeclareVariable(rtObject, top_shadower, , );

rtDeclareVariable(int, max_depth, , );

rtDeclareVariable(float, rnd1, , );
rtDeclareVariable(float, rnd2, , );

RT_PROGRAM void any_hit_shadow()
{
  // this material is opaque, so it fully attenuates all shadow rays
	/*
	if(t_hit < ray.tmax && t_hit > ray.tmin){
		prd_shadow.attenuation = make_float3(0);
		rtTerminateRay();
		
	}
	*/
	prd_shadow.contribution = make_float3(0.0f);
	rtTerminateRay();
  
}

RT_PROGRAM void generic_material()
{
	// diffuse reflection
	float3 color = make_float3(0.0);
	float3 brdfLight = make_float3(0.0);
	float3 directLight = make_float3(0.0);
	float3 norm = shading_normal;
	float3 hit = ray.origin + ray.direction * t_hit;
	
	if(prd_radiance.depth < max_depth){
		unsigned int seed = prd_radiance.seed;
		float r1 = rnd(seed);
		seed = lcg(seed);
		float r2 = rnd(seed);
		float3 rd = sampleHemisphere(norm, r1, r2);
		PerRayData_radiance diffuse_refl_prd;
		diffuse_refl_prd.depth = prd_radiance.depth + 1;
		optix::Ray diffuse_refl_ray( hit, rd, radiance_ray_type, scene_epsilon );
		rtTrace(top_object, diffuse_refl_ray, diffuse_refl_prd);
		brdfLight = diffuse_refl_prd.result;// * dot(norm, rd); 

	}
	
	for(int i = 0; i < lights.size(); ++i) {
		
		// diffuse
		BasicLight light = lights[i];
		float3 l = normalize(light.pos - hit);
		float ndl = dot(norm, l);
		if(ndl > 0.0f){
			// cast shadow ray
			PerRayData_shadow shadow_prd;
			shadow_prd.contribution = make_float3(1.0f);
			float ldist = length(light.pos - hit);
			optix::Ray shadow_ray( hit, l, shadow_ray_type, scene_epsilon, ldist);
			rtTrace(top_object, shadow_ray, shadow_prd);
			float3 light_attenuation = shadow_prd.contribution;

			if( fmaxf(light_attenuation) > 0.0f ){
				directLight = ndl*light.color;

				// specular
				float3 R = normalize(2*ndl*norm - l);
				float rdv = dot(-ray.direction, R);
				if(rdv > 0.f){
					color += Ks * pow(rdv, phong_exp) * light.color;
				}
			} 
		}
	}
	//float pathFactor = 1.0f/float(max_depth - prd_radiance.depth + 1.0f); 
	/*
	if(prd_radiance.depth < max_depth){
		PerRayData_radiance refl_prd;
		refl_prd.depth = prd_radiance.depth + 1;
		float3 refdir = ray.direction - 2*dot(norm, ray.direction)*norm;
		optix::Ray refl_ray( hit, refdir, 0, scene_epsilon );
		rtTrace(top_object, refl_ray, refl_prd);
		color += reflectivity * refl_prd.result;  
	} 	
	*/
	//prd_radiance.result = Kd * (pathFactor * directLight + (1.0f- pathFactor) * brdfLight);
	prd_radiance.result = Kd * (directLight + brdfLight);
}

RT_PROGRAM void diffuse(){
	// diffuse reflection
	float3 color = make_float3(0.0);
	float3 brdfLight = make_float3(0.0);
	float3 norm = shading_normal;
	float3 hit = ray.origin + ray.direction * t_hit;
	if(dot(norm, -ray.direction) < 0)
		norm = -norm;
	
	if(prd_radiance.depth < max_depth){
		unsigned int seed = prd_radiance.seed;
		float r1 = rnd(seed);
		float r2 = rnd(seed);
		float3 p;
		//optix::cosine_sample_hemisphere(r1, r2, p);
		p = sampleHemisphere(r1, r2);
		float3 v1, v2;
		createONB(norm, v1, v2);
		float3 rd = v1 * p.x + v2 * p.y + norm * p.z;
		
		//float3 rd = sampleHemisphere(norm, r1, r2);
		PerRayData_radiance diffuse_refl_prd;
		diffuse_refl_prd.seed = seed;
		diffuse_refl_prd.depth = prd_radiance.depth + 1;
		optix::Ray diffuse_refl_ray( hit, rd, radiance_ray_type, scene_epsilon );
		rtTrace(top_object, diffuse_refl_ray, diffuse_refl_prd);
		brdfLight = diffuse_refl_prd.result;// * dot(norm, rd);
		color = brdfLight * Kd;
	}
	prd_radiance.result = color;
	


}

RT_PROGRAM void diffuse_and_specular(){
	// diffuse reflection
	float3 color;
	float3 reflectedLight = make_float3(0.0);
	float3 norm = shading_normal;
	float3 hit = ray.origin + ray.direction * t_hit;
	if(dot(norm, -ray.direction) < 0)
		norm = -norm;
	
	if(prd_radiance.depth < max_depth){
		float3 rd;
		unsigned int seed = prd_radiance.seed;
		float r = rnd(seed);
		float specAvg = (Ks.x + Ks.y + Ks.z) / 3;
		if(r <= specAvg){
			// specular reflection
			float3 eye = -ray.direction;
			float3 perfect_specular = 2*norm*dot(norm, eye) - eye;
			float r1 = rnd(seed);
			float r2 = rnd(seed);
			float3 p = sample_specular(phong_exp, r1, r2);
			float3 v1, v2;
			createONB(perfect_specular, v1, v2);
			rd = v1 * p.x + v2*p.y + perfect_specular*p.z;
			color = Ks;
		}
		else{
			float diffAvg = (Kd.x + Kd.y + Kd.z) / 3;
			if(r - specAvg > diffAvg){
				// absortion
				prd_radiance.result = color;
				return;

			}
			else{
				// diffuse reflection
				float r1 = rnd(seed);
				float r2 = rnd(seed);
				float3 p;
				optix::cosine_sample_hemisphere(r1, r2, p);
				//p = sampleHemisphere(r1, r2);
				float3 v1, v2;
				createONB(norm, v1, v2);
				rd = v1 * p.x + v2 * p.y + norm * p.z;
				color = Kd;
			}
		}

		PerRayData_radiance recursive_prd;
		recursive_prd.seed = seed;
		recursive_prd.depth = prd_radiance.depth + 1;
		optix::Ray recursive_ray( hit, rd, radiance_ray_type, scene_epsilon );
		rtTrace(top_object, recursive_ray, recursive_prd);
		reflectedLight = recursive_prd.result;// * dot(norm, rd);
		prd_radiance.result = reflectedLight * color;
	}
	else prd_radiance.result = make_float3(0.0);
	


}

RT_PROGRAM void emitter(){
	
	float3 normal = geometric_normal;
	// la normal esta al revés!
	if(dot(-ray.direction, normal) < 0.0f) normal = -normal;
	float LnDL = dot(-ray.direction, normal);
	/*
	if( LnDL < 0){
		LnDL *= -1;
		//prd_radiance.result = Kd; 
	}
	*/
	//else prd_radiance.result = make_float3(0.0f);
	
	prd_radiance.result = Kd * LnDL;
}