#!/usr/bin/env python3
from fractions import Fraction
from math import gcd

def gcd3(a, b, c):
    return gcd(gcd(abs(int(a)), abs(int(b))), abs(int(c)))

class Dual:
    # a + bε, with ε² = 0
    def __init__(self, a, b=0):
        self.a = Fraction(a)
        self.b = Fraction(b)

    def __add__(self, other):
        other = other if isinstance(other, Dual) else Dual(other)
        return Dual(self.a + other.a, self.b + other.b)

    __radd__ = __add__

    def __sub__(self, other):
        other = other if isinstance(other, Dual) else Dual(other)
        return Dual(self.a - other.a, self.b - other.b)

    def __rsub__(self, other):
        other = other if isinstance(other, Dual) else Dual(other)
        return Dual(other.a - self.a, other.b - self.b)

    def __mul__(self, other):
        other = other if isinstance(other, Dual) else Dual(other)
        return Dual(self.a * other.a, self.a * other.b + self.b * other.a)

    __rmul__ = __mul__

    def __truediv__(self, other):
        other = other if isinstance(other, Dual) else Dual(other)
        if other.a == 0:
            raise ZeroDivisionError("division by dual number with zero real part")
        inv_a = Fraction(1, 1) / other.a
        return Dual(
            self.a * inv_a,
            (self.b * other.a - self.a * other.b) * inv_a * inv_a
        )

    def __repr__(self):
        return f"{self.a} + {self.b}ε"


class Quad:
    # a + b√d, with exact rational a,b and fixed squarefree d
    def __init__(self, a=0, b=0, d=2):
        self.a = Fraction(a)
        self.b = Fraction(b)
        self.d = d

    def _coerce(self, other):
        return other if isinstance(other, Quad) else Quad(other, 0, self.d)

    def __add__(self, other):
        other = self._coerce(other)
        return Quad(self.a + other.a, self.b + other.b, self.d)

    __radd__ = __add__

    def __sub__(self, other):
        other = self._coerce(other)
        return Quad(self.a - other.a, self.b - other.b, self.d)

    def __rsub__(self, other):
        other = self._coerce(other)
        return Quad(other.a - self.a, other.b - self.b, self.d)

    def __mul__(self, other):
        other = self._coerce(other)
        return Quad(
            self.a * other.a + self.b * other.b * self.d,
            self.a * other.b + self.b * other.a,
            self.d
        )

    __rmul__ = __mul__

    def inv(self):
        n = self.a * self.a - self.d * self.b * self.b
        if n == 0:
            raise ZeroDivisionError("division by zero in quadratic field")
        return Quad(self.a / n, -self.b / n, self.d)

    def __truediv__(self, other):
        other = self._coerce(other)
        return self * other.inv()

    def __eq__(self, other):
        other = self._coerce(other)
        return self.a == other.a and self.b == other.b and self.d == other.d

    def __repr__(self):
        return f"{self.a} + {self.b}√{self.d}"


def cayley_step(X, Y, W, p, q):
    """
    Exact projective rotation using t = p/q:
        [X',Y',W'] = [(q²-p²)X - 2pqY, 2pqX + (q²-p²)Y, (p²+q²)W]
    """
    a = q*q - p*p
    b = 2*p*q
    X2 = a * X - b * Y
    Y2 = b * X + a * Y
    W2 = (p*p + q*q) * W

    g = gcd3(X2, Y2, W2)
    if g > 1:
        X2 //= g
        Y2 //= g
        W2 //= g
    return X2, Y2, W2


def affine(X, Y, W):
    return Fraction(X, W), Fraction(Y, W)


def rotate_quad(x, y, c, s):
    # Standard 2D rotation, but with exact algebraic coefficients
    return c * x - s * y, s * x + c * y


def main():
    print("Layer 1: exact rational/projective Cayley rotation")
    # t = 1/2 -> 3-4-5 triple, exact rotation by 2*atan(1/2)
    X, Y, W = 1, 0, 1
    X, Y, W = cayley_step(X, Y, W, 1, 2)
    print("  projective =", (X, Y, W))
    print("  affine     =", affine(X, Y, W))
    print()

    print("Layer 2: exact algebraic rotation in Q(√2)")
    # 45°: cos = sin = √2 / 2
    d = 2
    c = Quad(0, Fraction(1, 2), d)
    s = Quad(0, Fraction(1, 2), d)

    x = Quad(1, 0, d)
    y = Quad(0, 0, d)

    for _ in range(8):
        x, y = rotate_quad(x, y, c, s)

    print("  after 8×45°:")
    print("  x =", x)
    print("  y =", y)
    print()

    print("Layer 3: first-order residual using dual numbers")
    # t = 1/3 + ε
    t = Dual(Fraction(1, 3), 1)
    one = Dual(1, 0)

    cos_t = (one - t*t) / (one + t*t)
    sin_t = (Dual(2, 0) * t) / (one + t*t)

    print("  cos(t) =", cos_t)
    print("  sin(t) =", sin_t)
    print("  (This is linearized in ε; it is not exact for a truly irrational angle.)")


if __name__ == "__main__":
    main()
