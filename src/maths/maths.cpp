#include "maths.hpp"
#include <cmath>
#include <numbers>

double maths::vector3d::length() const
{
	return std::sqrt(x * x + y * y + z * z);
}



maths::vector3d maths::vector3d::operator+(const vector3d& other) const
{
	return {x + other.x, y + other.y, z + other.z};
}

maths::vector3d maths::vector3d::operator-(const vector3d& other) const
{
	return { x - other.x, y - other.y, z - other.z };
}

maths::vector3d maths::vector3d::operator*(double coefficient) const
{
	return { x * coefficient, y * coefficient, z * coefficient};
}

maths::vector3d maths::vector3d::operator^(const vector3d& other) const
{
	return
	{
		y * other.z - other.y * z,
		z * other.x - other.z * x,
		x * other.y - other.x * y
	};
}


maths::angles maths::vector3d::get_angles() const
{
	double hypxz = std::sqrt(x * x + z * z);

	double pitchRad = std::atan(-y / hypxz);

	double yawDeg = 0.0;
	double pitchDeg = to_degrees(pitchRad);

	if (x != 0.0)
	{
		double yawRad = std::atan2(z, x) - (std::numbers::pi / 2.0);
		yawDeg = to_degrees(yawRad);
	}

	return angles((float)yawDeg, (float)pitchDeg);
}




maths::angles maths::angles::mod360() const
{
	return angles(maths::mod360(yaw), maths::mod360(pitch));
}

maths::angles maths::angles::operator-(const angles& other) const
{
	return angles(yaw - other.yaw, pitch - other.pitch);
}

maths::angles maths::angles::operator+(const angles& other) const
{
	return angles(yaw + other.yaw, pitch + other.pitch);
}

maths::angles maths::angles::operator*(float coefficient) const
{
	return angles(yaw * coefficient, pitch * coefficient);
}

float maths::mod360(float angle)
{
	while (angle <= -180.0f)
		angle += 360.0f;
	while (angle > 180.0f)
		angle -= 360.0f;
	return angle;
}
