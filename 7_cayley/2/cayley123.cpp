#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>

// ============================================================
//  Exact rational arithmetic
// ============================================================

static inline int64_t iabs64(int64_t x) { return x < 0 ? -x : x; }

struct Rational
{
    int64_t num{0};
    int64_t den{1};

    Rational() = default;
    Rational(int64_t n, int64_t d = 1) : num(n), den(d) { normalize(); }

    void normalize()
    {
        if (den == 0)
            throw std::runtime_error("Rational: zero denominator");

        if (den < 0)
        {
            den = -den;
            num = -num;
        }

        const int64_t g = std::gcd(iabs64(num), iabs64(den));
        if (g > 1)
        {
            num /= g;
            den /= g;
        }
    }

    friend Rational operator+(const Rational& a, const Rational& b)
    {
        __int128 n = (__int128)a.num * b.den + (__int128)b.num * a.den;
        __int128 d = (__int128)a.den * b.den;
        return Rational((int64_t)n, (int64_t)d);
    }

    friend Rational operator-(const Rational& a, const Rational& b)
    {
        __int128 n = (__int128)a.num * b.den - (__int128)b.num * a.den;
        __int128 d = (__int128)a.den * b.den;
        return Rational((int64_t)n, (int64_t)d);
    }

    friend Rational operator*(const Rational& a, const Rational& b)
    {
        __int128 n = (__int128)a.num * b.num;
        __int128 d = (__int128)a.den * b.den;
        return Rational((int64_t)n, (int64_t)d);
    }

    friend Rational operator/(const Rational& a, const Rational& b)
    {
        if (b.num == 0)
            throw std::runtime_error("Rational: divide by zero");
        __int128 n = (__int128)a.num * b.den;
        __int128 d = (__int128)a.den * b.num;
        return Rational((int64_t)n, (int64_t)d);
    }

    friend Rational operator-(const Rational& a)
    {
        return Rational(-a.num, a.den);
    }

    friend bool operator==(const Rational& a, const Rational& b)
    {
        return a.num == b.num && a.den == b.den;
    }

    explicit operator long double() const
    {
        return (long double)num / (long double)den;
    }
};

std::ostream& operator<<(std::ostream& os, const Rational& r)
{
    if (r.den == 1) os << r.num;
    else os << r.num << "/" << r.den;
    return os;
}

// ============================================================
//  Exact projective rotation via Cayley parameter t = p/q
// ============================================================

struct ProjectiveVec2
{
    int64_t X{0};
    int64_t Y{0};
    int64_t W{1};

    void reduce()
    {
        const int64_t g = std::gcd(std::gcd(iabs64(X), iabs64(Y)), iabs64(W));
        if (g > 1)
        {
            X /= g;
            Y /= g;
            W /= g;
        }
        if (W < 0)
        {
            X = -X;
            Y = -Y;
            W = -W;
        }
    }
};

static inline void cayley_rotate(ProjectiveVec2& v, int64_t p, int64_t q)
{
    const int64_t A = q*q - p*p;
    const int64_t B = 2*p*q;
    const int64_t S = p*p + q*q;

    __int128 x = (__int128)A * v.X - (__int128)B * v.Y;
    __int128 y = (__int128)B * v.X + (__int128)A * v.Y;
    __int128 w = (__int128)S * v.W;

    v.X = (int64_t)x;
    v.Y = (int64_t)y;
    v.W = (int64_t)w;
    v.reduce();
}

static inline std::pair<Rational, Rational> affine(const ProjectiveVec2& v)
{
    return { Rational(v.X, v.W), Rational(v.Y, v.W) };
}

// ============================================================
//  Exact quadratic extensions Q(√D)
// ============================================================

template<int D>
struct Quad
{
    Rational a{0,1};
    Rational b{0,1};

    Quad() = default;
    Quad(Rational aa, Rational bb) : a(std::move(aa)), b(std::move(bb)) {}
    Quad(int64_t aa, int64_t bb = 0) : a(aa), b(bb) {}

    friend Quad operator+(const Quad& x, const Quad& y)
    {
        return Quad(x.a + y.a, x.b + y.b);
    }

    friend Quad operator-(const Quad& x, const Quad& y)
    {
        return Quad(x.a - y.a, x.b - y.b);
    }

    friend Quad operator-(const Quad& x)
    {
        return Quad(-x.a, -x.b);
    }

    friend Quad operator*(const Quad& x, const Quad& y)
    {
        return Quad(
            x.a * y.a + x.b * y.b * Rational(D),
            x.a * y.b + x.b * y.a
        );
    }

    Rational norm() const
    {
        return a*a - Rational(D) * b*b;
    }

    Quad inv() const
    {
        const Rational n = norm();
        if (n.num == 0)
            throw std::runtime_error("Quad: non-invertible element");
        return Quad(a / n, -b / n);
    }

    friend Quad operator/(const Quad& x, const Quad& y)
    {
        return x * y.inv();
    }

    explicit operator long double() const
    {
        return (long double)a + (long double)b * std::sqrt((long double)D);
    }
};

template<int D>
std::ostream& operator<<(std::ostream& os, const Quad<D>& q)
{
    os << q.a << " + " << q.b << "*sqrt(" << D << ")";
    return os;
}

template<int D>
struct ExactVec2
{
    Quad<D> x;
    Quad<D> y;
};

template<int D>
static inline void rotate(ExactVec2<D>& v, const Quad<D>& c, const Quad<D>& s)
{
    const Quad<D> nx = c * v.x - s * v.y;
    const Quad<D> ny = s * v.x + c * v.y;
    v.x = nx;
    v.y = ny;
}

// ============================================================
//  Lazy transcendental extension: symbolic nodes + cached eval
// ============================================================

class LazyReal
{
public:
    struct Expr
    {
        mutable bool cached{false};
        mutable long double cache{0.0L};
        virtual ~Expr() = default;
        long double eval() const
        {
            if (!cached)
            {
                cache = compute();
                cached = true;
            }
            return cache;
        }
    protected:
        virtual long double compute() const = 0;
    };

private:
    using Ptr = std::shared_ptr<const Expr>;

    struct LitNode : Expr
    {
        long double v;
        explicit LitNode(long double vv) : v(vv) {}
        long double compute() const override { return v; }
    };

    struct UnaryNode : Expr
    {
        Ptr a;
        explicit UnaryNode(Ptr aa) : a(std::move(aa)) {}
    };

    struct BinaryNode : Expr
    {
        Ptr a, b;
        BinaryNode(Ptr aa, Ptr bb) : a(std::move(aa)), b(std::move(bb)) {}
    };

    struct AddNode : BinaryNode
    {
        using BinaryNode::BinaryNode;
        long double compute() const override { return a->eval() + b->eval(); }
    };

    struct SubNode : BinaryNode
    {
        using BinaryNode::BinaryNode;
        long double compute() const override { return a->eval() - b->eval(); }
    };

    struct MulNode : BinaryNode
    {
        using BinaryNode::BinaryNode;
        long double compute() const override { return a->eval() * b->eval(); }
    };

    struct DivNode : BinaryNode
    {
        using BinaryNode::BinaryNode;
        long double compute() const override { return a->eval() / b->eval(); }
    };

    struct NegNode : UnaryNode
    {
        using UnaryNode::UnaryNode;
        long double compute() const override { return -a->eval(); }
    };

    struct SinNode : UnaryNode
    {
        using UnaryNode::UnaryNode;
        long double compute() const override { return std::sin(a->eval()); }
    };

    struct CosNode : UnaryNode
    {
        using UnaryNode::UnaryNode;
        long double compute() const override { return std::cos(a->eval()); }
    };

    struct ExpNode : UnaryNode
    {
        using UnaryNode::UnaryNode;
        long double compute() const override { return std::exp(a->eval()); }
    };

    struct LogNode : UnaryNode
    {
        using UnaryNode::UnaryNode;
        long double compute() const override { return std::log(a->eval()); }
    };

    Ptr expr;

    explicit LazyReal(Ptr p) : expr(std::move(p)) {}

public:
    LazyReal() : expr(std::make_shared<LitNode>(0.0L)) {}
    explicit LazyReal(long double v) : expr(std::make_shared<LitNode>(v)) {}
    explicit LazyReal(const Rational& r) : expr(std::make_shared<LitNode>((long double)r)) {}

    long double eval() const { return expr->eval(); }

    static LazyReal sin(const LazyReal& x) { return LazyReal(std::make_shared<SinNode>(x.expr)); }
    static LazyReal cos(const LazyReal& x) { return LazyReal(std::make_shared<CosNode>(x.expr)); }
    static LazyReal exp(const LazyReal& x) { return LazyReal(std::make_shared<ExpNode>(x.expr)); }
    static LazyReal log(const LazyReal& x) { return LazyReal(std::make_shared<LogNode>(x.expr)); }

    friend LazyReal operator+(const LazyReal& a, const LazyReal& b)
    {
        return LazyReal(std::make_shared<AddNode>(a.expr, b.expr));
    }

    friend LazyReal operator-(const LazyReal& a, const LazyReal& b)
    {
        return LazyReal(std::make_shared<SubNode>(a.expr, b.expr));
    }

    friend LazyReal operator*(const LazyReal& a, const LazyReal& b)
    {
        return LazyReal(std::make_shared<MulNode>(a.expr, b.expr));
    }

    friend LazyReal operator/(const LazyReal& a, const LazyReal& b)
    {
        return LazyReal(std::make_shared<DivNode>(a.expr, b.expr));
    }

    friend LazyReal operator-(const LazyReal& a)
    {
        return LazyReal(std::make_shared<NegNode>(a.expr));
    }
};

struct LazyVec2
{
    LazyReal x;
    LazyReal y;
};

struct LazyRotation
{
    LazyReal theta;
    LazyReal c;
    LazyReal s;

    explicit LazyRotation(const LazyReal& angle)
        : theta(angle), c(LazyReal::cos(theta)), s(LazyReal::sin(theta))
    {}

    LazyVec2 apply(const LazyVec2& v) const
    {
        return { c * v.x - s * v.y,
                 s * v.x + c * v.y };
    }
};

static inline long double deg_to_rad(long double deg)
{
    static const long double pi = std::acos(-1.0L);
    return deg * pi / 180.0L;
}

// ============================================================
//  Demo
// ============================================================

int main()
{
    std::cout << std::fixed << std::setprecision(18);

    std::cout << "=== Layer 1: exact Cayley/projective rotation ===\n";
    {
        ProjectiveVec2 v{1, 0, 1};
        cayley_rotate(v, 1, 2); // t = 1/2
        auto [x, y] = affine(v);
        std::cout << "projective: (" << v.X << ", " << v.Y << ", " << v.W << ")\n";
        std::cout << "affine:     (" << (long double)x << ", " << (long double)y << ")\n";
    }

    std::cout << "\n=== Layer 2: exact algebraic rotation in Q(sqrt(2)) ===\n";
    {
        using Q2 = Quad<2>;
        ExactVec2<2> v{ Q2(1,0), Q2(0,0) };
        const Q2 c(Rational(0), Rational(1,2));
        const Q2 s(Rational(0), Rational(1,2));

        for (int i = 0; i < 8; ++i)
            rotate(v, c, s);

        std::cout << "after 8 x 45deg:\n";
        std::cout << "x = " << v.x << "\n";
        std::cout << "y = " << v.y << "\n";
    }

    std::cout << "\n=== Layer 3: lazy transcendental rotation ===\n";
    {
        LazyRotation rot(LazyReal(deg_to_rad(37.0L)));
        LazyVec2 p{ LazyReal(1.0L), LazyReal(0.0L) };
        LazyVec2 q = rot.apply(p);

        // Nothing was numerically evaluated until now.
        std::cout << "rotating (1,0) by 37deg ->\n";
        std::cout << "x = " << q.x.eval() << "\n";
        std::cout << "y = " << q.y.eval() << "\n";
    }

    std::cout << "\n=== Lazy transcendental composition example ===\n";
    {
        LazyReal t = LazyReal(deg_to_rad(10.0L));
        LazyReal expr = LazyReal::sin(t) * LazyReal::cos(t) + LazyReal::exp(LazyReal(0.0L));
        std::cout << "sin(10deg)*cos(10deg) + exp(0) = " << expr.eval() << "\n";
    }

    return 0;
}
