Now the full theoretical exposition of what's implemented and why it works.

---

## The problem, precisely stated

Standard rotation of a vector in ℝ² uses:

```
x' = x·cos θ − y·sin θ
y' = x·sin θ + y·cos θ
```

The issue is structural, not numerical. `cos` and `sin` are homeomorphisms from the *topological* circle S¹ to ℝ², dragging π (transcendental, not in any finite algebraic extension of ℚ) into every operation. When evaluated in GF(2ⁿ) floating-point, you get irreducible approximation error that accumulates with every composition.

---

## Layer 1 — Cayley parameterisation (all rotations, exact integers)

The **Cayley map** is a birational isomorphism between the affine line and SO(2) minus one point:

```
t = tan(θ/2)  ↔  R(t) = 1/(1+t²) · [[1−t², −2t], [2t, 1−t²]]
```

For any **t = p/q ∈ ℚ**, multiply through by the denominator (p²+q²) to get a **pure-integer matrix**:

```
(p²+q²)·R = [[q²−p², −2pq],
              [2pq,    q²−p²]]
```

In homogeneous projective coordinates `[X:Y:W]` (where affine point is X/W, Y/W):

```
[X', Y', W'] = [(q²−p²)X − 2pq·Y,   2pq·X + (q²−p²)Y,   (p²+q²)·W]
```

**No division. No trig. All arithmetic in ℤ.** The constraint x²+y²=1 becomes X²+Y²=W² exactly in ℤ by construction (the matrix has determinant (p²+q²)², preserving the integer sphere). The scale factor W accumulates as (p²+q²)ᴺ — managed by GCD reduction or kept as a projective scale.

The t=1/2 case recovers the 3-4-5 Pythagorean triple: cos=3/5, sin=4/5 — an integer rotation matrix that fits in a 4-bit lookup.

---

## Layer 2 — Quadratic field extensions ℚ(√d) (exact algebraic rotations)

For angles that are rational multiples of π, cos θ and sin θ are **algebraic** numbers. The exact field needed is determined by their minimal polynomial over ℚ, which is related to the cyclotomic polynomial Φₙ:

| Angle | 2cos θ min-poly | Field | Degree |
|-------|----------------|-------|--------|
| 90°   | X              | ℚ     | 1      |
| 45°   | X²−2           | ℚ(√2) | 2      |
| 30°, 60° | X²−3      | ℚ(√3) | 2      |
| 36°, 72° | X²−5      | ℚ(√5) | 2      |
| 22.5° | X⁴−4X²+2     | ℚ(√(2+√2)) | 4 |

The implementation represents every element of ℚ(√d) as a pair **(a, b) ∈ ℚ²** with implicit basis {1, √d}, and multiplication as:

```
(a + b√d)(c + e√d) = (ac + bde) + (ae + bc)√d
```

All arithmetic stays inside the pair — **√d is never evaluated as a float**. The algebraic norm N(a+b√d) = a²−db² stays in ℚ and is used for inversion. This means rotating by 45° ten thousand times incurs **exactly zero** rounding error — the coordinates cycle through a finite set of ℚ(√2) values and return exactly to their starting values.

---

## Layer 3 — Residual field extension ℚ[ε]/(ε²) for genuinely irrational angles

For an angle like 37° that has no convenient low-degree algebraic representation, the approach is:

**1.** Find the nearest rational Cayley parameter: t₀ = 1/3 ∈ ℚ, giving exact rotation by 2·arctan(1/3) ≈ 36.87° (a 3-4-5 triple angle).

**2.** Write the true parameter as t = t₀ + ε, where ε = tan(18.5°) − 1/3 ≈ 0.001266 is the residual.

**3.** Extend the coefficient ring to ℚ[ε]/(ε²) — the **dual number ring**, or equivalently the quotient of the polynomial ring by the ideal (ε²). Elements are a+bε with ε²=0.

**4.** Taylor-expand the Cayley formulae in ε (all derivatives are rational at t₀=1/3):

```
d/dt cos = −4t/(1+t²)²   →  at t₀=1/3: −27/25 ∈ ℚ
d/dt sin = (2−2t²)/(1+t²)² → at t₀=1/3: 36/25 ∈ ℚ

cos(θ) = 4/5 + (−27/25)·ε   ∈ ℚ ⊕ ℚε
sin(θ) = 3/5 + (36/25)·ε    ∈ ℚ ⊕ ℚε
```

**5.** The rotation matrix decomposes as **R = R₀ + ε·R₁** where both R₀, R₁ ∈ M₂(ℚ). The ε term is carried **formally** — never numerically approximated. For n-th order accuracy, use ℚ[ε]/(εⁿ⁺¹), a rank-(n+1) free ℚ-module, recovering the Taylor series of sin/cos without ever touching π.

---

## Relation to binary arithmetic in GF(2ⁿ)

The Cayley integer layer is particularly natural for hardware. For t = p/q with p,q chosen as small integers (say 3 bits each), the rotation matrix entries `q²−p²` and `2pq` fit in ~6 bits. Composition is just **integer matrix multiplication**, exactly the operation GF(2ⁿ) ALUs are optimised for. The scale accumulation is a **bit-shift** (when p²+q² is a power of 2, which it isn't for 3-4-5 but is achievable with other Pythagorean families like (1,1) → scale 2, (1,3) → scale 10, (3,5) → scale 34). Projective coordinates already appear in GPU rasterisation pipelines — the W register exists precisely to defer this division.

The ℚ(√d) layer maps to **SIMD pair registers**: each coordinate is a 2-wide vector (a,b) representing a+b√d, and all field operations are 2-wide integer FMAs. No transcendental unit involved at any point.
