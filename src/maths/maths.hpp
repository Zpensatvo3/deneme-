#pragma once
#include <concepts>
#include <numbers>

namespace maths
{
	struct vector3d;

	struct angles
	{
		angles mod360() const;

		angles operator-(const angles& other) const;
		angles operator+(const angles& other) const;
		angles operator*(float coefficient) const;

		float yaw = 0.f, pitch = 0.f;
	};

	inline constexpr std::floating_point auto to_degrees(std::floating_point auto angle)
	{
		constexpr double coef = 180.0 / std::numbers::pi;
		return angle * coef;
	}
	float mod360(float angle);

	struct vector3d
	{
		double length() const;

		vector3d operator+(const vector3d& other) const;
		vector3d operator-(const vector3d& other) const;
		vector3d operator*(double coefficient) const;
		vector3d operator^(const vector3d& other) const;

		angles get_angles() const;

		double x = 0.0, y = 0.0, z = 0.0;
	};

}
