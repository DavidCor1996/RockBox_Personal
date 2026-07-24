/* Small freestanding fmod implementation for achievement value expressions. */
double fmod(double value, double divisor)
{
    long quotient;

    if (divisor == 0.0)
        return 0.0;
    quotient = (long)(value / divisor);
    return value - (double)quotient * divisor;
}
