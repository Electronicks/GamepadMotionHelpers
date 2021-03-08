// Copyright (c) 2020-2021 Julian "Jibb" Smart
// Released under the MIT license. See https://github.com/JibbSmart/GamepadMotionHelpers/blob/main/LICENSE for more info
// Revision 4

#define _USE_MATH_DEFINES
#include <math.h>

// You don't need to look at these. These will just be used internally by the GamepadMotion class declared below.
// You can ignore anything in namespace GamepadMotionHelpers.

namespace GamepadMotionHelpers
{
	struct GyroCalibration
	{
		float X;
		float Y;
		float Z;
		float AccelMagnitude;
		int NumSamples;
	};

	struct Quat
	{
		float w;
		float x;
		float y;
		float z;

		Quat();
		Quat(float inW, float inX, float inY, float inZ);
		void Set(float inW, float inX, float inY, float inZ);
		Quat& operator*=(const Quat& rhs);
		friend Quat operator*(Quat lhs, const Quat& rhs);
		void Normalize();
		Quat Normalized() const;
		void Invert();
		Quat Inverse() const;
	};

	struct Vec
	{
		float x;
		float y;
		float z;

		Vec();
		Vec(float inX, float inY, float inZ);
		void Set(float inX, float inY, float inZ);
		float Length() const;
		void Normalize();
		Vec Normalized() const;
		float Dot(const Vec& other) const;
		Vec Cross(const Vec& other) const;
		Vec& operator+=(const Vec& rhs);
		friend Vec operator+(Vec lhs, const Vec& rhs);
		Vec& operator-=(const Vec& rhs);
		friend Vec operator-(Vec lhs, const Vec& rhs);
		Vec& operator*=(const float rhs);
		friend Vec operator*(Vec lhs, const float rhs);
		Vec& operator/=(const float rhs);
		friend Vec operator/(Vec lhs, const float rhs);
		Vec& operator*=(const Quat& rhs);
		friend Vec operator*(Vec lhs, const Quat& rhs);
		Vec operator-() const;
	};

	struct SensorMinMaxWindow
	{
		Vec MinGyro;
		Vec MaxGyro;
		Vec MinAccel;
		Vec MaxAccel;
		int NumSamples;
		float TimeSampled;

		SensorMinMaxWindow();
		void Reset(float remainder);
		void AddSample(const Vec& inGyro, const Vec& inAccel, float deltaTime);
		Vec GetMedianGyro();
	};

	struct AutoCalibration
	{
		const int NumWindows = 2;
		SensorMinMaxWindow MinMaxWindows[2];

		AutoCalibration();
		bool AddSample(const Vec& inGyro, const Vec& inAccel, float deltaTime);
		void SetCalibrationData(GyroCalibration* calibrationData);

	private:
		float MinDeltaGyroX = 10.f;
		float MinDeltaGyroY = 10.f;
		float MinDeltaGyroZ = 10.f;
		float MinDeltaAccelX = 10.f;
		float MinDeltaAccelY = 10.f;
		float MinDeltaAccelZ = 10.f;
		float RecalibrateThreshold = 1.f;

		const int MinAutoWindowSamples = 5;
		const float MinAutoWindowTime = 1.f;
		const float MaxRecalibrateThreshold = 1.5f;
		const float MinClimbRate = 0.5f;
		const float RecalibrateClimbRate = 0.5f;
		const float RecalibrateDrop = 0.25f;

		GyroCalibration* CalibrationData;
	};

	struct Motion
	{
		Quat Quaternion;
		Vec Accel;
		Vec Grav;

		const int NumGravDirectionSamples = 10;
		Vec GravDirectionSamples[10];
		int LastGravityIdx = 9;
		int NumGravDirectionSamplesCounted = 0;
		float TimeCorrecting = 0.0f;

		Motion();
		void Reset();
		void Update(float inGyroX, float inGyroY, float inGyroZ, float inAccelX, float inAccelY, float inAccelZ, float gravityLength, float deltaTime);
	};
}

// Note that I'm using a Y-up coordinate system. This is to follow the convention set by the motion sensors in
// PlayStation controllers, which was what I was using when writing in this. But for the record, Z-up is
// better for most games (XY ground-plane in 3D games simplifies using 2D vectors in navigation, for example).

// Gyro units should be degrees per second. Accelerometer should be Gs (approx. 9.8m/s^2 = 1G). If you're using
// radians per second, meters per second squared, etc, conversion should be simple.

enum class CalibrationMode
{
	Basic,
	Auto,
};

class GamepadMotion
{
public:
	GamepadMotion();

	void Reset();

	void ProcessMotion(float gyroX, float gyroY, float gyroZ,
		float accelX, float accelY, float accelZ, float deltaTime);

	// reading the current state
	void GetCalibratedGyro(float& x, float& y, float& z);
	void GetGravity(float& x, float& y, float& z);
	void GetProcessedAcceleration(float& x, float& y, float& z);
	void GetOrientation(float& w, float& x, float& y, float& z);

	// gyro calibration functions
	void StartContinuousCalibration();
	void PauseContinuousCalibration();
	void ResetContinuousCalibration();
	void GetCalibrationOffset(float& xOffset, float& yOffset, float& zOffset);
	void SetCalibrationOffset(float xOffset, float yOffset, float zOffset, int weight);

	CalibrationMode GetCalibrationMode();
	void SetCalibrationMode(CalibrationMode calibrationMode);

	void ResetMotion();

private:
	GamepadMotionHelpers::Vec Gyro;
	GamepadMotionHelpers::Vec RawAccel;
	GamepadMotionHelpers::Motion Motion;
	GamepadMotionHelpers::GyroCalibration GyroCalibration;
	GamepadMotionHelpers::AutoCalibration AutoCalibration;
	CalibrationMode CurrentCalibrationMode;

	bool IsCalibrating;
	void PushSensorSamples(float gyroX, float gyroY, float gyroZ, float accelMagnitude);
	void GetCalibratedSensor(float& gyroOffsetX, float& gyroOffsetY, float& gyroOffsetZ, float& accelMagnitude);
};

///////////// Everything below here are just implementation details /////////////

namespace GamepadMotionHelpers
{
	Quat::Quat()
	{
		w = 1.0f;
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
	}

	Quat::Quat(float inW, float inX, float inY, float inZ)
	{
		w = inW;
		x = inX;
		y = inY;
		z = inZ;
	}

	static Quat AngleAxis(float inAngle, float inX, float inY, float inZ)
	{
		Quat result = Quat(cosf(inAngle * 0.5f), inX, inY, inZ);
		result.Normalize();
		return result;
	}

	void Quat::Set(float inW, float inX, float inY, float inZ)
	{
		w = inW;
		x = inX;
		y = inY;
		z = inZ;
	}

	Quat& Quat::operator*=(const Quat& rhs)
	{
		Set(w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
			w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
			w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
			w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w);
		return *this;
	}

	Quat operator*(Quat lhs, const Quat& rhs)
	{
		lhs *= rhs;
		return lhs;
	}

	void Quat::Normalize()
	{
		//printf("Normalizing: %.4f, %.4f, %.4f, %.4f\n", w, x, y, z);
		const float length = sqrtf(x * x + y * y + z * z);
		float targetLength = 1.0f - w * w;
		if (targetLength <= 0.0f || length <= 0.0f)
		{
			Set(1.0f, 0.0f, 0.0f, 0.0f);
			return;
		}
		targetLength = sqrtf(targetLength);
		const float fixFactor = targetLength / length;

		x *= fixFactor;
		y *= fixFactor;
		z *= fixFactor;

		//printf("Normalized: %.4f, %.4f, %.4f, %.4f\n", w, x, y, z);
		return;
	}

	Quat Quat::Normalized() const
	{
		Quat result = *this;
		result.Normalize();
		return result;
	}

	void Quat::Invert()
	{
		x = -x;
		y = -y;
		z = -z;
		return;
	}

	Quat Quat::Inverse() const
	{
		Quat result = *this;
		result.Invert();
		return result;
	}

	Vec::Vec()
	{
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
	}

	Vec::Vec(float inX, float inY, float inZ)
	{
		x = inX;
		y = inY;
		z = inZ;
	}

	void Vec::Set(float inX, float inY, float inZ)
	{
		x = inX;
		y = inY;
		z = inZ;
	}

	float Vec::Length() const
	{
		return sqrtf(x * x + y * y + z * z);
	}

	void Vec::Normalize()
	{
		const float length = Length();
		if (length == 0.0)
		{
			return;
		}
		const float fixFactor = 1.0f / length;

		x *= fixFactor;
		y *= fixFactor;
		z *= fixFactor;
		return;
	}

	Vec Vec::Normalized() const
	{
		Vec result = *this;
		result.Normalize();
		return result;
	}

	Vec& Vec::operator+=(const Vec& rhs)
	{
		Set(x + rhs.x, y + rhs.y, z + rhs.z);
		return *this;
	}

	Vec operator+(Vec lhs, const Vec& rhs)
	{
		lhs += rhs;
		return lhs;
	}

	Vec& Vec::operator-=(const Vec& rhs)
	{
		Set(x - rhs.x, y - rhs.y, z - rhs.z);
		return *this;
	}

	Vec operator-(Vec lhs, const Vec& rhs)
	{
		lhs -= rhs;
		return lhs;
	}

	Vec& Vec::operator*=(const float rhs)
	{
		Set(x * rhs, y * rhs, z * rhs);
		return *this;
	}

	Vec operator*(Vec lhs, const float rhs)
	{
		lhs *= rhs;
		return lhs;
	}

	Vec& Vec::operator/=(const float rhs)
	{
		Set(x / rhs, y / rhs, z / rhs);
		return *this;
	}

	Vec operator/(Vec lhs, const float rhs)
	{
		lhs /= rhs;
		return lhs;
	}

	Vec& Vec::operator*=(const Quat& rhs)
	{
		Quat temp = rhs * Quat(0.0f, x, y, z) * rhs.Inverse();
		Set(temp.x, temp.y, temp.z);
		return *this;
	}

	Vec operator*(Vec lhs, const Quat& rhs)
	{
		lhs *= rhs;
		return lhs;
	}

	Vec Vec::operator-() const
	{
		Vec result = Vec(-x, -y, -z);
		return result;
	}

	float Vec::Dot(const Vec& other) const
	{
		return x * other.x + y * other.y + z * other.z;
	}

	Vec Vec::Cross(const Vec& other) const
	{
		return Vec(y * other.z - z * other.y,
			z * other.x - x * other.z,
			x * other.y - y * other.x);
	}

	Motion::Motion()
	{
		Reset();
	}

	void Motion::Reset()
	{
		Quaternion.Set(1.0f, 0.0f, 0.0f, 0.0f);
		Accel.Set(0.0f, 0.0f, 0.0f);
		Grav.Set(0.0f, 0.0f, 0.0f);
		NumGravDirectionSamplesCounted = 0;
	}

	/// <summary>
	/// The gyro inputs should be calibrated degrees per second but have no other processing. Acceleration is in G units (1 = approx. 9.8m/s^2)
	/// </summary>
	void Motion::Update(float inGyroX, float inGyroY, float inGyroZ, float inAccelX, float inAccelY, float inAccelZ, float gravityLength, float deltaTime)
	{
		const Vec axis = Vec(inGyroX, inGyroY, inGyroZ);
		const Vec accel = Vec(inAccelX, inAccelY, inAccelZ);
		float angle = axis.Length() * (float)M_PI / 180.0f;
		angle *= deltaTime;

		// rotate
		Quat rotation = AngleAxis(angle, axis.x, axis.y, axis.z);
		Quaternion *= rotation; // do it this way because it's a local rotation, not global
		//printf("Quat: %.4f %.4f %.4f %.4f _",
		//	Quaternion.w, Quaternion.x, Quaternion.y, Quaternion.z);
		float accelMagnitude = accel.Length();
		if (accelMagnitude > 0.0f)
		{
			const Vec accelNorm = accel / accelMagnitude;
			LastGravityIdx = (LastGravityIdx + NumGravDirectionSamples - 1) % NumGravDirectionSamples;
			// for comparing and perhaps smoothing gravity samples, we need them to be global
			Vec absoluteAccel = accel * Quaternion;
			//printf("Absolute Accel: %.4f %.4f %.4f\n",
			//	absoluteAccel.x, absoluteAccel.y, absoluteAccel.z);
			GravDirectionSamples[LastGravityIdx] = absoluteAccel;
			Vec gravityMin = absoluteAccel;
			Vec gravityMax = absoluteAccel;
			const float steadyGravityThreshold = 0.05f;
			NumGravDirectionSamplesCounted++;
			const int numGravSamples = NumGravDirectionSamplesCounted < NumGravDirectionSamples ? NumGravDirectionSamplesCounted : NumGravDirectionSamples;
			for (int idx = 1; idx < numGravSamples; idx++)
			{
				Vec thisSample = GravDirectionSamples[(LastGravityIdx + idx) % NumGravDirectionSamples];
				if (thisSample.x > gravityMax.x)
				{
					gravityMax.x = thisSample.x;
				}
				if (thisSample.y > gravityMax.y)
				{
					gravityMax.y = thisSample.y;
				}
				if (thisSample.z > gravityMax.z)
				{
					gravityMax.z = thisSample.z;
				}
				if (thisSample.x < gravityMin.x)
				{
					gravityMin.x = thisSample.x;
				}
				if (thisSample.y < gravityMin.y)
				{
					gravityMin.y = thisSample.y;
				}
				if (thisSample.y < gravityMin.y)
				{
					gravityMin.z = thisSample.z;
				}
			}
			const Vec gravityBoxSize = gravityMax - gravityMin;
			//printf(" Gravity Box Size: %.4f _ ", gravityBoxSize.Length());
			if (gravityBoxSize.x <= steadyGravityThreshold &&
				gravityBoxSize.y <= steadyGravityThreshold &&
				gravityBoxSize.z <= steadyGravityThreshold)
			{
				absoluteAccel = gravityMin + (gravityBoxSize * 0.5f);
				const Vec gravityDirection = -absoluteAccel.Normalized();
				const Vec expectedGravity = Vec(0.0f, -1.0f, 0.0f) * Quaternion.Inverse();
				const float errorAngle = acosf(Vec(0.0f, -1.0f, 0.0f).Dot(gravityDirection)) * 180.0f / (float)M_PI;

				const Vec flattened = gravityDirection.Cross(Vec(0.0f, -1.0f, 0.0f)).Normalized();

				if (errorAngle > 0.0f)
				{
					const float EaseInTime = 0.25f;
					TimeCorrecting += deltaTime;

					const float tighteningThreshold = 5.0f;

					float confidentSmoothCorrect = errorAngle;
					confidentSmoothCorrect *= 1.0f - exp2f(-deltaTime * 4.0f);

					if (TimeCorrecting < EaseInTime)
					{
						confidentSmoothCorrect *= TimeCorrecting / EaseInTime;
					}

					Quaternion = AngleAxis(confidentSmoothCorrect * (float)M_PI / 180.0f, flattened.x, flattened.y, flattened.z) * Quaternion;
				}
				else
				{
					TimeCorrecting = 0.0f;
				}

				Grav = Vec(0.0f, -gravityLength, 0.0f) * Quaternion.Inverse();
				Accel = accel + Grav; // gravity won't be shaky. accel might. so let's keep using the quaternion's calculated gravity vector.
			}
			else
			{
				TimeCorrecting = 0.0f;
				Grav = Vec(0.0f, -gravityLength, 0.0f) * Quaternion.Inverse();
				Accel = accel + Grav;
			}
		}
		else
		{
			TimeCorrecting = 0.0f;
			Accel.Set(0.0f, 0.0f, 0.0f);
		}
		Quaternion.Normalize();
	}

	SensorMinMaxWindow::SensorMinMaxWindow()
	{
		Reset(0.f);
	}

	void SensorMinMaxWindow::Reset(float remainder)
	{
		NumSamples = 0;
		TimeSampled = remainder;
	}

	void SensorMinMaxWindow::AddSample(const Vec& inGyro, const Vec& inAccel, float deltaTime)
	{
		if (NumSamples == 0)
		{
			MaxGyro = inGyro;
			MinGyro = inGyro;
			MaxAccel = inAccel;
			MinAccel = inAccel;
			NumSamples = 1;
			TimeSampled += deltaTime;
			return;
		}

		if (inGyro.x > MaxGyro.x)
		{
			MaxGyro.x = inGyro.x;
		}
		else if (inGyro.x < MinGyro.x)
		{
			MinGyro.x = inGyro.x;
		}

		if (inGyro.y > MaxGyro.y)
		{
			MaxGyro.y = inGyro.y;
		}
		else if (inGyro.y < MinGyro.y)
		{
			MinGyro.y = inGyro.y;
		}

		if (inGyro.z > MaxGyro.z)
		{
			MaxGyro.z = inGyro.z;
		}
		else if (inGyro.z < MinGyro.z)
		{
			MinGyro.z = inGyro.z;
		}

		if (inAccel.x > MaxAccel.x)
		{
			MaxAccel.x = inAccel.x;
		}
		else if (inAccel.x < MinAccel.x)
		{
			MinAccel.x = inAccel.x;
		}

		if (inAccel.y > MaxAccel.y)
		{
			MaxAccel.y = inAccel.y;
		}
		else if (inAccel.y < MinAccel.y)
		{
			MinAccel.y = inAccel.y;
		}

		if (inAccel.z > MaxAccel.z)
		{
			MaxAccel.z = inAccel.z;
		}
		else if (inAccel.z < MinAccel.z)
		{
			MinAccel.z = inAccel.z;
		}

		NumSamples++;
		TimeSampled += deltaTime;
	}

	Vec SensorMinMaxWindow::GetMedianGyro()
	{
		return (MaxGyro + MinGyro) * 0.5f;
	}

	AutoCalibration::AutoCalibration()
	{
		CalibrationData = nullptr;
		for (int Idx = 0; Idx < NumWindows; Idx++)
		{
			// -1/x, -2/x, -3/x, etc
			MinMaxWindows[Idx].TimeSampled = MinAutoWindowTime * (-Idx / (float)NumWindows);
		}
	}

	bool AutoCalibration::AddSample(const Vec& inGyro, const Vec& inAccel, float deltaTime)
	{
		bool calibrated = false;
		MinDeltaGyroX += MinClimbRate * deltaTime;
		MinDeltaGyroY += MinClimbRate * deltaTime;
		MinDeltaGyroZ += MinClimbRate * deltaTime;
		MinDeltaAccelX += MinClimbRate * deltaTime;
		MinDeltaAccelY += MinClimbRate * deltaTime;
		MinDeltaAccelZ += MinClimbRate * deltaTime;

		RecalibrateThreshold += RecalibrateClimbRate * deltaTime;
		if (RecalibrateThreshold > MaxRecalibrateThreshold) RecalibrateThreshold = MaxRecalibrateThreshold;

		for (int Idx = 0; Idx < NumWindows; Idx++)
		{
			SensorMinMaxWindow* thisSample = &MinMaxWindows[Idx];
			SensorMinMaxWindow* otherSample = &MinMaxWindows[(Idx + NumWindows - 1) % NumWindows];
			thisSample->AddSample(inGyro, inAccel, deltaTime);
			if (thisSample->NumSamples < MinAutoWindowSamples || thisSample->TimeSampled < MinAutoWindowTime)
			{
				continue;
			}

			// get deltas
			Vec gyroDelta = thisSample->MaxGyro - thisSample->MinGyro;
			Vec accelDelta = thisSample->MaxAccel - thisSample->MinAccel;

			if (gyroDelta.x < MinDeltaGyroX) MinDeltaGyroX = gyroDelta.x;
			if (gyroDelta.y < MinDeltaGyroY) MinDeltaGyroY = gyroDelta.y;
			if (gyroDelta.z < MinDeltaGyroZ) MinDeltaGyroZ = gyroDelta.z;
			if (accelDelta.x < MinDeltaAccelX) MinDeltaAccelX = accelDelta.x;
			if (accelDelta.y < MinDeltaAccelY) MinDeltaAccelY = accelDelta.y;
			if (accelDelta.z < MinDeltaAccelZ) MinDeltaAccelZ = accelDelta.z;

			if (gyroDelta.x < MinDeltaGyroX * RecalibrateThreshold &&
				gyroDelta.x < MinDeltaGyroY * RecalibrateThreshold &&
				gyroDelta.x < MinDeltaGyroZ * RecalibrateThreshold &&
				accelDelta.x < MinDeltaAccelX * RecalibrateThreshold &&
				accelDelta.x < MinDeltaAccelY * RecalibrateThreshold &&
				accelDelta.x < MinDeltaAccelZ * RecalibrateThreshold)
			{
				printf("Recalibrating... with gyro deltas: %.2f, %.2f, %.2f and accel deltas: %.2f, %.2f, %.2f\n",
					gyroDelta.x, gyroDelta.y, gyroDelta.z,
					accelDelta.x, accelDelta.y, accelDelta.z);

				RecalibrateThreshold -= RecalibrateDrop;
				if (RecalibrateThreshold < 1.f) RecalibrateThreshold = 1.f;

				if (CalibrationData != nullptr)
				{
					Vec calibratedGyro = thisSample->GetMedianGyro();

					CalibrationData->X = calibratedGyro.x;
					CalibrationData->Y = calibratedGyro.y;
					CalibrationData->Z = calibratedGyro.z;

					CalibrationData->AccelMagnitude = (thisSample->MaxAccel + thisSample->MinAccel).Length() * 0.5;

					CalibrationData->NumSamples = 1;

					calibrated = true;
				}
			}

			if (otherSample->TimeSampled + deltaTime >= MinAutoWindowTime)
			{
				thisSample->Reset(MinAutoWindowTime / (float)NumWindows);
			}
			else
			{
				thisSample->Reset(otherSample->TimeSampled - (MinAutoWindowTime / (float)NumWindows)); // keep in sync with other windows
			}
		}

		return calibrated;
	}

	void AutoCalibration::SetCalibrationData(GyroCalibration* calibrationData)
	{
		CalibrationData = calibrationData;
	}

} // namespace GamepadMotionHelpers

GamepadMotion::GamepadMotion()
{
	IsCalibrating = false;
	CurrentCalibrationMode = CalibrationMode::Basic;
	Reset();
	AutoCalibration.SetCalibrationData(&GyroCalibration);
}

void GamepadMotion::Reset()
{
	GyroCalibration = {};
	Gyro = {};
	RawAccel = {};
	Motion.Reset();
}

void GamepadMotion::ProcessMotion(float gyroX, float gyroY, float gyroZ,
	float accelX, float accelY, float accelZ, float deltaTime)
{
	float accelMagnitude = sqrtf(accelX * accelX + accelY * accelY + accelZ * accelZ);

	switch (CurrentCalibrationMode)
	{
	case CalibrationMode::Basic:
		if (IsCalibrating)
		{
			PushSensorSamples(gyroX, gyroY, gyroZ, accelMagnitude);
		}
		break;
	case CalibrationMode::Auto:
		AutoCalibration.AddSample(GamepadMotionHelpers::Vec(gyroX, gyroY, gyroZ), GamepadMotionHelpers::Vec(accelX, accelY, accelZ), deltaTime);
		break;
	}

	float gyroOffsetX, gyroOffsetY, gyroOffsetZ;
	GetCalibratedSensor(gyroOffsetX, gyroOffsetY, gyroOffsetZ, accelMagnitude);

	gyroX -= gyroOffsetX;
	gyroY -= gyroOffsetY;
	gyroZ -= gyroOffsetZ;

	Motion.Update(gyroX, gyroY, gyroZ, accelX, accelY, accelZ, accelMagnitude, deltaTime);

	Gyro.x = gyroX;
	Gyro.y = gyroY;
	Gyro.z = gyroZ;
	RawAccel.x = accelX;
	RawAccel.y = accelY;
	RawAccel.z = accelZ;
}

// reading the current state
void GamepadMotion::GetCalibratedGyro(float& x, float& y, float& z)
{
	x = Gyro.x;
	y = Gyro.y;
	z = Gyro.z;
}

void GamepadMotion::GetGravity(float& x, float& y, float& z)
{
	x = Motion.Grav.x;
	y = Motion.Grav.y;
	z = Motion.Grav.z;
}

void GamepadMotion::GetProcessedAcceleration(float& x, float& y, float& z)
{
	x = Motion.Accel.x;
	y = Motion.Accel.y;
	z = Motion.Accel.z;
}

void GamepadMotion::GetOrientation(float& w, float& x, float& y, float& z)
{
	w = Motion.Quaternion.w;
	x = Motion.Quaternion.x;
	y = Motion.Quaternion.y;
	z = Motion.Quaternion.z;
}

// gyro calibration functions
void GamepadMotion::StartContinuousCalibration()
{
	IsCalibrating = true;
}

void GamepadMotion::PauseContinuousCalibration()
{
	IsCalibrating = false;
}

void GamepadMotion::ResetContinuousCalibration()
{
	GyroCalibration = {};
}

void GamepadMotion::GetCalibrationOffset(float& xOffset, float& yOffset, float& zOffset)
{
	float accelMagnitude;
	GetCalibratedSensor(xOffset, yOffset, zOffset, accelMagnitude);
}

void GamepadMotion::SetCalibrationOffset(float xOffset, float yOffset, float zOffset, int weight)
{
	if (GyroCalibration.NumSamples > 1)
	{
		GyroCalibration.AccelMagnitude *= ((float)weight) / GyroCalibration.NumSamples;
	}
	else
	{
		GyroCalibration.AccelMagnitude = weight;
	}

	GyroCalibration.NumSamples = weight;
	GyroCalibration.X = xOffset * weight;
	GyroCalibration.Y = yOffset * weight;
	GyroCalibration.Z = zOffset * weight;
}

CalibrationMode GamepadMotion::GetCalibrationMode()
{
	return CurrentCalibrationMode;
}

void GamepadMotion::SetCalibrationMode(CalibrationMode calibrationMode)
{
	CurrentCalibrationMode = calibrationMode;
}

void GamepadMotion::ResetMotion()
{
	Motion.Reset();
}

// Private Methods

void GamepadMotion::PushSensorSamples(float gyroX, float gyroY, float gyroZ, float accelMagnitude)
{
	// accumulate
	GyroCalibration.NumSamples++;
	GyroCalibration.X += gyroX;
	GyroCalibration.Y += gyroY;
	GyroCalibration.Z += gyroZ;
	GyroCalibration.AccelMagnitude += accelMagnitude;
}

void GamepadMotion::GetCalibratedSensor(float& gyroOffsetX, float& gyroOffsetY, float& gyroOffsetZ, float& accelMagnitude)
{
	if (GyroCalibration.NumSamples <= 0)
	{
		gyroOffsetX = 0.f;
		gyroOffsetY = 0.f;
		gyroOffsetZ = 0.f;
		accelMagnitude = 0.f;
		return;
	}

	float inverseSamples = 1.f / GyroCalibration.NumSamples;
	gyroOffsetX = GyroCalibration.X * inverseSamples;
	gyroOffsetY = GyroCalibration.Y * inverseSamples;
	gyroOffsetZ = GyroCalibration.Z * inverseSamples;
	accelMagnitude = GyroCalibration.AccelMagnitude * inverseSamples;
}
