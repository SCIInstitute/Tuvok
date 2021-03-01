// Small/hacky test program for expressions.
#include <cstdint>
#include <iostream>
#include "parser.h"
#include "treenode.h"

using namespace tuvok::expression;
extern Node* parser_tree_root();
extern void parser_free();

// Function pointer for traversals.  First argument is the node, second
// argument is "user data"; a given traversal function should cast it
// to whatever is needed.
/// @todo fixme should probably be a std::function or something like that.
typedef bool (*TraversalFunc)(const std::shared_ptr<Node> n, void* data);

bool print_tree(const std::shared_ptr<Node> n, void*);
void inorder(const std::shared_ptr<Node> node, TraversalFunc f, void*);

int main() {
  int x = yyparse();
  if(x != 1) {
    // NullDeleter: "parser" owns that memory; it'll free it with parser_free.
    std::shared_ptr<Node> tree = std::shared_ptr<Node>(
      parser_tree_root(), NullDeleter<Node>
    );
    inorder(tree, print_tree, NULL);

    std::vector<std::vector<int8_t>> inputs;
    inputs.resize(1);
    inputs[0].resize(128);
    std::vector<int8_t> output;
    evaluate<int8_t>(*tree, inputs, output);
  }

  parser_free();
  return 0;
}

// perform an inorder traversal.  The traversal function can return
// false to terminate the recursion early.
void inorder(const std::shared_ptr<Node> node, TraversalFunc f, void* data)
{
  if(f(node, data) == false) {
    return;
  }
  std::cerr << "traversing " << std::distance(node->begin(), node->end())
            << " children.\n";
  for(Node::citer iter = node->begin(); iter != node->end(); ++iter) {
    inorder(*iter, f, data);
  }
}

bool print_tree(const std::shared_ptr<Node> n, void*)
{
  n->Print(std::cout);
  std::cout << "\n";
  return true;
}
