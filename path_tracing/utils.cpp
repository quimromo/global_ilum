
#include <iostream>
#include <ctime>
#include "utils.h"


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
