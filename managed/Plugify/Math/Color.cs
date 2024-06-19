using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Globalization;

namespace Plugify
{
	[StructLayout(LayoutKind.Sequential)]
	public struct Color : IEquatable<Color>, IFormattable
	{
		public float r;
		public float g;
		public float b;
		public float a;

		public Color(float r, float g, float b, float a)
		{
			this.r = r; this.g = g; this.b = b; this.a = a;
		}

		public Color(float r, float g, float b)
		{
			this.r = r; this.g = g; this.b = b; this.a = 1.0F;
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
				format = "F3";
			if (formatProvider == null)
				formatProvider = CultureInfo.InvariantCulture.NumberFormat;
			return $"RGBA({r.ToString(format, formatProvider)}, {g.ToString(format, formatProvider)}, {b.ToString(format, formatProvider)}, {a.ToString(format, formatProvider)})";
		}

		public override int GetHashCode()
		{
			return ((Vector4)this).GetHashCode();
		}

		public override bool Equals(object other)
		{
			if (!(other is Color)) return false;

			return Equals((Color)other);
		}

		public bool Equals(Color other)
		{
			return r.Equals(other.r) && g.Equals(other.g) && b.Equals(other.b) && a.Equals(other.a);
		}

		public static Color operator+(Color a, Color b) { return new Color(a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a); }

		public static Color operator-(Color a, Color b) { return new Color(a.r - b.r, a.g - b.g, a.b - b.b, a.a - b.a); }

		public static Color operator*(Color a, Color b) { return new Color(a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a); }

		public static Color operator*(Color a, float b) { return new Color(a.r * b, a.g * b, a.b * b, a.a * b); }

		public static Color operator*(float b, Color a) { return new Color(a.r * b, a.g * b, a.b * b, a.a * b); }

		public static Color operator/(Color a, float b) { return new Color(a.r / b, a.g / b, a.b / b, a.a / b); }

		public static bool operator==(Color lhs, Color rhs)
		{
			return (Vector4)lhs == (Vector4)rhs;
		}

		public static bool operator!=(Color lhs, Color rhs)
		{
			return !(lhs == rhs);
		}

		public static Color Lerp(Color a, Color b, float t)
		{
			t = Mathf.Clamp01(t);
			return new Color(
				a.r + (b.r - a.r) * t,
				a.g + (b.g - a.g) * t,
				a.b + (b.b - a.b) * t,
				a.a + (b.a - a.a) * t
			);
		}

		public static Color LerpUnclamped(Color a, Color b, float t)
		{
			return new Color(
				a.r + (b.r - a.r) * t,
				a.g + (b.g - a.g) * t,
				a.b + (b.b - a.b) * t,
				a.a + (b.a - a.a) * t
			);
		}

		internal Color RGBMultiplied(float multiplier) { return new Color(r * multiplier, g * multiplier, b * multiplier, a); }
		internal Color AlphaMultiplied(float multiplier) { return new Color(r, g, b, a * multiplier); }
		internal Color RGBMultiplied(Color multiplier) { return new Color(r * multiplier.r, g * multiplier.g, b * multiplier.b, a); }

		public static Color Red { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => new Color(1F, 0F, 0F, 1F); }
		public static Color Green { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => new Color(0F, 1F, 0F, 1F); }
		public static Color Blue { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => new Color(0F, 0F, 1F, 1F); }
		public static Color White { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => new Color(1F, 1F, 1F, 1F); }
		public static Color Black { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => new Color(0F, 0F, 0F, 1F); }
		public static Color Yellow { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => new Color(1F, 235F / 255F, 4F / 255F, 1F); }
		public static Color Cyan { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => new Color(0F, 1F, 1F, 1F); }
		public static Color Magenta { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => new Color(1F, 0F, 1F, 1F); }
		public static Color Gray { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => new Color(.5F, .5F, .5F, 1F); }
		public static Color Grey { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => new Color(.5F, .5F, .5F, 1F); }
		public static Color Clear { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => new Color(0F, 0F, 0F, 0F); }

		public float Grayscale { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => 0.299F * r + 0.587F * g + 0.114F * b; }

		public float MaxColorComponent => Math.Max(Math.Max(r, g), b);

		public static implicit operator Vector4(Color c)
		{
			return new Vector4(c.r, c.g, c.b, c.a);
		}

		public static implicit operator Color(Vector4 v)
		{
			return new Color(v.x, v.y, v.z, v.w);
		}

		public float this[int index]
		{
			get
			{
				switch (index)
				{
					case 0: return r;
					case 1: return g;
					case 2: return b;
					case 3: return a;
					default:
						throw new IndexOutOfRangeException("Invalid Color index(" + index + ")!");
				}
			}

			set
			{
				switch (index)
				{
					case 0: r = value; break;
					case 1: g = value; break;
					case 2: b = value; break;
					case 3: a = value; break;
					default:
						throw new IndexOutOfRangeException("Invalid Color index(" + index + ")!");
				}
			}
		}

		public static void RGBToHSV(Color rgbColor, out float H, out float S, out float V)
		{
			if ((rgbColor.b > rgbColor.g) && (rgbColor.b > rgbColor.r))
				RGBToHSVHelper((float)4, rgbColor.b, rgbColor.r, rgbColor.g, out H, out S, out V);
			else if (rgbColor.g > rgbColor.r)
				RGBToHSVHelper((float)2, rgbColor.g, rgbColor.b, rgbColor.r, out H, out S, out V);
			else
				RGBToHSVHelper((float)0, rgbColor.r, rgbColor.g, rgbColor.b, out H, out S, out V);
		}

		static void RGBToHSVHelper(float offset, float dominantcolor, float colorone, float colortwo, out float H, out float S, out float V)
		{
			V = dominantcolor;
			if (V != 0)
			{
				float small = 0;
				if (colorone > colortwo) small = colortwo;
				else small = colorone;

				float diff = V - small;

				if (diff != 0)
				{
					S = diff / V;
					H = offset + ((colorone - colortwo) / diff);
				}
				else
				{
					S = 0;
					H = offset + (colorone - colortwo);
				}

				H /= 6;

				if (H < 0)
					H += 1.0f;
			}
			else
			{
				S = 0;
				H = 0;
			}
		}

		public static Color HSVToRGB(float H, float S, float V)
		{
			return HSVToRGB(H, S, V, true);
		}

		public static Color HSVToRGB(float H, float S, float V, bool hdr)
		{
			Color retval = Color.White;
			if (S == 0)
			{
				retval.r = V;
				retval.g = V;
				retval.b = V;
			}
			else if (V == 0)
			{
				retval.r = 0;
				retval.g = 0;
				retval.b = 0;
			}
			else
			{
				retval.r = 0;
				retval.g = 0;
				retval.b = 0;

				var hToFloor = H * 6.0f;

				double temp = Math.Floor(hToFloor);
				float t = hToFloor - ((float)temp);
				float var1 = (V) * (1 - S);
				float var2 = V * (1 - S *  t);
				float var3 = V * (1 - S * (1 - t));

				switch (temp)
				{
					case 0:
						retval.r = V;
						retval.g = var3;
						retval.b = var1;
						break;

					case 1:
						retval.r = var2;
						retval.g = V;
						retval.b = var1;
						break;

					case 2:
						retval.r = var1;
						retval.g = V;
						retval.b = var3;
						break;

					case 3:
						retval.r = var1;
						retval.g = var2;
						retval.b = V;
						break;

					case 4:
						retval.r = var3;
						retval.g = var1;
						retval.b = V;
						break;

					case 5:
						retval.r = V;
						retval.g = var1;
						retval.b = var2;
						break;

					case 6:
						retval.r = V;
						retval.g = var3;
						retval.b = var1;
						break;

					case -1:
						retval.r = V;
						retval.g = var1;
						retval.b = var2;
						break;
				}

				if (!hdr)
				{
					retval.r = Mathf.Clamp(retval.r, 0.0f, 1.0f);
					retval.g = Mathf.Clamp(retval.g, 0.0f, 1.0f);
					retval.b = Mathf.Clamp(retval.b, 0.0f, 1.0f);
				}
			}
			return retval;
		}
	}
}