#pragma once
#include "Engine/Math/Vec3.hpp"
struct Plane3D
{
public:
	Plane3D(Vec3 const& normal, float distance);
	Plane3D() {};
	~Plane3D() {}
	Vec3 GetNearestPointToOrigin() const;
	Vec3 GetNearestPoint(Vec3 const& referencePos) const;
	float GetAltitudeFromPoint(Vec3 const& referencePos) const;

	bool IsPointInFrontOf(Vec3 const& point) const;

public:
	Vec3 m_normal = Vec3::ONE;
	float m_distanceAlongNormal = 0.f;
};

