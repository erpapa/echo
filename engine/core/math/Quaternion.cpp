#include "Matrix4.h"
#include "Quaternion.h"

#define QUAN_EPSILON	(Real)(1e-03)

namespace Echo
{
	const Quaternion Quaternion::IDENTITY(1, 0, 0, 0);
	const Quaternion Quaternion::INVALID(Math::MAX_REAL, Math::MAX_REAL, Math::MAX_REAL, Math::MAX_REAL);
	ui32  Quaternion::OP_COUNT = 0;
	/*
	// This version of slerp, used by squad, does not check for theta > 180.
	Quan QuanSlerpNoInvert(const Quan &q1, const Quan &q2, Real t)
	{
	Real dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;
	Quan quan;

	if (dot > -0.95f && dot < 0.95f)
	{
	Real angle = Math::ACos(dot);
	Real sina, sinat, sinaomt;
	sina = Math::Sin(angle);
	sinat = Math::Sin(angle * t);
	sinaomt = Math::Sin(angle * (1 - t));
	quan = (q1 * sinaomt + q2 * sinat) / sina;
	}

	//if the angle is small, use linear interpolation
	else
	quan.lerp(q1, q2, t);

	return quan;
	}
	*/

	void Quaternion::Slerp(Quaternion& quan, const Quaternion& q1, const Quaternion& q2, Real t, bool bShortestPath)
	{
#ifdef ECHO_DEBUG
		++Quaternion::OP_COUNT;
#endif
		Real fCos = q1.dot(q2);
		Quaternion q;

		// do we need to invert rotation?
		if (fCos < 0.0f && bShortestPath)
		{
			fCos = -fCos;
			q = -q2;
		}
		else
		{
			q = q2;
		}

		if (Math::Abs(fCos) < 1 - QUAN_EPSILON)
		{
			// Standard case (slerp)
			Real fSin = Math::Sqrt(1 - Math::Sqr(fCos));
			Real radian = Math::ATan2(fSin, fCos);
			Real fInvSin = 1.0f / fSin;
			Real fCoeff0 = Math::Sin((1.0f - t) * radian) * fInvSin;
			Real fCoeff1 = Math::Sin(t * radian) * fInvSin;
			quan = fCoeff0 * q1 + fCoeff1 * q;
		}
		else
		{
			// There are two situations:
			// 1. "q1" and "q2" are very close (fCos ~= +1), so we can do a linear
			//    interpolation safely.
			// 2. "q1" and "q2" are almost inverse of each other (fCos ~= -1), there
			//    are an infinite number of possibilities interpolation. but we haven't
			//    have method to fix this case, so just use linear interpolation here.
			quan = (1.0f - t) * q1 + t * q;
			// taking the complement requires renormalisation
			quan.normalize();
		}
	}

	void Quaternion::Squad(Quaternion& quan, const Quaternion& q1,const Quaternion& q2, const Quaternion& a, const Quaternion& b, Real t, bool bShortestPath)
	{
#ifdef ECHO_DEBUG
		++Quaternion::OP_COUNT;
#endif
		Real slerpT = 2.0f * t * (1.0f - t);
		Quaternion slerpQ1;
		Slerp(slerpQ1, q1, q2, t, bShortestPath);
		Quaternion slerpQ2;
		Slerp(slerpQ2, a, b, t);
		Slerp(quan, slerpQ1 ,slerpQ2, slerpT);
	}

	void Quaternion::Spline(Quaternion& quan, const Quaternion& q1, const Quaternion& q2, const Quaternion& q3)
	{
#ifdef ECHO_DEBUG
		++Quaternion::OP_COUNT;
#endif
		Quaternion q;

		q.w = q2.w;
		q.x = -q2.x;
		q.y = -q2.y;
		q.z = -q2.z;

		quan = q2 * Exp((Log(q * q1) + Log(q * q3)) / -4);
	}

	// 通过两个单位朝向构造旋转
	void Quaternion::fromVec3ToVec3(const Vector3& from, const Vector3& to)
	{
		ADD_MATH_OP_COUNT
			Vector3 s = from;
		s.normalize();
		Vector3 t = to;
		t.normalize();

		Vector3 sxt = s.cross(t); // SxT
		Real sxtLen = sxt.len(); // == cos(angle)
		Real dot = s.dot(t);
		if (sxtLen == 0.0)
		{
			// parallel case.

			// 04/7/6 added. fix bug.
			if (dot > 0.0)
				*this = IDENTITY;
			else
				// inverse.
				fromAxisAngle(from.perpendicular(), Math::PI);

			return;
		}

		Vector3 u = sxt / sxtLen;

		Real angle2 = Math::ACos(dot) / 2.0f;

		Real sina = Math::Sin(angle2);
		Real cosa = Math::Cos(angle2);

		x = u.x * sina;
		y = u.y * sina;
		z = u.z * sina;
		w = cosa;
	}
}
