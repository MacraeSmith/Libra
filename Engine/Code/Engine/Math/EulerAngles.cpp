#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/StringUtils.hpp"

EulerAngles const EulerAngles::ZERO = EulerAngles(0.f, 0.f, 0.f);

EulerAngles::EulerAngles(float yawDegrees, float pitchDegrees, float rollDegrees)
	:m_yawDegrees(yawDegrees)
	,m_pitchDegrees(pitchDegrees)
	,m_rollDegrees(rollDegrees)
{
}


void EulerAngles::GetAsVectors_IFwd_JLeft_KUp(Vec3& out_forwardIBasis, Vec3& out_leftJBasis, Vec3& out_upKBasis) const
{
	float cY = CosDegrees(m_yawDegrees);
	float sY = SinDegrees(m_yawDegrees);

	float cP = CosDegrees(m_pitchDegrees);
	float sP = SinDegrees(m_pitchDegrees);

	float cR = CosDegrees(m_rollDegrees);
	float sR = SinDegrees(m_rollDegrees);

	out_forwardIBasis = Vec3(cY * cP, sY * cP, -sP);
	out_leftJBasis = Vec3((-sY * cR) + (cY * sP * sR), (cY * cR) + (sY * sP * sR), cP * sR);
	out_upKBasis = Vec3((-sY * -sR) + (cY * sP * cR), (cY * -sR) + (sY * sP * cR), cP * cR);
}

Mat44 EulerAngles::GetAsMatrix_IFwd_JLeft_KUp() const
{
	float cY = CosDegrees(m_yawDegrees);
	float sY = SinDegrees(m_yawDegrees);

	float cP = CosDegrees(m_pitchDegrees);
	float sP = SinDegrees(m_pitchDegrees);

	float cR = CosDegrees(m_rollDegrees);
	float sR = SinDegrees(m_rollDegrees);

	Vec4 iBasis(cY * cP, sY * cP, -sP, 0.f);
	Vec4 jBasis((-sY * cR) + (cY * sP * sR), (cY * cR) + (sY * sP * sR), cP * sR, 0.f);
	Vec4 kBasis((-sY * -sR) + (cY * sP * cR), (cY * -sR) + (sY * sP * cR), cP * cR, 0.f);
	Vec4 translation(0.f, 0.f, 0.f, 1.f);

	return Mat44(iBasis, jBasis, kBasis, translation);
}

Vec3 EulerAngles::Get_IFwd() const
{
	float cY = CosDegrees(m_yawDegrees);
	float sY = SinDegrees(m_yawDegrees);

	float cP = CosDegrees(m_pitchDegrees);
	float sP = SinDegrees(m_pitchDegrees);

	return Vec3(cY * cP, sY * cP, -sP);
}

Vec3 EulerAngles::Get_JLeft() const
{
	float cY = CosDegrees(m_yawDegrees);
	float sY = SinDegrees(m_yawDegrees);

	float cP = CosDegrees(m_pitchDegrees);
	float sP = SinDegrees(m_pitchDegrees);

	float cR = CosDegrees(m_rollDegrees);
	float sR = SinDegrees(m_rollDegrees);

	return Vec3((-sY * cR) + (cY * sP * sR), (cY * cR) + (sY * sP * sR), cP * sR);
}

Vec3 EulerAngles::Get_KUp() const
{
	float cY = CosDegrees(m_yawDegrees);
	float sY = SinDegrees(m_yawDegrees);

	float cP = CosDegrees(m_pitchDegrees);
	float sP = SinDegrees(m_pitchDegrees);

	float cR = CosDegrees(m_rollDegrees);
	float sR = SinDegrees(m_rollDegrees);

	return Vec3((-sY * -sR) + (cY * sP * cR), (cY * -sR) + (sY * sP * cR), cP * cR);
}

void EulerAngles::SetFromText(char const* text)
{
	Strings numsFromText = SplitStringOnDelimiter(text, ',');
	m_yawDegrees = (float)(atof(numsFromText[0].c_str()));
	m_pitchDegrees = (float)(atof(numsFromText[1].c_str()));
	m_rollDegrees = (float)(atof(numsFromText[2].c_str()));
}

const EulerAngles EulerAngles::operator*(float uniformScale) const
{
	return EulerAngles(m_yawDegrees * uniformScale, m_pitchDegrees * uniformScale, m_rollDegrees * uniformScale);
}

void EulerAngles::operator+=(EulerAngles const& eulerAnglesToAdd)
{
	m_yawDegrees += eulerAnglesToAdd.m_yawDegrees;
	m_pitchDegrees += eulerAnglesToAdd.m_pitchDegrees;
	m_rollDegrees += eulerAnglesToAdd.m_rollDegrees;
}

void EulerAngles::operator*=(const float uniformScale)
{
	m_yawDegrees *= uniformScale;
	m_pitchDegrees *= uniformScale;
	m_rollDegrees *= uniformScale;
}
