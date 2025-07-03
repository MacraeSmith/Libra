#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/AABB2.hpp"
#pragma once
struct AABB3;
class Camera
{
public:
	enum Mode : int 
	{
		eMode_Orthograhic,
		eMode_Perspective,

		eMode_Count
	};
	
public:

	void SetOrthoView(Vec2 const& bottomLeft, Vec2 const& topRight, float nearZ = 0.f, float farZ = 1.f );
	void SetOrthoView(AABB3 const& cameraBounds);
	void SetPerspectiveView(float aspect, float fov, float nearZ, float farZ);

	void SetPositionAndOrientation(Vec3 const& pos, EulerAngles const& orientation);
	void SetPosition(Vec3 const& position);
	Vec3 GetPosition() const;
	void SetOrientation(EulerAngles const& orientation);
	EulerAngles GetOrientation() const;

	Mat44 GetCameraToWorldTransform() const;
	Mat44 GetWorldToCameraTransform() const;

	void SetCameraToRenderTransform(Mat44 const& matrix);
	Mat44 GetCameraToRenderTransform() const;

	Mat44 GetRenderToClipTransform() const;

	Vec2 GetOrthoBottomLeft() const;
	Vec2 GetOrthoTopRight() const;
	void Translate2D(Vec2 const& translation);

	Mat44 GetOrthoMatrix() const;
	Mat44 GetPerspectiveMatrix() const;
	Mat44 GetProjectionMatrix() const;

	void SetViewportBounds(AABB2 const& bounds);
	AABB2 GetViewportBounds() const;
	Vec2 GetViewportDimensions() const;

protected:
	Mode m_mode = eMode_Orthograhic;

	Vec3 m_position;
	EulerAngles m_orientation;

	Vec2 m_orthoBottomLeft;
	Vec2 m_orthoTopRight;
	float m_orthoNear;
	float m_orthoFar;

	float m_perspectiveAspect;
	float m_perspectiveFOV;
	float m_perspectiveNear;
	float m_perspectiveFar;

	Mat44 m_cameraToRenderTransform;
	AABB2 m_viewportBounds = AABB2::ZERO_TO_ONE;

};

