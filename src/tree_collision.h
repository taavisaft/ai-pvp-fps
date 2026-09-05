#pragma once
#include "tree_scatter.h"

// Unit-height spruce trunk, shared with both rendered mesh LODs.
constexpr int TREE_TRUNK_SIDES = 6;
constexpr float TREE_TRUNK_BASE = 0.020f;
constexpr float TREE_TRUNK_TOP = 0.004f;
constexpr float TREE_TRUNK_HEIGHT = 0.86f;

bool segmentTreeTrunk(const TreeInstance& tree, const glm::vec3& start,
                      const glm::vec3& end, float& fraction, glm::vec3& normal);
// Nearest solid trunk along the entire segment. No allocation; uses placement grid.
bool sweepTreeTrunks(const glm::vec3& start, const glm::vec3& end,
                     float& fraction, glm::vec3& normal);
