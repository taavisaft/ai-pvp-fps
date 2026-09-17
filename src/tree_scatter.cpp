#include "tree_collision.h"
#include "map.h"       // gMapId, mapRand, PALDISKI_HALF, LOBBY_HALF, MAP_LOBBY
#include "terrain.h"   // terrainHeight, pineForestBiome
#include <cmath>

std::vector<TreeInstance> gTrees;
std::vector<TreeCol>      gTreeCols;
SpatialGrid               gTreeColGrid;
float gTreeTrunkMaxRadius = 0.0f;

// Deterministic spruce scatter. Moved verbatim (math-identical) from the old
// Vegetation::buildTrees so the rendered forest and the collision cylinders agree.
static void scatterTrees(std::vector<TreeInstance>& out) {
    out.clear();
    if (gMapId == MAP_KEILA) return;
    if (gMapId == MAP_LOBBY) {
        // Loose spruce ring outside the pad, firing lane kept clear.
        for (int i = 0; i < 40; i++) {
            float a  = (i / 40.0f + (mapRand(i, 1, 61) - 0.5f) * 0.02f) * 6.2831853f;
            float rr = 34.0f + mapRand(i, 2, 62) * 22.0f;
            float x = cosf(a) * rr, z = sinf(a) * rr;
            if (x > 8.0f && fabsf(z) < 16.0f) continue;   // range lane to the wall
            TreeInstance t;
            t.x = x; t.z = z; t.y = terrainHeight(x, z) - 0.15f;
            t.scale = mapRand(i, 3, 63) < 0.4f ? 2.5f + mapRand(i, 4, 64) * 2.0f
                                               : 6.5f + mapRand(i, 4, 64) * 4.0f;
            t.yaw   = mapRand(i, 5, 65) * 6.2831853f;
            t.tint  = 0.82f + mapRand(i, 6, 66) * 0.36f;
            out.push_back(t);
        }
        auto open=[](float x,float z) {
            if(fabsf(x)>1016 || fabsf(z)>1016) return false;
            if(x*x+z*z<75*75 || trainingPondRadius(x,z)<1.15f) return false;
            return !(fabsf(x)<30 && z>95 && z<166);
        };
        const int side=340;
        for(int iz=0;iz<side;++iz) for(int ix=0;ix<side;++ix) {
            float x=-1020+(ix+.5f)*6+(mapRand(ix,iz,301)-.5f)*5;
            float z=-1020+(iz+.5f)*6+(mapRand(ix,iz,302)-.5f)*5;
            if(!open(x,z)) continue;
            float forest=trainingForest(x,z);
            if(mapRand(ix,iz,303)>forest*.92f) {
                if(forest>.03f && forest<.55f && mapRand(ix,iz,308)<.40f)
                    out.push_back({x,terrainHeight(x,z)-.10f,z,2.4f+mapRand(ix,iz,309)*3.2f,
                                   mapRand(ix,iz,305)*6.2831853f,.80f+mapRand(ix,iz,306)*.25f});
                continue;
            }
            float scale=9+mapRand(ix,iz,304)*10;
            if(forest<.5f) scale*=1.15f;
            if(mapRand(ix,iz,307)<.12f) scale*=.45f;
            out.push_back({x,terrainHeight(x,z)-.15f,z,scale,
                           mapRand(ix,iz,305)*6.2831853f,.78f+mapRand(ix,iz,306)*.30f});
        }
        for(int cz=-3;cz<5;++cz) for(int cx=-4;cx<4;++cx) {
            if(mapRand(cx,cz,321)>.60f) continue;
            float centerX=(cx+.2f+.6f*mapRand(cx,cz,322))*120, centerZ=(cz+.2f+.6f*mapRand(cx,cz,323))*120;
            if(centerX*centerX+centerZ*centerZ<150*150 || trainingPondRadius(centerX,centerZ)<2.4f) continue;
            if(fabsf(centerX)<45 && centerZ>80 && centerZ<185) continue;
            if(trainingForest(centerX,centerZ)>.10f) continue;
            int count=5+int(mapRand(cx,cz,324)*8);
            for(int k=0;k<count;++k) {
                float angle=k*2.39996323f+mapRand(cx,cz,325)*6.2831853f;
                float reach=sqrtf((k+.35f)/count)*14;
                float x=centerX+cosf(angle)*reach, z=centerZ+sinf(angle)*reach;
                if(!open(x,z)) continue;
                int key=cx*64+k;
                float scale=(20-9.0f*k/count)*(.85f+.30f*mapRand(key,cz,326));
                out.push_back({x,terrainHeight(x,z)-.15f,z,scale,
                               mapRand(key,cz,327)*6.2831853f,.80f+mapRand(key,cz,328)*.25f});
            }
            for(int k=0;k<6;++k) {
                int key=cx*64+32+k;
                float angle=mapRand(key,cz,331)*6.2831853f, reach=11+8*mapRand(key,cz,332);
                float x=centerX+cosf(angle)*reach, z=centerZ+sinf(angle)*reach;
                if(!open(x,z)) continue;
                out.push_back({x,terrainHeight(x,z)-.10f,z,2.2f+3.0f*mapRand(key,cz,333),
                               mapRand(key,cz,334)*6.2831853f,.82f+mapRand(key,cz,335)*.22f});
            }
        }
        for(int k=0;k<56;++k) {
            float angle=k*2.39996323f, ring=1.17f+.40f*mapRand(k,7,341);
            float x=cosf(angle)*TRAINING_POND_RADIUS_X*ring, z=TRAINING_POND_Z+sinf(angle)*TRAINING_POND_RADIUS_Z*ring;
            if(!open(x,z) || (z<172 && fabsf(x)<42) || mapRand(k,7,342)>.75f) continue;
            out.push_back({x,terrainHeight(x,z)-.10f,z,2.0f+3.6f*mapRand(k,7,343),
                           mapRand(k,7,344)*6.2831853f,.84f+mapRand(k,7,345)*.20f});
        }
        // Landmark trees frame the water; all use the same collision scatter.
        const float landmarks[][3]={{62,170,24},{56,191,19},{73,195,17},{51,151,12},
            {14,227,16},{-8,233,18},{-24,225,13},{-57,172,26},{-69,193,15},{-73,160,12}};
        for(const auto& p:landmarks)
            out.push_back({p[0],terrainHeight(p[0],p[1])-.15f,p[1],p[2],
                           mapRand(int(p[0]),int(p[1]),310)*6.2831853f,.91f});
        return;
    }
    const float STEP = 5.0f;
    const int n = (int)(2.0f * PALDISKI_HALF / STEP);
    for (int iz = 0; iz < n; iz++)
        for (int ix = 0; ix < n; ix++) {
            float x = -PALDISKI_HALF + (ix + 0.5f) * STEP + (mapRand(ix, iz, 41) - 0.5f) * 4.2f;
            float z = -PALDISKI_HALF + (iz + 0.5f) * STEP + (mapRand(ix, iz, 42) - 0.5f) * 4.2f;
            if (forestSiteClearance(x, z, 1.0f)) continue;
            float b = pineForestBiome(x, z);
            float dens = b * b * 2.2f + b * 0.30f + 0.045f;
            if (mapRand(ix, iz, 43) > dens) continue;
            float h = terrainHeight(x, z);
            if (h < 1.6f || h > 95.0f) continue;
            float gx = (terrainHeight(x + 2.0f, z) - terrainHeight(x - 2.0f, z)) * 0.25f;
            float gz = (terrainHeight(x, z + 2.0f) - terrainHeight(x, z - 2.0f)) * 0.25f;
            if (gx * gx + gz * gz > 0.30f) continue;
            TreeInstance t;
            t.x = x; t.z = z; t.y = h - 0.15f;
            float r     = mapRand(ix, iz, 44);
            float young = 2.2f + r * 2.6f;               // 2.2-4.8 m sapling
            float grown = 6.5f + r * 5.0f;               // 6.5-11.5 m stand tree
            float pYoung = b < 0.35f ? 0.78f : 0.18f;    // meadow vs core mix
            t.scale = mapRand(ix, iz, 47) < pYoung ? young : grown;
            t.yaw   = mapRand(ix, iz, 45) * 6.2831853f;
            t.tint  = 0.82f + mapRand(ix, iz, 46) * 0.36f;
            out.push_back(t);
        }
}

void buildTreeColliders() {
    scatterTrees(gTrees);

    gTreeCols.clear();
    gTreeCols.reserve(gTrees.size());
    gTreeTrunkMaxRadius = 0.0f;
    for (const TreeInstance& t : gTrees) {
        gTreeCols.push_back({t.x, t.z, treeCollRadius(t.scale)});
        gTreeTrunkMaxRadius = fmaxf(gTreeTrunkMaxRadius, TREE_TRUNK_BASE * t.scale);
    }

    // XZ grid matching the render cull grid's resolution, so cell queries are cheap.
    float half  = (gMapId == MAP_LOBBY) ? LOBBY_HALF : PALDISKI_HALF;
    int   cells = (gMapId == MAP_LOBBY) ? 64 : 32;
    gTreeColGrid.init(half, cells, 0.0f, 1.0f);
    for (int i = 0; i < (int)gTreeCols.size(); i++)
        gTreeColGrid.insert(gTreeCols[i].x, gTreeCols[i].z, i);
}
