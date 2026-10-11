#include "khrushchevka.h"
#include "map.h"
#include <initializer_list>

namespace {
struct Detail {
    KhrushSink sink; void* context; int section;
    void box(float x,float y,float z,float hx,float hy,float hz,glm::vec3 c,
             bool near=true,bool solid=false) {
        sink({{x,y,z},{hx,hy,hz},c,section,false,near,solid},context);
    }
    void window(float x,float y,float z,float width,float height,int seed,bool end=false) {
        auto part=[&](float a,float b,float d,float ha,float hb,float hd,glm::vec3 c,
                     bool near=true,bool solid=false) {
            if(end) box(d,b,a,hd,hb,ha,c,near,solid);
            else box(a,b,d,ha,hb,hd,c,near,solid);
        };
        glm::vec3 frame=seed%4==0 ? glm::vec3(.32f,.28f,.21f) : glm::vec3(.73f,.73f,.66f);
        float side=z<0 ? -1.f : 1.f, recess=z-side*.10f;
        // Opaque glazing avoids sorting/overdraw. One in seven windows is open.
        if(seed%7!=0) {
            float variation=mapRand(seed,3,910);
            glm::vec3 glass=glm::mix(glm::vec3(.026f,.042f,.045f),glm::vec3(.10f,.14f,.15f),variation);
            part(x,y,recess,width*.5f,height*.5f,.016f,glass,false,true);
        }
        // Revealed dark interior backing is cosmetic, removable for room art.
        part(x,y,z-side*.65f,width*.48f,height*.48f,.015f,{.018f,.020f,.017f},false);
        for(int k:{-1,1}) {
            part(x+k*width*.5f,y,recess,.046f,height*.5f+.046f,.065f,frame,false);
            part(x,y+k*height*.5f,recess,width*.5f,.045f,.065f,frame,false);
        }
        part(x+width*.10f,y,recess,.025f,height*.5f,.072f,frame);
        part(x-width*.18f,y+height*.19f,recess,width*.30f,.022f,.072f,frame);
        part(x,y-height*.5f-.055f,z+side*.11f,width*.5f+.13f,.035f,.23f,{.34f,.35f,.31f});
        if(seed%3==0) {
            glm::vec3 curtain(.29f,.28f,.22f);
            part(x-width*.31f,y,recess-side*.07f,width*.12f,height*.47f,.02f,curtain);
        }
    }
};
}

void visitKhrushchevkaDetails(KhrushSink sink,void* context) {
    const float widths[5]={2.6f,3.2f,2.6f,2.6f,3.2f};
    const glm::vec3 joint(.20f,.21f,.19f), metal(.18f,.20f,.19f);
    for(int s=0;s<KHRUSH_SECTIONS;++s) {
        Detail b{sink,context,s};
        float left=-KHRUSH_LENGTH*.5f+s*KHRUSH_SECTION;
        for(int side:{-1,1}) {
            float z=side*5.94f;
            for(int f=0;f<=KHRUSH_FLOORS;++f)
                b.box(left+7.1f,.19f+f*KHRUSH_FLOOR,z,7.1f,.014f,.007f,joint,false);
            float x=left;
            for(int j=0;j<=5;++j) {
                b.box(x,6.94f,z,.016f,6.75f,.007f,joint,false);
                if(j<5) x+=widths[j];
            }
            // Eaves, gutter and drain pipe, with elbow-like offsets at the plinth.
            b.box(left+7.1f,13.85f,side*6.08f,7.22f,.085f,.10f,metal,false);
            b.box(left+.24f,6.85f,side*6.04f,.055f,6.80f,.055f,metal);
            b.box(left+.24f,.24f,side*6.16f,.06f,.20f,.14f,metal);
        }
        for(int f=0;f<KHRUSH_FLOORS;++f) {
            float y=.19f+f*KHRUSH_FLOOR, x=left;
            for(int j=0;j<5;++j) {
                float w=widths[j], cx=x+w*.5f;
                for(int side:{-1,1}) {
                    bool entry=side==1 && j==2 && f==0;
                    bool stair=side==1 && j==2 && f>0;
                    bool balcony=side==-1 && (j==1 || j==4) && f>0;
                    if(entry) {
                        // Door left open; dark vestibule beyond the real opening.
                        b.box(cx-.84f,y+1.12f,6.15f,.045f,1.12f,.42f,{.11f,.14f,.13f},false,true);
                        b.box(cx+.98f,y+1.65f,5.95f,.09f,.12f,.03f,{.12f,.15f,.17f});
                    } else {
                        float sill=balcony ? .05f : stair ? .75f : .85f;
                        b.window(cx,y+(sill+2.30f)*.5f,side*5.76f,1.65f,2.30f-sill,s*73+f*11+j+side+1);
                    }
                    if(balcony) {
                        int style=(s*7+j+f*3)%5;
                        // Resident alterations: open rails, painted screens,
                        // old timber enclosures and newer white framed glazing.
                        glm::vec3 c=style==0 ? glm::vec3(.31f,.38f,.34f) :
                                    style==1 ? glm::vec3(.38f,.31f,.24f) : glm::vec3(.54f,.55f,.50f);
                        b.box(cx,y+.52f,-7.30f,1.25f,.43f,.015f,c,false);
                        b.box(cx,y+.99f,-7.25f,1.34f,.027f,.03f,metal,false,true);
                        for(int k=-4;k<=4;++k) b.box(cx+k*.30f,y+.57f,-7.27f,.016f,.40f,.018f,metal);
                        if(style>=2) {
                            b.box(cx,y+2.26f,-6.45f,1.34f,.055f,.85f,c,false);
                            b.box(cx,y+1.61f,-7.24f,1.29f,.61f,.015f,{.083f,.12f,.125f},false,true);
                            for(int k=-2;k<=2;++k) b.box(cx+k*.64f,y+1.61f,-7.28f,.026f,.64f,.045f,c,false);
                            for(int side2:{-1,1}) {
                                b.box(cx+side2*1.29f,y+1.61f,-6.44f,.02f,.64f,.78f,{.083f,.12f,.125f},false,true);
                                b.box(cx+side2*1.31f,y+1.61f,-6.44f,.04f,.64f,.03f,c);
                            }
                        }
                    }
                }
                x+=w;
            }
        }
        if(s==0 || s==KHRUSH_SECTIONS-1) {
            float ex=s==0 ? left : left+KHRUSH_SECTION;
            for(int f=0;f<=KHRUSH_FLOORS;++f)
                b.box(ex+(s==0 ? -.18f : .18f),.19f+f*KHRUSH_FLOOR,0,.007f,.014f,5.76f,joint,false);
            for(float z:{-1.78f,0.0f,1.78f})
                b.box(ex+(s==0 ? -.18f : .18f),6.94f,z,.007f,6.75f,.016f,joint,false);
            for(int f=0;f<KHRUSH_FLOORS;++f) for(int side:{-1,1})
                b.window(side*3.77f,.19f+f*KHRUSH_FLOOR+1.575f,ex,1.45f,1.45f,s*31+f*7+side+1,true);
        }
        // Roof ventilation stacks and simple caps; no per-frame animation.
        for(int k=0;k<2;++k) {
            float x=left+3.1f+k*7.0f;
            b.box(x,15.02f,.85f,.30f,.48f,.30f,{.30f,.31f,.29f},false,true);
            b.box(x,15.51f,.85f,.38f,.04f,.38f,metal);
        }
    }
}
