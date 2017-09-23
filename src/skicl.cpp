// skicl.cpp
//

#include "skicl.h"
#include <iostream>
#include <stdexcept>

using namespace std;

int main (int argc, char* argv[]) {

	Node_ptr env = init_env ();
	repl (env);
	return 0;
}