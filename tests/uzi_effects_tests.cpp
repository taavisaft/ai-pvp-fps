#include "uzi_effects.h"
#include "imported_uzi_mesh.h"
#include "terrain.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>

static int failures=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); ++failures; } } while(0)
static int alive(const UziEffects& fx) {
    int count=0;for(const auto& c:fx.casings) if(c.life>0)++count;return count;
}
static void mesh(const float* v,size_t n,const unsigned* idx,size_t count) {
    CHECK(n%8==0 && count%3==0);
    for(size_t i=0;i<n;i+=8) {
        for(size_t j=0;j<8;++j) CHECK(std::isfinite(v[i+j]));
        float normal=v[i+3]*v[i+3]+v[i+4]*v[i+4]+v[i+5]*v[i+5];
        CHECK(fabsf(normal-1)<.01f);
        CHECK(fabsf(v[i])<.1f && fabsf(v[i+1])<.3f && fabsf(v[i+2])<.4f);
    }
    for(size_t i=0;i<count;++i) CHECK(idx[i]<n/8);
}
int main() {
    UziEffects fx;
    CHECK(fabsf(fx.boltOffset()+.030f)<.00001f);
    glm::mat4 mount=glm::rotate(glm::translate(glm::mat4(1),{11,2,8}),glm::radians(90.0f),{0,1,0});
    fx.shot(mount);
    CHECK(fx.boltOffset()==0 && alive(fx)==0);
    fx.tick(.01f);CHECK(alive(fx)==0);
    fx.tick(.01f);CHECK(alive(fx)==1 && !fx.pending);
    const auto& c=fx.casings[0];
    glm::vec3 port=glm::vec3(mount*glm::vec4(UziEffects::PORT,1));
    CHECK(glm::distance(c.pos,port)<.05f);
    CHECK(c.velocity.y>0 && std::isfinite(c.angle));
    float offset=fx.boltOffset();
    fx.tick(.02f);CHECK(fx.boltOffset()<offset);
    fx.tick(.1f);CHECK(alive(fx)==1 && fabsf(fx.boltOffset()+.030f)<.00001f);
    for(int i=0;i<200;++i)fx.tick(1.0f/60);
    CHECK(alive(fx)==0);
    // Sustained fire wraps only the fixed casing pool; PRNG is private/deterministic.
    UziEffects repeat;
    fx.clear();fx.seed=repeat.seed;
    for(int i=0;i<100;++i) {
        fx.shot(mount);repeat.shot(mount);
        fx.tick(.02f);repeat.tick(.02f);
        CHECK(alive(fx)<=UziEffects::CAPACITY);
        CHECK(fx.casings[(fx.next+47)%48].velocity==repeat.casings[(repeat.next+47)%48].velocity);
    }
    fx.clear();CHECK(alive(fx)==0 && !fx.pending);
    fx.shot(mount);fx.clear();fx.tick(.1f);CHECK(alive(fx)==0);
#define MESH(name) mesh(name##_VERTS,sizeof(name##_VERTS)/sizeof(float),name##_IDX,sizeof(name##_IDX)/sizeof(unsigned))
    MESH(IMPORTED_UZI);MESH(IMPORTED_UZI_LOW);MESH(IMPORTED_UZI_BOLT);
    CHECK(sizeof(IMPORTED_UZI_IDX)/sizeof(unsigned)/3<12500);
    CHECK(sizeof(IMPORTED_UZI_LOW_IDX)/sizeof(unsigned)/3<3100);
    return failures ? 1 : 0;
}
