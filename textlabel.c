#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include <fontconfig/fontconfig.h>
#include <inttypes.h>

typedef struct /*_TLABEL_OPTS*/ {
	char* fontspec;
	char** lines;
	unsigned width;
} OPTIONS;

int usage(char* fn){
	fprintf(stderr, "textlabel - generate bitmap data from text\n");
	fprintf(stderr, "Usage: %s [<options>] <text>\n", fn);
	fprintf(stderr, "Recognized options:\n");
	fprintf(stderr, "\t--font <fontspec>\tSet font to use by fontconfig name\n");
	fprintf(stderr, "\t--width <width>\tSet output width (default: 64)\n");
	fprintf(stderr, "\t--\t\t\tEnd option parsing\n");
	return -1;
}

size_t stored_lines(char** lines){
	size_t u;
	if(!lines){
		return 0;
	}

	for(u = 0; lines[u]; u++){
	}
	return u;
}

int line_store(char*** lines, char* line){
	size_t size = 0;

	//expand array
	size = stored_lines(*lines);
	*lines = realloc(*lines, (size + 2) * sizeof(char*));
	if(!(*lines)){
		return -1;
	}

	//allocate
	*((*lines) + size + 1) = NULL;
	*((*lines) + size) = calloc(strlen(line) + 1, sizeof(char));
	if(!((*lines) + size)){
		return -1;
	}

	//store
	strncpy(*((*lines) + size), line, strlen(line));
	return 0;
}

int args_parse(OPTIONS* opts, int argc, char** argv){
	size_t i, c;
	bool stop_parsing = false;
	char* current_line = NULL;
	int current_offset = 0;

	for(i = 0; i < argc; i++){
		if(!stop_parsing && !strcmp(argv[i], "--font")){
			if(argc > i + 1){
				opts->fontspec = argv[++i];
			}
			else{
				return -1;
			}
		}
		else if(!stop_parsing && !strcmp(argv[i], "--width")){
			if(argc > i + 1){
				opts->width = strtoul(argv[++i], NULL, 10);
			}
			else{
				return -1;
			}
		}
		else if(!stop_parsing && !strcmp(argv[i], "--")){
			stop_parsing = true;
		}
		else{
			//add current token to line
			current_line = realloc(current_line, current_offset + strlen(argv[i]) + 2);
			if(current_offset != 0){
				current_line[current_offset]=' ';
				current_offset++;
			}
			strncpy(current_line + current_offset, argv[i], strlen(argv[i]) + 1);
			current_offset += strlen(argv[i]);
		}
	}

	if(!current_line){
		fprintf(stderr, "No text given\n");
		return -1;
	}

	//convert escaped characters
	c = 0;
	for(i = 0; current_line[i]; i++){
		if(current_line[i] == '\\'){
			switch(current_line[i + 1]){
				case 'n':
					current_line[c] = '\n';
					break;
				default:
					current_line[c] = current_line[i + 1];
			}
			i++;
		}
		else{
			current_line[c] = current_line[i];
		}
		c++;
	}
	current_line[c] = 0;

	//split into lines
	c = 0;
	for(i = 0; current_line[i]; i++){
		if(current_line[i] == '\n'){
			current_line[i] = 0;
			if(line_store(&(opts->lines), current_line + c) < 0){
				fprintf(stderr, "Failed to store line\n");
				free(current_line);
				return -1;
			}
			c = i + 1;
		}
	}
	if(line_store(&(opts->lines), current_line + c) < 0){
		fprintf(stderr, "Failed to store line\n");
		free(current_line);
		return -1;
	}

	free(current_line);
	return 0;
}

FcPattern* match_font(char* fontspec){
	FcPattern* font_pattern = NULL;
	FcPattern* match = NULL;
	FcResult fc_result = 0;

	font_pattern = FcNameParse((FcChar8*)fontspec);

	if(!font_pattern){
		return NULL;
	}

	FcDefaultSubstitute(font_pattern);
	if(FcConfigSubstitute(NULL, font_pattern, FcMatchFont)){
		match = FcFontMatch(NULL, font_pattern, &fc_result);
		FcPatternDestroy(font_pattern);
		switch(fc_result){
			case FcResultMatch:
				return match;
			case FcResultNoMatch:
				fprintf(stderr, "No Match: %s\n", FcNameUnparse(font_pattern));
				break;
			case FcResultTypeMismatch:
				fprintf(stderr, "Type Mismatch: %s\n", FcNameUnparse(match));
				return match;
			case FcResultNoId:
				fprintf(stderr, "No ID: %s\n", FcNameUnparse(match));
				break;
			case FcResultOutOfMemory:
				fprintf(stderr, "FontConfig error: out of memory\n");
				return NULL;
		}
	}

	fprintf(stderr, "Failed to match font to config\n");
	return NULL;
}

bool load_font(FT_Library ft, FcPattern* pattern, unsigned char_height, FT_Face* face){
	char* font_file = NULL;

	if(!pattern){
		fprintf(stderr, "No pattern provided\n");
		return false;
	}

	switch(FcPatternGetString(pattern, FC_FILE, 0, (FcChar8**)&font_file)){
		case FcResultMatch:
			//fprintf(stderr, "Font file %s\n", font_file);
			break;
		default:
			fprintf(stderr, "Failed to find font location\n");
			FcPatternDestroy(pattern);
			return false;
	}

	switch(FT_New_Face(ft, font_file, 0, face)){
		case FT_Err_Unknown_File_Format:
			fprintf(stderr, "Unknown font file format\n");
			FcPatternDestroy(pattern);
			return false;
		case 0:
			FcPatternDestroy(pattern);
			break;
		default:
			fprintf(stderr, "Unknown freetype error\n");
			FcPatternDestroy(pattern);
			return false;
	}

	//fprintf(stderr, "Setting glyph sizes to %d pixels\n", char_height);
	if(FT_Set_Pixel_Sizes(*face, 0, char_height)){
		fprintf(stderr, "Failed to set glyph size %d - font might not offer that size or might not be scalable\n", char_height);
		return false;
	}

	return true;
}

bool load_glyphs(char** lines, FT_Face font_face, FT_BitmapGlyph** glyphs){
	//render all strings into glyph arrays
	size_t i, c;
	for(i = 0; i < stored_lines(lines); i++){
		for(c = 0; c < strlen(lines[i]); c++){
			//get glyph
			if(FT_Load_Glyph(font_face, FT_Get_Char_Index(font_face, lines[i][c]), FT_LOAD_DEFAULT)){
				fprintf(stderr, "Font has no glyph for %c, aborting\n", lines[i][c]);
				return false;
			}
			//copy glyph to array
			if(FT_Get_Glyph(font_face->glyph, (FT_Glyph*)(glyphs[i]) + c)){
				fprintf(stderr, "Failed to copy glyph, aborting\n");
				return false;
			}
			//render to bitmap
			if(FT_Glyph_To_Bitmap((FT_Glyph*)(glyphs[i]) + c, FT_RENDER_MODE_MONO, 0, 1)){
				fprintf(stderr, "Failed to render glyph, aborting\n");
				return false;
			}
			//test pixel format
			if(glyphs[i][c]->bitmap.pixel_mode != FT_PIXEL_MODE_MONO){
				fprintf(stderr, "Got unsupported pixel format\n");
				return false;
			}
		}
	}
	return true;
}

void render(char** lines, FT_BitmapGlyph** glyphs, unsigned line_height, unsigned extra_filler){
	ssize_t i, c;
	bool done_rendering;
	int* current_character = calloc(stored_lines(lines), sizeof(int));
	int* current_rasterline = calloc(stored_lines(lines), sizeof(int));
	if(!current_character || !current_rasterline){
		fprintf(stderr, "Failed to allocate memory");
		return;
	}

	//TODO kerning (see http://www.freetype.org/freetype2/docs/tutorial/step2.html)
	//actual rendering portion
	do{
		//process one rasterline
		done_rendering = true;

		//start at the lowest line and work upwards
		for(i = stored_lines(lines) - 1; i >= 0; i--){
			//line already ended, fill with zeroes
			if(current_character[i] >= strlen(lines[i])){
				for(c = 0; c < line_height; c++){
					fputc(stdout, '0');
				}
				continue;
			}

			FT_BitmapGlyph current = glyphs[i][current_character[i]];
			uint8_t mask = 1 << (7 - (current_rasterline[i] % 8));

			//fprintf(stderr, "line %d of char %d of line %d, mask %2X, offset %d, rows %d, width %d, pitch %d\n", current_rasterline[i], current_character[i], i, mask, current_rasterline[i]/8, current->bitmap.rows, current->bitmap.width, current->bitmap.pitch);

			if(current_rasterline[i] < current->bitmap.width){
				done_rendering = false;
			}
			else{
				if(current_character[i] < strlen(lines[i])){
					current_character[i]++;
					current_rasterline[i] = 0;
					done_rendering = false;
					//print separator line
					for(c = 0; c < line_height; c++){
						printf("0");
					}
					continue;
				}
				else{
					fprintf(stderr, "This should not have happened\n");
					continue;
				}
			}

			//print pixels
			for(c = current->bitmap.rows - 1; c >= 0; c--){
				uint8_t cell_index = (c * current->bitmap.pitch) + (current_rasterline[i] / 8);
				uint8_t cell_value = current->bitmap.buffer[cell_index];
				//fprintf(stderr, "\nTesting cell %d with value %2X -> %2X -> ", cell_index, cell_value, cell_value&mask);

				printf("%c", ((cell_value&mask) > 0) ? '1' : '0');
			}

			//print filler
			for(c = current->bitmap.rows; c < line_height; c++){
				printf("0");
			}

			current_rasterline[i]++;
		}

		for(c = 0; c < extra_filler; c++){
			printf("0");
		}
		printf("\n");
	} while (!done_rendering);

	free(current_character);
	free(current_rasterline);
}

int main(int argc, char** argv){
	OPTIONS opts = {
		.fontspec = "FreeSerif",
		.lines = NULL,
		.width = 64
	};
	size_t i, c;
	FT_Library ft_handle;
	FT_BitmapGlyph** glyphs;
	FT_Face font_face;
	unsigned line_height = 64, extra_filler;

	if(argc < 2){
		exit(usage(argv[0]));
	}

	if(FT_Init_FreeType(&ft_handle)){
		fprintf(stderr, "Failed to initialize FreeType\n");
		return EXIT_FAILURE;
	}

	if(!FcInit()){
		fprintf(stderr, "Failed to initialize FontConfig\n");
		return EXIT_FAILURE;
	}

	//args_parse
	if(args_parse(&opts, argc - 1, argv + 1) < 0){
		fprintf(stderr, "Failed to parse arguments\n");
		exit(usage(argv[0]));
	}

	//sanity check
	if(!opts.lines || !opts.lines[0] || opts.width == 0){
		fprintf(stderr, "Invalid invocation (%zu lines, width %d)\n", stored_lines(opts.lines), opts.width);
		exit(usage(argv[0]));
	}

	//calculate required sizes
	line_height = opts.width / stored_lines(opts.lines);
	extra_filler = opts.width - (line_height * stored_lines(opts.lines));

	//allocate glyph buffer
	glyphs = calloc(stored_lines(opts.lines), sizeof(FT_BitmapGlyph*));
	if(!glyphs){
		printf("Failed to allocate memory\n");
		exit(EXIT_FAILURE);
	}

	for(i = 0; i < stored_lines(opts.lines); i++){
		glyphs[i] = calloc(strlen(opts.lines[i]), sizeof(FT_BitmapGlyph));
		if(!glyphs[i]){
			printf("Failed to allocate memory for glyph array\n");
			exit(EXIT_FAILURE);
		}
	}

	//load the font & render
	if(load_font(ft_handle, match_font(opts.fontspec), line_height, &font_face)
			&& load_glyphs(opts.lines, font_face, glyphs)){
		render(opts.lines, glyphs, line_height, extra_filler);
	}

	//clean up
	for(i = 0; opts.lines[i]; i++){
		for(c = 0; c < strlen(opts.lines[i]); c++){
			FT_Done_Glyph((FT_Glyph)(glyphs[i][c]));
		}
		free(glyphs[i]);
		free(opts.lines[i]);
	}
	free(glyphs);
	free(opts.lines);
	FT_Done_Face(font_face);

	FT_Done_FreeType(ft_handle);
	FcFini();

	return EXIT_SUCCESS;
}
