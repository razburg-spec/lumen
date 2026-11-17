#pragma once
#include <array>
#include <cmath>
#include <complex>
#include <algorithm>

// Prism's filters. Every section is a biquad designed with magnitude matching: the poles come
// from the analog prototype (impulse invariance), and the zeros are solved so the digital
// response equals the analog one at DC, at Nyquist and at the band's frequency. This keeps bells
// and shelves near the top of the spectrum the same shape as at the bottom (no "cramping"),
// without oversampling and without latency.
namespace PrismFilters
{
    constexpr double pi = 3.14159265358979323846;

    struct Coeffs
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

        // Magnitude response at w radians per sample.
        double magnitude (double w) const
        {
            const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
            return std::abs ((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2));
        }
    };

    // A band is up to four cascaded sections (a 48 dB/oct cut).
    struct Design
    {
        std::array<Coeffs, 4> sections {};
        int count = 0;

        double magnitude (double w) const
        {
            double m = 1.0;
            for (int i = 0; i < count; ++i)
                m *= sections[(size_t) i].magnitude (w);
            return m;
        }
    };

    // Digital poles for an analog pole pair at wp (rad/sample) with damping zeta.
    inline void poles (double wp, double zeta, double& a1, double& a2)
    {
        zeta = std::max (zeta, 1.0e-4);
        if (wp < 0.9 * pi)
        {
            // Impulse invariance: keeps the pole's frequency and bandwidth exactly.
            const double r = std::exp (-zeta * wp);
            a1 = zeta <= 1.0 ? -2.0 * r * std::cos (wp * std::sqrt (1.0 - zeta * zeta))
                             : -2.0 * r * std::cosh (wp * std::sqrt (zeta * zeta - 1.0));
            a2 = r * r;
        }
        else
        {
            // Poles at or above Nyquist (a high shelf near the top): bilinear transform instead.
            const double wa = 2.0 * std::tan (std::min (wp, 0.97 * pi) * 0.5);
            const double a0 = 4.0 + 4.0 * zeta * wa + wa * wa;
            a1 = (2.0 * wa * wa - 8.0) / a0;
            a2 = (4.0 - 4.0 * zeta * wa + wa * wa) / a0;
        }
    }

    // Solve the zeros so |H|^2 matches h0 (DC), hn (Nyquist) and hm (at wm). All arguments are squared magnitudes.
    inline Coeffs matched (double a1, double a2, double h0, double hn, double wm, double hm)
    {
        wm = std::clamp (wm, 1.0e-5, 0.98 * pi);
        const double A0 = (1.0 + a1 + a2) * (1.0 + a1 + a2);
        const double A1 = (1.0 - a1 + a2) * (1.0 - a1 + a2);
        const double A2 = -4.0 * a2;

        const double s = std::sin (wm * 0.5);
        const double phi1 = s * s, phi0 = 1.0 - phi1, phi2 = 4.0 * phi0 * phi1;

        const double B0 = A0 * h0, B1 = A1 * hn;
        const double B2 = (hm * (A0 * phi0 + A1 * phi1 + A2 * phi2) - B0 * phi0 - B1 * phi1) / phi2;

        const double r0 = std::sqrt (B0), r1 = std::sqrt (B1);
        const double W = 0.5 * (r0 + r1);
        const double disc = std::sqrt (std::max (0.0, W * W + B2));

        Coeffs c;
        c.b0 = 0.5 * (W + disc);
        c.b1 = 0.5 * (r0 - r1);
        c.b2 = c.b0 > 1.0e-300 ? -B2 / (4.0 * c.b0) : 0.0;
        c.a1 = a1;
        c.a2 = a2;
        return c;
    }

    // ---- Analog prototypes: squared magnitude at normalised frequency x = w / w0 ----

    // Bell. A = 10^(dB/40).
    inline Coeffs bell (double w0, double gainDb, double q)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        auto mag2 = [=] (double x)
        {
            const double d = 1.0 - x * x;
            return (d * d + (A * x / q) * (A * x / q)) / (d * d + (x / (A * q)) * (x / (A * q)));
        };
        double a1, a2;
        poles (w0, 1.0 / (2.0 * A * q), a1, a2);
        return matched (a1, a2, 1.0, mag2 (pi / w0), w0, A * A * A * A);
    }

    inline Coeffs lowShelf (double w0, double gainDb, double q)
    {
        const double A = std::pow (10.0, gainDb / 40.0), sa = std::sqrt (A);
        auto mag2 = [=] (double x)
        {
            const double n = A - x * x, d = 1.0 - A * x * x, k = A * x * x / (q * q);
            return A * A * (n * n + k) / (d * d + k);
        };
        double a1, a2;
        poles (w0 / sa, 1.0 / (2.0 * q), a1, a2);
        return matched (a1, a2, A * A * A * A, mag2 (pi / w0), w0, mag2 (1.0));
    }

    inline Coeffs highShelf (double w0, double gainDb, double q)
    {
        const double A = std::pow (10.0, gainDb / 40.0), sa = std::sqrt (A);
        auto mag2 = [=] (double x)
        {
            const double n = 1.0 - A * x * x, d = A - x * x, k = A * x * x / (q * q);
            return A * A * (n * n + k) / (d * d + k);
        };
        double a1, a2;
        poles (w0 * sa, 1.0 / (2.0 * q), a1, a2);
        return matched (a1, a2, 1.0, mag2 (pi / w0), w0, mag2 (1.0));
    }

    inline Coeffs highPass2 (double w0, double q)
    {
        auto mag2 = [=] (double x)
        {
            const double d = 1.0 - x * x;
            return (x * x * x * x) / (d * d + (x / q) * (x / q));
        };
        double a1, a2;
        poles (w0, 1.0 / (2.0 * q), a1, a2);
        return matched (a1, a2, 0.0, mag2 (pi / w0), w0, q * q);
    }

    inline Coeffs lowPass2 (double w0, double q)
    {
        auto mag2 = [=] (double x)
        {
            const double d = 1.0 - x * x;
            return 1.0 / (d * d + (x / q) * (x / q));
        };
        double a1, a2;
        poles (w0, 1.0 / (2.0 * q), a1, a2);
        return matched (a1, a2, 1.0, mag2 (pi / w0), w0, q * q);
    }

    // Butterworth cut of order 2 * sections. `resonance` scales the Q of the sharpest section
    // (1 = flat Butterworth, above 1 = a resonant bump at the corner).
    inline Design cut (bool highPass, double w0, int numSections, double resonance)
    {
        Design d;
        d.count = std::clamp (numSections, 1, 4);
        const int order = 2 * d.count;
        for (int i = 0; i < d.count; ++i)
        {
            double q = 1.0 / (2.0 * std::cos (pi * (2.0 * i + 1.0) / (2.0 * order)));
            if (i == d.count - 1)   // the last section is the one with the highest Q
                q *= resonance;
            d.sections[(size_t) i] = highPass ? highPass2 (w0, q) : lowPass2 (w0, q);
        }
        return d;
    }

    // Transposed direct form II biquad, double precision state.
    struct Biquad
    {
        double z1 = 0.0, z2 = 0.0;

        inline double process (const Coeffs& c, double x) noexcept
        {
            const double y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return y;
        }
        void reset() noexcept { z1 = z2 = 0.0; }
    };
}
