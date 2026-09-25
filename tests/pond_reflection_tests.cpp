#include "pond_reflection.h"
#include <cstdio>
#include <cmath>
#include <initializer_list>
static int failures=0;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); ++failures; } } while(0)
static glm::vec2 project(const glm::mat4& vp,glm::vec3 p) {
    glm::vec4 q=vp*glm::vec4(p,1);
    return glm::vec2(q)/q.w;
}
int main() {
    PondReflectionCache cache;
    glm::vec3 eye(0,12,136),target(0,6,184);
    glm::mat4 view=glm::lookAt(eye,target,{0,1,0});
    glm::mat4 proj=glm::perspective(glm::radians(75.0f),16.0f/9.0f,.1f,2000.0f);
    CHECK(cache.needsCapture(view,proj,eye));
    cache.capture(view,proj,eye,6);
    CHECK(!cache.needsCapture(view,proj,eye));
    CHECK(cache.needsCapture(view,proj,eye));
    // Points on the mirror plane project identically to the real camera.
    for(float x : {-30.0f,0.0f,30.0f}) {
        glm::vec3 p(x,6,184);
        CHECK(glm::distance(project(cache.captureVP,p),project(proj*view,p))<.00001f);
    }
    // A reflected tree tip lands on the same capture pixel as its virtual image.
    CHECK(glm::distance(project(cache.captureVP,{12,23,205}),
                        project(proj*view,{12,-11,205}))<.00001f);
    auto oldVP=cache.captureVP;
    glm::vec3 shifted=eye+glm::vec3(.5,0,0);
    auto moved=glm::lookAt(shifted,target,{0,1,0});
    cache.capture(view,proj,eye,6);
    CHECK(cache.needsCapture(moved,proj,shifted));
    CHECK(glm::distance(project(cache.captureVP,target),project(oldVP,target))==0);
    CHECK(glm::distance(project(proj*moved,{20,6,184}),project(oldVP,{20,6,184}))>.00001f);
    cache.capture(view,proj,eye,6);
    auto leaned=glm::rotate(view,glm::radians(1.0f),glm::vec3(0,0,1));
    CHECK(cache.needsCapture(leaned,proj,eye));
    cache.invalidate();
    CHECK(cache.needsCapture(view,proj,eye));
    cache.capture(view,proj,eye,6);
    CHECK(cache.needsCapture(view,proj,eye+glm::vec3(5,0,0)));
    cache.capture(view,proj,eye,6);
    CHECK(cache.needsCapture(glm::lookAt(eye,eye+glm::vec3(1,0,0),{0,1,0}),proj,eye));
    cache.capture(view,proj,eye,6);
    auto zoom=glm::perspective(glm::radians(45.0f),16.0f/9.0f,.1f,2000.0f);
    CHECK(cache.needsCapture(view,zoom,eye));
    return failures ? 1 : 0;
}
