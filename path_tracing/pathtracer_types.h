
#include <optixu/optixpp_namespace.h>
#include <vector>


struct TSphereLight{

	optix::float3 emission;
	optix::float3 center;
	float radius;

	TSphereLight();
	TSphereLight(optix::float3 aemission, optix::float3 acenter, float aradius); 
};


struct TObjModel{

	std::string filename;
	
	TObjModel();
	TObjModel(const char* afilename);

	void load(optix::GeometryGroup & group);

};

struct TCamera{

	optix::float3 position;
	optix::float3 target;

	float fov;
	float viewd;

	optix::float3 front, left, up;


	bool is_ok;

	TCamera();
	TCamera(optix::float3 aposition, optix::float3 atarget, float afov);
	
	bool isOk() {return is_ok;}

};

class CPathtracer{

	optix::Context m_context;
	TCamera camera;
	std::vector<TObjModel> models;
	std::vector<TSphereLight> sphere_lights;
	bool buffer_mapped;
	bool has_skydome;
	optix::float3 sky_dome_emission;

	unsigned int approx_block_size;
	unsigned int max_depth;
	unsigned int sqrt_spp;
	unsigned int width, height;
	bool ready;
	unsigned int current_sample;

	unsigned int max_block_size_x;
	unsigned int max_block_size_y;

	unsigned blocks_x;
	unsigned blocks_y;
	

	void prepareLights();
	void prepareObjModels();

public:

	double optix_time;
	//CPathtracer();
	void setSkyDomeEmission(optix::float3 sky_dome_emission);
	void disableSkyDome();
	void enableSkyDome();
	void setRenderSize(unsigned int a_width, unsigned int a_height)		{ width = a_width; height = a_height;}
	void setBlockSize(unsigned int a_approx_block_size)					{ approx_block_size = a_approx_block_size;}
	void setSqrtSamplesPerPixel(unsigned int a_sqrt_spp)				{ sqrt_spp = a_sqrt_spp;}
	void setMaxDepth(unsigned int a_max_depth)							{ max_depth = a_max_depth;}
	void setCamera(TCamera cam)											{ camera = cam; }

	void addLight(TSphereLight light);
	void addObjModel(TObjModel model);

	int saveToTGA(const char* filename);
	void renderAllSamples();
	void renderNextSample();
	
	int samplesToDo(); 

	void prepare();

	void destroy();
	float* getOutputBuffer();
	void releaseBuffer();
	
	

};
