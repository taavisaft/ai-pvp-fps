#include "nature_plants.h"
#include "nature_plant_mesh.h"

namespace {
using namespace nature;
const glm::vec3 green(.13f,.24f,.065f),wood(.20f,.16f,.11f);

void juniper(MeshBuilder& m) {
    // Several uneven leaders, with branched shoots rather than giant needles.
    for(int leader=0;leader<5;++leader) {
        float a=leader*2.39996f,h=1.05f+.38f*rand(leader+3);
        glm::vec3 root=radial(a)*.07f,top=root+glm::vec3(.08f*cosf(a),h,.08f*sinf(a));
        m.stem(root,top,.008f,wood*.75f);
        for(int branch=0;branch<9;++branch) {
            int key=leader*211+branch*17;
            float t=.10f+branch*.095f;
            glm::vec3 junction=glm::mix(root,top,t),out=radial(a+branch*2.4f);
            glm::vec3 end=junction+out*(.38f*(1-t)+.07f)+glm::vec3(0,.11f,0);
            m.stem(junction,end,.0028f,wood);
            for(int shoot=0;shoot<4;++shoot) {
                glm::vec3 p=glm::mix(junction,end,.18f+shoot*.21f);
                glm::vec3 lateral=radial(a+branch*2.4f+(shoot%2 ? .9f : -.9f));
                glm::vec3 tip=p+lateral*.08f+glm::vec3(0,.065f,0);
                m.stem(p,tip,.001f,green*.8f);
                // Crossed photographic needle sprays fill the twig volume at
                // distance; short geometric needles retain the close silhouette.
                m.spray(p,tip,.037f,.45f); m.spray(p,tip,.037f,2.02f);
                for(int node=0;node<4;++node) for(int needle=0;needle<3;++needle) {
                    glm::vec3 start=glm::mix(p,tip,node/4.0f);
                    glm::vec3 d=radial(node*1.1f+needle*2.0944f+key)+glm::vec3(0,.8f,0);
                    m.leaf(start,start+glm::normalize(d)*(.012f+.005f*rand(key+node)),.0011f,
                           {.17f,.27f,.22f},2);
                }
            }
        }
    }
}

void hazel(MeshBuilder& m) {
    // Young coppiced hazel: multiple stems and alternate leaves on side shoots.
    for(int cane=0;cane<7;++cane) {
        float a=cane*2.39996f,h=1.35f+.38f*rand(cane+91);
        glm::vec3 root=radial(a)*.08f,tip=radial(a)*.38f+glm::vec3(0,h,0);
        auto curve=[&](float t) {
            return root+radial(a)*(.38f*t*t)+radial(a+.8f)*(.10f*sinf(t*3.1f))+
                   glm::vec3(0,h*t,0);
        };
        for(int segment=0;segment<5;++segment)
            m.stem(curve(segment/5.0f),curve((segment+1)/5.0f),.016f*(1-.12f*segment),wood);
        for(int shoot=0;shoot<7;++shoot) {
            int key=cane*43+shoot*19;
            glm::vec3 p=curve(.25f+shoot*.10f);
            glm::vec3 out=radial(a+(shoot%2 ? .9f : -.8f));
            glm::vec3 end=p+out*(.35f+.16f*rand(key))+glm::vec3(0,.13f,0);
            m.stem(p,end,.004f,wood);
            for(int leaf=0;leaf<6;++leaf) {
                glm::vec3 junction=glm::mix(p,end,.12f+leaf*.17f);
                glm::vec3 direction=radial(a+(leaf%2 ? 1.3f : -1.3f)+.4f*rand(key+leaf));
                glm::vec3 petiole=junction+direction*.018f;
                m.stem(junction,petiole,.0008f,green);
                m.leaf(petiole,petiole+direction*(.085f+.03f*rand(key+leaf))+glm::vec3(0,.02f,0),
                       .038f+.010f*rand(key+leaf+11),green*(.80f+.35f*rand(key+leaf+7)),1,
                       (rand(key+leaf+27)-.5f)*1.3f);
            }
        }
    }
}

void compound(MeshBuilder& m,glm::vec3 root,glm::vec3 tip,int key) {
    m.stem(root,tip,.001f,green);
    glm::vec3 axis=glm::normalize(tip-root),side=glm::normalize(glm::cross(axis,glm::vec3(0,1,0)));
    for(int pair=0;pair<3;++pair) for(float sign:{-1.0f,1.0f}) {
        glm::vec3 p=glm::mix(root,tip,.2f+pair*.24f);
        glm::vec3 end=p+side*sign*.036f+axis*.017f+glm::vec3(0,.008f,0);
        m.leaf(p,end,.012f,green*(.8f+.25f*rand(key+pair)),1,sign*.35f);
    }
    m.leaf(tip,tip+axis*.046f,.014f,green,1);
}

void rose(MeshBuilder& m) {
    for(int cane=0;cane<7;++cane) {
        int key=cane*73;
        glm::vec3 out=radial(cane*2.39996f),last=out*.04f;
        // Curved canes rise, then arch outward; leaves attach to their nodes.
        for(int node=1;node<=8;++node) {
            float t=node/8.0f;
            glm::vec3 p=out*(.68f*t*t)+glm::vec3(0,(1.0f+.25f*rand(key))*sinf(t*1.8f),0);
            m.stem(last,p,.011f*(1-.65f*t),wood);
            glm::vec3 leafOut=radial(cane*2.39996f+(node%2 ? .9f : -.9f));
            compound(m,p,p+leafOut*.135f+glm::vec3(0,.02f,0),key+node);
            if(node%2==0) m.stem(p,p-out*.013f-glm::vec3(0,.012f,0),.003f,wood);
            last=p;
        }
        if(cane%2==0) {
            glm::vec3 flower=last+glm::vec3(0,.05f,0);
            m.stem(last,flower,.002f,green);
            m.bloom(flower,out*.55f+glm::vec3(0,.8f,0),5,.026f,{.74f,.43f,.49f},.18f,key);
        }
    }
}

void flowers(MeshBuilder& m,int type) {
    int count=type==5 ? 4 : 7;
    for(int stem=0;stem<count;++stem) {
        int key=stem*37+type*121;
        float a=stem*2.39996f;
        glm::vec3 root=radial(a)*(.05f+.13f*rand(key));
        float height=type==3 ? .55f+.22f*rand(key+1) : type==4 ? .38f+.22f*rand(key+1) : .95f+.30f*rand(key+1);
        glm::vec3 last=root;
        for(int node=1;node<=6;++node) {
            float t=node/6.0f;
            glm::vec3 p=root+glm::vec3(0,height*t,0)+radial(a)*(.05f*t*t);
            m.stem(last,p,type==5 ? .003f : .002f,green*(type==5 ? .85f : 1));
            if(node<5) {
                glm::vec3 d=radial(a+node*2.4f);
                if(type==4) for(int lobe=0;lobe<3;++lobe) {
                    glm::vec3 end=p+radial(a+node*2.4f+(lobe-1)*.55f)*.075f+glm::vec3(0,.018f,0);
                    m.leaf(p,end,.010f,green,1);
                } else m.leaf(p,p+d*(type==5 ? .11f : .065f)+glm::vec3(0,.015f,0),
                             type==5 ? .010f : .015f,green,type==3 ? 1 : 0,.35f*sinf(node));
            }
            last=p;
        }
        if(type==3) m.bloom(last,glm::vec3(.25f*cosf(a),1,.25f*sinf(a)),25,.028f,{.88f,.86f,.78f},-.10f,key,true);
        if(type==4) {
            m.bloom(last,{.15f,1,.1f},5,.014f,{.82f,.56f,.025f},.38f,key);
            // A forked flowering side stalk and its own five-petalled cup.
            glm::vec3 p=glm::mix(root,last,.68f),tip=last+radial(a)*.07f-glm::vec3(0,.08f,0);
            m.stem(p,tip,.0014f,green);
            m.bloom(tip,{.3f,1,-.2f},5,.012f,{.86f,.59f,.022f},.4f,key+9);
        }
        if(type==5) {
            glm::vec3 spike=last+glm::vec3(0,.23f,0); m.stem(last,spike,.0015f,green);
            for(int flower=0;flower<11;++flower) {
                glm::vec3 d=radial(a+flower*2.4f),p=last+glm::vec3(0,flower*.020f,0);
                glm::vec3 end=p+d*(.035f*(1-flower/16.0f));
                m.stem(p,end,.0007f,green);
                m.bloom(end,d+glm::vec3(0,.3f,0),4,.011f,{.58f,.18f,.38f},.08f,key+flower);
            }
        }
    }
}
}

void vegBuildNaturePlant(std::vector<float>& v,std::vector<unsigned>& idx,int type) {
    nature::MeshBuilder m{v,idx};
    if(type==0) juniper(m);
    else if(type==1) hazel(m);
    else if(type==2) rose(m);
    else flowers(m,type);
}
