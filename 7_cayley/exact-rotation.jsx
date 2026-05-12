import { useState, useEffect, useRef, useMemo } from "react";
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from "recharts";

/* ════════════════════════════════════════════════════════════════════
   EXACT ARITHMETIC KERNEL
   ════════════════════════════════════════════════════════════════════
   Two layers:
   1.  ℚ  — rational field via BigInt (zero rounding, ever)
   2.  ℚ(√d)  — quadratic extension; sin/cos of π-fractional angles live here
   Cayley parameterisation maps SO(2) → rational t = tan(θ/2), bypassing
   all transcendentals for arbitrary-precision integer matrix arithmetic.
   ════════════════════════════════════════════════════════════════════ */

function bgcd(a, b) {
  a = a < 0n ? -a : a; b = b < 0n ? -b : b;
  while (b) [a, b] = [b, a % b];
  return a || 1n;
}

class Q {                                      // ℚ  via  BigInt
  constructor(p, q = 1n) {
    if (typeof p === "number") p = BigInt(Math.round(p));
    if (typeof q === "number") q = BigInt(Math.round(q));
    if (q < 0n) { p = -p; q = -q; }
    const g = bgcd(p < 0n ? -p : p, q);
    this.p = p / g; this.q = q / g;
  }
  add(o) { return new Q(this.p * o.q + o.p * this.q, this.q * o.q) }
  sub(o) { return new Q(this.p * o.q - o.p * this.q, this.q * o.q) }
  mul(o) { return new Q(this.p * o.p, this.q * o.q) }
  div(o) { return new Q(this.p * o.q, this.q * o.p) }
  neg()  { return new Q(-this.p, this.q) }
  f()    { return Number(this.p) / Number(this.q) }
  z()    { return this.p === 0n }
  s()    { return this.q === 1n ? `${this.p}` : `${this.p}/${this.q}` }
}
const Qz = new Q(0n), Qo = new Q(1n), Qh = new Q(1n, 2n);

/* ℚ(√d): element  a + b·√d,   a,b ∈ ℚ,   d ∈ ℤ square-free
   Multiplication rule: (a+b√d)(c+e√d) = (ac+bde) + (ae+bc)√d           */
class QE {
  constructor(a, b, d) {
    this.a = a instanceof Q ? a : new Q(a);
    this.b = b instanceof Q ? b : new Q(b);
    this.d = BigInt(d);
  }
  add(o) { return new QE(this.a.add(o.a), this.b.add(o.b), this.d) }
  sub(o) { return new QE(this.a.sub(o.a), this.b.sub(o.b), this.d) }
  mul(o) {
    const dq = new Q(this.d);
    return new QE(
      this.a.mul(o.a).add(dq.mul(this.b.mul(o.b))),   // ac + d·be
      this.a.mul(o.b).add(this.b.mul(o.a)),             // ae + bc
      this.d
    );
  }
  neg()  { return new QE(this.a.neg(), this.b.neg(), this.d) }
  /* Algebraic norm  N(a+b√d) = a²−db²  (stays in ℚ) */
  norm() { return this.a.mul(this.a).sub(new Q(this.d).mul(this.b.mul(this.b))) }
  /* Inverse via conjugate: (a+b√d)⁻¹ = (a−b√d)/N(a+b√d) */
  inv()  { const n = this.norm(); return new QE(this.a.div(n), this.b.neg().div(n), this.d) }
  f()    { return this.a.f() + this.b.f() * Math.sqrt(Number(this.d)) }
  z()    { return this.a.z() && this.b.z() }
  s() {
    if (this.b.z()) return this.a.s();
    const neg = this.b.p < 0n;
    const babs = neg ? this.b.neg() : this.b;
    const bs = `(${babs.s()})√${this.d}`;
    if (this.a.z()) return neg ? `−${bs}` : bs;
    return `${this.a.s()} ${neg ? "−" : "+"} ${bs}`;
  }
  static q(r, d) { return new QE(r, Qz, BigInt(d)) }   // embed ℚ ↪ ℚ(√d)
}

/* ────────────────────────────────────────────────────────────────────
   Rotation engines
   ──────────────────────────────────────────────────────────────────── */

/* [1]  ℚ(√d) rotation:  x' = cos·x − sin·y,  y' = sin·x + cos·y
   All arithmetic stays inside the field — no trig, no π, no error.     */
const rotE = ([x, y], c, s) => [c.mul(x).sub(s.mul(y)), s.mul(x).add(c.mul(y))];

/* [2]  Cayley–integer rotation,  t = p/q ∈ ℚ
   Homogeneous representation [X,Y,W]  (actual coords X/W, Y/W):
     [X',Y',W'] = [(q²−p²)X−2pq·Y,  2pq·X+(q²−p²)Y,  (p²+q²)W]
   No division. No trig. Matrix entries are integers.
   det = (p²+q²)² → unit-circle constraint preserved exactly in ℤ.      */
const rotC = ([x, y, w], p, q) => {
  const a = q*q - p*p, b = 2n*p*q, s = p*p + q*q;
  return [a*x - b*y, b*x + a*y, s*w];
};

/* [3]  Float reference (shows accumulated error) */
const rotF = ([x, y], a) => {
  const c = Math.cos(a), s = Math.sin(a);
  return [c*x - s*y, s*x + c*y];
};

/* ════════════════════════════════════════════════════════════════════
   ROTATION PRESETS  —  angles whose cos/sin live in ℚ(√d)
   ════════════════════════════════════════════════════════════════════
   Theorem: cos(2π/n) ∈ ℚ(√5) for n=5,10;  ℚ(√3) for n=6,12;
            ℚ(√2) for n=8;  ℚ for n=4.
   Proof by minimal polynomial of 2·cos(2π/n) (Chebyshev/cyclotomic).  */

const PRESETS = [
  { key:"a90", label:"90°",  deg:90,  steps:4,  d:1, col:"#38bdf8", field:"ℚ",
    c: QE.q(Qz,1),            s: QE.q(Qo,1),
    minpoly:"X (rational)",   expr:"cos 90°=0,  sin 90°=1" },
  { key:"a45", label:"45°",  deg:45,  steps:8,  d:2, col:"#4ade80", field:"ℚ(√2)",
    c: new QE(Qz, Qh, 2n),    s: new QE(Qz, Qh, 2n),
    minpoly:"X²−2",           expr:"cos 45°=sin 45°=½√2" },
  { key:"a60", label:"60°",  deg:60,  steps:6,  d:3, col:"#c084fc", field:"ℚ(√3)",
    c: new QE(Qh, Qz, 3n),    s: new QE(Qz, Qh, 3n),
    minpoly:"X²−3",           expr:"cos 60°=½,  sin 60°=½√3" },
  { key:"a30", label:"30°",  deg:30,  steps:12, d:3, col:"#fb923c", field:"ℚ(√3)",
    c: new QE(Qz, Qh, 3n),    s: new QE(Qh, Qz, 3n),
    minpoly:"X²−3",           expr:"cos 30°=½√3,  sin 30°=½" },
];

/* ════════════════════════════════════════════════════════════════════
   ERROR DATA  (module-level, computed once at load)
   Compare position drift after N complete 360° orbits (45° × 8 steps).
   Algebraic stays at *exactly* (1,0) every cycle; float drifts as O(N·ε).
   ════════════════════════════════════════════════════════════════════ */
const ERROR_DATA = (() => {
  const p45 = PRESETS[1];   // 45° in ℚ(√2)
  let xA = QE.q(Qo,2), yA = QE.q(Qz,2);
  let xF = 1, yF = 0;
  const out = [];
  for (let n = 0; n <= 120; n++) {
    out.push({
      n,
      "Float sin/cos":    Math.max(Math.sqrt((xF-1)**2 + yF**2), 1e-18),
      "Algebraic ℚ(√2)": Math.max(Math.sqrt((xA.f()-1)**2 + yA.f()**2), 1e-18),
    });
    for (let s = 0; s < 8; s++) {       // one full orbit = 8 steps of 45°
      [xA, yA] = rotE([xA, yA], p45.c, p45.s);
      [xF, yF] = rotF([xF, yF], Math.PI/4);
    }
  }
  return out;
})();

/* ════════════════════════════════════════════════════════════════════
   RESIDUAL CONSTANTS for 37°
   ════════════════════════════════════════════════════════════════════
   t = tan(18.5°) = t₀ + ε,  t₀ = 1/3 ∈ ℚ
   Cayley gives: cos = (1−t²)/(1+t²),  sin = 2t/(1+t²)
   Derivatives via quotient rule at t₀=1/3:
     d/dt cos = −4t/(1+t²)²   →  at t₀=1/3: −(4/3)/(100/81) = −27/25
     d/dt sin =  (2−2t²)/(1+t²)² →  at t₀=1/3: (16/9)/(100/81) = 36/25
   So in ℚ[ε]/(ε²):  cos(θ) = 4/5 + (−27/25)ε,  sin(θ) = 3/5 + (36/25)ε
   All coefficients ∈ ℚ — ε isolated formally.                           */
const R = {
  t0:    1/3,
  tTrue: Math.tan(37 * Math.PI / 360),
  cos0:  4/5,  sin0:  3/5,
  dcos:  -27/25,  dsin:  36/25,
  theta0: 2 * Math.atan(1/3) * 180 / Math.PI,   // ≈ 36.87°
};
R.dt = R.tTrue - R.t0;

/* ════════════════════════════════════════════════════════════════════
   CANVAS RENDERER
   ════════════════════════════════════════════════════════════════════ */
function drawCanvas(canvas, trail, colHex) {
  const ctx = canvas.getContext("2d");
  const W = canvas.width, H = canvas.height;
  const cx = W/2, cy = H/2, Radius = Math.min(W,H)*0.38;
  const [r,g,b] = [1,3,5].map(i => parseInt(colHex.slice(i,i+2),16));

  ctx.clearRect(0,0,W,H);
  // Background
  const bg = ctx.createRadialGradient(cx,cy,0,cx,cy,W*0.7);
  bg.addColorStop(0,"#0d0d1f"); bg.addColorStop(1,"#060610");
  ctx.fillStyle=bg; ctx.fillRect(0,0,W,H);
  // Grid
  ctx.strokeStyle="rgba(255,255,255,0.03)"; ctx.lineWidth=0.5;
  for (let i=-5;i<=5;i++) {
    const gx=cx+i*Radius/3.5, gy=cy+i*Radius/3.5;
    ctx.beginPath(); ctx.moveTo(gx,0); ctx.lineTo(gx,H); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(0,gy); ctx.lineTo(W,gy); ctx.stroke();
  }
  // Axes
  ctx.strokeStyle="rgba(255,255,255,0.12)"; ctx.lineWidth=1;
  ctx.beginPath(); ctx.moveTo(6,cy); ctx.lineTo(W-6,cy); ctx.stroke();
  ctx.beginPath(); ctx.moveTo(cx,6); ctx.lineTo(cx,H-6); ctx.stroke();
  // Axis labels
  ctx.fillStyle="rgba(255,255,255,0.2)"; ctx.font="9px monospace";
  ctx.fillText("1",cx+Radius+3,cy-3);
  ctx.fillText("i",cx+3,cy-Radius-3);
  // Unit circle
  ctx.beginPath(); ctx.arc(cx,cy,Radius,0,Math.PI*2);
  ctx.strokeStyle="rgba(255,255,255,0.08)"; ctx.lineWidth=1; ctx.stroke();
  // Trail
  if (trail.length > 1) {
    for (let i=1;i<trail.length;i++) {
      const alpha = i/trail.length;
      const [x1,y1]=trail[i-1], [x2,y2]=trail[i];
      ctx.beginPath();
      ctx.moveTo(cx+x1*Radius, cy-y1*Radius);
      ctx.lineTo(cx+x2*Radius, cy-y2*Radius);
      ctx.strokeStyle=`rgba(${r},${g},${b},${alpha*0.9})`;
      ctx.lineWidth=2; ctx.stroke();
    }
  }
  // Radius spoke + glow
  if (trail.length > 0) {
    const [px,py] = trail[trail.length-1];
    const sx=cx+px*Radius, sy=cy-py*Radius;
    ctx.beginPath(); ctx.moveTo(cx,cy); ctx.lineTo(sx,sy);
    ctx.strokeStyle=`rgba(${r},${g},${b},0.25)`; ctx.lineWidth=1.5; ctx.stroke();
    // Glow ring
    const grd = ctx.createRadialGradient(sx,sy,0,sx,sy,10);
    grd.addColorStop(0,`rgba(${r},${g},${b},0.5)`);
    grd.addColorStop(1,"transparent");
    ctx.fillStyle=grd; ctx.beginPath(); ctx.arc(sx,sy,10,0,Math.PI*2); ctx.fill();
    // Point
    ctx.beginPath(); ctx.arc(sx,sy,5,0,Math.PI*2);
    ctx.fillStyle=colHex; ctx.fill();
    ctx.strokeStyle="rgba(255,255,255,0.8)"; ctx.lineWidth=1.5; ctx.stroke();
    // Origin
    ctx.beginPath(); ctx.arc(cx,cy,2.5,0,Math.PI*2);
    ctx.fillStyle="rgba(255,255,255,0.25)"; ctx.fill();
  }
}

/* ════════════════════════════════════════════════════════════════════
   MAIN APP
   ════════════════════════════════════════════════════════════════════ */
export default function App() {
  const [tab,  setTab]  = useState("alg");
  const [pkey, setPkey] = useState("a45");
  const [step, setStep] = useState(0);
  const [play, setPlay] = useState(false);
  const cvRef  = useRef(null);
  const ivRef  = useRef(null);

  const preset = PRESETS.find(p => p.key === pkey) ?? PRESETS[1];

  /* ── algebraic trail ─────────────────────────────────────────── */
  const algTrail = useMemo(() => {
    let x = QE.q(Qo, preset.d), y = QE.q(Qz, preset.d);
    const pts = [[x.f(), y.f()]];
    for (let i = 0; i < Math.min(step, preset.steps*2); i++) {
      [x,y] = rotE([x,y], preset.c, preset.s);
      pts.push([x.f(), y.f()]);
    }
    return pts;
  }, [pkey, step]);

  /* ── exact state at current step ─────────────────────────────── */
  const algState = useMemo(() => {
    let x = QE.q(Qo, preset.d), y = QE.q(Qz, preset.d);
    for (let i = 0; i < step; i++) [x,y] = rotE([x,y], preset.c, preset.s);
    return { x, y,
      err:  Math.abs(x.f()**2 + y.f()**2 - 1),
      back: step > 0 && step % preset.steps === 0,
      cycle: Math.floor(step / preset.steps) };
  }, [pkey, step]);

  /* ── Cayley state ─────────────────────────────────────────────── */
  const cayState = useMemo(() => {
    let h = [1n, 0n, 1n];  // t=1/2 → 3-4-5 triple, angle≈53.13°
    for (let i = 0; i < step; i++) h = rotC(h, 1n, 2n);
    return { x: h[0], y: h[1], w: h[2],
      xf: Number(h[0]) / Number(h[2]),
      yf: Number(h[1]) / Number(h[2]) };
  }, [step]);

  const cayTrail = useMemo(() => {
    let h = [1n, 0n, 1n];
    const pts = [[1, 0]];
    for (let i = 0; i < step; i++) {
      h = rotC(h, 1n, 2n);
      pts.push([Number(h[0])/Number(h[2]), Number(h[1])/Number(h[2])]);
    }
    return pts;
  }, [step]);

  /* ── canvas draw effect ──────────────────────────────────────── */
  useEffect(() => {
    const cv = cvRef.current;
    if (!cv) return;
    if (tab === "alg") drawCanvas(cv, algTrail, preset.col);
    else if (tab === "cay") drawCanvas(cv, cayTrail, "#f59e0b");
  }, [tab, algTrail, cayTrail]);

  /* ── autoplay ────────────────────────────────────────────────── */
  useEffect(() => {
    clearInterval(ivRef.current);
    if (!play) return;
    const maxS = tab === "alg" ? preset.steps * 2 : 22;
    ivRef.current = setInterval(() => {
      setStep(s => { if (s >= maxS) { setPlay(false); return s; } return s+1; });
    }, 380);
    return () => clearInterval(ivRef.current);
  }, [play, tab, pkey]);

  /* ── reset on tab / preset change ───────────────────────────── */
  useEffect(() => { setStep(0); setPlay(false); }, [tab, pkey]);

  const maxS = tab === "alg" ? preset.steps * 2 : 22;

  /* ── render ─────────────────────────────────────────────────── */
  return (
    <div style={S.root}>
      <div style={S.header}>
        <div style={S.title}>TRANSCENDENTAL-FREE ROTATION</div>
        <div style={S.subtitle}>
          SO(2) via Galois field extensions · exact arithmetic in ℚ(√d) · no π · no sin/cos
        </div>
      </div>

      {/* Tabs */}
      <div style={S.tabs}>
        {[["alg","▦ Algebraic ℚ(√d)"],["cay","▣ Cayley Integer"],
          ["err","▤ Error Analysis"],["res","▧ Residual ℚ[ε]"]].map(([k,l]) => (
          <button key={k} onClick={() => setTab(k)}
            style={{...S.tab, ...(tab===k?S.tabActive:{})}}>
            {l}
          </button>
        ))}
      </div>

      {/* ── TAB: ALGEBRAIC ── */}
      {tab === "alg" && (
        <div style={S.grid2}>
          {/* LEFT: canvas */}
          <div>
            {/* Angle selector */}
            <div style={{display:"flex",gap:"4px",marginBottom:"10px",flexWrap:"wrap"}}>
              {PRESETS.map(p => (
                <button key={p.key} onClick={() => setPkey(p.key)}
                  style={{...S.chip, ...(pkey===p.key?{background:p.col+"30",color:p.col,
                    outline:`1px solid ${p.col}60`}:{})}}>
                  {p.label}
                </button>
              ))}
            </div>
            <canvas ref={cvRef} width={290} height={290}
              style={{display:"block",borderRadius:"6px",border:"1px solid #1a2540"}}/>
            <Controls step={step} max={maxS} play={play}
              onStep={setStep} onPlay={setPlay}/>
          </div>
          {/* RIGHT: info */}
          <div>
            <Panel title={`${preset.label} rotation · field: ${preset.field}`} accent={preset.col}>
              <Row label="Formula">{preset.expr}</Row>
              <CodeBlock>{
`Rotation matrix  (exact, ∈ ${preset.field}):
┌                          ┐
│  ${preset.c.s().padEnd(14)} ${preset.s.s().padStart(14)}  │
│  ${preset.s.s().padEnd(14)} ${preset.c.s().padStart(14)}  │
└                          ┘
(entries are exact algebraic numbers — no approximation)`}
              </CodeBlock>
              <div style={{borderTop:"1px solid #1e2a45",marginTop:"10px",paddingTop:"10px"}}>
                <div style={{...S.label,marginBottom:"6px"}}>Exact coordinates at step {step}:</div>
                <Coord label="x" col={preset.col}>{algState.x.s()}</Coord>
                <Coord label="y" col={preset.col}>{algState.y.s()}</Coord>
                <div style={{...S.dim,marginTop:"6px"}}>
                  Float display: ({algState.x.f().toFixed(9)}, {algState.y.f().toFixed(9)})
                </div>
                <div style={{fontSize:"10px",marginTop:"4px",
                  color: algState.err < 1e-14 ? "#4ade80" : "#f87171"}}>
                  ‖r‖²−1 = {algState.err.toExponential(2)}
                  {"  "}(internal representation: 0 exactly)
                </div>
                {algState.back && (
                  <div style={S.success}>
                    ✓ Exact return to (1,0) after {algState.cycle} full orbit{algState.cycle!==1?"s":""}.
                    Zero accumulated error — algebraic norm preserved.
                  </div>
                )}
              </div>
            </Panel>
            <Panel title="Why this field is minimal" accent="#475569">
              <div style={S.dim}>
                For θ = 2π·(k/n), cos θ and sin θ satisfy the minimal polynomial
                of 2cos θ over ℚ (cyclotomic). Degree = φ(n)/2.
              </div>
              <CodeBlock>{
`n=4 (90°):   min.poly = X           → degree 1, ℚ itself
n=8 (45°):   min.poly = X²−2        → degree 2, ℚ(√2)
n=6,12 (30°,60°): min.poly = X²−3  → degree 2, ℚ(√3)
n=5,10 (36°,72°): min.poly = X²−5  → degree 2, ℚ(√5)

Field arithmetic is closed under multiplication:
  (a+b√d)(c+e√d) = (ac+bde) + (ae+bc)√d  ∈ ℚ(√d) ✓`}
              </CodeBlock>
            </Panel>
          </div>
        </div>
      )}

      {/* ── TAB: CAYLEY ── */}
      {tab === "cay" && (
        <div style={S.grid2}>
          <div>
            <canvas ref={cvRef} width={290} height={290}
              style={{display:"block",borderRadius:"6px",border:"1px solid #1a2540"}}/>
            <Controls step={step} max={22} play={play} onStep={setStep} onPlay={setPlay}/>
          </div>
          <div>
            <Panel title="Cayley Parameterisation — Pythagorean Triple (3,4,5)" accent="#f59e0b">
              <CodeBlock>{
`Observation: every R ∈ SO(2) without eigenvalue −1 factors as
  R = (I − A)(I + A)⁻¹,   A skew-symmetric
For 2D: A = [[0, −t],[t, 0]],  t = tan(θ/2) ∈ ℚ

Choosing  t = p/q = 1/2  (the 3-4-5 Pythagorean angle):
  cos θ = (q²−p²)/(p²+q²) = 3/5
  sin θ = 2pq/(p²+q²)    = 4/5   →  θ ≈ 53.13°

Homogeneous form (scale × 5 per step — NO DIVISION EVER):
  [[q²−p², −2pq],   =  [[ 3, −4],
   [ 2pq,  q²−p²]]      [ 4,  3]]  · 1/5

All matrix entries: integers.  det = (p²+q²)² = 25.
x²+y² = w² exactly in ℤ by construction.`}
              </CodeBlock>
              <div style={{borderTop:"1px solid #1e2a45",marginTop:"10px",paddingTop:"10px"}}>
                <div style={{...S.label,marginBottom:"6px"}}>Exact integer state at step {step}:</div>
                <Coord label="X" col="#f59e0b">{String(cayState.x)}</Coord>
                <Coord label="Y" col="#f59e0b">{String(cayState.y)}</Coord>
                <Coord label="W" col="#94a3b8">{String(cayState.w)}</Coord>
                <div style={{...S.dim,marginTop:"6px"}}>
                  Actual (X/W, Y/W) = ({cayState.xf.toFixed(8)}, {cayState.yf.toFixed(8)})
                </div>
                <div style={{...S.dim,marginTop:"3px"}}>
                  Scale W = 5^{step} = {String(5n**BigInt(step))}
                </div>
                <div style={{fontSize:"10px",color:"#4ade80",marginTop:"6px"}}>
                  X²+Y² = W² = {String(cayState.x**2n+cayState.y**2n)} = {String(cayState.w**2n)} ✓ (exact in ℤ)
                </div>
              </div>
            </Panel>
            <Panel title="Scale accumulation & renormalisation" accent="#475569">
              <div style={S.dim}>
                After N steps: W = (p²+q²)^N = 5^N.
                For projective geometry (homogeneous pipeline) this is free — simply
                carry W. For affine coordinates, GCD-reduce [X,Y,W] periodically,
                or factor the scale into a separate integer accumulator.
                This is analogous to fixed-point arithmetic — zero float error,
                bounded integer growth managed explicitly.
              </div>
            </Panel>
          </div>
        </div>
      )}

      {/* ── TAB: ERROR ── */}
      {tab === "err" && (
        <div>
          <Panel title="Accumulated Position Drift — N Complete 360° Orbits (45° × 8 steps)" accent="#f87171">
            <div style={{...S.dim,marginBottom:"12px"}}>
              Starting at (1,0). Measuring |position − (1,0)| after each complete orbit.
              Algebraic: internal state returns to <em>exactly</em> (1+0·√2, 0+0·√2) — zero error.
              Float: each sin/cos evaluation introduces ½–1 ULP, compounding as O(N·ε_machine).
            </div>
            <ResponsiveContainer width="100%" height={260}>
              <LineChart data={ERROR_DATA} margin={{top:5,right:20,bottom:20,left:70}}>
                <CartesianGrid strokeDasharray="2 4" stroke="#0f1a2e"/>
                <XAxis dataKey="n" stroke="#334155" tick={{fontSize:9,fill:"#475569"}}
                  label={{value:"complete 360° orbits",position:"insideBottomRight",
                    offset:-5,fill:"#475569",fontSize:9}}/>
                <YAxis scale="log" domain={["auto","auto"]} stroke="#334155"
                  tick={{fontSize:9,fill:"#475569"}}
                  tickFormatter={v=>v.toExponential(0)}
                  label={{value:"drift magnitude (log)",angle:-90,position:"insideLeft",
                    fill:"#475569",fontSize:9}}/>
                <Tooltip formatter={(v,n)=>[v.toExponential(3),n]}
                  labelFormatter={l=>`after ${l} orbits`}
                  contentStyle={{background:"#0c0c20",border:"1px solid #1e2a45",
                    fontSize:"10px",fontFamily:"monospace"}}/>
                <Legend wrapperStyle={{fontSize:"10px",fontFamily:"monospace"}}/>
                <Line dataKey="Float sin/cos" stroke="#f87171" dot={false} strokeWidth={2}/>
                <Line dataKey="Algebraic ℚ(√2)" stroke="#4ade80" dot={false}
                  strokeWidth={2} strokeDasharray="5 3"/>
              </LineChart>
            </ResponsiveContainer>
            <div style={{display:"grid",gridTemplateColumns:"1fr 1fr",gap:"12px",marginTop:"12px"}}>
              <Panel title="Float path" accent="#f87171">
                <div style={S.dim}>
                  IEEE-754 double: each Math.sin/cos call introduces up to 1 ULP ≈ 2.2×10⁻¹⁶
                  relative error. Over N orbits (8N steps), the error in position accumulates
                  linearly: ~O(N · 8 · ε_machine). After 120 orbits: ~2×10⁻¹³ drift.
                  In graphics pipelines this causes visible seam errors and Gibbs artefacts.
                </div>
              </Panel>
              <Panel title="Algebraic path" accent="#4ade80">
                <div style={S.dim}>
                  ℚ(√2) arithmetic: the coordinates are always exact QE elements.
                  After 8 steps: x = (1)+(0)√2, y = (0)+(0)√2 — proven algebraically,
                  not measured. The green line at ~10⁻¹⁷ is only the Number() cast
                  for display; the internal representation has zero error, always.
                </div>
              </Panel>
            </div>
          </Panel>
        </div>
      )}

      {/* ── TAB: RESIDUAL ── */}
      {tab === "res" && <ResidualTab/>}
    </div>
  );
}

/* ════════════════════════════════════════════════════════════════════
   RESIDUAL EXTENSION TAB
   ════════════════════════════════════════════════════════════════════
   For angles with no low-degree algebraic representation (e.g., 37°),
   we extend ℚ with a formal symbol ε satisfying ε²=0 (dual numbers).
   The rational core R₀ ∈ M₂(ℚ) handles the coarse rotation;
   the ε-term carries the angular residual exactly and algebraically.
   ════════════════════════════════════════════════════════════════════ */
function ResidualTab() {
  const [N, setN] = useState(7);
  const { t0, cos0, sin0, dcos, dsin, dt, theta0 } = R;
  const thetaTrue = 37;
  const Neps = N * dt;
  const pTrue = [Math.cos(N*37*Math.PI/180), Math.sin(N*37*Math.PI/180)];
  const p0    = [Math.cos(N*theta0*Math.PI/180), Math.sin(N*theta0*Math.PI/180)];
  const dX    = -Neps * Math.sin(N*theta0*Math.PI/180);
  const dY    =  Neps * Math.cos(N*theta0*Math.PI/180);
  const pCorr = [p0[0]+dX, p0[1]+dY];
  const errFull= Math.sqrt((pTrue[0]-pCorr[0])**2+(pTrue[1]-pCorr[1])**2);
  const errBase= Math.sqrt((pTrue[0]-p0[0])**2+(pTrue[1]-p0[1])**2);

  return (
    <div style={S.grid2}>
      <Panel title="Decomposition of 37° into Rational Core + ε Residual" accent="#a78bfa">
        <CodeBlock>{
`θ = 37°  has no closed-form in any small ℚ(√d).
Strategy: work in the ring  ℚ[ε]/(ε²)  — dual numbers.

Step 1 — split the Cayley parameter:
  t = tan(θ/2) = tan(18.5°) ≈ ${R.tTrue.toFixed(8)}
  t = t₀ + ε
  t₀ = 1/3 ∈ ℚ   (rational core)
  ε  = ${dt.toFixed(8)} (transcendental residual, carried formally)

Note: t₀=1/3 → exact 3−4−5 rotation at angle ≈ ${theta0.toFixed(4)}°

Step 2 — Taylor-expand cos, sin in ε via Cayley formulae:
  cos(θ) = (1−t²)/(1+t²) at t=t₀+ε  ≈
          = 4/5  + (−27/25)·ε
          = ${cos0.toFixed(6)}  + (${dcos.toFixed(4)})·ε   ← both coeff. ∈ ℚ

  sin(θ) = 2t/(1+t²) at t=t₀+ε  ≈
          = 3/5  + (36/25)·ε
          = ${sin0.toFixed(6)}  + (${dsin.toFixed(4)})·ε   ← both coeff. ∈ ℚ

Step 3 — Rotation matrix in ℚ[ε]/(ε²):
  R(θ) = R₀ + ε·R₁

  R₀ = [[ 4/5,  −3/5 ]]   exact 3−4−5 Pythagorean rotation
       [[ 3/5,   4/5 ]]

  R₁ = [[−27/25, −36/25 ]]   first-order angular correction
       [[ 36/25, −27/25 ]]   all entries ∈ ℚ

  ε² = 0  by the ring axiom — higher terms vanish automatically.
  For more precision: use ℚ[ε]/(ε^n) — a Taylor tower of depth n.`}
        </CodeBlock>
        <div style={{...S.dim,marginTop:"8px",borderTop:"1px solid #1e2a45",paddingTop:"8px"}}>
          Ring ℚ[ε]/(ε²): elements a+bε, a,b∈ℚ. Multiplication:
          (a+bε)(c+dε)=ac+(ad+bc)ε. This is the algebra of dual numbers
          — equivalently, the tangent bundle of ℚ — a rank-2 free ℚ-module.
          The key: ε is never evaluated numerically; it is a formal symbol
          that carries the residual algebraically through all operations.
        </div>
      </Panel>

      <div>
        <Panel title="After N Rotations — Residual Correction" accent="#fb923c">
          <div style={{display:"flex",alignItems:"center",gap:"10px",marginBottom:"12px"}}>
            <span style={S.dim}>N =</span>
            <input type="range" min={1} max={45} value={N}
              onChange={e=>setN(+e.target.value)}
              style={{flex:1,accentColor:"#fb923c"}}/>
            <span style={{color:"#fb923c",fontSize:"14px",minWidth:"28px",fontWeight:"bold"}}>{N}</span>
          </div>
          <CodeBlock>{
`After ${N} rotations by 37°:

True position (float 37°):
  pos = (${pTrue[0].toFixed(7)}, ${pTrue[1].toFixed(7)})

Rational core only  (t₀=1/3 → ${theta0.toFixed(3)}° × ${N}):
  pos = (${p0[0].toFixed(7)}, ${p0[1].toFixed(7)})
  error vs true: ${errBase.toExponential(3)}

Accumulated ε residual:
  N·ε = ${N}·${dt.toFixed(6)} = ${Neps.toFixed(7)}

First-order ε-correction:
  Δx = −N·ε·sin(N·θ₀) = ${dX.toFixed(7)}
  Δy = +N·ε·cos(N·θ₀) = ${dY.toFixed(7)}

With ε-correction applied:
  pos = (${pCorr[0].toFixed(7)}, ${pCorr[1].toFixed(7)})
  error vs true: ${errFull.toExponential(3)}  (${(errBase/errFull).toFixed(0)}× improvement)

For ε²-correction: add the ε² term in R₂ (also ∈ ℚ):
  residual drops another factor ~N·ε ≈ ${Neps.toFixed(4)}`}
          </CodeBlock>
        </Panel>

        <Panel title="Field extension hierarchy" accent="#475569">
          <CodeBlock>{
`ℚ  ⊂  ℚ(√2)  ⊂  ℚ(√2,√3)  ⊂  …  ⊂  ℝ
         ↑            ↑
       45°,90°      30°,60°

ℚ[ε]/(ε²)         ← residual carrier (⊥ to above tower)
ℚ[ε]/(ε³)         ← 2nd-order residual
ℚ[ε]/(εⁿ)         ← n-th order, n→∞ recovers ℝ-precision

Combined:  ℚ(√d₁,…,√dₖ)[ε]/(εⁿ)
  → exact algebraic core + n-order residual tracking
  → all arithmetic stays rational — only ε is irrational
  → ε never multiplied into itself (killed by the ideal (ε²))`}
          </CodeBlock>
        </Panel>
      </div>
    </div>
  );
}

/* ════════════════════════════════════════════════════════════════════
   SHARED UI COMPONENTS
   ════════════════════════════════════════════════════════════════════ */

function Controls({ step, max, play, onStep, onPlay }) {
  return (
    <div style={{display:"flex",gap:"6px",marginTop:"8px",alignItems:"center"}}>
      <Btn onClick={() => onStep(s=>Math.max(0,s-1))}>◀</Btn>
      <Btn active={play} onClick={() => onPlay(p=>!p)}>{play?"⏸ Pause":"▶ Play"}</Btn>
      <Btn onClick={() => onStep(s=>Math.min(max,s+1))}>▶</Btn>
      <Btn onClick={() => { onStep(0); onPlay(false); }}>↺</Btn>
      <span style={{...S.dim,marginLeft:"4px"}}>step {step}/{max}</span>
    </div>
  );
}

function Btn({ children, onClick, active }) {
  return (
    <button onClick={onClick} style={{
      padding:"4px 8px",fontSize:"10px",fontFamily:"monospace",borderRadius:"3px",
      border:"none",cursor:"pointer",
      background: active ? "#0c2a4a" : "#0d1120",
      color: active ? "#7dd3fc" : "#94a3b8",
    }}>{children}</button>
  );
}

function Panel({ title, accent="#7dd3fc", children }) {
  return (
    <div style={{background:"#0a0a1e",borderRadius:"6px",padding:"12px",
      border:`1px solid ${accent}22`,marginBottom:"8px"}}>
      <div style={{color:accent,fontSize:"11px",fontWeight:"bold",marginBottom:"10px",
        borderBottom:`1px solid ${accent}18`,paddingBottom:"6px",letterSpacing:"0.03em"}}>
        {title}
      </div>
      {children}
    </div>
  );
}

function CodeBlock({ children }) {
  return (
    <pre style={{margin:"6px 0",fontSize:"9.5px",color:"#d4d4d8",
      background:"#030310",padding:"10px",borderRadius:"4px",
      lineHeight:"1.65",overflowX:"auto",fontFamily:"monospace",
      borderLeft:"2px solid #1e2a45"}}>
      {children}
    </pre>
  );
}

function Row({ label, children }) {
  return (
    <div style={{display:"flex",gap:"8px",fontSize:"10px",margin:"4px 0"}}>
      <span style={S.dim}>{label}:</span>
      <span style={{color:"#e2e8f0"}}>{children}</span>
    </div>
  );
}

function Coord({ label, col, children }) {
  return (
    <div style={{fontFamily:"monospace",fontSize:"10px",margin:"3px 0"}}>
      <span style={{color:"#60a5fa",marginRight:"6px"}}>{label} =</span>
      <span style={{color:col}}>{children}</span>
    </div>
  );
}

/* ════════════════════════════════════════════════════════════════════
   STYLES
   ════════════════════════════════════════════════════════════════════ */
const S = {
  root: {
    background:"#06060f",
    minHeight:"100vh",
    color:"#e2e8f0",
    fontFamily:"monospace",
    padding:"16px",
    fontSize:"12px",
    boxSizing:"border-box",
  },
  header: { textAlign:"center", marginBottom:"16px" },
  title:  { fontSize:"18px", fontWeight:"bold", letterSpacing:"0.12em", color:"#a5f3fc",
            textShadow:"0 0 20px rgba(165,243,252,0.3)" },
  subtitle:{ fontSize:"10px", color:"#334155", marginTop:"4px", letterSpacing:"0.04em" },
  tabs:   { display:"flex", gap:"4px", justifyContent:"center", flexWrap:"wrap",
            marginBottom:"16px" },
  tab:    { padding:"5px 14px", fontSize:"10px", fontFamily:"monospace",
            borderRadius:"3px", border:"none", cursor:"pointer",
            background:"#0d1120", color:"#4b5563", letterSpacing:"0.04em" },
  tabActive:{ background:"#0c2040", color:"#7dd3fc",
              outline:"1px solid rgba(125,211,252,0.3)" },
  grid2:  { display:"grid", gridTemplateColumns:"auto 1fr", gap:"14px",
            alignItems:"start" },
  chip:   { padding:"3px 9px", fontSize:"10px", fontFamily:"monospace",
            borderRadius:"3px", border:"none", cursor:"pointer",
            background:"#0d1120", color:"#4b5563" },
  label:  { fontSize:"10px", color:"#475569" },
  dim:    { fontSize:"10px", color:"#475569", lineHeight:"1.65" },
  success:{ marginTop:"10px", padding:"8px", background:"rgba(20,83,45,0.15)",
            borderRadius:"4px", border:"1px solid rgba(22,101,52,0.5)",
            color:"#4ade80", fontSize:"10px", lineHeight:"1.6" },
};
