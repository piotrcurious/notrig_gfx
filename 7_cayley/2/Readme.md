For a graphics engine, I would not implement it exactly as written. I'd separate it into three numeric backends behind one common interface:

1. Projective integer backend (very fast, exact, ideal for repeated rotations using rational Cayley parameters)


2. Quadratic field backend (a+b√d) for exact rotations such as 45°, 30°, 36°, etc.


3. Floating backend only for the final rasterization or for arbitrary user angles.



This keeps almost the entire transformation pipeline exact while only converting to float when pixels are actually drawn.

A simple C++17 implementation without Boost could use 64-bit integers (int64_t) and an exact rational type.

Rational type

#pragma once
#include <cstdint>
#include <numeric>

struct Rational
{
    int64_t num;
    int64_t den;

    Rational(int64_t n=0,int64_t d=1)
        :num(n),den(d)
    {
        normalize();
    }

    void normalize()
    {
        if(den<0)
        {
            den=-den;
            num=-num;
        }

        auto g=std::gcd(num,den);
        if(g)
        {
            num/=g;
            den/=g;
        }
    }

    Rational operator+(const Rational& r) const
    {
        return Rational(
            num*r.den+r.num*den,
            den*r.den);
    }

    Rational operator-(const Rational& r) const
    {
        return Rational(
            num*r.den-r.num*den,
            den*r.den);
    }

    Rational operator*(const Rational& r) const
    {
        return Rational(
            num*r.num,
            den*r.den);
    }

    Rational operator/(const Rational& r) const
    {
        return Rational(
            num*r.den,
            den*r.num);
    }

    double value() const
    {
        return double(num)/double(den);
    }
};


---

Exact projective vector

struct ProjectiveVec2
{
    int64_t X;
    int64_t Y;
    int64_t W=1;

    void reduce()
    {
        auto g=std::gcd(std::gcd(
            std::llabs(X),
            std::llabs(Y)),
            std::llabs(W));

        if(g>1)
        {
            X/=g;
            Y/=g;
            W/=g;
        }
    }
};


---

Cayley rotation

class CayleyRotation
{
public:

    int64_t A;
    int64_t B;
    int64_t Scale;

    CayleyRotation(int64_t p,int64_t q)
    {
        A=q*q-p*p;
        B=2*p*q;
        Scale=p*p+q*q;
    }

    void rotate(ProjectiveVec2& v) const
    {
        int64_t x=A*v.X-B*v.Y;
        int64_t y=B*v.X+A*v.Y;

        v.X=x;
        v.Y=y;
        v.W*=Scale;

        v.reduce();
    }
};

No divisions occur during rotation.


---

Quadratic field

template<int D>
struct Quad
{
    Rational a;
    Rational b;

    Quad()
    {
    }

    Quad(Rational aa,Rational bb)
        :a(aa),b(bb)
    {
    }

    Quad operator+(const Quad& q) const
    {
        return {a+q.a,b+q.b};
    }

    Quad operator-(const Quad& q) const
    {
        return {a-q.a,b-q.b};
    }

    Quad operator*(const Quad& q) const
    {
        return
        {
            a*q.a+
            b*q.b*Rational(D),

            a*q.b+
            b*q.a
        };
    }
};


---

Exact vector over ℚ(√D)

template<int D>
struct ExactVec2
{
    Quad<D> x;
    Quad<D> y;
};


---

Exact rotation matrix

For 45°

cos = √2/2

sin = √2/2

becomes

Quad<2> c(
    Rational(0),
    Rational(1,2));

Quad<2> s(
    Rational(0),
    Rational(1,2));

Rotation

template<int D>
void rotate(
    ExactVec2<D>& v,
    const Quad<D>& c,
    const Quad<D>& s)
{
    auto nx=c*v.x-s*v.y;
    auto ny=s*v.x+c*v.y;

    v.x=nx;
    v.y=ny;
}

After eight successive 45° rotations the vector returns exactly to its original value.


---

Graphics engine architecture

Instead of storing

float x,y

every object stores

ProjectiveVec2 position;
ProjectiveTransform transform;

or

ExactVec2<2>

depending on the required precision.

Only immediately before rasterization do you convert:

float screenX =
    float(position.X)/
    float(position.W);

float screenY =
    float(position.Y)/
    float(position.W);


---

Where this approach shines

This is especially attractive for:

CAD kernels

vector graphics (SVG/PDF)

font rendering

geometric modeling

constructive solid geometry (CSG)

procedural geometry

deterministic game engines and lockstep networking (identical results across CPUs)


It is less suitable as a complete replacement for floating-point transforms in a modern GPU pipeline, because arbitrary rotations, perspective projections, interpolation, lighting, and physics still ultimately require transcendental or irrational quantities. A hybrid engine—using exact Cayley/projective transforms internally and converting to floating point only at rendering—is where this approach is most practical.
