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
#include <cassert>
#include <stdarg.h>
#include "treenode.h"

#include "binary-expression.h"
#include "conditional-expression.h"
#include "constant.h"
#include "volume.h"
#include <iostream>

namespace tuvok { namespace expression {

Node::~Node() { this->children.clear(); }
void Node::AddChild(Node* n)
{
  this->children.push_back(std::shared_ptr<Node>(n));
}

const std::shared_ptr<Node> Node::GetChild(size_t index) const
{
  assert(index < this->children.size());
  return this->children[index];
}

Node::citer Node::begin() const { return this->children.begin(); }
Node::citer Node::end() const { return this->children.end(); }

void Node::Print(std::ostream& os) const { os << "Node"; }

// only 'volume's need this... we just forward it here.
void Node::SetVolumes(const std::vector<VariantArray>& vols) {
  for(Node::citer i = this->begin(); i != this->end(); ++i) {
    (*i)->SetVolumes(vols);
  }
}

static Node* node_factory(NodeType);

Node* make_node(NodeType nt, ...)
{
  va_list ap;

  Node* n = node_factory(nt);
  assert(n);

  // Now take a null-terminated argument list.
  Node *iter;
  va_start(ap, nt);
  do {
    iter = va_arg(ap, Node*);
    if(iter) {
      n->AddChild(iter);
    }
  } while(iter);
  va_end(ap);

  return n;
}

static Node* node_factory(NodeType nt)
{
  switch(nt) {
    case EXPR_CONSTANT:    return new Constant();
    case EXPR_VOLUME:      return new Volume();
    case EXPR_BINARY:      return new BinaryExpression();
    case EXPR_CONDITIONAL: return new ConditionalExpression();
  }
  return NULL;
}

}}
