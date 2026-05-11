Galois Field Virtual Machine — 3D Symbolic Engine (v7)

A compact Arduino / ESP32 visual demo that treats geometry as a symbolic program. Instead of transforming a single rigid mesh in the usual way, this sketch builds algebraic vectors made of terms, applies rotations as history-aware instructions, and performs morphing between two symbolic shapes.

> Core idea: points are programs, rotations are instructions, and morphing is interpolation between programs.



This project is designed for TFT displays using TFT_eSPI and is especially suitable for microcontrollers where floating-point math is either expensive or undesirable.


---

What this sketch does

At runtime, the sketch:

1. Creates two simple 2D shapes in symbolic form:

a large square

a smaller diamond



2. Interpolates between them using a fixed-point alpha value in the range 0..1024


3. Applies two independent symbolic rotations


4. Projects the resulting 3D-like coordinates onto a 2D TFT screen


5. Draws the animated shape as connected line segments



The result is a pulsing, morphing wireframe figure that rotates in space.


---

Features

Fixed-point math instead of floats

Symbolic term tracking for transform history

Rotation stack with multiple layers

Morphing engine for blending two algebraic states

Perspective projection onto TFT display coordinates

Low-dependency Arduino sketch beyond Arduino.h and TFT_eSPI.h



---

Requirements

Hardware

Arduino-compatible board

Ideally an ESP32 or similar board with enough RAM and speed

TFT display supported by TFT_eSPI


Software

Arduino IDE, PlatformIO, or another Arduino build environment

TFT_eSPI library

A correctly configured User_Setup.h / display driver setup for TFT_eSPI



---

Installation

Arduino IDE

1. Install the TFT_eSPI library through the Library Manager or manually.


2. Configure TFT_eSPI for your display in the library setup files.


3. Paste the sketch into a new .ino file.


4. Compile and upload.



PlatformIO

Add TFT_eSPI as a dependency or include it in your project configuration, then place the sketch in src/main.cpp or a similar entry point.


---

High-level architecture

The code is organized into four conceptual layers:

1. Fast fixed-point trigonometry

The sketch uses a lookup table for sine values at integer degrees from 0 to 90. Cosine is derived from sine by phase shifting.

This avoids floating-point trig calls and keeps the math predictable on microcontrollers.

2. Symbolic geometry representation

Instead of storing a vector as one simple (x, y, z) triple, the engine stores a vector as a list of symbolic terms.

Each term includes:

a numeric value v[3]

a history of applied layers

a history of applied axes

a depth counter


This means the engine can treat transforms as part of the object’s symbolic identity, not just as overwritten coordinates.

3. Morphing engine

Two symbolic vectors are blended with fixed-point interpolation:

alpha = 0 means fully A

alpha = 1024 means fully B

values in between produce a mix


This is used to animate between the square and the diamond.

4. Collapse and projection

The engine collapses symbolic terms into a concrete 3D position by replaying stored rotations, then projects the result into 2D screen space using a basic perspective formula.


---

Fixed-point convention

This sketch uses SCALE = 1024 as its internal fixed-point base.

That means:

1024 represents 1.0

512 represents 0.5

2048 represents 2.0


The morphing and trig routines are built around this scale.

Why this approach is useful

deterministic on low-power hardware

avoids floating-point overhead

keeps arithmetic simpler to reason about on embedded targets

makes interpolation and projection cheap



---

Code walkthrough

SIN_TABLE

A lookup table that stores sine values for 0..90 degrees in fixed-point form.

This table is used by:

fx_sin(angle)

fx_cos(angle)


fx_sin() and fx_cos()

These functions provide integer-degree sine and cosine.

They work by:

normalizing the angle into 0..359

mapping it to the first quadrant when possible

reflecting or negating the table value for other quadrants


This is a fast approximation suitable for real-time display animation.

SymbolicTerm

Represents one symbolic piece of a vector.

Fields:

v[3]: the current numeric 3D vector value

layers[]: rotation layers applied over time

axes[]: the axis used for each step

depth: how many symbolic operations are stored


hasSameHistory() checks whether two terms have the same transform history so they can be merged during simplification.

AlgebraicVec3

A symbolic vector made of up to MAX_TERMS_PER_VEC terms.

Important methods:

fromRational(x, y, z): creates a vector from a single term

simplify(): removes zero terms and combines terms with the same history


This makes the structure behave more like a symbolic expression than a plain numeric vector.

MorphismStack

Stores a set of rotation layers.

pushRotation(angleDegrees) computes and stores:

cosine in val1[layer]

sine in val2[layer]


These values are later reused by the engine when collapsing symbolic terms.

RuntimeExactEngine

This is the core engine.

applyRotation(v, layer, axis)

Instead of immediately rotating coordinates in place, this function appends the rotation history to every term in the vector.

That makes the vector retain symbolic memory of how it was transformed.

morph(out, a, b, alpha)

Blends two vectors symbolically.

Formula:

result = (A * (1024 - alpha) + B * alpha) / 1024

Implementation details:

copies terms from a and scales them by 1024 - alpha

copies terms from b and scales them by alpha

simplifies the result


collapse(v, stack, ox, oy, oz)

Replays the stored rotational history term by term.

For each term:

start with the term’s vector values

apply every stored rotation from its history

accumulate the result into the output coordinates


project(v, stack, sx, sy, w, h)

Performs a simple perspective projection.

It:

collapses the symbolic vector into a concrete (x, y, z)

adds a forward offset to z so the object sits in front of the camera

rejects points that are too close

maps 3D coordinates to 2D screen coordinates



---

Main animation loop

Inside loop() the sketch:

1. Clears the screen


2. Resets the world rotation stack


3. Creates two time-based rotations:

rX

rY



4. Computes a pulsing morph amount alpha


5. Builds two shapes:

pA: large square

pB: smaller diamond



6. For each edge point:

morphs the corresponding point from pA to pB

applies the two rotations

projects both the current and next point

draws a line between them



7. Increments the animation frame


8. Waits 30 ms



This produces a smooth animated morphing wireframe.


---

Shape definitions

The sketch defines two sets of four points:

Shape A — square

A larger square centered on the origin.

Shape B — diamond

A smaller rotated-looking diamond, also centered on the origin.

Each corresponding vertex from A and B is morphed together, so the animation feels like one geometric program transforming into another rather than simply crossfading pixels.


---

Rendering pipeline

The pipeline is:

symbolic point
→ morph
→ append rotation history
→ collapse symbolic history
→ perspective project
→ draw line

This is the central idea behind the engine: geometry is not merely stored, it is interpreted.


---

Tuning parameters

SCALE

Controls fixed-point precision.

Higher values:

increase precision

increase arithmetic cost

may need more care with overflow


MAX_LAYERS

Maximum number of rotation layers that can be stored.

MAX_TERMS_PER_VEC

Maximum symbolic terms allowed in one vector.

MAX_DEPTH

Maximum transform history depth per term.

These constants can be increased for experimentation, but embedded RAM usage will also increase.


---

How to customize the demo

Change the displayed shapes

Replace the pA and pB definitions with your own control points.

For example, you can morph between:

triangle and square

cube face and cross

star and circle-like polygon


Change animation speed

Adjust:

frame++;
delay(30);

A smaller delay makes the animation faster.

Change the morph rhythm

This line controls the pulsing blend amount:

int alpha = (fx_sin(frame * 2) + 1024) / 2;

You can change the multiplier to alter morph speed.

Change camera feel

Modify the projection values in project():

the depth offset 450 * SCALE

the focal length-like constant 350



---

Limitations

This sketch is intentionally compact and experimental.

Known limitations:

no hidden-line removal

no depth sorting of segments

limited number of symbolic terms

limited rotation layers and history depth

fixed integer-degree trig only

projection is simple and not physically accurate


Despite that, it is well suited to small interactive visual experiments.


---

Performance notes

This design favors embedded performance by:

using integer arithmetic

avoiding transcendental math at runtime

keeping the scene small and predictable

reusing a compact transformation model


On ESP32-class hardware, it should be quite manageable for a simple TFT demo.

If performance becomes an issue, the first things to reduce are:

MAX_TERMS_PER_VEC

MAX_DEPTH

screen clearing frequency

number of drawn primitives per frame



---

Suggested improvements

Possible next versions could add:

Z-buffer or painter’s algorithm

line clipping against the screen bounds

more shape programs

user-controlled morph input

gesture or sensor-driven animation

support for multiple object groups

more advanced symbolic reduction rules

3D meshes stored as algebraic programs



---

Troubleshooting

Screen stays black

Check:

TFT wiring

correct display driver in TFT_eSPI

User_Setup.h configuration

backlight power

board selection and upload success


Compilation errors around TFT_eSPI

Usually caused by:

missing library installation

incorrect board/platform configuration

incorrect TFT_eSPI setup files


Shape flickers or disappears

Possible causes:

object moves behind the camera due to projection settings

screen is being cleared before the object is fully redrawn

display timing too fast or too slow for the panel



---

File overview

Typical single-file sketch structure:

includes

global constants

trig table and helper functions

symbolic data structures

morphing and projection engine

global engine / display objects

setup()

loop()



---

License

Add the license that matches your project needs.

Common choices:

MIT

BSD-2-Clause

Apache-2.0



---

Summary

This project is a compact embedded visualization engine that combines:

fixed-point arithmetic

symbolic transform history

morphing between vector programs

realtime TFT rendering


It is equal parts graphics demo and experimental algebraic machine.


---

Example project description

> A symbolic 3D wireframe engine for Arduino-class hardware where geometry is represented as algebraic terms, transformations are recorded as history, and shape interpolation produces animated morphing between program states.
