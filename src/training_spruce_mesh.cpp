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
        glm::vec3 side=trunk ? glm::vec3(1,0,0) : glm::normalize(glm::cross(axis,fabsf(axis.y)>.98f ? glm::vec3(1,0,0) : glm::vec3(0,1,0)));
        glm::vec3 other=trunk ? glm::vec3(0,0,1) : glm::cross(axis,side);
        const int sides=trunk ? TREE_TRUNK_SIDES : 3;
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
    // A continuous leader supports the crown above the collision trunk. Its
    // lower end overlaps the trunk even after the shader lifts mature crowns.
    wood({0,.65f,0},{0,.96f,0},.004f,.0004f,false);
    auto spray=[&](glm::vec3 root,glm::vec3 axis,float length,float width,float shade,int key,bool volume=false) {
        glm::vec3 along=glm::normalize(axis);
        glm::vec3 side=glm::normalize(glm::cross(along,
            fabsf(along.y)>.98f ? glm::vec3(1,0,0) : glm::vec3(0,1,0)));
        glm::vec3 out=glm::normalize(glm::vec3(root.x,.12f,root.z));
        // The compound bough's stem is centered in the cutout. Keep card
        // proportions close to the photo so needle groups stay at tree scale.
        const float cardWidth=fminf(width,length*.58f);
        for(int q=0;q<2;++q) {
            float roll=(q ? 1.20f : -.25f)+(rand(key)-.5f)*.35f;
            glm::vec3 across=side*cosf(roll)+glm::cross(along,side)*sinf(roll);
            unsigned rows[3][2];
            const int rowCount=low ? 2 : 3;
            for(int r=0;r<rowCount;++r) for(int edge=0;edge<2;++edge) {
                float t=float(r)/(rowCount-1);
                glm::vec3 p=root+along*(length*t)+across*((edge-.5f)*cardWidth);
                p.y-=length*.08f*sinf(t*3.14159265f);
                glm::vec3 n=glm::normalize(out+glm::vec3(0,.65f,0)+across*((edge-.5f)*.25f));
                rows[r][edge]=vertex(p,n,glm::vec3(shade*(.88f+.12f*t)),0,{float(edge),.01f+t*.98f});
            }
            for(int r=0;r<rowCount-1;++r)
                idx.insert(idx.end(),{rows[r][0],rows[r][1],rows[r+1][1],rows[r][0],rows[r+1][1],rows[r+1][0]});
        }
    };
    // Bough -> elbow -> tip, with lateral shoots rooted on those exact segments.
    // Both LODs retain the entire load-bearing skeleton; only needle shoots thin.
    for(int k=0;k<profile.tiers;++k) {
        float t=float(k)/(profile.tiers-1);
        float y=profile.crownBase+(.94f-profile.crownBase)*t
               +(rand(k*131+3)-.5f)*.035f*(1-t);
        int branches=profile.branches+(rand(k*131+11)>.65f ? 1 : 0)
                     -(rand(k*131+23)<.15f ? 1 : 0);
        for(int j=0;j<branches;++j) {
            int key=k*37+j;
            if(k>1 && k<profile.tiers-2 && rand(key+119)<.11f) continue;
            float a=k*2.39996f+j*6.2831853f/branches+(rand(key+5)-.5f)*.9f;
            glm::vec3 dir(cosf(a),0,sinf(a)),side(-sinf(a),0,cosf(a));
            float length=(profile.radius*powf(1-t,profile.taper)+.016f)*(.68f+.38f*rand(key+17));
            glm::vec3 root(0,y+(rand(key+45)-.5f)*.06f*(1-t),0);
            float sag=length*(.10f+.17f*rand(key+71))*(1-t)*profile.droop;
            glm::vec3 elbow=root+dir*(length*.48f)+glm::vec3(0,-sag,0);
            glm::vec3 tip=root+dir*(length*.83f)+glm::vec3(0,-sag*.35f+length*.06f,0);
            float r0=.0036f*(1-t)+.0005f,r1=.0015f*(1-t)+.00025f;
            wood(root,elbow,r0,r1,false);
            wood(elbow,tip,r1,.00025f,false);
            float shade=(.62f+.15f*rand(key+33))*profile.tint;
            // Keep a broad crossed needle mass in BOTH detail levels. Removing
            // this inner bough made the distant crown a see-through skeleton.
            spray(elbow,tip-elbow,length*.62f,length*1.10f,shade*.94f,key+201,true);
            spray(tip,tip-elbow,length*.28f,length*(low ? .55f : .30f),shade,key);
            // A connected inner shoot fills the trunk-side gap between whorls.
            glm::vec3 inner=glm::mix(root,elbow,.16f);
            wood(root,inner,r0*.70f,r1,false);
            spray(inner,elbow-root,length*.86f,length*.95f,shade*.86f,key+401,true);
            int twigs=3;
            for(int b=0;b<twigs;++b) {
                float u=.24f+.64f*(b+.35f)/twigs;
                glm::vec3 start=u<.48f ? glm::mix(root,elbow,u/.48f)
                    : glm::mix(elbow,tip,(u-.48f)/.52f);
                float sign=b%2 ? 1.0f : -1.0f;
                glm::vec3 axis=glm::normalize(dir*.42f+side*(sign*.70f)
                    +glm::vec3(0,-(.25f+profile.droop*.22f)*(1-t),0));
                float shootLength=length*(.52f-.24f*u);
                glm::vec3 end=start+axis*(shootLength*.32f);
                wood(start,end,r1*.65f,.00015f,false);
                spray(end,axis,shootLength*.88f,shootLength*(low ? 1.55f : 1.10f),shade+.025f,key+b+9);
            }
        }
    }
    // The terminal shoot grows from the leader, not above a missing trunk.
    spray({0,.96f,0},{0,1,0},.055f,.030f,.80f,991);
}
