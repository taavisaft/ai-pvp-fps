#include "khrushchevka.h"
#include "map.h"
#include <initializer_list>

namespace {
struct Builder {
    KhrushSink sink; void* context; int section;
    void box(float x,float y,float z,float hx,float hy,float hz,
             glm::vec3 color,bool concrete=true,bool solid=true,bool detail=false) {
        KhrushPart p{{x,y,z},{hx,hy,hz},color,section,concrete,detail,solid};
        sink(p,context);
    }
    // Four wall pieces around a real opening, including the reveal thickness.
    void opening(float x,float y,float z,float width,float sill,float head,
                 float aperture,glm::vec3 color,bool end=false) {
        auto piece=[&](float a,float b,float ha,float hb) {
            if(end) box(z,b,a,.16f,hb,ha,color);
            else box(a,b,z,ha,hb,.16f,color);
        };
        if(sill>0) piece(x,y+sill*.5f,width*.5f,sill*.5f);
        piece(x,y+(head+KHRUSH_FLOOR)*.5f,width*.5f,(KHRUSH_FLOOR-head)*.5f);
        float pier=(width-aperture)*.5f;
        for(int side:{-1,1}) piece(x+side*(aperture+pier)*.5f,y+(sill+head)*.5f,
                                  pier*.5f,(head-sill)*.5f);
    }
};
}

void visitKhrushchevka(KhrushSink sink,void* context) {
    const glm::vec3 plaster(.47f,.465f,.425f), base(.18f,.185f,.17f);
    const glm::vec3 roof(.105f,.125f,.135f);
    const float widths[5]={2.6f,3.2f,2.6f,2.6f,3.2f};
    for(int s=0;s<KHRUSH_SECTIONS;++s) {
        Builder b{sink,context,s};
        float left=-KHRUSH_LENGTH*.5f+s*KHRUSH_SECTION, mid=left+KHRUSH_SECTION*.5f;
        b.box(mid,-.35f,0,KHRUSH_SECTION*.5f,.35f,KHRUSH_DEPTH*.5f,base);
        // Dark painted plinth, with the entrance gap kept open.
        b.box(mid,.32f,-5.935f,7.1f,.32f,.018f,base,false,false);
        b.box(left+3.1375f,.32f,5.935f,3.1375f,.32f,.018f,base,false,false);
        b.box(left+11.0625f,.32f,5.935f,3.1375f,.32f,.018f,base,false,false);
        for(int side:{-1,1}) b.box(mid,-.06f,side*8.1f,7.1f,.06f,1.65f,base*1.6f);
        for(int f=0;f<=KHRUSH_FLOORS;++f) {
            // Empty shell floors. Stair apertures reserved on the entrance side.
            float y=f==0 ? -.09f : .10f+f*KHRUSH_FLOOR;
            if(f==0 || f==KHRUSH_FLOORS) b.box(mid,y,0,7.1f,.09f,5.76f,plaster);
            else {
                b.box(mid,y,-1.70f,7.1f,.09f,4.06f,plaster);
                b.box(left+2.6f,y,4.06f,2.6f,.09f,1.7f,plaster);
                b.box(left+11.0f,y,4.06f,3.2f,.09f,1.7f,plaster);
            }
        }
        for(int f=0;f<KHRUSH_FLOORS;++f) {
            float y=.19f+f*KHRUSH_FLOOR, x=left;
            for(int j=0;j<5;++j) {
                float w=widths[j], cx=x+w*.5f;
                float shade=.95f+.09f*mapRand(s*17+j,f,902);
                for(int side:{-1,1}) {
                    bool entry=side==1 && j==2 && f==0;
                    bool stair=side==1 && j==2 && f>0;
                    bool balcony=side==-1 && (j==1 || j==4) && f>0;
                    float sill=entry ? 0.0f : balcony ? .05f : stair ? .75f : .85f;
                    float head=entry ? 2.25f : 2.30f;
                    b.opening(cx,y,side*5.76f,w,sill,head,entry ? 1.65f : 1.65f,plaster*shade);
                    if(balcony) {
                        b.box(cx,y-.02f,-6.42f,1.32f,.10f,.83f,plaster*.82f);
                        // Concrete parapet and end cheeks are shared solid cover.
                        b.box(cx,y+.47f,-7.23f,1.32f,.47f,.055f,base*1.6f);
                        for(int k:{-1,1}) b.box(cx+k*1.28f,y+.47f,-6.44f,.045f,.47f,.78f,base*1.6f);
                    }
                }
                x+=w;
            }
        }
        // Small canopy and level approach at each entrance.
        float entry=left+7.1f;
        b.box(entry,2.60f,6.40f,1.18f,.08f,.80f,roof,false);
        b.box(entry,-.06f,6.70f,1.22f,.06f,1.08f,base);
        for(int side:{-1,1}) b.box(mid,13.78f,side*5.76f,7.1f,.10f,.16f,plaster*.86f);
        // Shallow pitched metal roof built from narrow, overlapping strips.
        for(int row=0;row<24;++row) {
            float z=-6.13f+(row+.5f)*12.26f/24;
            float y=13.93f+(6.13f-fabsf(z))*.14f;
            b.box(mid,y,z,7.22f,.065f,.28f,roof,false);
        }
        // Seal the attic end with stepped gable strips, matching the roof.
        if(s==0 || s==KHRUSH_SECTIONS-1) {
            float ex=s==0 ? left : left+KHRUSH_SECTION;
            b.box(ex+(s==0 ? -.175f : .175f),.32f,0,.018f,.32f,5.76f,base,false,false);
            for(int row=0;row<24;++row) {
                float z=-5.76f+(row+.5f)*11.52f/24, top=13.88f+(6.13f-fabsf(z))*.14f;
                b.box(ex,(13.6f+top)*.5f,z,.16f,(top-13.6f)*.5f,.24f,plaster*.88f);
            }
            for(int f=0;f<KHRUSH_FLOORS;++f) {
                float y=.19f+f*KHRUSH_FLOOR;
                b.box(ex,y+1.35f,0,.16f,1.35f,1.78f,plaster);
                for(int side:{-1,1}) b.opening(side*3.77f,y,ex,3.98f,.85f,2.30f,1.45f,plaster,true);
            }
        }
    }
    visitKhrushchevkaDetails(sink,context);
}

void buildKhrushchevkaCollision() {
    gKhrushBoxFirst=gTownBoxCount;
    visitKhrushchevka([](const KhrushPart& p,void*) {
        if(!p.solid) return;
        glm::vec3 center=p.center+glm::vec3(KHRUSH_X,KHRUSH_Y,KHRUSH_Z);
        townPush({center,p.half},p.concrete ? SURF_CONCRETE : SURF_METAL);
    },nullptr);
    gKhrushBoxCount=gTownBoxCount-gKhrushBoxFirst;
}
