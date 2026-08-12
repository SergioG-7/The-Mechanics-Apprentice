#include "CollisionMath.h"
#include <cmath>

namespace CollisionMath {

bool AABBIntersects(const BoundingBox& a, const BoundingBox& b) {
    if (a.max.x < b.min.x || a.min.x > b.max.x) return false;
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;
    if (a.max.z < b.min.z || a.min.z > b.max.z) return false;
    return true;
}

float DistanceSquared(Vector3 a, Vector3 b) {
    float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
    return dx * dx + dy * dy + dz * dz;
}

bool IsWithinRadius(Vector3 a, Vector3 b, float radius) {
    return DistanceSquared(a, b) <= (radius * radius);
}

Vector3 Normalize2D(Vector3 v) {
    float lengthSq = v.x * v.x + v.z * v.z;
    if (lengthSq < 0.000001f) return Vector3{ 0.0f, 0.0f, 0.0f };
    float invLength = 1.0f / sqrtf(lengthSq);
    return Vector3{ v.x * invLength, 0.0f, v.z * invLength };
}

} // namespace CollisionMath
