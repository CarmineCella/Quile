// skicl.cpp
//

#include "skicl.h"
#include <iostream>
#include <stdexcept>

using namespace std;

int main (int argc, char* argv[]) {

	Node_ptr n = Node::create (QUOTE);
	repl (n);
	return 0;
}