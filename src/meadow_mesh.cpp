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
