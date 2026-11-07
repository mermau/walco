/*
  FuzzToneCircuit.h  —  modello fisico del Walco "Full Frequency Fuzz Tone Generator"
  (circuito identico al Kent EA-3 Fuzz Tone Bender). Indipendente da JUCE.
  Ingresso e uscita in VOLT.
  ---------------------------------------------------------------------------
  Netlist ricostruita dal layout stripboard di tagboardeffects (mirosol), riportata
  ai valori ORIGINALI dove il layout li aveva adattati:
    - 500 kΩ e 50 nF (il layout usa 470 kΩ e 47 nF, come spiega l'autore)
    - niente pull-down da 1 MΩ in ingresso e niente diodo 1N5817: sono aggiunte del layout
    - 470 pF sul ramo fuzz (correzione del thread FSB: lo schema riportava 47 pF)
    - transistor 2SC828 (NPN al silicio), emettitori a massa

   pick-up ─ vol ─ cordone ─ IN ─50n─┬─ B(Q2)       C(Q2) ─4k7─ +9V
                                     ├─500k─ C(Q2)  C(Q2) ─50n── lug 1 ┐
                                     └─10n── C(Q2)  C(Q2) ─100n─ B(Q1)  │ FUZZ INCREASE
   B(Q1) ─330k─ C(Q1) ─820k─ +9V                                       │ 50k lin
   C(Q1) ─470p─ X ─18k─ F3 ─12k─ massa;   F3 = lug 3 ──────────────────┘ cursore = OUT
   OUT ─ cavo verso l'ampli ─ ingresso dell'ampli (1 MΩ)

  Il pedale si aggancia alla cintura: in ingresso c'è il suo cordone corto infilato
  nella chitarra, il cavo lungo è in uscita, verso l'amplificatore. Poiché l'uscita del
  ramo fuzz ha un'impedenza alta, la capacità di quel cavo ne smussa gli acuti estremi.

  Modello dei transistor (2SC828): Ebers-Moll con effetto Early, capacità di giunzione
  e rottura inversa della giunzione base-emettitore (VEBO ≥ 7 V da datasheet): con
  segnali forti la base di Q1 viene spinta oltre i −7 V attraverso i 100 nF.

  Rumore: il rumore termico e shot del circuito calcolato su questi valori resta intorno
  a −107 dBV, sotto la soglia udibile anche con un ampli molto spinto: non viene simulato.

  Metodo: analisi nodale (11 nodi), trapezio per C e L, metodo "K" (DK) con Newton-
  Raphson sulle 4 tensioni di giunzione; punto di lavoro con Eulero implicito.
*/
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace ftg
{
constexpr double kPi = 3.14159265358979323846;

//==============================================================================
struct Components
{
    static constexpr double Vcc   = 9.0;
    static constexpr double Cin   = 50e-9,  Cfb2 = 10e-9,   Rfb2 = 500e3, Rc2 = 4.7e3;
    static constexpr double Ccpl  = 100e-9, Clug1 = 50e-9;
    static constexpr double Rfb1  = 330e3,  Rc1  = 820e3;
    static constexpr double Chp   = 470e-12, R18 = 18e3,    R12  = 12e3;
    static constexpr double Rpot  = 50e3,   Rload = 1e6;
    static constexpr double Ccord   = 80e-12;    // cordone d'ingresso del pedale (corto)
    static constexpr double Cout    = 330e-12;   // cavo di 3 m verso l'ampli + ingresso dell'ampli
    static constexpr double CrecDI  = 470e-12;   // cavo usato per registrare la DI (correzione)
};

// 2SC828 (Matsushita): hFE 130-520 nelle classi Q (130-260), R (180-360), S (260-520);
// VEBO ≥ 7 V, fT 220 MHz. Valore predefinito dell'hFE: 250.
struct BJT
{
    static constexpr double Is = 1e-14, betaF = 250.0, betaR = 5.0, Vt = 0.02585;
    static constexpr double VAF = 80.0;                   // tensione di Early
    static constexpr double BV = 8.0, IBV = 1e-5, NBV = 1.5;   // rottura inversa B-E
    static constexpr double Cbe = 8e-12, Cbc = 3.5e-12;
};

// Pick-up e chitarra: resistenza e induttanza della bobina, capacità propria, carico dei
// potenziometri di volume e tono (Rload = volume ‖ tono, entrambi a 10).
struct Pickup
{
    double R, L, C = 150e-12, Rload = 250e3;
    bool ideal_ = false;
    static constexpr Pickup ideal()      { return { 10e3, 1e-6, 0.0,      1e12,  true }; }  // sorgente resistiva
    static constexpr Pickup singleCoil() { return { 6e3,  2.5,  120e-12,  125e3 }; }        // Strat/Tele, pot 250k
    static constexpr Pickup p90()        { return { 8e3,  6.0,  150e-12,  250e3 }; }        // pot 500k
    static constexpr Pickup humbucker()  { return { 10e3, 5.0,  180e-12,  250e3 }; }        // pot 500k
    static constexpr Pickup line()       { return { 300.0, 1e-6, 0.0,     1e12,  true }; }  // uscita di linea / synth
    double volumePot() const { return 2.0 * Rload; }
    bool operator== (const Pickup& o) const
    {
        return std::abs (R - o.R) < 1e-3 && std::abs (L - o.L) < 1e-9 && std::abs (C - o.C) < 1e-15
            && std::abs (Rload - o.Rload) < 1e-3 && ideal_ == o.ideal_;
    }
};

// Biquad da prototipo analogico (bilineare), doppia precisione
struct AnalogBiquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    void set (double n0, double n1, double n2, double d0, double d1, double d2, double fs)
    {
        const double K = 2.0 * fs, K2 = K * K;
        const double A0 = d0 + d1 * K + d2 * K2;
        b0 = (n0 + n1 * K + n2 * K2) / A0;  b1 = (2.0 * n0 - 2.0 * n2 * K2) / A0;  b2 = (n0 - n1 * K + n2 * K2) / A0;
        a1 = (2.0 * d0 - 2.0 * d2 * K2) / A0;  a2 = (d0 - d1 * K + d2 * K2) / A0;
    }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

//==============================================================================
class FuzzToneCircuit
{
public:
    enum Node { PU = 0, PK, IN, B2, C2, B1, C1, X, F3, F1, OUT, NN };
    static constexpr int N = NN;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        T = 1.0 / fs;
        dcBlock.setCutoff (5.0, fs);
        updateInputCorrection();
        dirty = true;
        reset();
    }

    /** Azzera lo stato e porta il circuito al punto di lavoro DC. */
    void reset()
    {
        hC.fill (0.0); hL = 0.0;
        x = { 0.6, 0.0, 0.6, -2.0 }; xPrev = x;
        v.fill (0.0);

        // accensione con Eulero implicito a passo largo (1 ms): niente oscillazioni
        // numeriche, regime raggiunto in poche centinaia di passi
        const double Treal = T;
        T = 1.0e-3; backwardEuler = true; dirty = true; rebuild();
        const double vNom = vcc;
        for (int n = 0; n < 40; ++n)  { vccNow = vNom * (n + 1) / 40.0; step (0.0, 200, 1e-10); }
        vccNow = vNom;
        for (int n = 0; n < 400; ++n) step (0.0, 200, 1e-10);

        // ripresa col trapezio al passo reale: a regime i_C = 0, i_L = corrente continua
        const double iL = hL;
        T = Treal; backwardEuler = false; dirty = true; rebuild();
        for (int k = 0; k < NC; ++k)
            hC[(size_t) k] = gC[(size_t) k] * capV (k);
        hL = iL + gL * (v[PU] - v[PK]);
        xPrev = x;
        inputCorrection.reset();
        dcBlock.reset();
        for (int n = 0; n < 64; ++n) dcBlock.hp (step (0.0, 40, kTol));
    }

    // FUZZ INCREASE: 0 = cursore su lug 1 (uscita di Q2), 1 = su lug 3 (ramo fuzz di Q1)
    void setFuzz (double pos)
    {
        pos = std::clamp (pos, 0.0, 1.0);
        if (std::abs (pos - pot) < 1e-5) return;
        pot = pos; dirty = true;
    }

    // volume della chitarra (0..1), potenziometro logaritmico (A, 10% a metà corsa)
    void setGuitarVolume (double v01)
    {
        v01 = std::clamp (v01, 0.0, 1.0);
        if (std::abs (v01 - guitarVol) < 1e-5) return;
        guitarVol = v01; dirty = true;
    }

    // alimentazione (V): entra solo nel vettore delle sorgenti, nessun ricalcolo
    void setSupply (double volts) { vcc = vccNow = std::clamp (volts, 1.0, 12.0); }
    double getSupply() const { return vcc; }

    void setHfe (double q1, double q2)
    {
        beta1 = std::clamp (q1, 10.0, 2000.0);
        beta2 = std::clamp (q2, 10.0, 2000.0);
    }

    void setPickup (Pickup pu)
    {
        pu.R = std::max (pu.R, 50.0); pu.L = std::max (pu.L, 1e-6);
        pu.C = std::max (pu.C, 0.0);  pu.Rload = std::max (pu.Rload, 1e3);
        if (pu == pickup) return;
        pickup = pu;
        updateInputCorrection();
        dirty = true;
    }

    inline double process (double vin)
    {
        if (dirty) rebuild();
        return dcBlock.hp (step (inputCorrection.process (vin), 24, kTol));
    }

    // vero se il circuito è a riposo (uscita e variazioni trascurabili)
    bool isSettled() const { return std::abs (lastOut) < 1e-7 && std::abs (lastOut - prevOut) < 1e-9; }

    // diagnostica
    double nodeVoltage (int n) const { return v[(size_t) n]; }
    int    lastIterations() const    { return lastIters; }
    bool   lastConverged() const     { return lastConv; }

private:
    // convergenza: passo di Newton sotto 10 µV. Vicino alla soluzione la convergenza è
    // quadratica (errore ≈ passo² / 2Vt), quindi l'errore residuo è sotto i 2 nV.
    static constexpr double kTol = 1e-5;

    static constexpr int GND = -1;
    struct Cap { int a, b; double C; };
    static constexpr int NC = 12;

    std::array<Cap, NC> caps() const
    {
        return {{
            { IN, B2, Components::Cin },   { B2, C2, Components::Cfb2 }, { C2, B1, Components::Ccpl },
            { C2, F1, Components::Clug1 }, { C1, X,  Components::Chp },
            { B2, GND, BJT::Cbe }, { B2, C2, BJT::Cbc }, { B1, GND, BJT::Cbe }, { B1, C1, BJT::Cbc },
            { PK, GND, pickup.C },                 // capacità propria del pick-up
            { IN, GND, Components::Ccord },        // cordone d'ingresso
            { OUT, GND, Components::Cout }         // cavo verso l'ampli
        }};
    }

    double capV (int k) const
    {
        const auto& c = cs[(size_t) k];
        return (c.a >= 0 ? v[(size_t) c.a] : 0.0) - (c.b >= 0 ? v[(size_t) c.b] : 0.0);
    }

    using Mat = std::array<std::array<double, N>, N>;

    static void stamp (Mat& G, int a, int b, double g)
    {
        if (a >= 0) G[(size_t) a][(size_t) a] += g;
        if (b >= 0) G[(size_t) b][(size_t) b] += g;
        if (a >= 0 && b >= 0) { G[(size_t) a][(size_t) b] -= g; G[(size_t) b][(size_t) a] -= g; }
    }

    void rebuild()
    {
        dirty = false;
        using C = Components;

        // correnti attuali di condensatori e induttanza (per riallineare le memorie
        // se cambiano C o L, es. cambio di pick-up mentre si suona)
        std::array<double, NC> iC {};
        double iLnow = 0.0;
        const bool rebase = built && ! backwardEuler && std::abs (T - Tbuilt) < 1e-15;
        if (rebase)
        {
            for (int k = 0; k < NC; ++k) iC[(size_t) k] = gC[(size_t) k] * capV (k) - hC[(size_t) k];
            iLnow = hL - gL * (v[PU] - v[PK]);
        }

        Mat G {};
        for (auto& r : G) r.fill (0.0);

        // pick-up: generatore (Norton) con R della bobina, induttanza PU → PK
        stamp (G, PU, GND, 1.0 / pickup.R);
        gL = backwardEuler ? T / pickup.L : T / (2.0 * pickup.L);
        stamp (G, PU, PK, gL);

        // potenziometri della chitarra: tono a 10 come carico, volume come partitore
        if (pickup.ideal_)
        {
            stamp (G, PK, IN, 1.0);
        }
        else
        {
            const double Rv = pickup.volumePot();
            const double f = (std::pow (81.0, guitarVol) - 1.0) / 80.0;   // curva logaritmica (A)
            stamp (G, PK, GND, 1.0 / Rv);                                   // tono a 10
            stamp (G, PK, IN, 1.0 / std::max (1.0, Rv * (1.0 - f)));        // volume: lato caldo
            stamp (G, IN, GND, 1.0 / std::max (1.0, Rv * f));               // volume: lato massa
        }

        stamp (G, B2, C2, 1.0 / C::Rfb2);
        stamp (G, C2, GND, 1.0 / C::Rc2);    // verso Vcc: la sorgente va nel vettore s
        stamp (G, B1, C1, 1.0 / C::Rfb1);
        stamp (G, C1, GND, 1.0 / C::Rc1);    // idem
        stamp (G, X,  F3, 1.0 / C::R18);
        stamp (G, F3, GND, 1.0 / C::R12);
        stamp (G, F1, OUT, 1.0 / std::max (1.0, C::Rpot * pot));
        stamp (G, OUT, F3, 1.0 / std::max (1.0, C::Rpot * (1.0 - pot)));
        stamp (G, OUT, GND, 1.0 / C::Rload);

        cs = caps();
        for (int k = 0; k < NC; ++k)
        {
            gC[(size_t) k] = (backwardEuler ? 1.0 : 2.0) * cs[(size_t) k].C / T;
            stamp (G, cs[(size_t) k].a, cs[(size_t) k].b, gC[(size_t) k]);
        }
        for (int i = 0; i < N; ++i) G[(size_t) i][(size_t) i] += 1e-12;

        invert (G, A);

        // iniezioni non lineari i = [Ib1, Ic1, Ib2, Ic2] (correnti che escono dai nodi)
        for (int r = 0; r < N; ++r)
        {
            Bm[(size_t) r][0] = -A[(size_t) r][B1];
            Bm[(size_t) r][1] = -A[(size_t) r][C1];
            Bm[(size_t) r][2] = -A[(size_t) r][B2];
            Bm[(size_t) r][3] = -A[(size_t) r][C2];
        }
        // tensioni di controllo: [Vbe1, Vbc1, Vbe2, Vbc2]
        for (int c = 0; c < N; ++c)
        {
            NvA[0][(size_t) c] = A[B1][(size_t) c];
            NvA[1][(size_t) c] = A[B1][(size_t) c] - A[C1][(size_t) c];
            NvA[2][(size_t) c] = A[B2][(size_t) c];
            NvA[3][(size_t) c] = A[B2][(size_t) c] - A[C2][(size_t) c];
        }
        for (int j = 0; j < 4; ++j)
        {
            K[0][(size_t) j] = Bm[B1][(size_t) j];
            K[1][(size_t) j] = Bm[B1][(size_t) j] - Bm[C1][(size_t) j];
            K[2][(size_t) j] = Bm[B2][(size_t) j];
            K[3][(size_t) j] = Bm[B2][(size_t) j] - Bm[C2][(size_t) j];
        }

        if (rebase)
        {
            for (int k = 0; k < NC; ++k) hC[(size_t) k] = gC[(size_t) k] * capV (k) - iC[(size_t) k];
            hL = iLnow + gL * (v[PU] - v[PK]);
        }
        built = true; Tbuilt = T;
    }

    static void invert (Mat M, Mat& out)
    {
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
                out[(size_t) i][(size_t) j] = (i == j) ? 1.0 : 0.0;

        for (int c = 0; c < N; ++c)
        {
            int piv = c; double best = std::abs (M[(size_t) c][(size_t) c]);
            for (int r = c + 1; r < N; ++r)
                if (std::abs (M[(size_t) r][(size_t) c]) > best) { best = std::abs (M[(size_t) r][(size_t) c]); piv = r; }
            std::swap (M[(size_t) c], M[(size_t) piv]);
            std::swap (out[(size_t) c], out[(size_t) piv]);
            const double d = 1.0 / M[(size_t) c][(size_t) c];
            for (int j = 0; j < N; ++j) { M[(size_t) c][(size_t) j] *= d; out[(size_t) c][(size_t) j] *= d; }
            for (int r = 0; r < N; ++r)
            {
                if (r == c) continue;
                const double f = M[(size_t) r][(size_t) c];
                if (std::abs (f) < 1e-300) continue;
                for (int j = 0; j < N; ++j)
                {
                    M[(size_t) r][(size_t) j]   -= f * M[(size_t) c][(size_t) j];
                    out[(size_t) r][(size_t) j] -= f * out[(size_t) c][(size_t) j];
                }
            }
        }
    }

    //--------------------------------------------------------------------------
    static inline void junction (double vj, double& i, double& g)
    {
        const double a = vj / BJT::Vt;
        constexpr double amax = 40.0;
        if (a < -30.0) { i = -BJT::Is + gmin * vj; g = gmin; return; }   // giunzione interdetta
        if (a < amax) { const double e = std::exp (a);    i = BJT::Is * (e - 1.0);                    g = BJT::Is * e / BJT::Vt; }
        else          { const double e = std::exp (amax); i = BJT::Is * (e * (1.0 + a - amax) - 1.0); g = BJT::Is * e / BJT::Vt; }
        i += gmin * vj; g += gmin;
    }

    // Ebers-Moll con effetto Early e rottura inversa B-E: [Vbe, Vbc] -> Ib, Ic, jacobiano
    static inline void bjt (double vbe, double vbc, double bF, double& ib, double& ic, double J[2][2])
    {
        double iF, gF, iR, gR;
        junction (vbe, iF, gF);
        junction (vbc, iR, gR);
        constexpr double bR = BJT::betaR;
        const double q = std::max (0.2, 1.0 - vbc / BJT::VAF);
        const double dq = (q > 0.2) ? -1.0 / BJT::VAF : 0.0;   // dq/dVbc
        const double it = iF - iR;
        ib = iF / bF + iR / bR;
        ic = it * q - iR / bR;
        J[0][0] = gF / bF;  J[0][1] = gR / bR;
        J[1][0] = gF * q;   J[1][1] = -gR * q + it * dq - gR / bR;

        // rottura inversa della giunzione base-emettitore (corrente da E verso B)
        const double vr = -vbe - BJT::BV;
        if (vr > -0.5)
        {
            constexpr double nvt = BJT::NBV * BJT::Vt;
            const double a = std::min (vr / nvt, 40.0);
            const double e = std::exp (a) * (vr / nvt > 40.0 ? 1.0 + vr / nvt - 40.0 : 1.0);
            const double ibv = BJT::IBV * e;
            ib -= ibv;
            J[0][0] += BJT::IBV * std::exp (a) / nvt;
        }
    }

    static bool solve4 (double M[4][4], double r[4])
    {
        for (int c = 0; c < 4; ++c)
        {
            int piv = c; double best = std::abs (M[c][c]);
            for (int k = c + 1; k < 4; ++k) if (std::abs (M[k][c]) > best) { best = std::abs (M[k][c]); piv = k; }
            if (best < 1e-300) return false;
            if (piv != c) { for (int j = 0; j < 4; ++j) std::swap (M[c][j], M[piv][j]); std::swap (r[c], r[piv]); }
            const double inv = 1.0 / M[c][c];
            for (int k = c + 1; k < 4; ++k)
            {
                const double f = M[k][c] * inv;
                for (int j = c; j < 4; ++j) M[k][j] -= f * M[c][j];
                r[k] -= f * r[c];
            }
        }
        for (int c = 3; c >= 0; --c)
        {
            double s = r[c];
            for (int j = c + 1; j < 4; ++j) s -= M[c][j] * r[j];
            r[c] = s / M[c][c];
        }
        return true;
    }

    //--------------------------------------------------------------------------
    inline double step (double vin, int maxIter, double tol)
    {
        using C = Components;
        std::array<double, N> s {};
        s[PU] += vin / pickup.R;
        s[C2] += vccNow / C::Rc2;
        s[C1] += vccNow / C::Rc1;
        for (int k = 0; k < NC; ++k)
        {
            const auto& c = cs[(size_t) k];
            if (c.a >= 0) s[(size_t) c.a] += hC[(size_t) k];
            if (c.b >= 0) s[(size_t) c.b] -= hC[(size_t) k];
        }
        s[PU] -= hL; s[PK] += hL;

        double p[4];
        for (int r = 0; r < 4; ++r)
        {
            double acc = 0.0;
            for (int c = 0; c < N; ++c) acc += NvA[(size_t) r][(size_t) c] * s[(size_t) c];
            p[r] = acc;
        }

        // predittore lineare come stima iniziale
        {
            const auto xc = x;
            if (! backwardEuler)
                for (int r = 0; r < 4; ++r)
                    x[(size_t) r] += std::clamp (x[(size_t) r] - xPrev[(size_t) r], -0.05, 0.05);
            xPrev = xc;
        }

        double i4[4] = {};
        double J1[2][2], J2[2][2], dx[4] = {};
        lastConv = false;
        int it = 0;
        for (; it < maxIter; ++it)
        {
            bjt (x[0], x[1], beta1, i4[0], i4[1], J1);
            bjt (x[2], x[3], beta2, i4[2], i4[3], J2);

            double F[4], Jm[4][4];
            for (int r = 0; r < 4; ++r)
            {
                double acc = p[r] - x[(size_t) r];
                for (int j = 0; j < 4; ++j) acc += K[(size_t) r][(size_t) j] * i4[j];
                F[r] = acc;
                Jm[r][0] = K[(size_t) r][0] * J1[0][0] + K[(size_t) r][1] * J1[1][0];
                Jm[r][1] = K[(size_t) r][0] * J1[0][1] + K[(size_t) r][1] * J1[1][1];
                Jm[r][2] = K[(size_t) r][2] * J2[0][0] + K[(size_t) r][3] * J2[1][0];
                Jm[r][3] = K[(size_t) r][2] * J2[0][1] + K[(size_t) r][3] * J2[1][1];
                Jm[r][r] -= 1.0;
            }
            double d[4] = { -F[0], -F[1], -F[2], -F[3] };
            if (! solve4 (Jm, d)) break;

            double m = 0.0;
            for (int r = 0; r < 4; ++r) m = std::max (m, std::abs (d[r]));
            const double sc = (m > 0.1) ? 0.1 / m : 1.0;   // limitazione del passo (V)
            for (int r = 0; r < 4; ++r) { dx[r] = sc * d[r]; x[(size_t) r] += dx[r]; }
            if (m < tol) { lastConv = true; ++it; break; }
        }
        lastIters = it;

        // correnti al punto finale: linearizzazione attorno all'ultimo punto valutato
        if (lastConv)
        {
            i4[0] += J1[0][0] * dx[0] + J1[0][1] * dx[1];
            i4[1] += J1[1][0] * dx[0] + J1[1][1] * dx[1];
            i4[2] += J2[0][0] * dx[2] + J2[0][1] * dx[3];
            i4[3] += J2[1][0] * dx[2] + J2[1][1] * dx[3];
        }
        else
        {
            bjt (x[0], x[1], beta1, i4[0], i4[1], J1);
            bjt (x[2], x[3], beta2, i4[2], i4[3], J2);
        }

        if (! std::isfinite (x[0] + x[1] + x[2] + x[3]))
        {
            x = { 0.6, 0.0, 0.6, -2.0 }; xPrev = x;
            hC.fill (0.0); hL = 0.0;
            return 0.0;
        }

        for (int r = 0; r < N; ++r)
        {
            double acc = 0.0;
            for (int c = 0; c < N; ++c) acc += A[(size_t) r][(size_t) c] * s[(size_t) c];
            for (int j = 0; j < 4; ++j) acc += Bm[(size_t) r][(size_t) j] * i4[j];
            v[(size_t) r] = acc;
        }

        for (int k = 0; k < NC; ++k)
            hC[(size_t) k] = backwardEuler ? gC[(size_t) k] * capV (k)
                                           : 2.0 * gC[(size_t) k] * capV (k) - hC[(size_t) k];
        hL = (backwardEuler ? 1.0 : 2.0) * gL * (v[PU] - v[PK]) + hL;

        prevOut = lastOut; lastOut = v[OUT];
        return v[OUT];
    }

    // La scheda audio registra il pick-up quasi "a vuoto" (ingresso Hi-Z da 1 MΩ): il
    // segnale contiene già la risonanza della bobina con la propria capacità, il cavo e
    // i pot della chitarra. Questo pedale, con l'ingresso in massa virtuale, carica il
    // pick-up pesantemente e quella risonanza non c'è. Si ricostruisce quindi la forza
    // elettromotrice del pick-up con l'inverso del partitore registrato, limitato sopra
    // i 10 kHz per non esaltare il fruscio della scheda (stesso metodo di FYN-2).
    void updateInputCorrection()
    {
        if (pickup.ideal_) { inputCorrection.set (1, 0, 0, 1, 0, 0, fs); inputCorrection.reset(); return; }
        const double G = 1.0 / pickup.Rload + 1.0 / 1.0e6;
        const double Ct = pickup.C + Components::CrecDI;
        const double R = pickup.R, L = pickup.L, n0 = 1.0 + R * G;
        const double wp = 2.0 * kPi * 10000.0;
        inputCorrection.set (1.0, (L * G + R * Ct) / n0, L * Ct / n0,
                             1.0, 2.0 / wp, 1.0 / (wp * wp), fs);
    }

    struct OnePole
    {
        double g = 0.0, s = 0.0;
        void reset() { s = 0.0; }
        void setCutoff (double fc, double sr) { const double w = std::tan (kPi * fc / sr); g = w / (1.0 + w); }
        inline double hp (double xx) { const double u = (xx - s) * g; const double y = u + s; s = y + u; return xx - y; }
    };

    //--------------------------------------------------------------------------
    double fs = 384000.0, T = 1.0 / 384000.0, Tbuilt = 0.0;
    double pot = 0.5, guitarVol = 1.0, vcc = Components::Vcc, vccNow = Components::Vcc;
    double beta1 = BJT::betaF, beta2 = BJT::betaF;
    Pickup pickup = Pickup::singleCoil();
    bool dirty = true, backwardEuler = false, built = false;
    static constexpr double gmin = 1e-12;

    Mat A {};
    std::array<std::array<double, 4>, N> Bm {};
    std::array<std::array<double, N>, 4> NvA {};
    std::array<std::array<double, 4>, 4> K {};
    std::array<double, NC> gC {}, hC {};
    std::array<Cap, NC> cs {};
    double gL = 0.0, hL = 0.0;

    std::array<double, 4> x { 0.6, 0.0, 0.6, -2.0 }, xPrev { 0.6, 0.0, 0.6, -2.0 };
    std::array<double, N> v {};
    double lastOut = 0.0, prevOut = 0.0;
    int lastIters = 0; bool lastConv = false;

    OnePole dcBlock;
    AnalogBiquad inputCorrection;
};

} // namespace ftg
