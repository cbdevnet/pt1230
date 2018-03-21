#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv){
	size_t bar_width = 1, image_height = 64, i;
	FILE* source = stdin;
	size_t x, y;

	for(i = 1; i < argc; i++){
		if(!strcmp(argv[i], "--width")){
			if(i + 1 < argc){
				bar_width = strtoul(argv[i + 1], NULL, 10);
				i++;
			}
			else{
				fprintf(stderr, "Missing argument for --width\n");
				return 1;
			}
		}
		else if(!strcmp(argv[i], "--height")){
			if(i + 1 < argc){
				image_height = strtoul(argv[i + 1], NULL, 10);
				i++;
			}
			else{
				fprintf(stderr, "Missing argument for --height\n");
				return 1;
			}
		}
		else{
			if(source && source != stdin){
				fclose(source);
			}

			source = fopen(argv[i], "r");
		}
	}

	if(!source){
		fprintf(stderr, "Failed to open input file\n");
		return 1;
	}

	for(i = fgetc(source); i && i != EOF && i != '\n'; i = fgetc(source)){
		for(x = 0; x < bar_width; x++){
			for(y = 0; y < image_height; y++){
				fputc(i, stdout);
			}
			fputc('\n', stdout);
		}
	}

	fclose(source);
	return 0;
}
