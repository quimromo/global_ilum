
#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_namespace.h>
#include <vector>
#include <cstdlib>
#include <iostream>
#include "commonStructs.h"
#include <sstream>
#include "ObjLoader.h"
#include <SDL/SDL.h>
#include <gl\GL.h>

#include <direct.h>
#include "pugixml.hpp"

#include "types.h"
#include "utils.h"

#include "pathtracer_types.h"


void CPathtracer::addLight(TSphereLight light){

	this->sphere_lights.push_back(light);
}
void CPathtracer::addObjModel(TObjModel model){
	this->models.push_back(model);
}

void CPathtracer::renderAllSamples(){

	while(samplesToDo() > 0){
		renderNextSample();
	}

}

void CPathtracer::enableSkyDome(){
	has_skydome = true;
}

void CPathtracer::disableSkyDome(){
	has_skydome = true;
}

void CPathtracer::setSkyDomeEmission(optix::float3 emission){
	sky_dome_emission = emission;
}
	
void CPathtracer::renderNextSample(){

	if(ready){ 
		try{
			if(buffer_mapped){ 
				m_context["output_buffer"]->getBuffer()->unmap();
				buffer_mapped = false;
			}
			for(unsigned block_y = 0; block_y < blocks_y; ++block_y){
					
				unsigned block_size_y = height/blocks_y;
				unsigned offset_y = block_size_y * block_y;
				if (block_y == blocks_y - 1)block_size_y += height%blocks_y;   
					
				for(int block_x = 0; block_x < blocks_x; ++block_x){
						
					unsigned block_size_x = width/blocks_x;
					unsigned offset_x = block_size_x * block_x;  
					if (block_x == blocks_x - 1)block_size_x += width%blocks_x;	
					/*
					std::cout << "OFFSET:" << offset_x << " " << offset_y << std::endl;
					std::cout << "BLOCK_SIZE:" << block_size_x << " " << block_size_y << std::endl;
					*/
					float rnd1 = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
					float rnd2 = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
						

					m_context["offset_x"]->setUint(offset_x);
					m_context["offset_y"]->setUint(offset_y);
					m_context["currentSample"]->setUint(current_sample);
				
					//m_context->validate();
					//m_context->compile();
					m_context->launch( 0, block_size_x, block_size_y );

					SDL_Event event;
					SDL_PollEvent(&event);
				
				}
			}

			current_sample++;
		} 
		catch( optix::Exception& e ){
			std::cout << e.getErrorString() << std::endl;
			int p;
			std::cin >> p;
			exit(1);
		}
	}
}

float* CPathtracer::getOutputBuffer(){
	if(buffer_mapped) return NULL;
	buffer_mapped = true;
	return static_cast<float*>( m_context["output_buffer"]->getBuffer()->map());
	
}

void CPathtracer::releaseBuffer(){
	if(buffer_mapped){
		m_context["output_buffer"]->getBuffer()->unmap();
		buffer_mapped = false;
	}
		
}
	
int CPathtracer::samplesToDo(){
	return sqrt_spp * sqrt_spp - current_sample;
}


void CPathtracer::prepareLights(){
	
	optix::Buffer light_buffer = m_context->createBuffer(RT_BUFFER_INPUT);
	light_buffer->setFormat(RT_FORMAT_USER);
	light_buffer->setElementSize(sizeof(TSphereLight));
	light_buffer->setSize( sphere_lights.size() );
	memcpy(light_buffer->map(), &sphere_lights[0], sphere_lights.size() * sizeof(TSphereLight));
	light_buffer->unmap();

	m_context["spherical_lights"]->set(light_buffer);

}

void CPathtracer::prepareObjModels(){

}

void CPathtracer::prepare(){

	blocks_x = width / approx_block_size;
	blocks_y = height / approx_block_size;
	max_block_size_x = width / blocks_x + width % blocks_x;
	max_block_size_y = height / (height / approx_block_size) + (height % (height / approx_block_size));


	m_context = optix::Context::create();
	m_context->setRayTypeCount( 2 );
	m_context->setEntryPointCount( 1 );

	m_context["max_depth"]->setInt(max_depth);
	m_context["radiance_ray_type"]->setUint(0);
	m_context["shadow_ray_type"]->setUint(1);
	m_context["scene_epsilon"]->setFloat( 1.e-4f );
		
	optix::Variable cumulated_buffer = m_context["cumulated_buffer"];
	optix::Buffer buffer_cumul = m_context->createBuffer( RT_BUFFER_OUTPUT, RT_FORMAT_FLOAT3, width*height);
	cumulated_buffer->set(buffer_cumul);
		
	optix::Variable output_buffer = m_context["output_buffer"];
	optix::Buffer buffer_out = m_context->createBuffer( RT_BUFFER_OUTPUT, RT_FORMAT_FLOAT3, width*height);
	output_buffer->set(buffer_out);

	// Ray generation program
		
	std::string ptx_path ( ptxpath( "gi_project", "pinhole_camera.cu" ) );
	optix::Program ray_gen_program = m_context->createProgramFromPTXFile( "pinhole_camera.cu.obj", "m_pinhole_camera" );
	m_context->setRayGenerationProgram( 0, ray_gen_program );
		
	// Set up camera

	m_context["eye"]->setFloat( camera.position );

	m_context["viewd"]->setFloat( camera.viewd );
		
	m_context["U"]->setFloat( camera.left );
	m_context["V"]->setFloat( camera.up );
	m_context["W"]->setFloat( camera.front );
		

	// Exception program
	optix::Program exception_program = m_context->createProgramFromPTXFile( "pinhole_camera.cu.obj", "exception" );
	m_context->setExceptionProgram( 0, exception_program );
	m_context["bad_color"]->setFloat( 0.0f, 1.0f, 0.0f );
		
	// Miss program
	ptx_path = ptxpath( "gi_project", "constantbg.cu" );
	m_context->setMissProgram( 0, m_context->createProgramFromPTXFile( "constantbg.cu.obj", "miss" ) );
	m_context["bg_color"]->setFloat( 0.0f, 0.0f, 0.0f );

	// set samples per pixel
	m_context["sqrtspp"]->setUint(sqrt_spp);

	float aspect_ratio = static_cast<float>(width) / static_cast<float>(height); 
	m_context["aspect_ratio"]->setFloat(aspect_ratio);
	m_context["screen_dim"]->setUint(optix::make_uint2(width, height));


	m_context->setPrintEnabled(true);
	m_context->setStackSize(4096);
	
	optix::Group obj_models_group = m_context->createGroup();
	obj_models_group->setAcceleration(m_context->createAcceleration("Sbvh", "Bvh"));
	obj_models_group->setChildCount(models.size());

	std::cout << "models size: " << models.size() << std::endl;

	optix::Program diffuse_prg = m_context->createProgramFromPTXFile( "m_obj_material.cu.obj" , "diffuse" );
	optix::Program any_hit = m_context->createProgramFromPTXFile( "m_obj_material.cu.obj" , "any_hit_shadow" );
	const char* path = "m_triangle_mesh.cu.obj";
	optix::Program mesh_bounds = m_context->createProgramFromPTXFile(path, "mesh_bounds");
	optix::Program mesh_isect = m_context->createProgramFromPTXFile(path, "mesh_intersect");

	optix::float3 scene_min = optix::make_float3(0.0f);
	optix::float3 scene_max = optix::make_float3(0.0f);

	for(int i = 0; i < models.size(); ++i){

		optix::GeometryGroup group = m_context->createGeometryGroup();
		optix::Material diffuse = m_context->createMaterial();
		
		if(has_skydome){
			diffuse["is_dome"]->setInt(1);
			diffuse["dome_emission"]->setFloat(sky_dome_emission);
		}
		else{
			diffuse["is_dome"]->setInt(0);
		}

		diffuse->setClosestHitProgram(0, diffuse_prg);
		diffuse->setAnyHitProgram(1, any_hit); 
		std::string filename = models[i].filename;
		
		if( ObjLoader::isMyFile( filename.c_str() ) ) {

			ObjLoader loader( filename.c_str(), m_context, group, diffuse, true, "Sbvh", "Bvh", "refine", false);
			loader.setBboxProgram(mesh_bounds);
			loader.setIntersectProgram(mesh_isect);
			loader.load();
			optix::Aabb aabb = loader.getSceneBBox();
			std::cout << "Object aabb.min = (" << aabb.m_min.x << ", " << aabb.m_min.y << ", " << aabb.m_min.z << ")" << std::endl;
			std::cout << "Object aabb.max = (" << aabb.m_max.x << ", " << aabb.m_max.y << ", " << aabb.m_max.z << ")" << std::endl;
			scene_max = optix::fmaxf(aabb.m_max, scene_max);
			scene_min = optix::fminf(aabb.m_min, scene_min);
		}
		else{
			std::cout << "can't load " << filename << " EXIT" << std::endl;
			exit(1);
		}

		obj_models_group->setChild(i, group);

	}

	optix::Aabb scene_aabb;
	scene_aabb.set(scene_min, scene_max);
	float dome_radius = optix::length(scene_aabb.extent()) * 2;
	
	optix::GeometryGroup light_group = m_context->createGeometryGroup();
	light_group->setAcceleration(m_context->createAcceleration("Sbvh", "Bvh"));
	unsigned childs = sphere_lights.size();
	if(has_skydome) childs++;
	light_group->setChildCount(childs);

	std::cout << "lights size: " << sphere_lights.size() << std::endl;

	std::string sphere_ptx(ptxpath("gi_project", "sphere.cu"));
	optix::Program sphere_isect = m_context->createProgramFromPTXFile(sphere_ptx, "sphere_isect");
	optix::Program sphere_bounds = m_context->createProgramFromPTXFile(sphere_ptx, "sphere_bounds");
	optix::Program diffuse_em = m_context->createProgramFromPTXFile( ptxpath("gi_project", "m_obj_material.cu") , "emitter" );
	
	for(int i = 0; i < sphere_lights.size(); ++i){

		optix::Geometry sphereLight = m_context->createGeometry();
		sphereLight->setPrimitiveCount(1u);
		sphereLight->setBoundingBoxProgram(sphere_bounds);
		sphereLight->setIntersectionProgram(sphere_isect);
		sphereLight["center"]->setFloat(sphere_lights[i].center );
		sphereLight["radius"]->setFloat(sphere_lights[i].radius );

		optix::Material diffuse_light = m_context->createMaterial();
		
		diffuse_light->setClosestHitProgram( 0, diffuse_em );
		diffuse_light["emission"]->setFloat(sphere_lights[i].emission);
		optix::GeometryInstance light = m_context->createGeometryInstance(sphereLight, &diffuse_light, &diffuse_light+1);
		std::cout << "light center: " << sphere_lights[i].center.x << " " << sphere_lights[i].center.y << " " << sphere_lights[i].center.z  << std::endl;

		light_group->setChild(i, light);

	}

	if(has_skydome){
		optix::Geometry sphereLight = m_context->createGeometry();
		sphereLight->setPrimitiveCount(1u);
		sphereLight->setBoundingBoxProgram(sphere_bounds);
		sphereLight->setIntersectionProgram(sphere_isect);
		sphereLight["center"]->setFloat(optix::make_float3(0.0f, 0.0f, 0.0f));
		sphereLight["radius"]->setFloat(dome_radius );

		optix::Material diffuse_light = m_context->createMaterial();
		
		diffuse_light->setClosestHitProgram( 0, diffuse_em );
		diffuse_light["emission"]->setFloat(sky_dome_emission);
		optix::GeometryInstance light = m_context->createGeometryInstance(sphereLight, &diffuse_light, &diffuse_light+1);
		light_group->setChild(sphere_lights.size(), light);
	}


	optix::Group all = m_context->createGroup();
	all->setAcceleration(m_context->createAcceleration("Sbvh", "Bvh"));
	all->setChildCount(2);
	all->setChild(0, obj_models_group);
	all->setChild(1, light_group);
	
	m_context["top_shadower"]->set(obj_models_group);
	m_context["top_object"]->set(all);

	prepareLights();

	try{
		m_context->validate();
		m_context->compile();
	}
	catch( optix::Exception& e ){
		std::cout << e.getErrorString() << std::endl;
		int p;
		std::cin >> p;
		exit(1);
	}
	ready = true;
	current_sample = 0;
	buffer_mapped = false;

	
}
	


int CPathtracer::saveToTGA(const char *filename){

	std::vector<unsigned char> pixels(width * height * 3, 0);
		
	float* buf = static_cast<float*>( m_context["output_buffer"]->getBuffer()->map());

	for(unsigned int p = 0; p < width * height; ++p){
		pixels[3*p] = static_cast<unsigned char>(saturate(buf[3*p + 2]) * 255.99);
		pixels[3*p + 1] = static_cast<unsigned char>(saturate(buf[3*p + 1]) * 255.99);
		pixels[3*p + 2] = static_cast<unsigned char>(saturate(buf[3*p]) * 255.99);
	}

	unsigned char tgah[18];
	FILE *f;

	f = fopen (filename, "wb");
	
	if (!f) return -1;
	
	memset (tgah, 0x00, 18);
	tgah[2]  = 0x02;
	tgah[12] = (char) (width & 0xFF);          // Resolution
	tgah[13] = (char) (width >> 8);
	tgah[14] = (char) (height & 0xFF);          
	tgah[15] = (char) (height >> 8);
	tgah[16] = 0x18;								// Color depth              
	tgah[17] = 0x00;                                // up..dw & lf..rt
	fwrite (tgah, 18, 1, f);                        // Save Header

	fwrite (&pixels[0], 3*sizeof(unsigned char), width*height, f);
  
	fclose(f);
	return 0;
	
}

void CPathtracer::destroy(){
	m_context->destroy();
}

optix::float3 getFromString(const char* float3_str){

	size_t len = strlen(float3_str);
	if(len > 50 || len < 5) return optix::make_float3(0.0, 0.0, 0.0); 

	char* value = new char[len];
	strcpy(value, float3_str);
	
	optix::float3 result;
	result.x = atof(strtok(value, " "));
	result.y = atof(strtok(NULL, " "));
	result.z = atof(strtok(NULL, " "));

	return result;

}

int loadSceneFromXML(CPathtracer& pt, const char* xml_filename){

	pugi::xml_document doc;
	doc.load_file(xml_filename);
	pugi::xml_node scene = doc.child("scene");
	
	
	
	pugi::xml_node camera_node = scene.child("camera");
	optix::float3 pos = getFromString( camera_node.attribute("pos").value() );
	optix::float3 target = getFromString( camera_node.attribute("target").value() );
	float fov = camera_node.attribute("fov").as_float();

	pt.setCamera(TCamera(pos, target, fov));

	pugi::xml_node lights = scene.child("lights");
	for(pugi::xml_node light = lights.child("sphere_light"); light != NULL; light = light.next_sibling("sphere_light")){
		TSphereLight l;
		l.center = getFromString(light.attribute("pos").value());
		l.emission = getFromString(light.attribute("emission").value());
		l.radius = light.attribute("radius").as_float();
		pt.addLight(l);
	}

	pugi::xml_node meshes = scene.child("meshes");
	for(pugi::xml_node obj_model = meshes.child("obj_model"); obj_model != NULL; obj_model = obj_model.next_sibling("obj_model")){
		pt.addObjModel( TObjModel( obj_model.attribute("path").value() ) );
	}

	return 0;
}


int main(int argc, char* argv[]){

	char dirPath[200];
	_getcwd(dirPath, sizeof(dirPath));
	std::cout << dirPath << std::endl;
	unsigned int width  = 1280;
	unsigned int height = 768;
	SDL_Init(SDL_INIT_VIDEO);
	
	SDL_Window* window = SDL_CreateWindow(
			"Pathtracer", 
			SDL_WINDOWPOS_CENTERED, 
			SDL_WINDOWPOS_CENTERED,
			width,
			height,
			SDL_WINDOW_OPENGL);
	SDL_GLContext maincontext;
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	maincontext = SDL_GL_CreateContext(window);
	unsigned int sqrtspp = 20;
	CPathtracer pt;

	pt.setRenderSize(width, height);
	pt.setSqrtSamplesPerPixel(sqrtspp);
	pt.setBlockSize(300u);
	pt.setMaxDepth(7);
	
	/*
	pt.setCamera(TCamera(optix::make_float3(8.0, 9.0, 1.0), optix::make_float3(7.0, 9.0, 0.5), 60.0f));
	pt.addObjModel(TObjModel("assets/dabrovic-sponza/sponza.obj"));
	pt.addLight(TSphereLight(optix::make_float3(2.0f, 2.0f, 2.0f), optix::make_float3(0.0f, 5.0f, 0.0f), 1.0f));
	*/

	loadSceneFromXML(pt, "assets/scenes/crytek_sponza_scene.xml");
	//pt.setSkyDomeEmission(optix::make_float3(2.0, 2.0, 2.0));
	//pt.enableSkyDome();
	pt.prepare();

	int current = 0;
	while(pt.samplesToDo() > 0){
		pt.renderNextSample();
		float* buf = pt.getOutputBuffer();
		glClearColor ( 0.0, 1.0, 0.0, 1.0 );
		glClear ( GL_COLOR_BUFFER_BIT );
						
		glDrawPixels(width, height, GL_RGB, GL_FLOAT, buf /*&render_image[0]*/);
		SDL_GL_SwapWindow(window);		
				
		std::cout << static_cast<float>((++current) * 100) / static_cast<float>(sqrtspp * sqrtspp);
		std::cout << "% completado" << std::endl;
		pt.releaseBuffer();

	}

	std::stringstream ss;
	ss << "render_" << currentDateTime() << "_" << sqrtspp * sqrtspp << "spp.tga"; 
	//pt.saveToTGA( ("test_" + currentDateTime() +  ".tga").c_str());
	
	SDL_DestroyWindow(window);
	
	pt.saveToTGA( (ss.str()).c_str());
	pt.destroy();
	int p;
	std::cin >> p;
	SDL_Quit();
	
	return 0;
}


