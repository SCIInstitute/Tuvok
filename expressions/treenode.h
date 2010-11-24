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
/// \brief Base class for a node in an AST.
#ifndef TUVOK_TREENODE_H
#define TUVOK_TREENODE_H

#include <cassert>
#include <iterator>
#ifdef _MSC_VER
# include <memory>
#else
# include <tr1/memory>
#endif
#include <vector>
#include <ostream>

#include "semantic.h"
#include "IO/VariantArray.h"

namespace tuvok { namespace expression {

class Node {
  public:
    virtual ~Node();
    void AddChild(Node*);
    /// For iteration over all children in the tree.  Order is
    /// "leftmost" to "rightmost".
    ///@{
    typedef std::vector<std::tr1::shared_ptr<Node> >::const_iterator citer;
    citer begin() const;
    citer end() const;
    ///@}

    /// Performs semantic analysis on the expression AST.
    virtual void Analyze() const throw(semantic::Error)=0;

    /// Print out the name of the node type.
    virtual void Print(std::ostream&) const;

    /// Gives the input volume to the tree; needed so that e.g. 'v[i]'
    /// can be pulled from somewhere.
    virtual void SetVolumes(const std::vector<VariantArray>&);

    virtual double Evaluate(size_t idx) const=0;

  protected:
    const std::tr1::shared_ptr<Node> GetChild(size_t index) const;

  private:
    std::vector<std::tr1::shared_ptr<Node> > children;
};

namespace { template<typename T> void NullDeleter(T*) {} }

/// Evaluates the expression.
/// @param tree: the AST for the expression
/// @param volumes: input volumes
/// @param output: the output volume.
template<typename T>
void evaluate(Node& tree,
              std::vector<std::vector<T> >& volumes,
              std::vector<T>& output)
{
  // First make sure the volumes make sense.
  assert(!volumes.empty());
  const size_t rootsize = volumes[0].size();
  for(size_t i=0; i < volumes.size(); ++i) {
    // hack, this should throw something instead.
    assert(volumes[i].size() == rootsize);
  }

  // create vec of VariantArrays to set for the tree's volumes.
  std::vector<VariantArray> vols(volumes.size());
  for(size_t i=0; i < volumes.size(); ++i) {
    vols[i].set(std::tr1::shared_ptr<T>(&(volumes[i].at(0)), NullDeleter<T>),
                volumes[i].size());
  }
  tree.SetVolumes(vols);

  output.resize(rootsize);
  typedef typename std::vector<T>::iterator oiter;
  for(oiter o = output.begin(); o != output.end(); ++o) {
    // iter through each, create expression, eval.
    // This cast isn't strictly valid.  True, we calculated the width of T
    // before calling this, but we based that purely on the types: a
    // combination of three uint16_t volumes will give a uint16_t volume, even
    // though it might need a uint32_t volume to represent that data.  A
    // division would mean we'd probably want to output a floating point
    // volume, too.
    // Anyway, we'll want this cast to shut the compiler up even after we fix
    // type calculation (see IdentifyType).
    *o = static_cast<T>(tree.Evaluate(std::distance(output.begin(), o)));
  }
}

enum OpType {
  OP_PLUS,
  OP_MINUS,
  OP_DIVIDE,
  OP_MULTIPLY,
  OP_GREATER_THAN,
  OP_LESS_THAN,
  OP_EQUAL_TO
};

enum NodeType {
  EXPR_VOLUME,
  EXPR_CONSTANT,
  EXPR_BINARY,
  EXPR_CONDITIONAL
};

Node* make_node(NodeType, ...);

}}

#endif // TUVOK_TREENODE_H
