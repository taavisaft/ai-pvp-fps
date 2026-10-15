#include "texture.h"
#include "satellite_map.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>

GLuint makeMapTexture(const Box* boxes,int count,float worldHalf) {
    constexpr int SIDE=1024; // 2 m/pixel over the 2 km map; RGB8 + mipmaps ~4 MiB.
    const auto start=std::chrono::steady_clock::now();
    auto* rgb=static_cast<unsigned char*>(malloc(size_t(SIDE)*SIDE*3));
    auto* heights=static_cast<float*>(malloc(size_t(SIDE)*SIDE*sizeof(float)));
    if(!rgb || !heights) { free(rgb); free(heights); return 0; }
    GLuint texture=0;
    if(bakeSatelliteMap(rgb,heights,SIDE,worldHalf,boxes,count))
        texture=uploadTextureRGB(rgb,SIDE,SIDE);
    free(rgb); free(heights);
    double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    printf("[map] satellite %dx%d baked/uploaded in %.1f ms\n",SIDE,SIDE,ms);
    return texture;
}
