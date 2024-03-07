using System;
using System.Runtime.InteropServices;
using System.Globalization;
using System.Runtime.CompilerServices;

namespace Plugify
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2 : IEquatable<Vector2>, IFormattable
    {
        private const float kEpsilon = 0.00001F;
        private const float kEpsilonNormalSqrt = 1e-15f;
        
        public float x;
        public float y;

        public float this[int index]
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get
            {
                switch (index)
                {
                    case 0: return x;
                    case 1: return y;
                    default:
                        throw new IndexOutOfRangeException("Invalid Vector2 index!");
                }
            }

            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            set
            {
                switch (index)
                {
                    case 0: x = value; break;
                    case 1: y = value; break;
                    default:
                        throw new IndexOutOfRangeException("Invalid Vector2 index!");
                }
            }
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public Vector2(float x, float y) { this.x = x; this.y = y; }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public void Set(float newX, float newY) { x = newX; y = newY; }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 Lerp(Vector2 a, Vector2 b, float t)
        {
            t = Mathf.Clamp01(t);
            return new Vector2(
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t
            );
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 LerpUnclamped(Vector2 a, Vector2 b, float t)
        {
            return new Vector2(
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t
            );
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 MoveTowards(Vector2 current, Vector2 target, float maxDistanceDelta)
        {
            float toVectorX = target.x - current.x;
            float toVectorY = target.y - current.y;

            float sqDist = toVectorX * toVectorX + toVectorY * toVectorY;

            if (sqDist == 0 || (maxDistanceDelta >= 0 && sqDist <= maxDistanceDelta * maxDistanceDelta))
                return target;

            float dist = (float)Math.Sqrt(sqDist);

            return new Vector2(current.x + toVectorX / dist * maxDistanceDelta,
                current.y + toVectorY / dist * maxDistanceDelta);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 Scale(Vector2 a, Vector2 b) { return new Vector2(a.x * b.x, a.y * b.y); }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public void Scale(Vector2 scale) { x *= scale.x; y *= scale.y; }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public void Normalize()
        {
            float mag = Length;
            if (mag > kEpsilon)
                this = this / mag;
            else
                this = Zero;
        }

        public Vector2 Normalized
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get
            {
                Vector2 v = new Vector2(x, y);
                v.Normalize();
                return v;
            }
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
            return $"({x.ToString(format, formatProvider)}, {y.ToString(format, formatProvider)})";
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public override int GetHashCode()
        {
            return x.GetHashCode() ^ (y.GetHashCode() << 2);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public override bool Equals(object other)
        {
            if (!(other is Vector2)) return false;

            return Equals((Vector2)other);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Equals(Vector2 other)
        {
            return x == other.x && y == other.y;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 Reflect(Vector2 inDirection, Vector2 inNormal)
        {
            float factor = -2F * Dot(inNormal, inDirection);
            return new Vector2(factor * inNormal.x + inDirection.x, factor * inNormal.y + inDirection.y);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 Perpendicular(Vector2 inDirection)
        {
            return new Vector2(-inDirection.y, inDirection.x);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Dot(Vector2 lhs, Vector2 rhs) { return lhs.x * rhs.x + lhs.y * rhs.y; }

        public float Length { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (float)Math.Sqrt(x * x + y * y); }
        public float LengthSquared { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => x * x + y * y; }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Angle(Vector2 from, Vector2 to)
        {
            float denominator = (float)Math.Sqrt(from.LengthSquared * to.LengthSquared);
            if (denominator < kEpsilonNormalSqrt)
                return 0F;

            float dot = Mathf.Clamp(Dot(from, to) / denominator, -1F, 1F);
            return (float)Math.Acos(dot) * Mathf.Rad2Deg;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float SignedAngle(Vector2 from, Vector2 to)
        {
            float unsignedAngle = Angle(from, to);
            float sign = Math.Sign(from.x * to.y - from.y * to.x);
            return unsignedAngle * sign;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Distance(Vector2 a, Vector2 b)
        {
            float diffX = a.x - b.x;
            float diffY = a.y - b.y;
            return (float)Math.Sqrt(diffX * diffX + diffY * diffY);
        }
        
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Magnitude(Vector4 a) { return (float)Math.Sqrt(Dot(a, a)); }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 ClampMagnitude(Vector2 vector, float maxLength)
        {
            float sqrMagnitude = vector.LengthSquared;
            if (sqrMagnitude > maxLength * maxLength)
            {
                float mag = (float)Math.Sqrt(sqrMagnitude);

                //of float precision. without this, the intermediate result can be of higher
                float normalizedX = vector.x / mag;
                float normalizedY = vector.y / mag;
                return new Vector2(normalizedX * maxLength,
                    normalizedY * maxLength);
            }
            return vector;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float SqrMagnitude(Vector2 a) { return a.x * a.x + a.y * a.y; }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public float SqrMagnitude() { return x * x + y * y; }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 Min(Vector2 lhs, Vector2 rhs) { return new Vector2(Math.Min(lhs.x, rhs.x), Math.Min(lhs.y, rhs.y)); }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 Max(Vector2 lhs, Vector2 rhs) { return new Vector2(Math.Max(lhs.x, rhs.x), Math.Max(lhs.y, rhs.y)); }

        public static Vector2 SmoothDamp(Vector2 current, Vector2 target, ref Vector2 currentVelocity, float smoothTime,float maxSpeed, float deltaTime)
        {
            smoothTime = Math.Max(0.0001F, smoothTime);
            float omega = 2F / smoothTime;

            float x = omega * deltaTime;
            float exp = 1F / (1F + x + 0.48F * x * x + 0.235F * x * x * x);

            float changeX = current.x - target.x;
            float changeY = current.y - target.y;
            Vector2 originalTo = target;

            float maxChange = maxSpeed * smoothTime;

            float maxChangeSq = maxChange * maxChange;
            float sqDist = changeX * changeX + changeY * changeY;
            if (sqDist > maxChangeSq)
            {
                var mag = (float)Math.Sqrt(sqDist);
                changeX = changeX / mag * maxChange;
                changeY = changeY / mag * maxChange;
            }

            target.x = current.x - changeX;
            target.y = current.y - changeY;

            float tempX = (currentVelocity.x + omega * changeX) * deltaTime;
            float tempY = (currentVelocity.y + omega * changeY) * deltaTime;

            currentVelocity.x = (currentVelocity.x - omega * tempX) * exp;
            currentVelocity.y = (currentVelocity.y - omega * tempY) * exp;

            float outputX = target.x + (changeX + tempX) * exp;
            float outputY = target.y + (changeY + tempY) * exp;

            float origMinusCurrentX = originalTo.x - current.x;
            float origMinusCurrentY = originalTo.y - current.y;
            float outMinusOrigX = outputX - originalTo.x;
            float outMinusOrigY = outputY - originalTo.y;

            if (origMinusCurrentX * outMinusOrigX + origMinusCurrentY * outMinusOrigY > 0)
            {
                outputX = originalTo.x;
                outputY = originalTo.y;

                currentVelocity.x = (outputX - originalTo.x) / deltaTime;
                currentVelocity.y = (outputY - originalTo.y) / deltaTime;
            }
            return new Vector2(outputX, outputY);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 operator+(Vector2 a, Vector2 b) { return new Vector2(a.x + b.x, a.y + b.y); }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 operator-(Vector2 a, Vector2 b) { return new Vector2(a.x - b.x, a.y - b.y); }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 operator*(Vector2 a, Vector2 b) { return new Vector2(a.x * b.x, a.y * b.y); }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 operator/(Vector2 a, Vector2 b) { return new Vector2(a.x / b.x, a.y / b.y); }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 operator-(Vector2 a) { return new Vector2(-a.x, -a.y); }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 operator*(Vector2 a, float d) { return new Vector2(a.x * d, a.y * d); }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 operator*(float d, Vector2 a) { return new Vector2(a.x * d, a.y * d); }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static Vector2 operator/(Vector2 a, float d) { return new Vector2(a.x / d, a.y / d); }
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool operator==(Vector2 lhs, Vector2 rhs)
        {
            float diffX = lhs.x - rhs.x;
            float diffY = lhs.y - rhs.y;
            return (diffX * diffX + diffY * diffY) < kEpsilon * kEpsilon;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool operator!=(Vector2 lhs, Vector2 rhs)
        {
            return !(lhs == rhs);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static implicit operator Vector2(Vector3 v)
        {
            return new Vector2(v.x, v.y);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static implicit operator Vector3(Vector2 v)
        {
            return new Vector3(v.x, v.y, 0);
        }
        
        public static Vector2 Zero
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get;
        } = new Vector2(0F, 0F);
        public static Vector2 One
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get;
        } = new Vector2(1F, 1F);
        public static Vector2 Up
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get;
        } = new Vector2(0F, 1F);
        public static Vector2 Down
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get;
        } = new Vector2(0F, -1F);
        public static Vector2 Left
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get;
        } = new Vector2(-1F, 0F);
        public static Vector2 Right
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get;
        } = new Vector2(1F, 0F);
        public static Vector2 PositiveInfinity
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get;
        } = new Vector2(float.PositiveInfinity, float.PositiveInfinity);
        public static Vector2 NegativeInfinity
        {
            [MethodImpl(MethodImplOptions.AggressiveInlining)]
            get;
        } = new Vector2(float.NegativeInfinity, float.NegativeInfinity);

    }
}