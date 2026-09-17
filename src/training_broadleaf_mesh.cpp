#include "training_woodland.h"
#include "tree_collision.h"
#include <cmath>

// Species 0 ash, 1 birch, 2 oak. The collision trunk is shared; branching and
// leaf-cluster silhouettes are cosmetic. Each atlas column is a different leaf.
void vegBuildBroadleaf(std::vector<float>& v,std::vector<unsigned>& idx,int species,bool low) {
    auto rand=[species](int k) { float x=sinf((k+species*719)*127.1f+19.7f)*43758.5453f; return x-floorf(x); };
    glm::vec3 bark=species==1 ? glm::vec3(.59f,.61f,.55f) : species==0 ? glm::vec3(.32f,.31f,.26f) : glm::vec3(.27f,.23f,.18f);
    auto vertex=[&](glm::vec3 p,glm::vec3 n,glm::vec3 col,float flex,glm::vec2 uv) {
        unsigned i=unsigned(v.size()/12);
        v.insert(v.end(),{p.x,p.y,p.z,n.x,n.y,n.z,col.x,col.y,col.z,flex,uv.x,uv.y});
        return i;
    };
    auto wood=[&](glm::vec3 root,glm::vec3 tip,float r0,float r1,bool trunk) {
        glm::vec3 axis=glm::normalize(tip-root);
        glm::vec3 side=trunk ? glm::vec3(1,0,0) : glm::normalize(glm::cross(axis,glm::vec3(0,1,0)));
        glm::vec3 other=trunk ? glm::vec3(0,0,1) : glm::cross(axis,side);
        unsigned base[6],end[6]; int sides=trunk ? TREE_TRUNK_SIDES : 5;
        for(int j=0;j<sides;++j) {
            float a=j*6.2831853f/sides;
            glm::vec3 n=side*cosf(a)+other*sinf(a);
            base[j]=vertex(root+n*r0,n,bark,0,{-1,trunk ? -1 : .1f});
            end[j]=vertex(tip+n*r1,n,bark,trunk ? 0 : .02f,{-1,trunk ? -1 : .1f});
        }
        for(int j=0;j<sides;++j) {
            int k=(j+1)%sides;
            idx.insert(idx.end(),{base[j],base[k],end[k],base[j],end[k],end[j]});
        }
        if(trunk) {
            unsigned lo=vertex(root,-axis,bark,0,{-1,-1}),hi=vertex(tip,axis,bark,0,{-1,-1});
            for(int j=0;j<sides;++j) { int k=(j+1)%sides; idx.insert(idx.end(),{lo,base[k],base[j],hi,end[j],end[k]}); }
        }
    };
    wood({0,0,0},{0,TREE_TRUNK_HEIGHT,0},TREE_TRUNK_BASE,TREE_TRUNK_TOP,true);
    auto cluster=[&](glm::vec3 center,float size,int key) {
        // Many small, bent sprays form an irregular volume; no single giant plane.
        int sprays=low ? (species==1 ? 7 : 9) : (species==1 ? 20 : 28);
        float cover=low ? 1.75f : 1.0f;
        for(int b=0;b<sprays;++b) {
            float a=b*2.399963f+rand(key)*6.28f, c=cosf(a),s=sinf(a);
            float y=rand(key+b+21)*2-1;
            float spread=.18f+.65f*rand(key+b+71);
            glm::vec3 offset(c*size*spread,y*size*.62f,s*size*spread);
            // Independent branchlet directions and roll avoid radial fern bouquets.
            float heading=rand(key+b+151)*6.2831853f;
            float lift=(rand(key+b+181)-.48f)*1.5f;
            if(species==1) lift-=.25f;
            glm::vec3 up=glm::normalize(glm::vec3(cosf(heading),lift,sinf(heading)));
            glm::vec3 across=glm::normalize(glm::cross(up,glm::vec3(0,1,0)));
            float roll=(rand(key+b+211)-.5f)*2.8f;
            glm::vec3 side=across*cosf(roll)+glm::cross(up,across)*sinf(roll);
            float w=size*cover*(.36f+.17f*rand(key+b+31)),h=size*cover*(.46f+.24f*rand(key+b+41));
            glm::vec3 root=center+offset-up*h*.45f;
            unsigned rows[3][2];
            for(int r=0;r<3;++r) for(int edge=0;edge<2;++edge) {
                float t=r*.5f;
                glm::vec3 p=root+up*h*t+side*((edge-.5f)*w);
                p.y-=size*.10f*sinf(t*3.14159265f);
                // Rounded lobe normals give coherent crown lighting across card seams.
                glm::vec3 n=glm::normalize(offset/size+glm::vec3(0,.35f,0)
                                          +side*((edge-.5f)*.35f)+up*((t-.5f)*.3f));
                float shade=.56f+.20f*rand(key+b+51);
                glm::vec3 tint=glm::vec3(shade);
                if(species==0) tint*=glm::vec3(1.02f,.94f,.94f);
                if(species==2) tint*=glm::vec3(.91f,.97f,.88f);
                float u=(species+.012f+edge*.976f)/3.0f;
                rows[r][edge]=vertex(p,n,tint,.035f+t*.12f,{u,.015f+t*.97f});
            }
            for(int r=0;r<2;++r) idx.insert(idx.end(),{rows[r][0],rows[r][1],rows[r+1][1],rows[r][0],rows[r+1][1],rows[r+1][0]});
        }
    };
    int limbs=species==1 ? 9 : 10;
    float radius=species==1 ? .20f : species==0 ? .30f : .37f;
    float crownSize=species==1 ? .135f : .185f;
    for(int b=0;b<limbs;++b) {
        float t=float(b)/(limbs-1),a=b*2.399963f+rand(b+1)*.8f;
        glm::vec3 dir(cosf(a),0,sinf(a)),side(-sinf(a),0,cosf(a));
        float spread=radius*(.58f+.42f*sinf((t*.85f+.12f)*3.14159265f))*(.80f+.26f*rand(b+81));
        float rise=(rand(b+91)-.5f)*.11f;
        glm::vec3 root(0,.11f+t*.50f,0);
        glm::vec3 elbow=dir*spread*.47f+side*((rand(b+101)-.5f)*.045f)+glm::vec3(0,.20f+t*.58f,0);
        glm::vec3 tip=dir*spread+glm::vec3(0,.25f+t*.62f+rise,0);
        wood(root,elbow,.009f*(1-t)+.003f,.005f,false);
        if(!low) wood(elbow,tip,.005f,.0012f,false);
        for(int j=0;j<3;++j) {
            float sign=j==0 ? -1.0f : 1.0f;
            float jitter=rand(b*31+j+301);
            glm::vec3 start=glm::mix(elbow,tip,.18f+j*.35f);
            glm::vec3 end=start+side*(sign*crownSize*(.20f+.38f*jitter))+dir*(crownSize*.12f)+glm::vec3(0,(jitter-.3f)*.09f,0);
            if(!low) wood(start,end,.0025f,.0005f,false);
            cluster(end,crownSize*(.92f+.27f*rand(b*17+j+61)),b*113+j*19);
        }
    }
    cluster({0,.90f,0},crownSize*.8f,1007);
    cluster({0,.62f,0},crownSize*1.15f,1013);
    for(int b=0;b<6;++b) {
        float a=b*1.0471976f+rand(b+401)*.7f;
        float reach=radius*(.62f+.30f*rand(b+411));
        cluster({cosf(a)*reach,.19f+.07f*rand(b+421),sinf(a)*reach},crownSize*(.85f+.25f*rand(b+431)),2003+b*29);
    }
}
