#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cmath>

namespace nature {
inline float rand(int k) { float f=sinf(k*127.1f+31.7f)*43758.5453f; return f-floorf(f); }
inline glm::vec3 radial(float angle) { return {cosf(angle),0,sinf(angle)}; }
struct MeshBuilder {
    std::vector<float>& v; std::vector<unsigned>& idx;
    unsigned vertex(glm::vec3 p,glm::vec3 n,glm::vec3 c,glm::vec2 uv={-1,-1}) {
        unsigned i=unsigned(v.size()/12);
        v.insert(v.end(),{p.x,p.y,p.z,n.x,n.y,n.z,c.x,c.y,c.z,
                         fmaxf(0,p.y)*.045f,uv.x,uv.y});
        return i;
    }
    void stem(glm::vec3 root,glm::vec3 tip,float radius,glm::vec3 color) {
        glm::vec3 axis=glm::normalize(tip-root);
        glm::vec3 side=glm::normalize(glm::cross(axis,fabsf(axis.y)>.95f ? glm::vec3(1,0,0) : glm::vec3(0,1,0)));
        glm::vec3 other=glm::cross(axis,side);
        unsigned first=unsigned(v.size()/12);
        for(int i=0;i<5;++i) {
            float a=glm::radians(i*72.0f); glm::vec3 n=side*cosf(a)+other*sinf(a);
            vertex(root+n*radius,n,color*.8f); vertex(tip+n*radius*.65f,n,color);
        }
        for(unsigned i=0;i<5;++i) {
            unsigned a=first+2*i,b=first+2*((i+1)%5);
            idx.insert(idx.end(),{a,b,b+1,a,b+1,a+1});
        }
    }
    void spray(glm::vec3 root,glm::vec3 tip,float width,float roll) {
        glm::vec3 axis=glm::normalize(tip-root);
        glm::vec3 side=glm::normalize(glm::cross(axis,glm::vec3(0,1,0)));
        glm::vec3 normal=glm::normalize(glm::cross(side,axis));
        side=side*cosf(roll)+normal*sinf(roll); normal=glm::normalize(glm::cross(side,axis));
        unsigned a=vertex(root-side*width,normal,glm::vec3(.95f),{0,0});
        unsigned b=vertex(root+side*width,normal,glm::vec3(.95f),{1,0});
        unsigned c=vertex(tip-side*width,normal,glm::vec3(.95f),{0,1});
        unsigned d=vertex(tip+side*width,normal,glm::vec3(.95f),{1,1});
        idx.insert(idx.end(),{a,b,d,a,d,c});
    }
    void leaf(glm::vec3 root,glm::vec3 tip,float width,glm::vec3 color,int shape=0,float roll=0) {
        glm::vec3 axis=glm::normalize(tip-root);
        glm::vec3 side=glm::normalize(glm::cross(axis,fabsf(axis.y)>.96f ? glm::vec3(1,0,0) : glm::vec3(0,1,0)));
        glm::vec3 normal=glm::normalize(glm::cross(side,axis));
        if(normal.y<0) { side=-side; normal=-normal; }
        side=side*cosf(roll)+normal*sinf(roll); normal=glm::normalize(glm::cross(side,axis));
        if(shape==2) {
            glm::vec3 mid=glm::mix(root,tip,.20f);
            unsigned a=vertex(root,normal,color,{-2.2f,0});
            unsigned b=vertex(mid-side*width,normal,color*.85f,{-2.0f,.2f});
            unsigned c=vertex(tip,normal,color,{-2.2f,1});
            unsigned d=vertex(mid+side*width,normal,color*.85f,{-2.4f,.2f});
            idx.insert(idx.end(),{a,b,c,a,c,d});
            return;
        }
        unsigned first=unsigned(v.size()/12);
        int rows=4;
        for(int r=0;r<=rows;++r) for(int s=0;s<3;++s) {
            float t=float(r)/rows,q=float(s-1);
            float outline=shape==2 ? 1-t : powf(sinf(t*3.14159265f),shape==1 ? .48f : .8f);
            if(shape==1 && r>0 && r<rows) outline*=r%2 ? 1.0f : .92f;
            glm::vec3 p=glm::mix(root,tip,t)+side*q*width*outline+
                        normal*(width*.20f*sinf(t*3.14159265f)*(1-q*q)-width*.18f*t*t);
            glm::vec3 n=glm::normalize(normal-side*q*.35f+axis*t*.12f);
            glm::vec3 c=color*(.82f+.18f*t)*(q==0 ? 1.08f : 1);
            vertex(p,n,c,{-2.0f-(q+1)*.2f,t});
        }
        for(int r=0;r<rows;++r) for(unsigned s=0;s<2;++s) {
            unsigned a=first+r*3+s;
            idx.insert(idx.end(),{a,a+3,a+4,a,a+4,a+1});
        }
    }
    void bloom(glm::vec3 center,glm::vec3 normal,int petals,float radius,
               glm::vec3 color,float cup,int key,bool daisy=false) {
        normal=glm::normalize(normal);
        glm::vec3 u=glm::normalize(glm::cross(normal,fabsf(normal.y)>.95f ? glm::vec3(1,0,0) : glm::vec3(0,1,0)));
        glm::vec3 w=glm::cross(normal,u);
        for(int petal=0;petal<petals;++petal) {
            float a=(petal+.12f*(rand(key+petal)-.5f))*6.2831853f/petals;
            glm::vec3 out=u*cosf(a)+w*sinf(a),side=-u*sinf(a)+w*cosf(a);
            unsigned first=unsigned(v.size()/12);
            for(int r=0;r<4;++r) for(int s=0;s<3;++s) {
                float t=r/3.0f,q=float(s-1),reach=radius*(.94f+.12f*rand(key+petal+20));
                float width=daisy ? .095f : .57f;
                float edge=daisy ? powf(sinf(t*3.14159265f),.3f) : sinf(t*3.14159265f);
                glm::vec3 p=center+out*(reach*(.14f+.86f*t))+side*(q*reach*width*edge)+
                            normal*(reach*(cup*t*t+.07f*q*q*sinf(t*3.14159265f)));
                glm::vec3 n=glm::normalize(normal-out*(cup*2*t)-side*q*.16f);
                vertex(p,n,color*(.84f+.16f*t));
            }
            for(unsigned r=0;r<3;++r) for(unsigned s=0;s<2;++s) {
                unsigned a=first+r*3+s;
                idx.insert(idx.end(),{a,a+3,a+4,a,a+4,a+1});
            }
        }
        float disk=radius*(daisy ? .32f : .19f);
        unsigned mid=vertex(center+normal*disk*.28f,normal,{.55f,.34f,.025f});
        for(int i=0;i<12;++i) {
            float a=i*6.2831853f/12,b=(i+1)*6.2831853f/12;
            unsigned x=vertex(center+(u*cosf(a)+w*sinf(a))*disk,normal,{.78f,.52f,.045f});
            unsigned y=vertex(center+(u*cosf(b)+w*sinf(b))*disk,normal,{.78f,.52f,.045f});
            idx.insert(idx.end(),{mid,x,y});
        }
    }
};
}
