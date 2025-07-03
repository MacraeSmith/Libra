#include "Engine/Renderer/Camera.hpp"
#include "Engine/Math/AABB3.hpp"

void Camera::SetOrthoView(Vec2 const& bottomLeft, Vec2 const& topRight, float nearZ, float farZ)
{
	m_mode = eMode_Orthograhic;
	m_orthoBottomLeft = bottomLeft;
	m_orthoTopRight = topRight;
	m_orthoNear = nearZ;
	m_orthoFar = farZ;
}

void Camera::SetOrthoView(AABB3 const& cameraBounds)
{
	m_mode = eMode_Orthograhic;
	m_orthoBottomLeft = Vec2(cameraBounds.m_mins.x, cameraBounds.m_mins.y);
	m_orthoTopRight = Vec2(cameraBounds.m_maxs.x, cameraBounds.m_maxs.y);
	m_orthoNear = cameraBounds.m_mins.z;
	m_orthoFar = cameraBounds.m_maxs.z;
}

void Camera::SetPerspectiveView(float aspect, float fov, float nearZ, float farZ)
{
	m_mode = eMode_Perspective;
	m_perspectiveAspect = aspect;
	m_perspectiveFOV = fov;
	m_perspectiveNear = nearZ;
	m_perspectiveFar = farZ;
}

void Camera::SetPositionAndOrientation(Vec3 const& pos, EulerAngles const& orientation)
{
	m_position = pos;
	m_orientation = orientation;
}

void Camera::SetPosition(Vec3 const& position)
{
	m_position = position;
}

Vec3 Camera::GetPosition() const
{
	return m_position;
}

void Camera::SetOrientation(EulerAngles const& orientation)
{
	m_orientation = orientation;
}

EulerAngles Camera::GetOrientation() const
{
	return m_orientation;
}


Vec2 Camera::GetOrthoBottomLeft() const
{
	return m_orthoBottomLeft;
}

Vec2 Camera::GetOrthoTopRight() const
{
	return m_orthoTopRight;
}

Mat44 Camera::GetOrthoMatrix() const
{
	return Mat44::MakeOrthoProjection(m_orthoBottomLeft.x, m_orthoTopRight.x, m_orthoBottomLeft.y, m_orthoTopRight.y, m_orthoNear, m_orthoFar);
}

Mat44 Camera::GetPerspectiveMatrix() const
{
	return Mat44::MakePerspectiveProjection(m_perspectiveFOV, m_perspectiveAspect, m_perspectiveNear, m_perspectiveFar);
}

Mat44 Camera::GetProjectionMatrix() const
{
	switch (m_mode)
	{
	case Camera::eMode_Orthograhic:
		return GetOrthoMatrix();
	case Camera::eMode_Perspective:
		return GetPerspectiveMatrix();
	}
	return GetOrthoMatrix();
}

void Camera::SetViewportBounds(AABB2 const& bounds)
{
	m_viewportBounds = bounds;
}

AABB2 Camera::GetViewportBounds() const
{
	return m_viewportBounds;
}

Vec2 Camera::GetViewportDimensions() const
{
	return Vec2(m_viewportBounds.m_maxs.x - m_viewportBounds.m_mins.x, m_viewportBounds.m_maxs.y - m_viewportBounds.m_mins.y);
}

Mat44 Camera::GetCameraToWorldTransform() const
{
	Mat44 transform = Mat44::MakeTranslation3D(m_position);
	Mat44 rotationMat = m_orientation.GetAsMatrix_IFwd_JLeft_KUp();
	transform.Append(rotationMat);
	return transform;
}

Mat44 Camera::GetWorldToCameraTransform() const
{
	Mat44 cameraTransform = GetCameraToWorldTransform();
	return cameraTransform.GetOrthonormalInverse();
}

void Camera::SetCameraToRenderTransform(Mat44 const& matrix)
{
	m_cameraToRenderTransform = matrix;
}

Mat44 Camera::GetCameraToRenderTransform() const
{
	return m_cameraToRenderTransform;
}

Mat44 Camera::GetRenderToClipTransform() const
{
	return GetProjectionMatrix();
}
