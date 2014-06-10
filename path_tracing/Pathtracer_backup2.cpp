
#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_namespace.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include "commonStructs.h"
#include <sstream>
#include "ObjLoader.h"
#include <SDL/SDL.h>
#include <gl\GL.h>

#include <direct.h>
#include "types.h"



typedef unsigned int Uint;

float saturate(float x){
	if(x < 0.0) return 0.0;
	if(x > 1.0) return 1.0;
	return x;
}

const std::string currentDateTime() {
	time_t rawtime;
	struct tm * timeinfo;
	char buffer [100];

	time (&rawtime);
	timeinfo = localtime (&rawtime);

	strftime (buffer,100,"%a%b%Y_%Hh%Mm%Ss",timeinfo);
	return std::string(buffer);
}

const char* const ptxpath( const std::string& target, const std::string& base )
{
  static std::string path;
  path = std::string(base + ".obj");
  std::cout << path << std::endl;
  return path.c_str();
}

class Pathtracer {

protected:

	optix::Context m_context;
	std::vector<float> image;
	std::vector<float> render_image;
	Uint width, height;
	Uint spp;
	Uint max_block_size_x;
	Uint max_block_size_y;
	Uint approx_block_size;

public:
	
	void init(Uint w, Uint h, Uint sqrtSamplesPerPixel, Uint a_approx_block_size){
		
		width = w;
		height = h;
		approx_block_size = a_approx_block_size;
		unsigned blocksx = width / approx_block_size;
		unsigned blocksy = height / approx_block_size;
		max_block_size_x = width / blocksx + width % blocksx;
		max_block_size_y = height / (height / approx_block_size) + (height % (height / approx_block_size));
		std::cout << "MAX_BLOCK_SIZE:" << max_block_size_x << " " << max_block_size_y << std::endl;

		image = std::vector<float>(width * height * 3, 0.0);
		render_image = std::vector<float>(width * height * 3, 0.0);
		spp = sqrtSamplesPerPixel;

		// Set up context
		m_context = optix::Context::create();
		m_context->setRayTypeCount( 2 );
		m_context->setEntryPointCount( 1 );

		m_context["max_depth"]->setInt(8);
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

		optix::float3 cam_eye = {8.92188,9.74951, 1.06658 };
		optix::float3 target = {7.0, 9.0, 0.5};
		
		optix::float3 camera_u ={0.300676,-2.1479e-008,-0.953726};
		optix::float3 camera_v ={-0.299202,0.949516,-0.0943276 };
		optix::float3 camera_w ={-0.905579,-0.313719,-0.285497};
		
		float fov = 90.0f;

		optix::float3 front = optix::normalize(target - cam_eye);
		optix::float3 left = optix::normalize(optix::cross(optix::make_float3(0.0f, 1.0f, 0.0f), front));
		optix::float3 up = optix::normalize(optix::cross(front, left));

		float viewd = 1.0 / tanf( fov * 0.5f * DEG2RAD);

		m_context["eye"]->setFloat( cam_eye );
		/*
		m_context["U"]->setFloat( left );
		m_context["V"]->setFloat( up );
		m_context["W"]->setFloat( front );
		*/
		m_context["viewd"]->setFloat( viewd );
		
		m_context["U"]->setFloat( camera_u );
		m_context["V"]->setFloat( camera_v );
		m_context["W"]->setFloat( camera_w );
		
		std::cout << "CAM_EYE: " << cam_eye.x << " " << cam_eye.y << " " << cam_eye.z << std::endl;
		std::cout << "CAM_U: " << camera_u.x << " " << camera_u.y << " " << camera_u.z << std::endl;
		std::cout << "CAM_V: " << camera_v.x << " " << camera_v.y << " " << camera_v.z << std::endl;
		std::cout << "CAM_W: " << camera_w.x << " " << camera_w.y << " " << camera_w.z << std::endl;

		// Exception program
		optix::Program exception_program = m_context->createProgramFromPTXFile( ptx_path, "exception" );
		m_context->setExceptionProgram( 0, exception_program );
		m_context["bad_color"]->setFloat( 0.0f, 1.0f, 0.0f );
		
		// Miss program
		ptx_path = ptxpath( "gi_project", "constantbg.cu" );
		m_context->setMissProgram( 0, m_context->createProgramFromPTXFile( ptx_path, "miss" ) );
		m_context["bg_color"]->setFloat( 0.0f, 0.0f, 0.0f );

		// set samples per pixel
		m_context["sqrtspp"]->setUint(spp);

		m_context->setPrintEnabled(true);
		m_context->setStackSize(4096);

	}


	void setCamera(optix::float3 eye, optix::float3 U, optix::float3 V, optix::float3 W){
		m_context["eye"]->setFloat( eye );
		m_context["U"]->setFloat( U );
		m_context["V"]->setFloat( V );
		m_context["W"]->setFloat( W );
	}

	void render(SDL_Window*  window){


		try{
			unsigned blocks_x = width/approx_block_size;
			unsigned blocks_y = height/approx_block_size;
			float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);  
			srand (static_cast <unsigned> (time(0)));
			
			for(unsigned int currentSample = 0; currentSample < spp * spp; ++currentSample){
				
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
						
						m_context["aspect_ratio"]->setFloat(aspect_ratio);
						m_context["screen_dim"]->setUint(optix::make_uint2(width, height));
						m_context["offset_x"]->setUint(offset_x);
						m_context["offset_y"]->setUint(offset_y);
						m_context["currentSample"]->setUint(currentSample);
						m_context["rnd1"]->setFloat(rnd1);
						m_context["rnd2"]->setFloat(rnd2);
						m_context["output_buffer_size"]->setUint(optix::make_uint2(max_block_size_x, max_block_size_y));
						m_context->validate();
						m_context->compile();
						m_context->launch( 0, block_size_x, block_size_y );

						SDL_Event event;
						SDL_PollEvent(&event);
				
					}
				}
				float* buf = static_cast<float*>( m_context["output_buffer"]->getBuffer()->map());
				glClearColor ( 0.0, 1.0, 0.0, 1.0 );
				glClear ( GL_COLOR_BUFFER_BIT );
						
				glDrawPixels(width, height, GL_RGB, GL_FLOAT, buf /*&render_image[0]*/);
				SDL_GL_SwapWindow(window);		
				m_context["output_buffer"]->getBuffer()->unmap();
				std::cout << static_cast<float>((currentSample + 1) * 100) / static_cast<float>(spp * spp);
				std::cout << "% completado" << std::endl; 
			}
			
		} 
		catch( optix::Exception& e ){
			std::cout << e.getErrorString() << std::endl;
			int p;
			std::cin >> p;
			exit(1);
		}
	}

	int saveToTGA (const char *filename){

		Uint w = width;
		Uint h = height;
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
		tgah[12] = (char) (w & 0xFF);          // Resolution
		tgah[13] = (char) (w >> 8);
		tgah[14] = (char) (h & 0xFF);          
		tgah[15] = (char) (h >> 8);
		tgah[16] = 0x18;								// Color depth              
		tgah[17] = 0x00;                                // up..dw & lf..rt
		fwrite (tgah, 18, 1, f);                        // Save Header

		fwrite (&pixels[0], 3*sizeof(unsigned char), w*h, f);
  
		fclose(f);
		return 0;
	
	}

	void destroy(){
		m_context->destroy();
	}

	


	void createObjGeometry(){
		using namespace optix;

		GeometryGroup group = m_context->createGeometryGroup();
		Program diffuse_prg = m_context->createProgramFromPTXFile( ptxpath("gi_project", "m_obj_material.cu") , "diffuse" );
		Program any_hit = m_context->createProgramFromPTXFile( ptxpath("gi_project", "m_obj_material.cu") , "any_hit_shadow" );
		Material diffuse = m_context->createMaterial();
		diffuse["is_dome"]->setInt(false);
		diffuse["dome_emission"]->setFloat(1.0f, 1.0f, 1.0f);
		diffuse["max_direct_samples"]->setUint(10u);
		diffuse["min_direct_samples"]->setUint(1u);
		diffuse->setClosestHitProgram(0, diffuse_prg);
		diffuse->setAnyHitProgram(1, any_hit); 
		std::string filename = "assets/dabrovic-sponza/sponza.obj";
		
		if( ObjLoader::isMyFile( filename.c_str() ) ) {
			std::cout << "test" << std::endl;
			const char* path = "m_triangle_mesh.cu.obj";
			Program mesh_bounds = m_context->createProgramFromPTXFile(path, "mesh_bounds");
			Program mesh_isect = m_context->createProgramFromPTXFile(path, "mesh_intersect");
			ObjLoader loader( filename.c_str(), m_context, group, diffuse, true, "Sbvh", "Bvh", "refine", false);
			loader.setBboxProgram(mesh_bounds);
			loader.setIntersectProgram(mesh_isect);
			loader.load();
			Aabb aabb = loader.getSceneBBox();
			std::cout << "Object aabb.min = (" << aabb.m_min.x << ", " << aabb.m_min.y << ", " << aabb.m_min.z << ")" << std::endl;
			std::cout << "Object aabb.max = (" << aabb.m_max.x << ", " << aabb.m_max.y << ", " << aabb.m_max.z << ")" << std::endl;
		}
		else{
			std::cout << "can't load " << filename << " EXIT" << std::endl;
			exit(1);
		}
		//m_context["top_shadower"]->set(group);

		
		//Create sphere
		std::string sphere_ptx(ptxpath("gi_project", "sphere.cu"));
		Program sphere_isect = m_context->createProgramFromPTXFile(sphere_ptx, "sphere_isect");
		Program sphere_bounds = m_context->createProgramFromPTXFile(sphere_ptx, "sphere_bounds");
		/*
		Geometry sphere = m_context->createGeometry();
		sphere->setPrimitiveCount(1u);
		sphere->setBoundingBoxProgram( sphere_bounds );
		sphere->setIntersectionProgram(sphere_isect);
		sphere["center"]->setFloat(0.0f, 0.0f, 0.0f);
		sphere["radius"]->setFloat(50.0f);
		*/
		Geometry sphereLight = m_context->createGeometry();
		sphereLight->setPrimitiveCount(1u);
		sphereLight->setBoundingBoxProgram(sphere_bounds);
		sphereLight->setIntersectionProgram(sphere_isect);
		sphereLight["center"]->setFloat(-5.0f, 7.0f, 4.0f );
		sphereLight["radius"]->setFloat(1.0f);

		/*
		Geometry sphere2 = m_context->createGeometry();
		sphere2->setPrimitiveCount(1u);
		sphere2->setBoundingBoxProgram(sphere_bounds);
		sphere2->setIntersectionProgram(sphere_isect);
		sphere2["center"]->setFloat(0.0f, 3.0f, 0.0f);
		sphere2["radius"]->setFloat(1.0f);
		
		
		Geometry sphere3 = m_context->createGeometry();
		sphere3->setPrimitiveCount(1u);
		sphere3->setBoundingBoxProgram(sphere_bounds);
		sphere3->setIntersectionProgram(sphere_isect);
		sphere3["center"]->setFloat(0.0f, 5.0f, 0.0f);
		sphere3["radius"]->setFloat(1.0f);

		
		Geometry sphere4 = m_context->createGeometry();
		sphere4->setPrimitiveCount(1u);
		sphere4->setBoundingBoxProgram(sphere_bounds);
		sphere4->setIntersectionProgram(sphere_isect);
		sphere4["center"]->setFloat(0.0f, 7.0f, 0.0f);
		sphere4["radius"]->setFloat(1.0f);

		Program specular = m_context->createProgramFromPTXFile(ptxpath("gi_project", "material.cu"), "diffuse_and_specular");
		Material spec1 = m_context->createMaterial();
		spec1->setClosestHitProgram(0, specular);
		spec1["Kd"]->setFloat(0.0, 0.0, 0.0);
		spec1["Ks"]->setFloat(1.0, 1.0, 1.0);
		spec1["phong_exp"]->setFloat(60.0);

		Material spec2 = m_context->createMaterial();
		spec2->setClosestHitProgram(0, specular);
		spec2["Kd"]->setFloat(0.0, 0.0, 0.0);
		spec2["Ks"]->setFloat(1.0, 1.0, 1.0);
		spec2["phong_exp"]->setFloat(5.0);

		Material spec3 = m_context->createMaterial();
		spec3->setClosestHitProgram(0, specular);
		spec3["Kd"]->setFloat(0.5, 0.2, 0.2);
		spec3["Ks"]->setFloat(0.4, 0.2, 0.2);
		spec3["phong_exp"]->setFloat(30.0);
		
		GeometryInstance ball1 = m_context->createGeometryInstance(sphere2, &spec1, &spec1+1);
		GeometryInstance ball2 = m_context->createGeometryInstance(sphere3, &spec2, &spec2+1);
		GeometryInstance ball3 = m_context->createGeometryInstance(sphere4, &spec3, &spec3+1);
		GeometryGroup specular_balls = m_context->createGeometryGroup();
		specular_balls->setChildCount(3u);
		specular_balls->setChild(0, ball1);
		specular_balls->setChild(1, ball2);
		specular_balls->setChild(2, ball3);
		specular_balls->setAcceleration(m_context->createAcceleration("NoAccel", "NoAccel"));
		*/

		Material diffuse_light = m_context->createMaterial();
		Program diffuse_em = m_context->createProgramFromPTXFile( ptxpath("gi_project", "m_obj_material.cu") , "emitter" );
		diffuse_light->setClosestHitProgram( 0, diffuse_em );
		diffuse_light["emission"]->setFloat(2.0, 2.0, 2.0);

		//GeometryInstance light = m_context->createGeometryInstance(sphere, &diffuse_light, &diffuse_light+1);
		GeometryInstance light = m_context->createGeometryInstance(sphereLight, &diffuse_light, &diffuse_light+1);
		GeometryGroup light_group = m_context->createGeometryGroup();
		light_group->setChildCount(1);
		light_group->setChild(0, light);

		light_group->setAcceleration(m_context->createAcceleration("NoAccel", "NoAccel"));
		Group all = m_context->createGroup();
		all->setAcceleration(m_context->createAcceleration("Sbvh", "Bvh"));
		all->setChildCount(2);
		all->setChild(0, group);
		all->setChild(1, light_group);
		//all->setChild(2, specular_balls);
		m_context["top_shadower"]->set(group);
		m_context["top_object"]->set(all);


		SphereLight lights[] = { 
			{ optix::make_float3( 2.0f, 2.0f, 2.0f ), optix::make_float3(-5.0f, 7.0f, 4.0f ), 1 }
		};

		optix::Buffer light_buffer = m_context->createBuffer(RT_BUFFER_INPUT);
		light_buffer->setFormat(RT_FORMAT_USER);
		light_buffer->setElementSize(sizeof(SphereLight));
		light_buffer->setSize( sizeof(lights)/sizeof(lights[0]) );
		memcpy(light_buffer->map(), lights, sizeof(lights));
		light_buffer->unmap();

		m_context["spherical_lights"]->set(light_buffer);
		

	}

};

int main(int argc, char* argv[]){

	char dirPath[200];
	_getcwd(dirPath, sizeof(dirPath));
	std::cout << dirPath << std::endl;
	unsigned int width  = 1024;
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
	unsigned int sqrtspp = 10;
	Pathtracer pt;
	pt.init(width, height, sqrtspp, 200u);
	pt.createObjGeometry();
	//pt.createCornellGeometry();
	pt.render(window);
	std::stringstream ss;
	ss << "test_" << currentDateTime() << "_" << sqrtspp * sqrtspp << "spp.tga"; 
	//pt.saveToTGA( ("test_" + currentDateTime() +  ".tga").c_str());
	
	SDL_DestroyWindow(window);
	
	pt.saveToTGA( (ss.str()).c_str());
	int p;
	std::cin >> p;
	SDL_Quit();
	return 0;
}


