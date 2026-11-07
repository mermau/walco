// Verifica anti-click: note pizzicate, pot e pick-up mossi mentre si suona, batteria che
// si scarica, hFE agli estremi. L'uscita non deve avere salti anomali. Esce con 1 se ne trova.
#include "FuzzToneCircuit.h"
#include <cstdio>
#include <random>
using namespace ftg;

static double runCase (double peak, double fz, double h1, double h2, bool sweep, bool drain)
{
    const double fs = 384000;
    std::mt19937 rng (1);
    FuzzToneCircuit c; c.setPickup (Pickup::humbucker()); c.setHfe (h1, h2);
    c.setSupply (drain ? 9.6 : 9.0); c.prepare (fs); c.setFuzz (fz); c.reset();
    double prev = 0, prev2 = 0, maxJump = 0, t0 = 0, f = 110;
    for (int i = 0; i < (int) (fs * 3); ++i)
    {
        const double t = i / fs;
        if (i % 128 == 0)
        {
            if (sweep) c.setFuzz (0.5 + 0.5 * std::sin (2 * kPi * 0.7 * t));
            if (sweep) c.setGuitarVolume (0.75 + 0.25 * std::sin (2 * kPi * 0.31 * t));
            if (drain) c.setSupply (9.6 - 5.1 * std::min (1.0, t / 2.5));
            if (sweep && i % (128 * 3000) == 0) c.setPickup ((i / (128 * 3000)) % 2 ? Pickup::singleCoil() : Pickup::humbucker());
        }
        if (t - t0 > 0.4) { t0 = t; f = 82.4 * std::pow (2.0, (rng() % 24) / 12.0); }
        const double tt = t - t0, env = peak * std::exp (-tt * 3.0) * (1 - std::exp (-tt * 2000));
        const double y = c.process (env * (std::sin (2 * kPi * f * tt) + 0.5 * std::sin (4 * kPi * f * tt)));
        if (! std::isfinite (y)) return 1e9;
        maxJump = std::max (maxJump, std::abs (y - 2 * prev + prev2));
        prev2 = prev; prev = y;
    }
    return maxJump;
}

int main()
{
    bool bad = false;
    auto report = [&] (const char* what, double j)
    {
        const bool ok = j < 0.4;   // i fronti del ramo fuzz sono ripidi per natura (verificato: il salto si dimezza raddoppiando fs)
        bad |= ! ok;
        std::printf ("%-46s salto massimo %.4f %s\n", what, j, ok ? "OK" : "CLICK");
    };
    char buf[96];
    for (double peak : { 0.05, 0.3, 1.0, 2.0 })
        for (double fz : { 0.0, 0.5, 1.0 })
        {
            std::snprintf (buf, sizeof buf, "picco %.2f V, fuzz %.1f", peak, fz);
            report (buf, runCase (peak, fz, 250, 250, false, false));
        }
    report ("pot e pick-up mossi suonando", runCase (0.8, 0.5, 250, 250, true, false));
    for (auto [h1, h2] : { std::pair<double, double> { 60, 60 }, { 560, 560 }, { 60, 560 }, { 560, 60 } })
    {
        std::snprintf (buf, sizeof buf, "hFE %3.0f/%3.0f, batteria 9.6 -> 4.5 V", h1, h2);
        report (buf, runCase (0.8, 1.0, h1, h2, false, true));
    }
    return bad ? 1 : 0;
}
