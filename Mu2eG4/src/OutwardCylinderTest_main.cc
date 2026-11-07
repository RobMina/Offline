#include "Offline/Mu2eG4/inc/OutwardCylinder.hh"

#include <cmath>
#include <iostream>
#include <limits>

namespace {
  struct Vector {
    double a, b, c;
    double x() const { return a; }
    double y() const { return b; }
    double z() const { return c; }
  };

  struct Case {
    const char* name;
    Vector position;
    Vector direction;
    bool expected;
  };
}

int main() {
  using mu2e::Mu2eG4Cuts::OutwardCylinder;
  const OutwardCylinder cylinder{{{0., 0.}}, 5., -2., 8.};
  const double inf = std::numeric_limits<double>::infinity();
  const double tiny = std::nextafter(0., 1.);
  const Case cases[] = {
    {"inside center", {0., 0., 0.}, {1., 0., 0.}, false},
    {"inside near side", {4., 0., 0.}, {1., 0., 0.}, false},
    {"side boundary outward", {5., 0., 0.}, {1., 0., 0.}, false},
    {"side boundary inward", {5., 0., 0.}, {-1., 0., 0.}, false},
    {"side boundary tangent", {5., 0., 0.}, {0., 1., 0.}, false},
    {"negative side boundary", {-5., 0., 0.}, {-1., 0., 0.}, false},
    {"oblique side boundary", {3., 4., 0.}, {.6, .8, 0.}, false},
    {"side outside outward", {6., 0., 0.}, {1., 0., 0.}, true},
    {"negative side outward", {-6., 0., 0.}, {-1., 0., 0.}, true},
    {"y side outward", {0., -6., 0.}, {0., -1., 0.}, true},
    {"oblique side outward", {6., 8., 0.}, {.6, .8, 0.}, true},
    {"side outside inward", {6., 0., 0.}, {-1., 0., 0.}, false},
    {"side outside tangent", {6., 0., 0.}, {0., 1., 0.}, false},
    {"side outside axial", {6., 0., 0.}, {0., 0., 1.}, false},
    {"side zero direction", {6., 0., 0.}, {0., 0., 0.}, false},
    {"lower boundary outward", {0., 0., -2.}, {0., 0., -1.}, false},
    {"upper boundary outward", {0., 0., 8.}, {0., 0., 1.}, false},
    {"lower outside outward", {0., 0., -3.}, {0., 0., -1.}, true},
    {"upper outside outward", {0., 0., 9.}, {0., 0., 1.}, true},
    {"lower outside inward", {0., 0., -3.}, {0., 0., 1.}, false},
    {"upper outside inward", {0., 0., 9.}, {0., 0., -1.}, false},
    {"lower outside tangent", {0., 0., -3.}, {1., 0., 0.}, false},
    {"upper outside tangent", {0., 0., 9.}, {1., 0., 0.}, false},
    {"upper corner boundary", {5., 0., 8.}, {.6, 0., .8}, false},
    {"lower corner boundary", {5., 0., -2.}, {.6, 0., -.8}, false},
    {"upper corner radial out axial in", {6., 0., 9.}, {.6, 0., -.8}, true},
    {"upper corner radial in axial out", {6., 0., 9.}, {-.6, 0., .8}, true},
    {"upper corner both in", {6., 0., 9.}, {-.6, 0., -.8}, false},
    {"lower corner radial out axial in", {6., 0., -3.}, {.6, 0., .8}, true},
    {"lower corner radial in axial out", {6., 0., -3.}, {-.6, 0., -.8}, true},
    {"lower corner both in", {6., 0., -3.}, {-.6, 0., .8}, false},
    {"corner tangent", {6., 0., 9.}, {0., 1., 0.}, false},
    {"side boundary cap outside", {5., 0., 9.}, {0., 0., 1.}, true},
    {"cap boundary side outside", {6., 0., 8.}, {1., 0., 0.}, true},
    {"side nextafter outside", {std::nextafter(5., inf), 0., 0.}, {1., 0., 0.}, true},
    {"side nextafter inside", {std::nextafter(5., 0.), 0., 0.}, {1., 0., 0.}, false},
    {"lower nextafter outside", {0., 0., std::nextafter(-2., -inf)}, {0., 0., -1.}, true},
    {"lower nextafter inside", {0., 0., std::nextafter(-2., 0.)}, {0., 0., -1.}, false},
    {"upper nextafter outside", {0., 0., std::nextafter(8., inf)}, {0., 0., 1.}, true},
    {"upper nextafter inside", {0., 0., std::nextafter(8., 0.)}, {0., 0., 1.}, false},
    {"radial just outward", {6., 0., 0.}, {tiny, 1., 0.}, true},
    {"radial just inward", {6., 0., 0.}, {-tiny, 1., 0.}, false},
    {"upper just outward", {0., 0., 9.}, {1., 0., tiny}, true},
    {"upper just inward", {0., 0., 9.}, {1., 0., -tiny}, false},
    {"lower just outward", {0., 0., -3.}, {1., 0., -tiny}, true},
    {"lower just inward", {0., 0., -3.}, {1., 0., tiny}, false}
  };
  int failures = 0;
  const auto check = [&failures](const char* name, bool result, bool expected) {
    if(result != expected) {
      std::cerr << name << ": expected " << expected << ", got " << result << '\n';
      ++failures;
    }
  };
  for(const auto& test : cases) {
    const auto& p = test.position;
    const auto& u = test.direction;
    check(test.name, cylinder.leaving(p.x(), p.y(), p.z(), u.x(), u.y(), u.z()), test.expected);
  }

  const OutwardCylinder shifted{{{3., -4.}}, 5., -2., 8.};
  const Vector origin{100., -200., 300.};
  const Case translated[] = {
    {"translated inside", {103., -204., 300.}, {1., 0., 0.}, false},
    {"translated side boundary", {108., -204., 300.}, {1., 0., 0.}, false},
    {"translated side outward", {109., -204., 300.}, {1., 0., 0.}, true},
    {"translated side inward", {109., -204., 300.}, {-1., 0., 0.}, false},
    {"translated y side outward", {103., -210., 300.}, {0., -1., 0.}, true},
    {"translated side tangent", {109., -204., 300.}, {0., 1., 0.}, false},
    {"translated lower boundary", {103., -204., 298.}, {0., 0., -1.}, false},
    {"translated upper boundary", {103., -204., 308.}, {0., 0., 1.}, false},
    {"translated lower outward", {103., -204., 297.}, {0., 0., -1.}, true},
    {"translated upper outward", {103., -204., 309.}, {0., 0., 1.}, true},
    {"translated upper inward", {103., -204., 309.}, {0., 0., -1.}, false},
    {"translated corner outward", {109., -204., 309.}, {-.6, 0., .8}, true}
  };
  for(const auto& test : translated) {
    check(test.name, shifted.leavingWorld(test.position, test.direction, origin), test.expected);
  }

  check("valid cylinder", cylinder.valid(), true);
  check("valid shifted cylinder", shifted.valid(), true);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const OutwardCylinder invalid[] = {
    {{{nan, 0.}}, 5., -2., 8.}, {{{0., nan}}, 5., -2., 8.},
    {{{inf, 0.}}, 5., -2., 8.}, {{{0., -inf}}, 5., -2., 8.},
    {{{0., 0.}}, 0., -2., 8.}, {{{0., 0.}}, -1., -2., 8.},
    {{{0., 0.}}, nan, -2., 8.}, {{{0., 0.}}, inf, -2., 8.},
    {{{0., 0.}}, 5., nan, 8.}, {{{0., 0.}}, 5., -2., nan},
    {{{0., 0.}}, 5., -inf, 8.}, {{{0., 0.}}, 5., -2., inf},
    {{{0., 0.}}, 5., 8., 8.}, {{{0., 0.}}, 5., 9., 8.}
  };
  for(const auto& bad : invalid) {
    check("invalid cylinder", bad.valid(), false);
  }
  return failures ? 1 : 0;
}
