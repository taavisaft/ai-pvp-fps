#include "nature_plants.h"
#include <glm/glm.hpp>
#include <cmath>

namespace {
struct PlantMesh {
    std::vector<float>& v;
    std::vector<unsigned>& idx;
    unsigned vertex(glm::vec3 p,glm::vec3 n,glm::vec3 color) {
        unsigned index=unsigned(v.size()/12);
        v.insert(v.end(),{p.x,p.y,p.z,n.x,n.y,n.z,color.x,color.y,color.z,
                         fmaxf(0,p.y)*.12f,-1,-1});
        return index;
    }
    void leaf(glm::vec3 root,glm::vec3 tip,float width,glm::vec3 color) {
        glm::vec3 axis=tip-root;
        glm::vec3 side=glm::normalize(glm::cross(axis,glm::vec3(.13f,1,.21f)))*width;
        glm::vec3 normal=glm::normalize(glm::cross(side,axis));
        if(normal.y<0) normal=-normal;
        glm::vec3 mid=(root+tip)*.5f;
        unsigned a=vertex(root,normal,color*.75f);
        unsigned b=vertex(mid-side,normal,color);
        unsigned c=vertex(mid+glm::vec3(0,.015f,0),normal,color*1.1f);
        unsigned d=vertex(mid+side,normal,color);
        unsigned e=vertex(tip,normal,color*.88f);
        idx.insert(idx.end(),{a,b,c,a,c,d,b,e,c,c,e,d});
    }
    void stem(glm::vec3 root,glm::vec3 tip,float radius,glm::vec3 color) {
        glm::vec3 axis=glm::normalize(tip-root);
        glm::vec3 side=glm::normalize(glm::cross(axis,glm::vec3(1,.2f,.3f)));
        glm::vec3 other=glm::cross(axis,side);
        unsigned first=unsigned(v.size()/12);
        for(int i=0;i<5;++i) {
            float angle=glm::radians(i*72.0f);
            glm::vec3 n=side*cosf(angle)+other*sinf(angle);
            vertex(root+n*radius,n,color*.8f);
            vertex(tip+n*radius*.4f,n,color);
        }
        for(unsigned i=0;i<5;++i) {
            unsigned a=first+2*i,b=first+2*((i+1)%5);
            idx.insert(idx.end(),{a,b,b+1,a,b+1,a+1});
        }
    }
    void flower(glm::vec3 center,int petals,float radius,glm::vec3 color) {
        for(int i=0;i<petals;++i) {
            float a=glm::radians(360.0f*i/petals);
            glm::vec3 dir(cosf(a),.20f,sinf(a));
            leaf(center+dir*radius*.10f,center+dir*radius,radius*(petals>8 ? .10f : .32f),color);
        }
        unsigned middle=vertex(center+glm::vec3(0,.012f,0),{0,1,0},{.62f,.39f,.035f});
        for(int i=0;i<8;++i) {
            float a=glm::radians(i*45.0f),b=glm::radians((i+1)*45.0f);
            unsigned x=vertex(center+glm::vec3(cosf(a)*radius*.19f,.01f,sinf(a)*radius*.19f),
                              {0,1,0},{.9f,.63f,.065f});
            unsigned y=vertex(center+glm::vec3(cosf(b)*radius*.19f,.01f,sinf(b)*radius*.19f),
                              {0,1,0},{.9f,.63f,.065f});
            idx.insert(idx.end(),{middle,y,x});
        }
    }
};
}

void vegBuildNaturePlant(std::vector<float>& v,std::vector<unsigned>& idx,int type) {
    PlantMesh m{v,idx};
    const glm::vec3 wood(.19f,.13f,.075f),green(.13f,.25f,.065f);
    if(type<3) {
        // Juniper: upright needle sprays. Hazel: broad rounded crown.
        // Wild rose: open woody arch with compound leaves and pink blooms.
        int branches=type==0 ? 11 : 9;
        float height=type==0 ? 1.3f : type==1 ? 1.6f : 1.15f;
        for(int b=0;b<branches;++b) {
            float a=glm::radians(b*137.507f),reach=type==0 ? .28f : .65f;
            glm::vec3 dir(cosf(a),0,sinf(a));
            glm::vec3 root=dir*.05f;
            glm::vec3 tip=dir*(reach*(.65f+.35f*sinf(b*1.7f)*sinf(b*1.7f)))+
                          glm::vec3(0,height*(.55f+.45f*(b%3)/2.0f),0);
            m.stem(root,tip,type==0 ? .013f : .018f,wood);
            for(int j=1;j<=7;++j) {
                glm::vec3 p=glm::mix(root,tip,j/8.0f);
                if(type==0) {
                    for(int spray=0;spray<5;++spray) {
                        float angle=glm::radians(j*53.0f+spray*72.0f);
                        glm::vec3 d(cosf(angle),.8f,sinf(angle));
                        m.leaf(p,p+d*.19f,.008f,{.08f,.18f,.12f});
                    }
                } else {
                    glm::vec3 side(-dir.z,0,dir.x);
                    for(int cluster=0;cluster<(type==1 ? 3 : 1);++cluster)
                    for(float sign:{-1.0f,1.0f}) {
                        glm::vec3 end=p+side*sign*(type==1 ? .25f : .18f)+dir*.08f+glm::vec3(0,.06f,0);
                        glm::vec3 offset=dir*((cluster-1)*.10f)+glm::vec3(0,cluster*.035f,0);
                        m.leaf(p+offset,end+offset,type==1 ? .095f : .043f,green*(.8f+.04f*j));
                    }
                }
            }
            if(type==2 && b%2==0) m.flower(tip,5,.105f,{.62f,.19f,.30f});
        }
        return;
    }
    // A single small clump per flower species, with individually posed stems.
    int stems=type==5 ? 5 : 7;
    for(int b=0;b<stems;++b) {
        float a=glm::radians(b*137.507f);
        glm::vec3 root(cosf(a)*.14f,0,sinf(a)*.14f);
        float h=type==3 ? .48f+.09f*(b%3) : type==4 ? .30f+.07f*(b%3) : .82f+.12f*(b%3);
        glm::vec3 tip=root+glm::vec3(cosf(a)*.07f,h,sinf(a)*.07f);
        m.stem(root,tip,.004f,green);
        for(int j=1;j<5;++j) {
            glm::vec3 p=glm::mix(root,tip,j*.14f);
            float angle=a+j*2.4f;
            m.leaf(p,p+glm::vec3(cosf(angle)*.14f,.035f,sinf(angle)*.14f),
                   type==4 ? .035f : .016f,green);
        }
        if(type==3) m.flower(tip,15,.075f,{.85f,.83f,.71f});
        if(type==4) m.flower(tip,5,.043f,{.95f,.66f,.025f});
        if(type==5) for(int j=0;j<8;++j) {
            float angle=a+j*2.4f;
            glm::vec3 p=tip-glm::vec3(0,j*.034f,0)+glm::vec3(cosf(angle)*.028f,0,sinf(angle)*.028f);
            m.flower(p,4,.032f,{.56f,.10f,.38f});
        }
    }
}
