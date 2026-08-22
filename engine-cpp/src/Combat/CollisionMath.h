#pragma once
#include "raylib.h"

namespace CollisionMath {
    bool AABBIntersects(const BoundingBox& a, const BoundingBox& b);

    float DistanceSquared(Vector3 a, Vector3 b);
    bool IsWithinRadius(Vector3 a, Vector3 b, float radius);

    // Normaliza un vector en el plano horizontal (X, Z), ignorando la altura.
    Vector3 Normalize2D(Vector3 v);

    // Dirección unitaria de 'from' a 'to' en el plano horizontal.
    Vector3 DirectionXZ(Vector3 from, Vector3 to);

    // Escala una dirección horizontal por una magnitud (para knockback).
    Vector3 ScaleXZ(Vector3 direction, float magnitude);

    // Convierte una dirección en el ángulo de rotación (en grados) para dibujarla.
    float HeadingDegrees(Vector3 direction);

    // true si el segmento entre start y end atraviesa la caja, en el plano horizontal.
    bool SegmentIntersectsBoxXZ(Vector3 start, Vector3 end, const BoundingBox& box);
}
