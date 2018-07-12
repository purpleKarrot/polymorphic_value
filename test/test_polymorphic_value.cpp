// Copyright (c) 2016-2017 Jonathan B. Coe
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#define BOOST_TEST_MODULE polymorphic_value
#include "boost/polymorphic_value.hpp"
#include <boost/math/constants/constants.hpp>
#include <boost/test/included/unit_test.hpp>

#include <new>
#include <stdexcept>

using boost::assume_polymorphic_value;
using boost::make_polymorphic_value;
using boost::polymorphic_value;
using boost::polymorphic_value_cast;

using namespace std::string_literals;

namespace {

  struct Shape {
    bool moved_from = false;
    virtual const char* name() const = 0;
    virtual double area() const = 0;

    virtual ~Shape() = default;
    Shape() = default;
    Shape(const Shape&) = default;
    Shape& operator=(const Shape&) = default;

    Shape(Shape&& s) { s.moved_from = true; }

    Shape& operator=(Shape&& s) {
      s.moved_from = true;
      return *this;
    }
  };

  class Square : public Shape {
    double side_;

  public:
    Square(double side) : side_(side) {}
    const char* name() const override { return "square"; }
    double area() const override { return side_ * side_; }
  };

  class Circle : public Shape {
    double radius_;

  public:
    Circle(double radius) : radius_(radius) {}
    const char* name() const override { return "circle"; }
    double area() const override {
      return boost::math::constants::pi<double>() * radius_ * radius_;
    }
  };


} // end namespace

BOOST_AUTO_TEST_CASE(empty_upon_default_construction) {
  polymorphic_value<Shape> pv;

  BOOST_TEST(!bool(pv));
}

BOOST_AUTO_TEST_CASE(support_for_incomplete_types) {
  class Foo;
  polymorphic_value<Foo> pv;

  BOOST_TEST(!bool(pv));
}

BOOST_AUTO_TEST_CASE(non_empty_upon_value_construction) {
  auto pv = make_polymorphic_value<Square>(2);

  BOOST_TEST(bool(pv));
}

BOOST_AUTO_TEST_CASE(pointer_like_methods_access_owned_object) {
  auto pv = make_polymorphic_value<Square>(2);

  BOOST_TEST(pv->area() == 4);
}

BOOST_AUTO_TEST_CASE(const_propagation) {
  auto pv = make_polymorphic_value<Square>(2);
  static_assert(std::is_same<Square*, decltype(pv.operator->())>::value, "");
  static_assert(std::is_same<Square&, decltype(pv.operator*())>::value, "");

  const auto cpv = make_polymorphic_value<Square>(2);
  static_assert(std::is_same<const Square*, decltype(cpv.operator->())>::value,
                "");
  static_assert(std::is_same<const Square&, decltype(cpv.operator*())>::value,
                "");
}

BOOST_AUTO_TEST_CASE(copy_constructor) {
  auto pv = make_polymorphic_value<Square>(2);
  auto pv2 = pv;

  BOOST_TEST(pv.operator->() != pv2.operator->());
  BOOST_TEST(pv2->area() == 4);
  BOOST_TEST(dynamic_cast<Square*>(pv2.operator->()));
}

BOOST_AUTO_TEST_CASE(copy_assignment) {
  auto pv = make_polymorphic_value<Square>(2);
  polymorphic_value<Square> pv2;
  pv2 = pv;

  BOOST_TEST(pv.operator->() != pv2.operator->());
  BOOST_TEST(pv2->area() == 4);
  BOOST_TEST(dynamic_cast<Square*>(pv2.operator->()));
}

BOOST_AUTO_TEST_CASE(move_constructor) {
  auto pv = make_polymorphic_value<Square>(2);
  const auto* p = pv.operator->();

  polymorphic_value<Square> pv2(std::move(pv));

  BOOST_TEST(!pv);
  BOOST_TEST(pv2.operator->() == p);
  BOOST_TEST(pv2->area() == 4);
  BOOST_TEST(dynamic_cast<Square*>(pv2.operator->()));
}

BOOST_AUTO_TEST_CASE(move_assignment) {
  auto pv = make_polymorphic_value<Square>(2);
  const auto* p = pv.operator->();

  polymorphic_value<Square> pv2;
  pv2 = std::move(pv);

  BOOST_TEST(!pv);
  BOOST_TEST(pv2.operator->() == p);
  BOOST_TEST(pv2->area() == 4);
  BOOST_TEST(dynamic_cast<Square*>(pv2.operator->()));
}

BOOST_AUTO_TEST_CASE(swap) {
  auto square = make_polymorphic_value<Shape, Square>(2);
  auto circle = make_polymorphic_value<Shape, Circle>(2);

  BOOST_TEST(square->name() == "square"s);
  BOOST_TEST(circle->name() == "circle"s);

  using std::swap;
  swap(square, circle);

  BOOST_TEST(square->name() == "circle"s);
  BOOST_TEST(circle->name() == "square"s);
}

BOOST_AUTO_TEST_CASE(member_swap) {
  auto square = make_polymorphic_value<Shape, Square>(2);
  auto circle = make_polymorphic_value<Shape, Circle>(2);

  BOOST_TEST(square->name() == "square"s);
  BOOST_TEST(circle->name() == "circle"s);

  using std::swap;
  square.swap(circle);

  BOOST_TEST(square->name() == "circle"s);
  BOOST_TEST(circle->name() == "square"s);
}

BOOST_AUTO_TEST_CASE(multiple_inheritance_with_virtual_base_classes) {
  // This is "Gustafsson's dilemma" and revealed a serious problem in an early
  // implementation.
  // Thanks go to Bengt Gustaffson.
  struct Base {
    int v_ = 42;
    virtual ~Base() = default;
  };
  struct IntermediateBaseA : virtual Base {
    int a_ = 3;
  };
  struct IntermediateBaseB : virtual Base {
    int b_ = 101;
  };
  struct MultiplyDerived : IntermediateBaseA, IntermediateBaseB {
    int value_ = 0;
    MultiplyDerived(int value) : value_(value){};
  };

  // Given a value-constructed multiply-derived-class polymorphic_value.
  int v = 7;
  auto cptr = assume_polymorphic_value<MultiplyDerived>(new MultiplyDerived(v));

  // Then when copied to a polymorphic_value to an intermediate base type, data
  // is accessible as expected.
  auto cptr_IA = polymorphic_value_cast<IntermediateBaseA>(cptr);
  BOOST_TEST(cptr_IA->a_ == 3);
  BOOST_TEST(cptr_IA->v_ == 42);

  // Then when copied to a polymorphic_value to an alternate intermediate base
  // type data is accessible as expected.
  auto cptr_IB = polymorphic_value_cast<IntermediateBaseB>(cptr);
  BOOST_TEST(cptr_IB->b_ == 101);
  BOOST_TEST(cptr_IB->v_ == 42);
}

BOOST_AUTO_TEST_CASE(dynamic_and_static_type_mismatch_throws_exception) {

  class UnitSquare : public Square {
  public:
    UnitSquare() : Square(1) {}
    double area() const { return 1.0; }
    const char* name() const { return "unit-square"; }
  };
  UnitSquare u;
  Square* s = &u;

  BOOST_CHECK_THROW([s] { return assume_polymorphic_value<Shape>(s); }(),
                    boost::bad_polymorphic_value_construction);
}

BOOST_AUTO_TEST_CASE(custom_copy_and_delete) {
  size_t copy_count = 0;
  size_t deletion_count = 0;
  auto pv = assume_polymorphic_value<Square>(new Square(2),
                                             [&](const Square& d) {
                                               ++copy_count;
                                               return new Square(d);
                                             },
                                             [&](const Square* d) {
                                               ++deletion_count;
                                               delete d;
                                             });
  // Restrict scope.
  {
    auto pv2 = pv;
    BOOST_TEST(copy_count == 1);
  }
  BOOST_TEST(deletion_count == 1);
}

// NOTE: This test passes because of implementation detail, not because of the
// contract of polymorphic_value. polymorphic_value does not guarantee reference
// stability after a move.
BOOST_AUTO_TEST_CASE(reference_stability) {
  struct tiny_t {};
  auto pv = make_polymorphic_value<tiny_t>();
  tiny_t* p = pv.operator->();

  auto moved_pv = std::move(pv);
  auto moved_p = moved_pv.operator->();

  // This will fail if a small-object optimisation is in place.
  BOOST_TEST(p == moved_p);
}

BOOST_AUTO_TEST_CASE(copy_polymorphic_value_T_from_polymorphic_value_const_T)
{
  auto cp = make_polymorphic_value<const int>(1);
  polymorphic_value<int> p = polymorphic_value_cast<int>(cp);

  BOOST_TEST(*p==1);
}

BOOST_AUTO_TEST_CASE(no_dangling_reference_in_forwarding_constuctor)
{
  int x = 7;
  int& rx = x;
  auto p = make_polymorphic_value<int>(rx);

  x = 6;
  BOOST_TEST(*p == 7);
}

