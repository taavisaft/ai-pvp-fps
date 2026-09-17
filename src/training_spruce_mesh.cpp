#include "training_spruce.h"
#include "tree_collision.h"
#include <cmath>

void vegBuildTrainingSpruce(std::vector<float>& v, std::vector<unsigned>& idx,int type,bool low) {
    const auto& profile=TRAINING_SPRUCE_PROFILES[type];
    auto rand=[type](int k) { k+=type*1021; float f=sinf(k*127.1f+17.3f)*43758.5453f; return f-floorf(f); };
    auto vertex=[&](glm::vec3 p,glm::vec3 n,glm::vec3 color,float flex,glm::vec2 uv) {
        unsigned i=(unsigned)v.size()/12;
        v.insert(v.end(),{p.x,p.y,p.z,n.x,n.y,n.z,color.x,color.y,color.z,flex,uv.x,uv.y});
        return i;
    };
    auto wood=[&](glm::vec3 root,glm::vec3 tip,float r0,float r1,bool trunk) {
        glm::vec3 axis=glm::normalize(tip-root);
        glm::vec3 side=trunk ? glm::vec3(1,0,0) : glm::normalize(glm::cross(axis,glm::vec3(0,1,0)));
        glm::vec3 other=trunk ? glm::vec3(0,0,1) : glm::cross(axis,side);
        const int sides=trunk ? TREE_TRUNK_SIDES : 4;
        unsigned base[6],end[6];
        for(int j=0;j<sides;++j) {
            float a=j*6.2831853f/sides;
            glm::vec3 n=side*cosf(a)+other*sinf(a);
            glm::vec3 color(.255f,.225f,.18f);
            base[j]=vertex(root+n*r0,n,color,0,{-1,trunk ? -1 : .1f});
            end[j]=vertex(tip+n*r1,n,color,trunk ? 0 : .025f,{-1,trunk ? -1 : .1f});
        }
        for(int j=0;j<sides;++j) {
            int k=(j+1)%sides;
            idx.insert(idx.end(),{base[j],base[k],end[k],base[j],end[k],end[j]});
        }
        if(trunk) {
            unsigned lo=vertex(root,-axis,glm::vec3(.2f),0,{-1,-1});
            unsigned hi=vertex(tip,axis,glm::vec3(.2f),0,{-1,-1});
            for(int j=0;j<sides;++j) { int k=(j+1)%sides; idx.insert(idx.end(),{lo,base[k],base[j],hi,end[j],end[k]}); }
        }
    };
    // Exact original trunk envelope: bark detail does not alter gameplay collision.
    wood({0,0,0},{0,TREE_TRUNK_HEIGHT,0},TREE_TRUNK_BASE,TREE_TRUNK_TOP,true);
    auto spray=[&](glm::vec3 root,glm::vec3 axis,float length,float width,float shade,int key) {
        glm::vec3 along=glm::normalize(axis);
        glm::vec3 side=glm::normalize(glm::cross(along,glm::vec3(.07f,1,.02f)));
        glm::vec3 out=glm::normalize(glm::vec3(root.x,.12f,root.z));
        // Broad, almost horizontal main boughs; secondary sprays hang beneath.
        for(int q=0;q<2;++q) {
            bool hanging=along.y<-.6f;
            float roll=(q ? -.35f : .24f)+(rand(key)-.5f)*.22f;
            if(hanging) roll=(q ? -.70f : .55f)+(rand(key)-.5f)*.3f;
            glm::vec3 across=side*cosf(roll)+glm::cross(along,side)*sinf(roll);
            unsigned rows[3][2];
            for(int r=0;r<3;++r) for(int s=0;s<2;++s) {
                float t=r*.5f;
                glm::vec3 p=root+along*(length*t)+across*((s-.5f)*width);
                p.y-=length*.12f*sinf(t*3.14159265f);
                glm::vec3 n=glm::normalize(out+glm::vec3(0,.75f,0)+across*((s-.5f)*.22f));
                float tint=shade*(.83f+.17f*t);
                rows[r][s]=vertex(p,n,glm::vec3(tint),.025f+.10f*t,{float(s),t});
            }
            for(int r=0;r<2;++r)
                idx.insert(idx.end(),{rows[r][0],rows[r][1],rows[r+1][1],rows[r][0],rows[r+1][1],rows[r+1][0]});
        }
    };
    // Each tier is built from one broad bough and hanging secondary sprays.
    // The outer tips lift slightly while the inner branch sags under its foliage.
    for(int k=0;k<profile.tiers;++k) {
        float t=float(k)/(profile.tiers-1), y=profile.crownBase+(.96f-profile.crownBase)*t;
        int branches=profile.branches+(k%3==0);
        for(int j=0;j<branches;++j) {
            int key=k*37+j;
            float a=k*2.39996f+j*6.2831853f/branches+(rand(key+5)-.5f)*.45f;
            glm::vec3 dir(cosf(a),0,sinf(a)),side(-sinf(a),0,cosf(a));
            float length=(profile.radius*powf(1-t,profile.taper)+.016f)*(.83f+.20f*rand(key+17));
            glm::vec3 root(0,y+(rand(key+45)-.5f)*.027f,0);
            float sag=length*(.22f+.12f*rand(key+71))*(1-t)*profile.droop;
            auto spine=[&](float u) {
                return root+dir*(length*u)+glm::vec3(0,-sag*sinf(u*3.14159265f)+length*.10f*u,0);
            };
            glm::vec3 elbow=spine(.58f),tip=spine(1);
            if(!low) {
                wood(root,elbow,.0028f*(1-t)+.0005f,.0012f*(1-t)+.0003f,false);
                wood(elbow,tip,.0012f*(1-t)+.0003f,.0003f,false);
            }
            float shade=(.62f+.15f*rand(key+33))*profile.tint;
            spray(spine(.12f),tip-spine(.12f),length*.94f,length*(low ? .95f : .72f),shade,key);
            // Inner foliage joins each tier into a bough, hiding bare spoke roots.
            spray(spine(.08f),dir+glm::vec3(0,-.28f,0),length*.68f,length*.62f,shade*.90f,key+201);
            // Fine hanging foliage gives the tier a ragged curtain underneath.
            for(int b=0;b<(low ? 0 : profile.twigs);++b) {
                float u=.28f+b*(.51f/(profile.twigs-1)), sign=b%2 ? 1.0f : -1.0f;
                float sweep=(1-u)*length*.40f;
                glm::vec3 start=spine(u)+side*(sign*sweep*.25f);
                glm::vec3 twig=dir*.20f+side*(sign*.55f)+glm::vec3(0,-.85f,0);
                float twigLen=length*(.30f+.12f*rand(key+b+83));
                spray(start,twig,twigLen,length*(.25f-.07f*u),shade+.035f,key+b+9);
            }
        }
    }
    // A few bare lower limbs and a narrow upright leader.
    for(int j=0;j<(low ? 0 : type==3 ? 12 : 5);++j) {
        float a=j*2.39996f;
        wood({0,.13f+j*.023f,0},{cosf(a)*.085f,.12f+j*.023f,sinf(a)*.085f},.0018f,.0003f,false);
    }
    spray({0,.94f,0},{.02f,1,.01f},.08f,.025f,.80f,991);
}
