using System;

namespace Plugify
{
	internal static class Mathf
	{
		public const float Deg2Rad = (float)Math.PI * 2F / 360F;
		public const float Rad2Deg = 1F / Deg2Rad;
		
		public static float Clamp(float value, float min, float max)
		{
			if (value < min)
				value = min;
			else if (value > max)
				value = max;
			return value;
		}
		
		public static int Clamp(int value, int min, int max)
		{
			if (value < min)
				value = min;
			else if (value > max)
				value = max;
			return value;
		}

		public static float Clamp01(float value)
		{
			if (value < 0F)
				return 0F;
			else if (value > 1F)
				return 1F;
			else
				return value;
		}
		
		public static double CopySign(double x, double y)
		{
			double xx = Math.Abs(x);
			return y < 0 ? -xx : xx;
		}
	}
}