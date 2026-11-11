/* Check the selected TTC face and glyphs from real game text. */
#include <SDL/SDL_ttf.h>
#undef main
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv)
{
    FT_Library library;
    FT_Face face;
    TTF_Font *font;
    FILE *corpus;
    unsigned codepoint, count=0, missing=0;
    long index;
    if(argc!=5) return 2;
    index=strtol(argv[2],NULL,10);
    if(FT_Init_FreeType(&library) || FT_New_Face(library,argv[1],index,&face)) return 1;
    if(!FT_Get_Postscript_Name(face) || strcmp(FT_Get_Postscript_Name(face),argv[3])) {
        fprintf(stderr,"CoreText/FreeType face mismatch: index=%ld, FreeType=%s, CoreText=%s\n",index,FT_Get_Postscript_Name(face),argv[3]);
        return 1;
    }
    FT_Done_Face(face); FT_Done_FreeType(library);
    if(TTF_Init()!=0) return 1;
    font=TTF_OpenFontIndex(argv[1],20,index);
    if(!font) { fprintf(stderr,"%s\n",TTF_GetError()); return 1; }
    corpus=fopen(argv[4],"r"); if(!corpus) return 1;
    while(fscanf(corpus,"%x",&codepoint)==1) {
        count++;
        if(!TTF_GlyphIsProvided(font,(Uint16)codepoint)) { fprintf(stderr,"Missing real-game glyph U+%04X\n",codepoint); missing++; }
    }
    fclose(corpus); TTF_CloseFont(font); TTF_Quit();
    printf("Original Big5 text font coverage: %u characters, %u missing; TTC index=%ld\n",count,missing,index);
    return missing?1:0;
}
