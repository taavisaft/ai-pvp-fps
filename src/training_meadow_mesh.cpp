#include "vegetation.h"
#include <cmath>

// Appended after the original online mesh. Training draws this index range only.
// Color.z marks green (1), straw (2), seed panicles (3), broad leaves (4).
void vegBuildTrainingMeadow(std::vector<float>& v, std::vector<unsigned>& idx, bool far) {
    auto random=[](int n) { float f=sinf(n*127.1f+41.7f)*43758.5453f; return f-floorf(f); };
    auto strip=[&](glm::vec3 root, glm::vec3 end, glm::vec3 bow, float width,
                   float kind, int key, int segments) {
        glm::vec3 side=glm::normalize(glm::cross(end-root,glm::vec3(.37f,.13f,1)));
        unsigned first=(unsigned)v.size()/12;
        for(int r=0;r<=segments;++r) for(int s=0;s<2;++s) {
            float t=float(r)/segments;
            float taper=kind==4 ? sinf(t*3.14159265f) : powf(1-t,.7f);
            glm::vec3 p=glm::mix(root,end,t)+bow*(4*t*(1-t))+side*((s-.5f)*width*taper);
            glm::vec3 n=glm::normalize(glm::vec3(0,.8f,0)+side*((s-.5f)*.5f));
            v.insert(v.end(),{p.x,p.y,p.z,n.x,n.y,n.z,random(key),random(key+61),kind,
                             far ? 0.0f : t*t,(far ? -10.0f : -8.0f)-s*.9f,.025f+t*.955f});
        }
        for(int r=0;r<segments;++r) {
            unsigned a=first+r*2;
            idx.insert(idx.end(),{a,a+1,a+3,a,a+3,a+2});
        }
    };
    for(int b=0;b<(far ? 10 : 34);++b) {
        float a=b*2.39996323f, c=cosf(a), s=sinf(a);
        bool dry=b%3==0;
        float radius=.04f+.23f*random(b+21);
        float height=dry ? .18f+.28f*random(b+1) : .32f+.44f*random(b+1);
        float reach=dry ? .24f+.23f*random(b+41) : .06f+.20f*random(b+41);
        glm::vec3 root(c*radius,-.02f,s*radius);
        glm::vec3 end=root+glm::vec3(-s*reach,height,c*reach);
        glm::vec3 bow(-s*.055f,dry ? .075f : .035f,c*.055f);
        strip(root,end,bow,dry ? .0045f : .005f+.006f*random(b+81),dry ? 2 : 1,b,far ? 2 : 4);
    }
    if(far) return;
    // A branched, airy seed head rather than a wide opaque billboard.
    glm::vec3 root(.09f,-.02f,.04f), tip(.16f,1.02f,.10f);
    strip(root,tip,glm::vec3(.025f,0,0),.0035f,3,101,4);
    for(int b=0;b<9;++b) {
        float t=.64f+b*.036f, a=b*2.39996323f;
        glm::vec3 start=glm::mix(root,tip,t);
        glm::vec3 end=start+glm::vec3(cosf(a)*(.10f-b*.007f),.11f,sinf(a)*(.10f-b*.007f));
        strip(start,end,glm::vec3(0,.018f,0),.004f,3,110+b,2);
        strip(glm::mix(start,end,.55f),end+glm::vec3(0,.035f,0),glm::vec3(0),.011f,3,130+b,2);
    }
    for(int b=0;b<4;++b) {
        float a=b*2.39996323f;
        strip(glm::vec3(0,-.01f,0),glm::vec3(cosf(a)*.22f,.19f,sinf(a)*.22f),
              glm::vec3(0,.06f,0),.045f,4,150+b,4);
    }
}
