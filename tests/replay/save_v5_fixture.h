#ifndef RICH4_TEST_SAVE_V5_FIXTURE_H
#define RICH4_TEST_SAVE_V5_FIXTURE_H

/* Emit the frozen format-5 layout, independently of the current writer. */
typedef enum SaveCodecKind {
    SAVE_CODEC_I8, SAVE_CODEC_U8, SAVE_CODEC_I16, SAVE_CODEC_U16,
    SAVE_CODEC_I32, SAVE_CODEC_U32, SAVE_CODEC_I64, SAVE_CODEC_U64,
    SAVE_CODEC_F32
} SaveCodecKind;
typedef struct SaveCodec {
    unsigned char data[1024*1024];
    size_t position;
    int reading;
} SaveCodec;
static void codec_scalar(SaveCodec *c, void *value, SaveCodecKind kind)
{
    uint64_t bits=0;
    size_t n=kind<=SAVE_CODEC_U8?1:kind<=SAVE_CODEC_U16?2:
             kind==SAVE_CODEC_I64||kind==SAVE_CODEC_U64?8:4;
    assert(c->position+n<=sizeof(c->data));
    memcpy(&bits,value,n);
    for (size_t i=0;i<n;++i) c->data[c->position++]=(unsigned char)(bits>>(8*i));
}
#include "app/game_save_codec_v5.inc"
static void fixture_put(unsigned char *p,uint32_t value)
{
    for (int i=0;i<4;++i) p[i]=(unsigned char)(value>>(8*i));
}
static void write_v5_fixture(const char *path,GameSession *s,int slot)
{
    SaveCodec *c=calloc(1,sizeof(*c));
    uint32_t crc=0xffffffffu;
    unsigned char header[20]={0};
    FILE *f;
    assert(c!=NULL);
    codec_v5_GameSession(c,s);
    for (size_t i=0;i<c->position;++i) {
        crc^=c->data[i];
        for (int b=0;b<8;++b) crc=(crc>>1)^((crc&1)?0xedb88320u:0);
    }
    memcpy(header,"R4SV",4);
    fixture_put(header+4,5); fixture_put(header+8,(uint32_t)slot);
    fixture_put(header+12,(uint32_t)c->position); fixture_put(header+16,~crc);
    f=fopen(path,"wb"); assert(f!=NULL);
    assert(fwrite(header,1,20,f)==20);
    assert(fwrite(c->data,1,c->position,f)==c->position);
    assert(fclose(f)==0); free(c);
}
#endif
