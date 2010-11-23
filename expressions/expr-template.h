/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2010 Scientific Computing and Imaging Institute,
   University of Utah.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/
#ifndef TUVOK_EXPRESSION_TEMPLATES_H
#define TUVOK_EXPRESSION_TEMPLATES_H

namespace tuvok { namespace expression {

struct etBaseExpression {
  virtual double operator() (double)=0;
};

struct etVolume {
  double operator() (double v) { return v; } // fixme
};

struct etConstant : public etBaseExpression {
  double constant;
  etConstant(double d): constant(d) {}
  double operator() (double) { return this->constant; }
};

template<class A, class B, class Oper>
struct etBinaryExpression {
  A a; B b;
  etBinaryExpression(A _a, B _b) : a(_a), b(_b) {}
  double operator() (double d) { return Oper::apply(a(d), b(d)); }
};

/// Operations one can apply to data.
///@{
struct etAdd {
  static double apply(double a, double b) { return a + b; }
};
struct etSubtract {
  static double apply(double a, double b) { return a - b; }
};
struct etMultiply {
  static double apply(double a, double b) { return a * b; }
};
struct etDivide {
  static double apply(double a, double b) { return a / b; }
};
///@}

template <class Expr>
struct etExpression: public etBaseExpression {
  Expr expr;
  etExpression(Expr e) : expr(e) {}
  double operator() (double d) { return expr(d); }
};

template<class L, class R>
etExpression<etBinaryExpression<etExpression<L>, etExpression<R>, etAdd> >
operator+ (etExpression<L> l, etExpression<R> r) {
  typedef etBinaryExpression<etExpression<L>, etExpression<R>, etAdd> eType;
  return etExpression<eType>(eType(l, r));
}

template<class L, class R>
etExpression<etBinaryExpression<etExpression<L>, etExpression<R>,
             etSubtract> >
operator- (etExpression<L> l, etExpression<R> r) {
  typedef etBinaryExpression<etExpression<L>, etExpression<R>,
                             etSubtract> eType;
  return etExpression<eType>(eType(l, r));
}


#if 0
  typedef etExpression<etVolume> volume;
  typedef etExpression<etConstant> constant;
  typedef etBinaryExpression<volume, constant, etAdd> addvc;
  typedef etExpression<addvc> adder;

  volume x(etVolume());
  constant c(Constant(4.2));
  addvc addxc(x, c);
  adder expr(addxc);
#endif

}}

#endif // TUVOK_EXPRESSION_TEMPLATES_H
