// The battle map's bridges (WBridge 0x497890-0x498750 on the base 0x4862C0-0x4868A0; the three kinds, scenery
// types 445-447, are 0x48D050 and its neighbours): a straight run of WOF segments from one end to the other, each
// segment's ends rebuilt every update from a cross-section of six points so the deck runs level with its ends and
// ramps down to the ground at both. See docs/BATTLEMAP.md.
#pragma once
#include "core/Landscape.h"
#include <cstdint>

namespace wow
{
    // A cross-section (0x48D2F0, laid out as the game's): six world points, x, y and height, then their screen
    // points, x, the iso projection and depth (0x498300). 0 to 2 are one side (the ground, the deck, the rail's top),
    // 3 to 5 the other.
    struct BridgeJoint
    {
        int32_t world[6][3] = {};
        int16_t screen[6][3] = {};
    };

    class Bridge
    {
    public:
        // 0x48D0A0 (the saved ends, each clamped into the map) and 0x486490 / 0x497900 (each end moved onto the
        // vertex grid, then the heading, the length and the number of segments)
        static Bridge Create(const Landscape& land, int ax, int ay, int bx, int by);

        int Heading() const { return heading; }    // +0x40, 4096 a turn, from 0x43FDA0
        int Length() const { return length; }      // +0xD8
        int Count() const { return count; }        // +0xF0
        int EndX(int e) const { return e ? bx : ax; }
        int EndY(int e) const { return e ? by : ay; }

        // 0x497900: the world point of segment i, the first half a step from end A, each next a step on, worked
        // as the game does: relative to the bridge's position (px, py) in floats
        void SegmentAt(int i, float px, float py, int& x, int& y) const;

        // 0x48D2F0: the cross-section at dist along the bridge (joint index i of 0 to Count()); the two end joints
        // sit on the ground (the deck there 4 above it), the rest have the deck 8 and the rail 16 over the higher
        // of the straight line between the ends' heights and the ground under them
        BridgeJoint Joint(const Landscape& land, float dist, int index) const;

        // 0x498110: the distance of joint i, the segment length added up as a float
        float JointDistance(int i) const;

    private:
        int ax = 0, ay = 0, bx = 0, by = 0;   // +0xC8..+0xD4
        int heading = 0, length = 0, count = 0;
    };

    // 0x43FDA0: the heading (4096 a turn, -2048 to 2047) and length from one point to another
    void HeadingAndLength(float x0, float y0, float x1, float y1, int& heading, float& length);
}
