
/*
 * Copyright (c) 2008 - 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and proprietary
 * rights in and to this software, related documentation and any modifications thereto.
 * Any use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation is strictly
 * prohibited.
 *
 * TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED *AS IS*
 * AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL NVIDIA OR ITS SUPPLIERS BE LIABLE FOR ANY
 * SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT
 * LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF
 * BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR
 * INABILITY TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES
 */

#include "device_funcs.h"
#include "commonStructs.h"
#include "random.h"
#include <optix.h>
#include <optixu/optixu_math_namespace.h>


using namespace optix;

rtTextureSampler<float4, 2>   ambient_map;         // 
rtTextureSampler<float4, 2>   diffuse_map;         // Correspond to OBJ mtl params
rtTextureSampler<float4, 2>   specular_map;        //
rtTextureSampler<float4, 2> bump_map;
rtDeclareVariable(uint, usebump, , );
rtDeclareVariable(float,      phong_exp, , );          //
rtDeclareVariable(int,        illum, , );              //

rtDeclareVariable(float3, texcoord, attribute texcoord, ); 
rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, ); 
rtDeclareVariable(float3, shading_normal, attribute shading_normal, );

rtDeclareVariable(float3,    tangent, attribute    tangent, );
rtDeclareVariable(float3, bi_tangent, attribute bi_tangent, );

rtDeclareVariable(PerRayData_radiance, prd_radiance, rtPayload, );
rtDeclareVariable(PerRayData_shadow,   prd_shadow,   rtPayload, );

rtDeclareVariable(optix::Ray, ray,          rtCurrentRay, );
rtDeclareVariable(float,      t_hit,        rtIntersectionDistance, );

rtDeclareVariable(unsigned int, radiance_ray_type, , );
rtDeclareVariable(unsigned int, shadow_ray_type , , );
rtDeclareVariable(float,        scene_epsilon, , );
rtDeclareVariable(rtObject,     top_object, , );

rtDeclareVariable(int, max_depth, , );
rtDeclareVariable(int, is_dome, , );
rtDeclareVariable(float3, dome_emission, , );
rtDeclareVariable(unsigned int, max_direct_samples, , );
rtDeclareVariable(unsigned int, min_direct_samples, , );

rtBuffer<SphereLight, 1> spherical_lights;


RT_PROGRAM void any_hit_shadow()
{
  prd_shadow.contribution = make_float3(0.0f);
  rtTerminateRay();
}

RT_PROGRAM void diffuse()
{
  float3 direction              = ray.direction;
  float3 world_shading_normal   = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, shading_normal ) );
  float3 world_geometric_normal = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, geometric_normal ) );
  float3 uv                     = texcoord;
  //rtPrintf("objmaterial");
  /*
  if(usebump){
	  float z = 1.0;
	  float x = make_float3(tex2D(bump_map, uv.x + 0.001, uv.y)).x - make_float3(tex2D(bump_map, uv.x - 0.001, uv.y)).x;
	  float y = make_float3(tex2D(bump_map, uv.x, uv.y + 0.001)).x - make_float3(tex2D(bump_map, uv.x, uv.y - 0.001)).x;
	  float3 bump_normal = normalize(make_float3(x,y,z));
	  //float3 obj_norm = bump_normal.x * tangent + bump_normal.y * bi_tangent + bump_normal.z * shading_normal;
	  float3 obj_norm = shading_normal + tangent * x + bi_tangent * y;
	  world_shading_normal   = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, obj_norm ) );
	  prd_radiance.result = world_shading_normal;

  }
  */
  
  
	float3 ffnormal               = normalize(faceforward( world_shading_normal, -direction, world_geometric_normal ));
 
	float3 black = make_float3(0.0f, 0.0f, 0.0f);

	float3 Kd = make_float3( tex2D( diffuse_map,  uv.x, uv.y ) );
	float3 Ks = make_float3( tex2D( specular_map, uv.x, uv.y ) );

	float prob_diff = (Kd.x + Kd.y + Kd.z) / 3.0f;
	float prob_spec = (Ks.x + Ks.y + Ks.z) / 3.0f;
	float total_prob = prob_diff + prob_spec; 
	if( total_prob > 1.0f){
		prob_diff /= total_prob;
		prob_spec /= total_prob;
		Kd /= total_prob;
		Ks /= total_prob;
		total_prob = 1.0f;
	}

  	// diffuse reflection
	float3 color = make_float3(0.0);
	float3 brdfLight = make_float3(0.0);
	float3 hit = ray.origin + ray.direction * t_hit;
	
	bool hit_light = false;
	if(prd_radiance.depth < max_depth){
		unsigned int seed = prd_radiance.seed;
		float rnd_event = rnd(seed);
		float r1 = rnd(seed);
		float r2 = rnd(seed);
		float3 p;
		float3 u, v;
		if(rnd_event <= prob_spec){
			float3 eye = -ray.direction;
			float3 perfect_specular = normalize(2.0 * ffnormal * dot(ffnormal, eye) - eye);
			createONB(perfect_specular, u, v);
			p = sample_specular2(phong_exp, r1, r2);
			float3 rd = normalize(u * p.x + v * p.y + perfect_specular * p.z);
			if(dot(rd, ffnormal) > 0.0f){
				PerRayData_radiance specular_refl_prd;
				specular_refl_prd.seed = seed;
				specular_refl_prd.depth = prd_radiance.depth + 1;
				specular_refl_prd.contribution *= Ks;
				specular_refl_prd.is_light = false;
				optix::Ray specular_refl_ray( hit, rd, radiance_ray_type, scene_epsilon );
				rtTrace(top_object, specular_refl_ray, specular_refl_prd);
				brdfLight = specular_refl_prd.result;// * dot(norm, rd);
				hit_light = specular_refl_prd.is_light;
				
				color += ( brdfLight * Ks * (phong_exp + 2) * dot(rd, ffnormal) ) / ( (phong_exp + 1) * prob_spec );
			}

		}
		else if(rnd_event <= total_prob){

			cosine_sample_hemisphere(r1, r2, p);
			createONB(ffnormal, u, v);
			float3 rd = normalize(u * p.x + v * p.y + ffnormal * p.z);
			PerRayData_radiance diffuse_refl_prd;
			diffuse_refl_prd.seed = seed;
			diffuse_refl_prd.depth = prd_radiance.depth + 1;
			diffuse_refl_prd.contribution *= Kd;
			diffuse_refl_prd.is_light = false;
			optix::Ray diffuse_refl_ray( hit, rd, radiance_ray_type, scene_epsilon );
			rtTrace(top_object, diffuse_refl_ray, diffuse_refl_prd);
			brdfLight = diffuse_refl_prd.result;// * dot(norm, rd);
			hit_light = diffuse_refl_prd.is_light;
			color += brdfLight * Kd / prob_diff;
		}

		
		
		
	}

	if(!hit_light){
		float3 perfect_specular = normalize(2.0 * ffnormal * dot(ffnormal, -ray.direction) + ray.direction);
		if(is_dome == 1){
			float3 direct_color = make_float3(0.0);
				unsigned int seed = prd_radiance.seed;
				float r1 = rnd(seed);
				float r2 = rnd(seed);
				float3 p;
				cosine_sample_hemisphere(r1, r2, p); p = normalize(p);
				//p = sampleHemisphere(r1, r2);
				float z = 1.0f - 2.0f * r1;
				float r = sqrtf( fmaxf( 0.0f, 1.0f - z*z) );
				float phi = 2.0f * r2 * PI;
				float x = r * cos(phi);
				float y = r * sin(phi);
 
				//p = normalize( make_float3(x, y, z ) );
				//float pdf = 1.0f / (4.0f * PI);
			
				/* sample solid angle
				float theta = 2.0f * PI;
				float phi = 2.0f * PI * r1;
				float alpha = acos( 1 - (1- cosf(theta) ) * r2 );
				float x = sin(alpha)*cosf(phi);
				float y = sin(alpha)*sinf(phi);
				float z = -cos(alpha);
				p = make_float3(x, y, z);
				*/

				float3 v1, v2;
				createONB(ffnormal, v1, v2);
				float3 rd = normalize(v1 * p.x + v2 * p.y + ffnormal * p.z);
				//if(dot(rd, ffnormal) < 0.0f) rd *= -1;
				PerRayData_shadow shadow_prd;
				shadow_prd.contribution = make_float3(1.0f);
				Ray shadow_ray = Ray( hit, rd, shadow_ray_type, scene_epsilon);
				rtTrace(top_object, shadow_ray, shadow_prd);
				direct_color +=  shadow_prd.contribution;// * dot(rd, ffnormal);

			
			direct_color *= Kd * dome_emission;
			color += direct_color;
		}
		
		for(int i = 0; i < spherical_lights.size(); ++i){
			float3 light_dir = spherical_lights[i].center - hit;
			float dist2 = dot(light_dir, light_dir);
			float radius2 = spherical_lights[i].radius * spherical_lights[i].radius;
			if(dist2 - radius2 < scene_epsilon){
				continue;
			}
			unsigned int seed = prd_radiance.seed;
			float cos_theta_max = sqrtf(1 - radius2/dist2);
			float inv_pdf = 2.0f * PI * (1.0f - cos_theta_max);
			
			float r1 = rnd(seed);
			float r2 = rnd(seed);

			float cos_theta = 1 + r1 * (cos_theta_max - 1);
			float sin2theta = 1 - cos_theta * cos_theta;
			float sin_theta = sqrtf(sin2theta);
			float sin_phi = sinf(2 * PI * r2);
			float cos_phi = cosf(2 * PI * r2);
			//float3 dir = make_float3( sqrtf(sin2theta), cos_theta, 2 * PI * r2);
			float3 w = normalize(light_dir);
			float3 u, v;
			createONB(w, u, v);
			float3 dir = make_float3(u.x * cos_phi * sin_theta + v.x * sin_phi * sin_theta + w.x * cos_theta,
									 u.y * cos_phi * sin_theta + v.y * sin_phi * sin_theta + w.y * cos_theta,
									 u.z * cos_phi * sin_theta + v.z * sin_phi * sin_theta + w.z * cos_theta
									);

			if(dot(dir, ffnormal) < 0) continue;
			PerRayData_shadow shadow_prd;
			shadow_prd.contribution = spherical_lights[i].emission;
			float delta = sqrtf(radius2 - sin2theta * dist2);
			Ray shadow_ray = Ray( hit, dir, shadow_ray_type, scene_epsilon, cos_theta * length(light_dir) - delta );
			rtTrace(top_object, shadow_ray, shadow_prd);
			color += inv_pdf * shadow_prd.contribution * dot(dir, ffnormal) * (Kd + Ks * fmaxf(dot(dir, perfect_specular), 0.0f));

		}
		

	}

	prd_radiance.result = color;
	//prd_radiance.dist = fabs(t_hit) / 10.0f;
  
	
  //prd_radiance.result = Kd;
}

rtDeclareVariable(float3, emission, , );

RT_PROGRAM void emitter(){
	
	float3 world_shading_normal   = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, shading_normal ) );
	float3 world_geometric_normal = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, geometric_normal ) );
	float3 ffnormal               = normalize(faceforward( world_shading_normal, -ray.direction, world_geometric_normal ));
	// la normal esta al revés!
	float LnDL = dot(-ray.direction, ffnormal);
	//if( LnDL < 0){

		prd_radiance.result = emission;// * LnDL;
		//prd_radiance.dist = t_hit/10.0f;
		prd_radiance.is_light = true;
	//}
	//else prd_radiance.result = make_float3(0.0f);
	
	//prd_radiance.result = Kd;
}
