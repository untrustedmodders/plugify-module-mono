using System;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

namespace Plugify
{
	[StructLayout(LayoutKind.Sequential)]
	public struct Quaternion : IEquatable<Quaternion>, IFormattable
	{
		public float x;
		public float y;
		public float z;
		public float w;

		public float this[int index]
		{
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			get
			{
				switch (index)
				{
					case 0: return x;
					case 1: return y;
					case 2: return z;
					case 3: return w;
					default:
						throw new IndexOutOfRangeException("Invalid Quaternion index!");
				}
			}

			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			set
			{
				switch (index)
				{
					case 0: x = value; break;
					case 1: y = value; break;
					case 2: z = value; break;
					case 3: w = value; break;
					default:
						throw new IndexOutOfRangeException("Invalid Quaternion index!");
				}
			}
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public Quaternion(float x, float y, float z, float w) { this.x = x; this.y = y; this.z = z; this.w = w; }

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Set(float newX, float newY, float newZ, float newW)
		{
			x = newX;
			y = newY;
			z = newZ;
			w = newW;
		}

		public static Quaternion Identity
		{
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			get;
		} = new Quaternion(0F, 0F, 0F, 1F);

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Quaternion operator*(Quaternion lhs, Quaternion rhs)
		{
			return new Quaternion(
				lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
				lhs.w * rhs.y + lhs.y * rhs.w + lhs.z * rhs.x - lhs.x * rhs.z,
				lhs.w * rhs.z + lhs.z * rhs.w + lhs.x * rhs.y - lhs.y * rhs.x,
				lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z);
		}

		public static Vector3 operator*(Quaternion rotation, Vector3 point)
		{
			float x = rotation.x * 2F;
			float y = rotation.y * 2F;
			float z = rotation.z * 2F;
			float xx = rotation.x * x;
			float yy = rotation.y * y;
			float zz = rotation.z * z;
			float xy = rotation.x * y;
			float xz = rotation.x * z;
			float yz = rotation.y * z;
			float wx = rotation.w * x;
			float wy = rotation.w * y;
			float wz = rotation.w * z;

			Vector3 res;
			res.x = (1F - (yy + zz)) * point.x + (xy - wz) * point.y + (xz + wy) * point.z;
			res.y = (xy + wz) * point.x + (1F - (xx + zz)) * point.y + (yz - wx) * point.z;
			res.z = (xz - wy) * point.x + (yz + wx) * point.y + (1F - (xx + yy)) * point.z;
			return res;
		}

		public const float kEpsilon = 0.000001F;

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		private static bool IsEqualUsingDot(float dot)
		{
			return dot > 1.0f - kEpsilon;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool operator==(Quaternion lhs, Quaternion rhs)
		{
			return IsEqualUsingDot(Dot(lhs, rhs));
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool operator!=(Quaternion lhs, Quaternion rhs)
		{
			return !(lhs == rhs);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Dot(Quaternion a, Quaternion b)
		{
			return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Angle(Quaternion a, Quaternion b)
		{
			float dot = Math.Min(Math.Abs(Dot(a, b)), 1.0F);
			return IsEqualUsingDot(dot) ? 0.0f : (float) Math.Acos(dot) * 2.0F * Mathf.Rad2Deg;
		}

		private static Vector3 MakePositive(Vector3 euler)
		{
			float negativeFlip = -0.0001f * Mathf.Rad2Deg;
			float positiveFlip = 360.0f + negativeFlip;

			if (euler.x < negativeFlip)
				euler.x += 360.0f;
			else if (euler.x > positiveFlip)
				euler.x -= 360.0f;

			if (euler.y < negativeFlip)
				euler.y += 360.0f;
			else if (euler.y > positiveFlip)
				euler.y -= 360.0f;

			if (euler.z < negativeFlip)
				euler.z += 360.0f;
			else if (euler.z > positiveFlip)
				euler.z -= 360.0f;

			return euler;
		}

		public Vector3 EulerAngles
		{
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			get => MakePositive(ToEulerAngles(this) * Mathf.Rad2Deg);
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			set => this = ToQuaternion(value * Mathf.Deg2Rad);
		}
		
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Quaternion Euler(float x, float y, float z) { return ToQuaternion(new Vector3(x, y, z) * Mathf.Deg2Rad); }
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Quaternion Euler(Vector3 euler) { return ToQuaternion(euler * Mathf.Deg2Rad); }

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Quaternion Normalize(Quaternion q)
		{
			float mag = (float) Math.Sqrt(Dot(q, q));
			if (mag < kEpsilon)
				return Quaternion.Identity;

			return new Quaternion(q.x / mag, q.y / mag, q.z / mag, q.w / mag);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Normalize()
		{
			this = Normalize(this);
		}

		public Quaternion Normalized
		{
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			get => Normalize(this);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public override int GetHashCode()
		{
			return x.GetHashCode() ^ (y.GetHashCode() << 2) ^ (z.GetHashCode() >> 2) ^ (w.GetHashCode() >> 1);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public override bool Equals(object other)
		{
			if (!(other is Quaternion quaternion)) return false;

			return Equals(quaternion);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public bool Equals(Quaternion other)
		{
			return x.Equals(other.x) && y.Equals(other.y) && z.Equals(other.z) && w.Equals(other.w);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public override string ToString()
		{
			return ToString(null);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public string ToString(string format, IFormatProvider formatProvider = null)
		{
			if (string.IsNullOrEmpty(format))
				format = "F5";
			if (formatProvider == null)
				formatProvider = CultureInfo.InvariantCulture.NumberFormat;
			return $"({x.ToString(format, formatProvider)}, {y.ToString(format, formatProvider)}, {z.ToString(format, formatProvider)}, {w.ToString(format, formatProvider)})";
		}

		private static Quaternion ToQuaternion(Vector3 v)
		{
			double halfRoll = v.z * 0.5;
			double halfPitch = v.y * 0.5;
			double halfYaw = v.x * 0.5;
			
			float cy = (float)Math.Cos(halfRoll);
			float sy = (float)Math.Sin(halfRoll);
			
			float cp = (float)Math.Cos(halfPitch);
			float sp = (float)Math.Sin(halfPitch);
			
			float cr = (float)Math.Cos(halfYaw);
			float sr = (float)Math.Sin(halfYaw);

			return new Quaternion
			{
				w = cr * cp * cy + sr * sp * sy,
				x = sr * cp * cy - cr * sp * sy,
				y = cr * sp * cy + sr * cp * sy,
				z = cr * cp * sy - sr * sp * cy
			};
		}

		private static Vector3 ToEulerAngles(Quaternion q)
		{
			Vector3 angles = new Vector3();

			double sinrCosp = 2 * (q.w * q.x + q.y * q.z);
			double cosrCosp = 1 - 2 * (q.x * q.x + q.y * q.y);
			angles.x = (float)Math.Atan2(sinrCosp, cosrCosp);

			double sinp = 2 * (q.w * q.y - q.z * q.x);
			if (Math.Abs(sinp) >= 1)
			{
				angles.y = (float)Mathf.CopySign((Math.PI / 2), sinp);
			}
			else
			{
				angles.y = (float)Math.Asin(sinp);
			}

			double sinyCosp = 2 * (q.w * q.z + q.x * q.y);
			double cosyCosp = 1 - 2 * (q.y * q.y + q.z * q.z);
			angles.z = (float)Math.Atan2(sinyCosp, cosyCosp);
			return angles;
		}
	}
}