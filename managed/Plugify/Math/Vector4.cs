using System;
using System.Runtime.InteropServices;
using System.Globalization;
using System.Runtime.CompilerServices;

namespace Plugify
{
	[StructLayout(LayoutKind.Sequential)]
	public struct Vector4 : IEquatable<Vector4>, IFormattable
	{
		private const float kEpsilon = 0.00001F;

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
						throw new IndexOutOfRangeException("Invalid Vector4 index!");
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
						throw new IndexOutOfRangeException("Invalid Vector4 index!");
				}
			}
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public Vector4(float x, float y, float z, float w) { this.x = x; this.y = y; this.z = z; this.w = w; }
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public Vector4(float x, float y, float z) { this.x = x; this.y = y; this.z = z; this.w = 0F; }
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public Vector4(float x, float y) { this.x = x; this.y = y; this.z = 0F; this.w = 0F; }

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Set(float newX, float newY, float newZ, float newW) { x = newX; y = newY; z = newZ; w = newW; }

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 Lerp(Vector4 a, Vector4 b, float t)
		{
			t = Mathf.Clamp01(t);
			return new Vector4(
				a.x + (b.x - a.x) * t,
				a.y + (b.y - a.y) * t,
				a.z + (b.z - a.z) * t,
				a.w + (b.w - a.w) * t
			);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 LerpUnclamped(Vector4 a, Vector4 b, float t)
		{
			return new Vector4(
				a.x + (b.x - a.x) * t,
				a.y + (b.y - a.y) * t,
				a.z + (b.z - a.z) * t,
				a.w + (b.w - a.w) * t
			);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 MoveTowards(Vector4 current, Vector4 target, float maxDistanceDelta)
		{
			float toVectorX = target.x - current.x;
			float toVectorY = target.y - current.y;
			float toVectorZ = target.z - current.z;
			float toVectorW = target.w - current.w;

			float sqdist = (toVectorX * toVectorX +
				toVectorY * toVectorY +
				toVectorZ * toVectorZ +
				toVectorW * toVectorW);

			if (sqdist == 0 || (maxDistanceDelta >= 0 && sqdist <= maxDistanceDelta * maxDistanceDelta))
				return target;

			var dist = (float)Math.Sqrt(sqdist);

			return new Vector4(current.x + toVectorX / dist * maxDistanceDelta,
				current.y + toVectorY / dist * maxDistanceDelta,
				current.z + toVectorZ / dist * maxDistanceDelta,
				current.w + toVectorW / dist * maxDistanceDelta);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 Scale(Vector4 a, Vector4 b)
		{
			return new Vector4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Scale(Vector4 scale)
		{
			x *= scale.x;
			y *= scale.y;
			z *= scale.z;
			w *= scale.w;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public override int GetHashCode()
		{
			return x.GetHashCode() ^ (y.GetHashCode() << 2) ^ (z.GetHashCode() >> 2) ^ (w.GetHashCode() >> 1);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public override bool Equals(object other)
		{
			if (!(other is Vector4)) return false;

			return Equals((Vector4)other);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public bool Equals(Vector4 other)
		{
			return x == other.x && y == other.y && z == other.z && w == other.w;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 Normalize(Vector4 a)
		{
			float mag = Magnitude(a);
			if (mag > kEpsilon)
				return a / mag;
			else
				return Zero;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void Normalize()
		{
			float mag = Magnitude(this);
			if (mag > kEpsilon)
				this = this / mag;
			else
				this = Zero;
		}

		public Vector4 Normalized
		{
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			get => Vector4.Normalize(this);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Dot(Vector4 a, Vector4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 Project(Vector4 a, Vector4 b) { return b * (Dot(a, b) / Dot(b, b)); }

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Distance(Vector4 a, Vector4 b) { return Magnitude(a - b); }

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float Magnitude(Vector4 a) { return (float)Math.Sqrt(Dot(a, a)); }

		public float Length
		{
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			get => (float)Math.Sqrt(Dot(this, this));
		}

		public float LengthSquared
		{
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			get => Dot(this, this);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 Min(Vector4 lhs, Vector4 rhs)
		{
			return new Vector4(Math.Min(lhs.x, rhs.x), Math.Min(lhs.y, rhs.y), Math.Min(lhs.z, rhs.z), Math.Min(lhs.w, rhs.w));
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 Max(Vector4 lhs, Vector4 rhs)
		{
			return new Vector4(Math.Max(lhs.x, rhs.x), Math.Max(lhs.y, rhs.y), Math.Max(lhs.z, rhs.z), Math.Max(lhs.w, rhs.w));
		}

		public static Vector4 Zero
		{
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			get;
		} = new Vector4(0F, 0F, 0F, 0F);

		public static Vector4 One
		{
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			get;
		} = new Vector4(1F, 1F, 1F, 1F);

		public static Vector4 PositiveInfinity
		{
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			get;
		} = new Vector4(float.PositiveInfinity, float.PositiveInfinity, float.PositiveInfinity, float.PositiveInfinity);

		public static Vector4 NegativeInfinity
		{
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			get;
		} = new Vector4(float.NegativeInfinity, float.NegativeInfinity, float.NegativeInfinity, float.NegativeInfinity);

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 operator+(Vector4 a, Vector4 b) { return new Vector4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 operator-(Vector4 a, Vector4 b) { return new Vector4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 operator-(Vector4 a) { return new Vector4(-a.x, -a.y, -a.z, -a.w); }
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 operator*(Vector4 a, float d) { return new Vector4(a.x * d, a.y * d, a.z * d, a.w * d); }
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 operator*(float d, Vector4 a) { return new Vector4(a.x * d, a.y * d, a.z * d, a.w * d); }
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector4 operator/(Vector4 a, float d) { return new Vector4(a.x / d, a.y / d, a.z / d, a.w / d); }

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool operator==(Vector4 lhs, Vector4 rhs)
		{
			float diffx = lhs.x - rhs.x;
			float diffy = lhs.y - rhs.y;
			float diffz = lhs.z - rhs.z;
			float diffw = lhs.w - rhs.w;
			float sqrmag = diffx * diffx + diffy * diffy + diffz * diffz + diffw * diffw;
			return sqrmag < kEpsilon * kEpsilon;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool operator!=(Vector4 lhs, Vector4 rhs)
		{
			return !(lhs == rhs);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static implicit operator Vector4(Vector3 v)
		{
			return new Vector4(v.x, v.y, v.z, 0.0F);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static implicit operator Vector3(Vector4 v)
		{
			return new Vector3(v.x, v.y, v.z);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static implicit operator Vector4(Vector2 v)
		{
			return new Vector4(v.x, v.y, 0.0F, 0.0F);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static implicit operator Vector2(Vector4 v)
		{
			return new Vector2(v.x, v.y);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public override string ToString()
		{
			return ToString(null, null);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public string ToString(string format)
		{
			return ToString(format, null);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public string ToString(string format, IFormatProvider formatProvider)
		{
			if (string.IsNullOrEmpty(format))
				format = "F2";
			if (formatProvider == null)
				formatProvider = CultureInfo.InvariantCulture.NumberFormat;
			return $"({x.ToString(format, formatProvider)}, {y.ToString(format, formatProvider)}, {z.ToString(format, formatProvider)}, {w.ToString(format, formatProvider)})";
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static float SqrMagnitude(Vector4 a) { return Vector4.Dot(a, a); }
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public float SqrMagnitude() { return Dot(this, this); }
	}
}