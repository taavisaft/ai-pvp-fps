#pragma once
#include <cstdint>
#include <glm/glm.hpp>

constexpr float KEILA_HALF       = 512.0f;
constexpr float KEILA_WORLD_HALF = 1024.0f;
constexpr float KEILA_GRID_STEP  = 2.0f;
constexpr int   KEILA_GRID       = 1025;
constexpr int   KEILA_BARE_SIDE  = 1024;
constexpr bool  KEILA_GRASS      = false;

struct KeilaBuilding {
    int32_t first, count;
    int32_t roofFirst, roofCount;
    float   height;
    int32_t kind;
};

struct KeilaLandmark {
    int32_t     building;
    int32_t     floors;
    float       plinth, parapet;
    const char* texture;
    const char* walls;
};

struct KeilaWall {
    float ax, az, bx, bz;
    float nx, nz;
    float bottom, top;
};

extern const float         KEILA_HEIGHT_BASE;
extern const uint16_t      KEILA_HEIGHT_CM[KEILA_GRID * KEILA_GRID];
extern const int           KEILA_BUILDING_COUNT;
extern const KeilaBuilding KEILA_BUILDINGS[];
extern const float         KEILA_BUILDING_XZ[];
extern const uint16_t      KEILA_ROOF_INDEX[];
extern const int           KEILA_LANDMARK_COUNT;
extern const KeilaLandmark KEILA_LANDMARKS[];
extern const uint8_t       KEILA_BARE[KEILA_BARE_SIDE * KEILA_BARE_SIDE / 8];
extern const int           KEILA_SPAWN_COUNT;
extern const float         KEILA_SPAWNS[][2];

float keilaHeight(float x, float z);
bool  keilaBare(float x, float z);

void  buildKeilaColliders();
void  clearKeilaColliders();
float keilaBuildingBase(int building);
float keilaBuildingTop(int building);
float keilaRoofUnder(float x, float z, float fromY, float floor);
void  keilaCollidePlayer(glm::vec3& pos, float radius, float bodyHeight);
bool  keilaSweep(const glm::vec3& start, const glm::vec3& end, float& fraction, glm::vec3& normal);
