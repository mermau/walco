// Test del modello: punto di lavoro, livelli al variare di FUZZ INCREASE e pick-up, CPU.
#include "FuzzToneCircuit.h"
#include <chrono>
#include <cstdio>
using namespace ftg;

int main()
{
    const double fs = 384000.0;
    FuzzToneCircuit c;
    c.setPickup (Pickup::ideal());
    c.prepare (fs);
    const char* names[] = { "PU", "IN", "B2", "C2", "B1", "C1", "X", "F3", "F1", "OUT" };
    std::printf ("Punto di lavoro (9 V, hFE 250):\n");
    for (int n = 0; n < FuzzToneCircuit::N; ++n) std::printf ("  %-3s %8.4f V\n", names[n], c.nodeVoltage (n));
    const double vc2 = c.nodeVoltage (FuzzToneCircuit::C2), vc1 = c.nodeVoltage (FuzzToneCircuit::C1);
    std::printf ("  Ic(Q2) = %.2f mA, Ic(Q1) = %.1f uA\n", (9 - vc2) / 4.7e3 * 1e3, (9 - vc1) / 820e3 * 1e6);
    bool ok = vc2 > 2.0 && vc2 < 4.0 && vc1 > 0.4 && vc1 < 0.8;

    struct P { const char* n; Pickup p; } pus[] = { { "Ideale", Pickup::ideal() }, { "Single", Pickup::singleCoil() },
                                                     { "Humbuck", Pickup::humbucker() }, { "Linea", Pickup::line() } };
    for (auto& pu : pus)
        for (double fz : { 0.0, 0.5, 1.0 })
            for (double a : { 0.05, 0.3 })
            {
                c.setPickup (pu.p); c.setFuzz (fz); c.reset();
                double rms = 0, pk = 0; long n = 0; int fails = 0;
                const int len = (int) (0.25 * fs);
                for (int i = 0; i < len; ++i)
                {
                    const double o = c.process (a * std::sin (2 * kPi * 196.0 * i / fs));
                    if (! c.lastConverged()) ++fails;
                    if (i > len / 3) { rms += o * o; pk = std::max (pk, std::abs (o)); ++n; }
                }
                rms = std::sqrt (rms / (double) n);
                std::printf ("%-8s fuzz %.1f  in %.2f Vpk -> %.3f Vrms (pk %.3f)  mancate conv. %d\n", pu.n, fz, a, rms, pk, fails);
                ok &= std::isfinite (rms) && fails == 0;
            }

    c.setPickup (Pickup::singleCoil()); c.setFuzz (1.0); c.reset();
    const auto t0 = std::chrono::steady_clock::now();
    double acc = 0;
    for (int i = 0; i < (int) fs * 5; ++i) acc += c.process (0.4 * std::sin (2 * kPi * 330.0 * i / fs));
    const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("CPU a %.0f kHz: %.1f%% di un core [%g]\n", fs / 1000, sec / 5 * 100, acc);
    std::printf (ok ? "OK\n" : "ERRORE\n");
    return ok ? 0 : 1;
}
