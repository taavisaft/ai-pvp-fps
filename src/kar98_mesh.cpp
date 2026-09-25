#include "kar98_mesh.h"
#include "mesh.h"
#include <vector>
#include <cmath>

namespace {
struct Builder {
    std::vector<float> vertices;
    std::vector<unsigned> indices;

    void vertex(float x,float y,float z,float nx,float ny,float nz,
                float r,float g,float b,float spec) {
        vertices.insert(vertices.end(),{x,y,z,nx,ny,nz,r,g,b,spec});
    }
    void quad(const float p[4][3],float nx,float ny,float nz,
              float r,float g,float b,float spec) {
        unsigned base=(unsigned)(vertices.size()/10);
        for(int i=0;i<4;++i) vertex(p[i][0],p[i][1],p[i][2],nx,ny,nz,r,g,b,spec);
        indices.insert(indices.end(),{base,base+1,base+2,base,base+2,base+3});
    }
    void box(float x0,float y0,float z0,float x1,float y1,float z1,
             float r,float g,float b,float spec) {
        const float front[4][3]={{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}};
        const float back[4][3] ={{x1,y0,z0},{x0,y0,z0},{x0,y1,z0},{x1,y1,z0}};
        const float left[4][3] ={{x0,y0,z0},{x0,y0,z1},{x0,y1,z1},{x0,y1,z0}};
        const float right[4][3]={{x1,y0,z1},{x1,y0,z0},{x1,y1,z0},{x1,y1,z1}};
        const float top[4][3]  ={{x0,y1,z1},{x1,y1,z1},{x1,y1,z0},{x0,y1,z0}};
        const float bottom[4][3]={{x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1}};
        quad(front,0,0,1,r,g,b,spec); quad(back,0,0,-1,r,g,b,spec);
        quad(left,-1,0,0,r,g,b,spec); quad(right,1,0,0,r,g,b,spec);
        quad(top,0,1,0,r,g,b,spec); quad(bottom,0,-1,0,r,g,b,spec);
    }
    void tube(float cx,float cy,float z0,float z1,float radius,
              float r,float g,float b,float spec) {
        constexpr int S=10;
        constexpr float TAU=6.28318530718f;
        for(int i=0;i<S;++i) {
            float a=TAU*i/S, n=TAU*(i+1)/S;
            float ca=cosf(a),sa=sinf(a),cn=cosf(n),sn=sinf(n);
            unsigned base=(unsigned)(vertices.size()/10);
            vertex(cx+radius*ca,cy+radius*sa,z0,ca,sa,0,r,g,b,spec);
            vertex(cx+radius*cn,cy+radius*sn,z0,cn,sn,0,r,g,b,spec);
            vertex(cx+radius*cn,cy+radius*sn,z1,cn,sn,0,r,g,b,spec);
            vertex(cx+radius*ca,cy+radius*sa,z1,ca,sa,0,r,g,b,spec);
            indices.insert(indices.end(),{base,base+1,base+2,base,base+2,base+3});
            const float rear[4][3]={{cx,cy,z0},{cx+radius*cn,cy+radius*sn,z0},
                                    {cx+radius*ca,cy+radius*sa,z0},{cx,cy,z0}};
            const float fore[4][3]={{cx,cy,z1},{cx+radius*ca,cy+radius*sa,z1},
                                    {cx+radius*cn,cy+radius*sn,z1},{cx,cy,z1}};
            quad(rear,0,0,-1,r,g,b,spec); quad(fore,0,0,1,r,g,b,spec);
        }
    }
};
}

bool buildKar98Mesh(Mesh& mesh) {
    Builder b;
    b.vertices.reserve(5000); b.indices.reserve(2000);
    // Walnut stock and handguard, narrow receiver, long blued barrel.
    b.box(-.055f,-.12f,-.52f,.055f,.015f,-.27f,.27f,.13f,.065f,.08f);
    b.box(-.047f,-.095f,-.28f,.047f,-.015f,.08f,.32f,.18f,.08f,.08f);
    b.box(-.035f,-.08f,.08f,.035f,-.015f,.47f,.29f,.15f,.065f,.07f);
    b.box(-.05f,-.125f,-.55f,.05f,.02f,-.53f,.12f,.12f,.12f,.25f);
    b.box(-.042f,-.025f,-.05f,.042f,.045f,.17f,.10f,.11f,.12f,.45f);
    b.tube(0,.015f,.16f,.86f,.013f,.085f,.09f,.10f,.62f);
    b.box(-.017f,-.12f,-.12f,.017f,-.055f,-.035f,.30f,.16f,.07f,.08f);
    b.box(.04f,.01f,-.02f,.12f,.028f,.005f,.11f,.12f,.12f,.45f); // bolt handle
    b.box(-.018f,.04f,-.05f,.018f,.13f,-.025f,.11f,.12f,.13f,.45f);
    b.box(-.018f,.04f,.17f,.018f,.13f,.195f,.11f,.12f,.13f,.45f);
    b.tube(0,.155f,-.18f,.27f,.027f,.075f,.08f,.085f,.45f);
    b.tube(0,.155f,-.20f,-.16f,.041f,.07f,.075f,.08f,.4f);
    b.tube(0,.155f,.25f,.29f,.038f,.07f,.075f,.08f,.4f);
    return mesh.create(b.vertices.data(),b.vertices.size(),b.indices.data(),b.indices.size(),true,false,true);
}
