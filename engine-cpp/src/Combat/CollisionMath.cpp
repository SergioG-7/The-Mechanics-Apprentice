#include "CollisionMath.h"
#include <cmath>
#include <utility>

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

bool SegmentIntersectsBoxXZ(Vector3 start, Vector3 end, const BoundingBox& box) {
    float dx = end.x - start.x;
    float dz = end.z - start.z;

    float tMin = 0.0f;
    float tMax = 1.0f;

    // Eje X: si el segmento no se mueve en X, solo puede estar dentro de la
    // franja si el origen ya está entre min.x y max.x.
    if (fabsf(dx) < 1e-6f) {
        if (start.x < box.min.x || start.x > box.max.x) return false;
    } else {
        float t1 = (box.min.x - start.x) / dx;
        float t2 = (box.max.x - start.x) / dx;
        if (t1 > t2) std::swap(t1, t2);
        tMin = (t1 > tMin) ? t1 : tMin;
        tMax = (t2 < tMax) ? t2 : tMax;
        if (tMin > tMax) return false;
    }

    // Eje Z: mismo test, recorta el intervalo [tMin, tMax] que ya dejó el X.
    if (fabsf(dz) < 1e-6f) {
        if (start.z < box.min.z || start.z > box.max.z) return false;
    } else {
        float t1 = (box.min.z - start.z) / dz;
        float t2 = (box.max.z - start.z) / dz;
        if (t1 > t2) std::swap(t1, t2);
        tMin = (t1 > tMin) ? t1 : tMin;
        tMax = (t2 < tMax) ? t2 : tMax;
        if (tMin > tMax) return false;
    }

    return tMin <= tMax;
}

} // namespace CollisionMath
