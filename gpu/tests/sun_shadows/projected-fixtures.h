#pragma once
struct ProjectedFixture {
    unsigned event, count, attributes;
    bool depth_export;
};
inline constexpr ProjectedFixture projected_fixtures[] = {
    {9376, 6, 3, false},  {9446, 6, 3, false},  {9472, 6, 3, false},  {9486, 6, 3, false},
    {9500, 6, 3, false},  {9514, 6, 3, false},  {11467, 6, 4, false}, {11474, 6, 4, false},
    {11497, 6, 4, false}, {11504, 6, 4, false}, {11511, 6, 4, false}, {11869, 6, 4, false},
    {14498, 6, 4, false}, {14522, 6, 4, false}, {16724, 6, 4, false}, {16878, 6, 3, false},
    {17128, 6, 3, false}, {17517, 6, 4, false}, {17525, 6, 4, false}};
