#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include <fontconfig/fontconfig.h>

typedef struct /*_TLABEL_OPTS*/ {
	char* fontspec;
	char** lines;
	unsigned width;
} OPTIONS;

int usage(char* fn){
	printf("textlabel - generate bitmap data from text\n");
	printf("Usage: %s [<options>] <text>\n", fn);
	printf("Recognized options:\n");
	printf("\t--font <fontspec>\tSet font to use by fontconfig name\n");
	printf("\t--width <width>\tSet output width (default: 64)\n");
	printf("\t--\t\t\tEnd option parsing\n");
	return -1;
}

int stored_lines(char** lines_array){
	unsigned i=0;

	if(!lines_array){
		return 0;
	}

	for(i=0;lines_array[i];i++){
	}
	return i;
}

int line_store(char*** lines_array, char* line){
	unsigned lines=0;

	if(!*lines_array){
		*lines_array=calloc(2, sizeof(char*));
		if(!*lines_array){
			return -1;
		}
		lines=0;
	}
	else{
		//get current length
		lines=stored_lines(*lines_array);
		*lines_array=realloc(*lines_array, (lines+2)*sizeof(char*));
		if(!*lines_array){
			return -1;
		}
	}

	(*lines_array)[lines]=calloc(strlen(line)+1, sizeof(char));
	if(!(*lines_array)[lines]){
		return -1;
	}
	strncpy((*lines_array)[lines], line, strlen(line)+1);
	(*lines_array)[lines+1]=NULL;
	return 0;
}

int args_parse(OPTIONS* opts, int argc, char** argv){
	unsigned i, c;
	bool stop_parsing=false;
	char* current_line=NULL;
	int current_offset=0;

	for(i=0;i<argc;i++){
		if(!stop_parsing&&!strcmp(argv[i], "--font")){
			if(argc>i+1){
				opts->fontspec=argv[++i];
			}
			else{
				return -1;
			}
		}
		else if(!stop_parsing&&!strcmp(argv[i], "--width")){
			if(argc>i+1){
				opts->width=strtoul(argv[++i], NULL, 10);
			}
			else{
				return -1;
			}
		}
		else if(!stop_parsing&&!strcmp(argv[i], "--")){
			stop_parsing=true;
		}
		else{
			//add current token to line
			current_line=realloc(current_line, current_offset+strlen(argv[i])+2);
			if(current_offset!=0){
				current_line[current_offset]=' ';
				current_offset++;
			}
			strncpy(current_line+current_offset, argv[i], strlen(argv[i])+1);
			current_offset+=strlen(argv[i]);
		}
	}

	if(!current_line){
		return -1;
	}

	//convert escaped characters
	c=0;
	for(i=0;current_line[i];i++){
		if(current_line[i]=='\\'){
			switch(current_line[i+1]){
				case 'n':
					current_line[c]='\n';
					break;
				default:
					current_line[c]=current_line[i+1];
			}
			i++;
		}
		else{
			current_line[c]=current_line[i];
		}
		c++;
	}
	current_line[c]=0;

	//split into lines
	c=0;
	for(i=0;current_line[i];i++){
		if(current_line[i]=='\n'){
			current_line[i]=0;
			if(line_store(&(opts->lines), current_line+c)<0){
				printf("Failed to store line\n");
				return -1;
			}
			c=i+1;
		}
	}
	if(line_store(&(opts->lines), current_line+c)<0){
		printf("Failed to store line\n");
		return -1;
	}
	
	free(current_line);
	return 0;
}

FcPattern* match_font(char* fontspec){
	FcPattern* font_pattern=NULL;
	FcPattern* match=NULL;
	FcResult fc_result=0;
	
	font_pattern=FcNameParse((FcChar8*)fontspec);
	
	if(!font_pattern){
		return NULL;
	}

	FcDefaultSubstitute(font_pattern);
	if(FcConfigSubstitute(NULL, font_pattern, FcMatchFont)){
		match=FcFontMatch(NULL, font_pattern, &fc_result);	
		FcPatternDestroy(font_pattern);
		switch(fc_result){
			case FcResultMatch:
				return match;
			case FcResultNoMatch:
				printf("No Match: %s\n", FcNameUnparse(font_pattern));
				break;
			case FcResultTypeMismatch:
				printf("Type Mismatch: %s\n", FcNameUnparse(match));
				return match;
			case FcResultNoId:
				printf("No ID: %s\n", FcNameUnparse(match));
				break;
			case FcResultOutOfMemory:
				printf("FontConfig error: out of memory\n");
				return NULL;
		}
	}
	
	printf("Failed to match font to config\n");
	return NULL;
}

bool load_font(FT_Library ft, FcPattern* pattern, unsigned char_height, FT_Face* face){
	char* font_file=NULL;
	switch(FcPatternGetString(pattern, FC_FILE, 0, (FcChar8**)&font_file)){
		case FcResultMatch:
			printf("Font file %s\n", font_file);
			break;
		default:
			printf("Failed to find font location\n");
			return false;
	}

	switch(FT_New_Face(ft, font_file, 0, face)){
		case FT_Err_Unknown_File_Format:
			printf("Unknown font file format\n");
			return false;
		case 0:
			break;
		default:
			printf("Unknown freetype error\n");
			return false;
	}

	printf("Setting glyph sizes to %d pixels\n", char_height);
	if(FT_Set_Pixel_Sizes(*face, 0, char_height)){
		printf("Failed to set glyph size %d - font might not offer that size or might not be scalable\n", char_height);
		return false;
	}

	return true;
}

int main(int argc, char** argv){
	OPTIONS opts = {
		.fontspec = "FreeSerif",
		.lines = NULL,
		.width = 64
	};
	int i, c;
	bool done_printing=false;
	FT_Library ft_handle;
	FcPattern* font_pattern=NULL;
	FT_BitmapGlyph* glyphs;
	FT_Face font_face;

	if(argc<2){
		exit(usage(argv[0]));
	}

	if(FT_Init_FreeType(&ft_handle)){
		printf("Failed to initialize FreeType\n");
		return EXIT_FAILURE;
	}

	if(!FcInit()){
		printf("Failed to initialize FontConfig\n");
		return EXIT_FAILURE;
	}

	//args_parse
	if(args_parse(&opts, argc-1, argv+1)<0){
		printf("Failed to parse arguments\n");
		exit(usage(argv[0]));
	}

	//sanity check
	if(!opts.lines || !opts.lines[0] || opts.width==0){
		printf("Invalid invocation (%d lines, width %d)\n", stored_lines(opts.lines), opts.width);
		exit(usage(argv[0]));
	}

	glyphs=calloc(stored_lines(opts.lines), sizeof(FT_BitmapGlyph));
	if(!glyphs){
		printf("Failed to allocate memory for glyph array\n");
		exit(EXIT_FAILURE);
	}

	//load the font
	font_pattern=match_font(opts.fontspec);
	if(font_pattern && load_font(ft_handle, font_pattern, opts.width/stored_lines(opts.lines), &font_face)){
		//TODO render all strings into glyph arrays, work on them.
		c=0;
		do{
			done_printing=true;
			for(i=stored_lines(opts.lines)-1;i>=0;i--){
				//get nth character if possible
				if(strlen(opts.lines[i])>c){
					done_printing=false;
				}
				else{
					continue;
				}

				//get glyph
				if(FT_Load_Glyph(font_face, FT_Get_Char_Index(font_face, opts.lines[i][c]), FT_LOAD_DEFAULT)){
					printf("Font has no glyph for %c, aborting\n", opts.lines[i][c]);
					break;
				}

				//store to current glyph array
				if(glyphs[i]){
					FT_Done_Glyph((FT_Glyph)glyphs[i]);
				}
				if(FT_Get_Glyph(font_face->glyph, (FT_Glyph*)glyphs+i)){
					printf("Failed to copy glyph, aborting\n");
					break;
				}

				if(FT_Glyph_To_Bitmap((FT_Glyph*)glyphs+i, FT_RENDER_MODE_MONO, 0, 0)){
					printf("Failed to render glyph, aborting\n");
					break;
				}	
			}

			//if(!done_printing){
				//iterate over glyph array, render glyphs
				for(i=stored_lines(opts.lines)-1;i>=0;i--){
					printf("Glyph %c rows=%d, width=%d\n", opts.lines[i][c], glyphs[i]->bitmap.rows, glyphs[i]->bitmap.width);
				}
			//}

			c++;
		} while(!done_printing);

		//TODO kerning (see http://www.freetype.org/freetype2/docs/tutorial/step2.html)
		FT_Done_Face(font_face);
	}


	//clean up
	for(i=0;opts.lines[i];i++){
		free(opts.lines[i]);
	}

	free(opts.lines);
	if(font_pattern){
		FcPatternDestroy(font_pattern);
	}
	
	FT_Done_FreeType(ft_handle);
	FcFini();

	free(glyphs);
	return EXIT_SUCCESS;
}
