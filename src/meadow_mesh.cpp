#include "vegetation.h"
#include <cmath>

// Folded ribbons give each plant a lit ridge rather than a flat triangular face.
// Negative UV tags identify grass (-3), broad leaves (-4), seed heads (-5).
void vegBuildMeadow(std::vector<float>& v, std::vector<unsigned>& indices) {
    auto vertex = [&](glm::vec3 p, glm::vec3 n, glm::vec3 c, float flex, float tag) {
        unsigned i = (unsigned)v.size()/12;
        v.insert(v.end(),{p.x,p.y,p.z,n.x,n.y,n.z,c.x,c.y,c.z,flex,tag,flex});
        return i;
    };
    for (int blade=0; blade<16; ++blade) {
        bool leaf = blade >= 12 && blade < 15, seed = blade == 15;
        float tag = seed ? -5 : leaf ? -4 : -3;
        float a = blade*2.39996323f, ca = cosf(a), sa = sinf(a);
        float h = seed ? .83f : leaf ? .24f : .27f+.033f*(blade%11);
        float width = seed ? .003f : leaf ? .037f : .006f+.0012f*(blade%5);
        float bend = leaf ? .32f : .12f+.026f*(blade%6);
        glm::vec3 root(ca*.12f,0,sa*.12f);
        auto point = [&](float across, float y, float forward) {
            return root+glm::vec3(ca*across-sa*forward,y,sa*across+ca*forward);
        };
        unsigned rows[4][3];
        for (int r=0;r<4;++r) {
            float t=r/3.0f;
            float w = width*(leaf ? sinf(3.14159265f*t) : powf(1-t,.65f));
            if (seed && r==2) w=.016f;
            for (int side=0;side<3;++side) {
                float across=(side-1)*w;
                float fold=side==1 ? w*.32f : 0;
                glm::vec3 n=glm::normalize(glm::vec3(ca*(side-1)*.35f,1,sa*(side-1)*.35f));
                glm::vec3 color=glm::mix(glm::vec3(.075,.105,.027),glm::vec3(.27,.34,.095),t);
                if (leaf) color*=glm::vec3(.72f,.88f,.72f);
                if (seed) color=glm::mix(glm::vec3(.18,.19,.065),glm::vec3(.44,.36,.17),t);
                rows[r][side]=vertex(point(across,h*t-fold,bend*t*t),n,color,t,tag);
            }
        }
        for (int r=0;r<3;++r) for(int s=0;s<2;++s)
            indices.insert(indices.end(),{rows[r][s],rows[r][s+1],rows[r+1][s+1],
                                           rows[r][s],rows[r+1][s+1],rows[r+1][s]});
    }
}

// Twelve simple blades: 36 triangles instead of 192 in the detailed clump.
// Tag -6 selects static, coverage-compensated middle/far grass in both shaders.
void vegBuildMeadowFar(std::vector<float>& v, std::vector<unsigned>& indices) {
    for(int b=0;b<12;++b) {
        float a=b*2.39996323f, c=cosf(a), s=sinf(a);
        float h=.28f+.035f*(b%7), width=.011f+.002f*(b%4);
        unsigned first=(unsigned)v.size()/12;
        const float x[5]={-width,width,-width*.65f,width*.65f,0};
        const float y[5]={0,0,h*.58f,h*.58f,h};
        for(int i=0;i<5;++i) {
            float t=y[i]/h, bend=.09f*t*t;
            glm::vec3 p(c*(x[i]+.09f)-s*bend,y[i],s*(x[i]+.09f)+c*bend);
            glm::vec3 col=glm::mix(glm::vec3(.075,.105,.027),glm::vec3(.27,.34,.095),t);
            v.insert(v.end(),{p.x,p.y,p.z,0,1,0,col.x,col.y,col.z,0,-6,t});
        }
        for(unsigned i : {0u,1u,3u,0u,3u,2u,2u,3u,4u}) indices.push_back(first+i);
    }
}

// Individually bent narrow strips, distributed through a volume rather than
// intersecting billboard walls. Each strip samples a different part of the atlas.
void vegBuildMeadowCards(std::vector<float>& v, std::vector<unsigned>& idx, bool far) {
    const int strips=far ? 9 : 32, segments=far ? 2 : 4;
    auto random=[](int n) { float x=sinf(n*127.1f+31.7f)*43758.5453f; return x-floorf(x); };
    for(int blade=0;blade<strips;++blade) {
        float a=blade*2.39996323f, c=cosf(a), s=sinf(a);
        float h=.27f+.32f*random(blade+1);
        float radius=.05f+.16f*random(blade+71);
        float bend=.035f+.17f*random(blade+91);
        float lean=(random(blade+121)-.5f)*.12f;
        int slice=blade%8;
        float width=.009f+.012f*random(blade+151);
        unsigned first=(unsigned)v.size()/12;
        for(int row=0;row<=segments;++row) for(int side=0;side<2;++side) {
            float t=(float)row/segments, x=(side-.5f)*width*powf(1.0f-t,.7f);
            float curl=bend*t*t;
            glm::vec3 p(c*(radius+x+lean*t)-s*curl,h*(t-.16f*t*t*t)-.025f,s*(radius+x+lean*t)+c*curl);
            glm::vec3 n=glm::normalize(glm::vec3(c*(side-.5f)*.5f-s*.45f,.7f+t*.3f,s*(side-.5f)*.5f+c*.45f));
            float u=(slice+(float)side)/8.0f;
            v.insert(v.end(),{p.x,p.y,p.z,n.x,n.y,n.z,random(blade+191),random(blade+211),1,far ? 0 : t*t,
                              (far ? -10.0f : -8.0f)-u,.025f+t*.955f});
        }
        for(int row=0;row<segments;++row) {
            unsigned a0=first+row*2;
            idx.insert(idx.end(),{a0,a0+1,a0+3,a0,a0+3,a0+2});
        }
    }
}
