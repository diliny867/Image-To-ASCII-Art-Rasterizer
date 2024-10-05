
#include <stdlib.h>
#include <stdio.h>

#include "image_raster.h"


int main(int argc, char** argv) {
	char* image_name = "image.jpg";
	char* file_out_name = "";
	int resolution = 5;
	if(argc > 1) {
		image_name = argv[1];
		if(argc > 2) {
			resolution = atoi(argv[2]);
		}
	}
	raster_to_ascii(image_name, file_out_name, resolution);
	return 0;
}
