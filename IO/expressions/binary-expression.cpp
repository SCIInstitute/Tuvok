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
#include <cmath>

#include "binary-expression.h"
#include "constant.h"

namespace tuvok { namespace expression {

void BinaryExpression::SetOperator(enum OpType o) { this->oper = o; }
void BinaryExpression::Analyze() const {
  // If they gave us a divide, check for division by zero.
  if(this->oper == OP_DIVIDE) {
    const std::shared_ptr<Node> rhs = this->GetChild(1);
    const std::shared_ptr<Constant> c =
      std::dynamic_pointer_cast<Constant>(rhs);
    if(c->GetValue() == 0.0) {
      throw semantic::DivisionByZero("Cannot divide by zero.", __FILE__,
                                     __LINE__);
    }
  }
}

void BinaryExpression::Print(std::ostream& os) const {
  os << "BinaryExpression(";
  this->GetChild(0)->Print(os);
  switch(this->oper) {
    case OP_PLUS:     os << " + "; break;
    case OP_MINUS:    os << " - "; break;
    case OP_DIVIDE:   os << " / "; break;
    case OP_MULTIPLY: os << " * "; break;
    case OP_GREATER_THAN: os << " > "; break;
    case OP_LESS_THAN: os << " < "; break;
    case OP_EQUAL_TO: os << " = "; break;
  }
  this->GetChild(1)->Print(os);
  os << ")";
}

static bool fp_equal(double a, double b) {
  return (fabs(a-b) < 0.001);
}

double BinaryExpression::Evaluate(size_t i) const {
  double lhs, rhs;
  lhs = this->GetChild(0)->Evaluate(i);
  rhs = this->GetChild(1)->Evaluate(i);
  switch(this->oper) {
    case OP_PLUS:         return lhs + rhs;
    case OP_MINUS:        return lhs - rhs;
    case OP_DIVIDE:       return lhs / rhs;
    case OP_MULTIPLY:     return lhs * rhs;
    case OP_GREATER_THAN: return lhs > rhs;
    case OP_LESS_THAN:    return lhs < rhs;
    case OP_EQUAL_TO:     return fp_equal(lhs, rhs);
  }
  assert(1 == 0);
  return 0.0;
}

}}
